/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
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

#pragma once

#include "monger/db/pipeline/document_source.h"
#include "monger/db/pipeline/value_comparator.h"

namespace monger {

/**
 * This class is not a registered stage, it is only used as an optimized replacement for $sample
 * when the storage engine allows us to use a random cursor.
 */
class DocumentSourceSampleFromRandomCursor final : public DocumentSource {
public:
    GetNextResult getNext() final;
    const char* getSourceName() const final;
    Value serialize(boost::optional<ExplainOptions::Verbosity> explain = boost::none) const final;
    DepsTracker::State getDependencies(DepsTracker* deps) const final;

    StageConstraints constraints(Pipeline::SplitState pipeState) const final {
        return {StreamType::kStreaming,
                PositionRequirement::kFirst,
                HostTypeRequirement::kAnyShard,
                DiskUseRequirement::kNoDiskUse,
                FacetRequirement::kNotAllowed,
                TransactionRequirement::kAllowed,
                LookupRequirement::kAllowed};
    }

    boost::optional<DistributedPlanLogic> distributedPlanLogic() final {
        return boost::none;
    }

    static boost::intrusive_ptr<DocumentSourceSampleFromRandomCursor> create(
        const boost::intrusive_ptr<ExpressionContext>& expCtx,
        long long size,
        std::string idField,
        long long collectionSize);

private:
    DocumentSourceSampleFromRandomCursor(const boost::intrusive_ptr<ExpressionContext>& expCtx,
                                         long long size,
                                         std::string idField,
                                         long long collectionSize);

    /**
     * Keep asking for documents from the random cursor until it yields a new document. Errors if a
     * a document is encountered without a value for '_idField', or if the random cursor keeps
     * returning duplicate elements.
     */
    GetNextResult getNextNonDuplicateDocument();

    long long _size;

    // The field to use as the id of a document. Usually '_id', but 'ts' for the oplog.
    std::string _idField;

    // Keeps track of the documents that have been returned, since a random cursor is allowed to
    // return duplicates.
    ValueUnorderedSet _seenDocs;

    // The approximate number of documents in the collection (includes orphans).
    const long long _nDocsInColl;

    // The value to be assigned to the randMetaField of outcoming documents. Each call to getNext()
    // will decrement this value by an amount scaled by _nDocsInColl as an attempt to appear as if
    // the documents were produced by a top-k random sort.
    double _randMetaFieldVal = 1.0;
};

}  // namespace monger
