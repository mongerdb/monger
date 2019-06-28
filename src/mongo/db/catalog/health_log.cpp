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

#include "monger/platform/basic.h"

#include "monger/db/catalog/health_log.h"
#include "monger/db/catalog/health_log_gen.h"
#include "monger/db/concurrency/write_conflict_exception.h"
#include "monger/db/db_raii.h"

namespace monger {

namespace {
const ServiceContext::Decoration<HealthLog> getHealthLog =
    ServiceContext::declareDecoration<HealthLog>();

const int64_t kDefaultHealthlogSize = 100'000'000;

CollectionOptions getOptions(void) {
    CollectionOptions options;
    options.capped = true;
    options.cappedSize = kDefaultHealthlogSize;
    return options;
}
}

HealthLog::HealthLog() : _writer(nss, getOptions(), kMaxBufferSize) {}

void HealthLog::startup(void) {
    _writer.startup(std::string("healthlog writer"));
}

void HealthLog::shutdown(void) {
    _writer.shutdown();
}

HealthLog& HealthLog::get(ServiceContext* svcCtx) {
    return getHealthLog(svcCtx);
}

HealthLog& HealthLog::get(OperationContext* opCtx) {
    return getHealthLog(opCtx->getServiceContext());
}

bool HealthLog::log(const HealthLogEntry& entry) {
    BSONObjBuilder builder;
    OID oid;
    oid.init();
    builder.append("_id", oid);
    entry.serialize(&builder);
    return _writer.insertDocument(builder.obj());
}

const NamespaceString HealthLog::nss("local", "system.healthlog");
}
