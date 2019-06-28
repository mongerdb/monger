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

#include "monger/db/logical_session_id.h"
#include "monger/db/operation_context.h"
#include "monger/db/service_liaison.h"
#include "monger/util/periodic_runner.h"
#include "monger/util/time_support.h"

namespace monger {

/**
 * This is the service liaison to mongers for the logical session cache.
 *
 * This class will return active sessions for cursors stored in the
 * global cursor manager and cursors in per-collection managers. This
 * class will also walk the service context to find all sessions for
 * currently-running operations on this server.
 *
 * Job scheduling on this class will be handled behind the scenes by a
 * periodic runner for this mongers. The time will be returned from the
 * system clock.
 */
class ServiceLiaisonMongos : public ServiceLiaison {
public:
    LogicalSessionIdSet getActiveOpSessions() const override;
    LogicalSessionIdSet getOpenCursorSessions(OperationContext* opCtx) const override;

    void scheduleJob(PeriodicRunner::PeriodicJob job) override;

    void join() override;

    Date_t now() const override;

    std::pair<Status, int> killCursorsWithMatchingSessions(
        OperationContext* opCtx, const SessionKiller::Matcher& matcher) override;

protected:
    /**
     * Returns the service context.
     */
    ServiceContext* _context() override;

    stdx::mutex _mutex;
    std::vector<PeriodicJobAnchor> _jobs;
};

}  // namespace monger
