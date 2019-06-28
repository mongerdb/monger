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

#include "monger/platform/basic.h"

#include "monger/db/service_context_test_fixture.h"

#include <memory>

#include "monger/db/client.h"
#include "monger/db/op_observer_registry.h"
#include "monger/util/assert_util.h"

namespace monger {

ScopedGlobalServiceContextForTest::ScopedGlobalServiceContextForTest() {
    setGlobalServiceContext(ServiceContext::make());
    auto const serviceContext = getGlobalServiceContext();
    auto observerRegistry = std::make_unique<OpObserverRegistry>();
    serviceContext->setOpObserver(std::move(observerRegistry));
}

ScopedGlobalServiceContextForTest::~ScopedGlobalServiceContextForTest() {
    setGlobalServiceContext({});
}

ServiceContext* ScopedGlobalServiceContextForTest::getServiceContext() {
    return getGlobalServiceContext();
}


ServiceContextTest::ServiceContextTest() : _threadClient(getServiceContext()) {}

Client* ServiceContextTest::getClient() {
    return Client::getCurrent();
}

}  // namespace monger
