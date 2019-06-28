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

#include <vector>

#include "monger/base/status_with.h"
#include "monger/bson/bsonobj.h"
#include "monger/db/namespace_string.h"
#include "monger/db/pipeline/aggregation_request.h"

namespace monger {

/**
 * Represents a resolved definition, composed of a base collection namespace and a pipeline
 * built from one or more views.
 */
class ResolvedView final : public ErrorExtraInfo {
public:
    ResolvedView(const NamespaceString& collectionNs,
                 std::vector<BSONObj> pipeline,
                 BSONObj defaultCollation)
        : _namespace(collectionNs),
          _pipeline(std::move(pipeline)),
          _defaultCollation(std::move(defaultCollation)) {}

    static ResolvedView fromBSON(const BSONObj& commandResponseObj);

    /**
     * Convert an aggregation command on a view to the equivalent command against the view's
     * underlying collection.
     */
    AggregationRequest asExpandedViewAggregation(const AggregationRequest& aggRequest) const;

    const NamespaceString& getNamespace() const {
        return _namespace;
    }

    const std::vector<BSONObj>& getPipeline() const {
        return _pipeline;
    }

    const BSONObj& getDefaultCollation() const {
        return _defaultCollation;
    }

    // ErrorExtraInfo API
    static constexpr auto code = ErrorCodes::CommandOnShardedViewNotSupportedOnMongod;
    void serialize(BSONObjBuilder* bob) const final;
    static std::shared_ptr<const ErrorExtraInfo> parse(const BSONObj&);

private:
    NamespaceString _namespace;
    std::vector<BSONObj> _pipeline;

    // The default collation associated with this view. An empty object means that the default is
    // the simple collation.
    //
    // Currently all operations which run over a view must use the default collation. This means
    // that operations on the view which do not specify a collation inherit the default. Operations
    // on the view which specify any other collation fail with a user error.
    BSONObj _defaultCollation;
};

}  // namespace monger
