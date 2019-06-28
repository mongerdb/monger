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

#include <boost/intrusive_ptr.hpp>
#include <vector>

#include "monger/base/string_data.h"
#include "monger/bson/bsonmisc.h"
#include "monger/bson/bsonobj.h"
#include "monger/bson/bsonobjbuilder.h"
#include "monger/db/pipeline/aggregation_context_fixture.h"
#include "monger/db/pipeline/document.h"
#include "monger/db/pipeline/document_source_count.h"
#include "monger/db/pipeline/document_source_group.h"
#include "monger/db/pipeline/document_source_single_document_transformation.h"
#include "monger/db/pipeline/document_value_test_util.h"
#include "monger/db/pipeline/value.h"

namespace monger {
namespace {
using boost::intrusive_ptr;
using std::list;
using std::vector;

class CountReturnsGroupAndProjectStages : public AggregationContextFixture {
public:
    void testCreateFromBsonResult(BSONObj countSpec) {
        list<intrusive_ptr<DocumentSource>> result =
            DocumentSourceCount::createFromBson(countSpec.firstElement(), getExpCtx());

        ASSERT_EQUALS(result.size(), 2UL);

        const auto* groupStage = dynamic_cast<DocumentSourceGroup*>(result.front().get());
        ASSERT(groupStage);

        const auto* projectStage =
            dynamic_cast<DocumentSourceSingleDocumentTransformation*>(result.back().get());
        ASSERT(projectStage);

        auto explain = ExplainOptions::Verbosity::kQueryPlanner;
        vector<Value> explainedStages;
        groupStage->serializeToArray(explainedStages, explain);
        projectStage->serializeToArray(explainedStages, explain);
        ASSERT_EQUALS(explainedStages.size(), 2UL);

        StringData countName = countSpec.firstElement().valueStringData();
        Value expectedGroupExplain =
            Value{Document{{"_id", Document{{"$const", BSONNULL}}},
                           {countName, Document{{"$sum", Document{{"$const", 1}}}}}}};
        auto groupExplain = explainedStages[0];
        ASSERT_VALUE_EQ(groupExplain["$group"], expectedGroupExplain);

        Value expectedProjectExplain = Value{Document{{"_id", false}, {countName, true}}};
        auto projectExplain = explainedStages[1];
        ASSERT_VALUE_EQ(projectExplain["$project"], expectedProjectExplain);
    }
};

TEST_F(CountReturnsGroupAndProjectStages, ValidStringSpec) {
    BSONObj spec = BSON("$count"
                        << "myCount");
    testCreateFromBsonResult(spec);

    spec = BSON("$count"
                << "quantity");
    testCreateFromBsonResult(spec);
}

class InvalidCountSpec : public AggregationContextFixture {
public:
    list<intrusive_ptr<DocumentSource>> createCount(BSONObj countSpec) {
        auto specElem = countSpec.firstElement();
        return DocumentSourceCount::createFromBson(specElem, getExpCtx());
    }
};

TEST_F(InvalidCountSpec, NonStringSpec) {
    BSONObj spec = BSON("$count" << 1);
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40156);

    spec = BSON("$count" << BSON("field1"
                                 << "test"));
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40156);
}

TEST_F(InvalidCountSpec, EmptyStringSpec) {
    BSONObj spec = BSON("$count"
                        << "");
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40157);
}

TEST_F(InvalidCountSpec, FieldPathSpec) {
    BSONObj spec = BSON("$count"
                        << "$x");
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40158);
}

TEST_F(InvalidCountSpec, EmbeddedNullByteSpec) {
    BSONObj spec = BSON("$count"
                        << "te\0st"_sd);
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40159);
}

TEST_F(InvalidCountSpec, PeriodInStringSpec) {
    BSONObj spec = BSON("$count"
                        << "test.string");
    ASSERT_THROWS_CODE(createCount(spec), AssertionException, 40160);
}
}  // namespace
}  // namespace monger
