/**
 *    Copyright (C) 2018-present MongerDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongerDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongerdb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::monger::logger::LogComponent::kIndex

#include "monger/platform/basic.h"

#include "monger/db/ttl.h"

#include "monger/base/counter.h"
#include "monger/db/auth/authorization_session.h"
#include "monger/db/auth/user_name.h"
#include "monger/db/catalog/collection.h"
#include "monger/db/catalog/collection_catalog_entry.h"
#include "monger/db/catalog/database_holder.h"
#include "monger/db/catalog/index_catalog.h"
#include "monger/db/client.h"
#include "monger/db/commands/fsync_locked.h"
#include "monger/db/commands/server_status_metric.h"
#include "monger/db/concurrency/write_conflict_exception.h"
#include "monger/db/db_raii.h"
#include "monger/db/exec/delete.h"
#include "monger/db/index/index_descriptor.h"
#include "monger/db/namespace_string.h"
#include "monger/db/ops/insert.h"
#include "monger/db/query/internal_plans.h"
#include "monger/db/repl/replication_coordinator.h"
#include "monger/db/service_context.h"
#include "monger/db/storage/durable_catalog.h"
#include "monger/db/ttl_collection_cache.h"
#include "monger/db/ttl_gen.h"
#include "monger/util/background.h"
#include "monger/util/concurrency/idle_thread_block.h"
#include "monger/util/exit.h"
#include "monger/util/log.h"

namespace monger {

MONGO_FAIL_POINT_DEFINE(hangTTLMonitorWithLock);

Counter64 ttlPasses;
Counter64 ttlDeletedDocuments;

ServerStatusMetricField<Counter64> ttlPassesDisplay("ttl.passes", &ttlPasses);
ServerStatusMetricField<Counter64> ttlDeletedDocumentsDisplay("ttl.deletedDocuments",
                                                              &ttlDeletedDocuments);

class TTLMonitor : public BackgroundJob {
public:
    TTLMonitor(ServiceContext* serviceContext) : _serviceContext(serviceContext) {}
    virtual ~TTLMonitor() {}

    virtual std::string name() const {
        return "TTLMonitor";
    }

    static std::string secondsExpireField;

    virtual void run() {
        ThreadClient tc(name(), _serviceContext);
        AuthorizationSession::get(cc())->grantInternalAuthorization(&cc());

        {
            stdx::lock_guard<Client> lk(*tc.get());
            tc.get()->setSystemOperationKillable(lk);
        }

        while (!globalInShutdownDeprecated()) {
            {
                MONGO_IDLE_THREAD_BLOCK;
                sleepsecs(ttlMonitorSleepSecs.load());
            }

            LOG(3) << "thread awake";

            if (!ttlMonitorEnabled.load()) {
                LOG(1) << "disabled";
                continue;
            }

            if (lockedForWriting()) {
                // Note: this is not perfect as you can go into fsync+lock between this and actually
                // doing the delete later.
                LOG(3) << "locked for writing";
                continue;
            }

            try {
                doTTLPass();
            } catch (const WriteConflictException&) {
                LOG(1) << "got WriteConflictException";
            } catch (const ExceptionForCat<ErrorCategory::Interruption>& interruption) {
                LOG(1) << "TTLMonitor was interrupted: " << interruption;
            }
        }
    }

private:
    void doTTLPass() {
        const ServiceContext::UniqueOperationContext opCtxPtr = cc().makeOperationContext();
        auto durableCatalog = DurableCatalog::get(opCtxPtr.get());
        OperationContext& opCtx = *opCtxPtr;

        // If part of replSet but not in a readable state (e.g. during initial sync), skip.
        if (repl::ReplicationCoordinator::get(&opCtx)->getReplicationMode() ==
                repl::ReplicationCoordinator::modeReplSet &&
            !repl::ReplicationCoordinator::get(&opCtx)->getMemberState().readable())
            return;

        TTLCollectionCache& ttlCollectionCache = TTLCollectionCache::get(getGlobalServiceContext());
        std::vector<std::string> ttlCollections = ttlCollectionCache.getCollections();
        std::vector<BSONObj> ttlIndexes;

        ttlPasses.increment();

        // Get all TTL indexes from every collection.
        for (const std::string& collectionNS : ttlCollections) {
            NamespaceString collectionNSS(collectionNS);
            AutoGetCollection autoGetCollection(&opCtx, collectionNSS, MODE_IS);
            Collection* coll = autoGetCollection.getCollection();
            if (!coll) {
                // Skip since collection has been dropped.
                continue;
            }

            std::vector<std::string> indexNames;
            durableCatalog->getAllIndexes(&opCtx, coll->ns(), &indexNames);
            for (const std::string& name : indexNames) {
                BSONObj spec = durableCatalog->getIndexSpec(&opCtx, coll->ns(), name);
                if (spec.hasField(secondsExpireField)) {
                    ttlIndexes.push_back(spec.getOwned());
                }
            }
        }

        for (const BSONObj& idx : ttlIndexes) {
            try {
                doTTLForIndex(&opCtx, idx);
            } catch (const ExceptionForCat<ErrorCategory::Interruption>&) {
                warning() << "TTLMonitor was interrupted, waiting " << ttlMonitorSleepSecs.load()
                          << " seconds before doing another pass";
                return;
            } catch (const DBException& dbex) {
                error() << "Error processing ttl index: " << idx << " -- " << dbex.toString();
                // Continue on to the next index.
                continue;
            }
        }
    }

    /**
     * Remove documents from the collection using the specified TTL index after a sufficient amount
     * of time has passed according to its expiry specification.
     */
    void doTTLForIndex(OperationContext* opCtx, BSONObj idx) {
        const NamespaceString collectionNSS(idx["ns"].String());
        if (collectionNSS.isDropPendingNamespace()) {
            return;
        }
        if (!userAllowedWriteNS(collectionNSS).isOK()) {
            error() << "namespace '" << collectionNSS
                    << "' doesn't allow deletes, skipping ttl job for: " << idx;
            return;
        }

        const BSONObj key = idx["key"].Obj();
        const StringData name = idx["name"].valueStringData();
        if (key.nFields() != 1) {
            error() << "key for ttl index can only have 1 field, skipping ttl job for: " << idx;
            return;
        }

        LOG(1) << "ns: " << collectionNSS << " key: " << key << " name: " << name;

        AutoGetCollection autoGetCollection(opCtx, collectionNSS, MODE_IX);
        if (MONGO_FAIL_POINT(hangTTLMonitorWithLock)) {
            log() << "Hanging due to hangTTLMonitorWithLock fail point";
            MONGO_FAIL_POINT_PAUSE_WHILE_SET_OR_INTERRUPTED(opCtx, hangTTLMonitorWithLock);
        }


        Collection* collection = autoGetCollection.getCollection();
        if (!collection) {
            // Collection was dropped.
            return;
        }

        if (!repl::ReplicationCoordinator::get(opCtx)->canAcceptWritesFor(opCtx, collectionNSS)) {
            return;
        }

        const IndexDescriptor* desc = collection->getIndexCatalog()->findIndexByName(opCtx, name);
        if (!desc) {
            LOG(1) << "index not found (index build in progress? index dropped?), skipping "
                   << "ttl job for: " << idx;
            return;
        }

        // Re-read 'idx' from the descriptor, in case the collection or index definition changed
        // before we re-acquired the collection lock.
        idx = desc->infoObj();

        if (IndexType::INDEX_BTREE != IndexNames::nameToType(desc->getAccessMethodName())) {
            error() << "special index can't be used as a ttl index, skipping ttl job for: " << idx;
            return;
        }

        BSONElement secondsExpireElt = idx[secondsExpireField];
        if (!secondsExpireElt.isNumber()) {
            error() << "ttl indexes require the " << secondsExpireField << " field to be "
                    << "numeric but received a type of " << typeName(secondsExpireElt.type())
                    << ", skipping ttl job for: " << idx;
            return;
        }

        const Date_t kDawnOfTime =
            Date_t::fromMillisSinceEpoch(std::numeric_limits<long long>::min());
        const Date_t expirationTime = Date_t::now() - Seconds(secondsExpireElt.numberLong());
        const BSONObj startKey = BSON("" << kDawnOfTime);
        const BSONObj endKey = BSON("" << expirationTime);
        // The canonical check as to whether a key pattern element is "ascending" or
        // "descending" is (elt.number() >= 0).  This is defined by the Ordering class.
        const InternalPlanner::Direction direction = (key.firstElement().number() >= 0)
            ? InternalPlanner::Direction::FORWARD
            : InternalPlanner::Direction::BACKWARD;

        // We need to pass into the DeleteStageParams (below) a CanonicalQuery with a BSONObj that
        // queries for the expired documents correctly so that we do not delete documents that are
        // not actually expired when our snapshot changes during deletion.
        const char* keyFieldName = key.firstElement().fieldName();
        BSONObj query =
            BSON(keyFieldName << BSON("$gte" << kDawnOfTime << "$lte" << expirationTime));
        auto qr = std::make_unique<QueryRequest>(collectionNSS);
        qr->setFilter(query);
        auto canonicalQuery = CanonicalQuery::canonicalize(opCtx, std::move(qr));
        invariant(canonicalQuery.getStatus());

        auto params = std::make_unique<DeleteStageParams>();
        params->isMulti = true;
        params->canonicalQuery = canonicalQuery.getValue().get();

        auto exec =
            InternalPlanner::deleteWithIndexScan(opCtx,
                                                 collection,
                                                 std::move(params),
                                                 desc,
                                                 startKey,
                                                 endKey,
                                                 BoundInclusion::kIncludeBothStartAndEndKeys,
                                                 PlanExecutor::YIELD_AUTO,
                                                 direction);

        Status result = exec->executePlan();
        if (!result.isOK()) {
            error() << "ttl query execution for index " << idx
                    << " failed with status: " << redact(result);
            return;
        }

        const long long numDeleted = DeleteStage::getNumDeleted(*exec);
        ttlDeletedDocuments.increment(numDeleted);
        LOG(1) << "deleted: " << numDeleted;
    }

    ServiceContext* _serviceContext;
};

namespace {
// The global TTLMonitor object is intentionally leaked.  Even though it is only used in one
// function, we declare it here to indicate to the leak sanitizer that the leak of this object
// should not be reported.
TTLMonitor* ttlMonitor = nullptr;
}  // namespace

void startTTLBackgroundJob(ServiceContext* serviceContext) {
    ttlMonitor = new TTLMonitor(serviceContext);
    ttlMonitor->go();
}

std::string TTLMonitor::secondsExpireField = "expireAfterSeconds";
}  // namespace monger
