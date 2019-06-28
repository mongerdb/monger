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

#include "monger/db/ftdc/ftdc_system_stats.h"

#include <string>

#include "monger/base/status.h"
#include "monger/base/string_data.h"
#include "monger/bson/bsonobjbuilder.h"
#include "monger/db/ftdc/collector.h"

namespace monger {

namespace {

/**
 * Name of FTDC collector to create.
 */
constexpr auto kSystemMetricsCollector = "systemMetrics";

}  // namespace

std::string SystemMetricsCollector::name() const {
    return kSystemMetricsCollector;
}

void SystemMetricsCollector::processStatusErrors(Status s, BSONObjBuilder* builder) {
    if (!s.isOK()) {
        builder->append("error", s.toString());
    }
}

}  // namespace monger
