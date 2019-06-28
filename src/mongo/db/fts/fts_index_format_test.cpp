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

#define MONGO_LOG_DEFAULT_COMPONENT ::monger::logger::LogComponent::kDefault

#include "monger/platform/basic.h"

#include <set>

#include "monger/bson/json.h"
#include "monger/bson/simple_bsonobj_comparator.h"
#include "monger/db/fts/fts_index_format.h"
#include "monger/db/fts/fts_spec.h"
#include "monger/unittest/unittest.h"
#include "monger/util/log.h"
#include "monger/util/str.h"

namespace monger {

namespace fts {

using std::string;
using unittest::assertGet;

TEST(FTSIndexFormat, Simple1) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text")))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    FTSIndexFormat::getKeys(spec,
                            BSON("data"
                                 << "cat sat"),
                            &keys);

    ASSERT_EQUALS(2U, keys.size());
    for (BSONObjSet::const_iterator i = keys.begin(); i != keys.end(); ++i) {
        BSONObj key = *i;
        ASSERT_EQUALS(2, key.nFields());
        ASSERT_EQUALS(String, key.firstElement().type());
    }
}

TEST(FTSIndexFormat, ExtraBack1) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text"
                                                               << "x"
                                                               << 1)))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    FTSIndexFormat::getKeys(spec,
                            BSON("data"
                                 << "cat"
                                 << "x"
                                 << 5),
                            &keys);

    ASSERT_EQUALS(1U, keys.size());
    BSONObj key = *(keys.begin());
    ASSERT_EQUALS(3, key.nFields());
    BSONObjIterator i(key);
    ASSERT_EQUALS(StringData("cat"), i.next().valuestr());
    ASSERT(i.next().numberDouble() > 0);
    ASSERT_EQUALS(5, i.next().numberInt());
}

TEST(FTSIndexFormat, ExtraFront1) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("x" << 1 << "data"
                                                                   << "text")))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    FTSIndexFormat::getKeys(spec,
                            BSON("data"
                                 << "cat"
                                 << "x"
                                 << 5),
                            &keys);

    ASSERT_EQUALS(1U, keys.size());
    BSONObj key = *(keys.begin());
    ASSERT_EQUALS(3, key.nFields());
    BSONObjIterator i(key);
    ASSERT_EQUALS(5, i.next().numberInt());
    ASSERT_EQUALS(StringData("cat"), i.next().valuestr());
    ASSERT(i.next().numberDouble() > 0);
}

TEST(FTSIndexFormat, StopWords1) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text")))));

    BSONObjSet keys1 = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    FTSIndexFormat::getKeys(spec,
                            BSON("data"
                                 << "computer"),
                            &keys1);
    ASSERT_EQUALS(1U, keys1.size());

    BSONObjSet keys2 = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    FTSIndexFormat::getKeys(spec,
                            BSON("data"
                                 << "any computer"),
                            &keys2);
    ASSERT_EQUALS(1U, keys2.size());
}

/**
 * Helper function to compare keys returned in getKeys() result
 * with expected values.
 */
void assertEqualsIndexKeys(std::set<std::string>& expectedKeys, const BSONObjSet& keys) {
    ASSERT_EQUALS(expectedKeys.size(), keys.size());
    for (BSONObjSet::const_iterator i = keys.begin(); i != keys.end(); ++i) {
        BSONObj key = *i;
        ASSERT_EQUALS(2, key.nFields());
        ASSERT_EQUALS(String, key.firstElement().type());
        string s = key.firstElement().String();
        std::set<string>::const_iterator j = expectedKeys.find(s);
        if (j == expectedKeys.end()) {
            str::stream ss;
            ss << "unexpected key " << s << " in FTSIndexFormat::getKeys result. "
               << "expected keys:";
            for (std::set<string>::const_iterator k = expectedKeys.begin(); k != expectedKeys.end();
                 ++k) {
                ss << "\n    " << *k;
            }
            FAIL(ss);
        }
    }
}

/**
 * Tests keys for long terms using text index version 1.
 * Terms that are too long are not truncated in version 1.
 */
TEST(FTSIndexFormat, LongWordsTextIndexVersion1) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text")
                                                       << "textIndexVersion"
                                                       << 1))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    string longPrefix(1024U, 'a');
    // "aaa...aaacat"
    string longWordCat = longPrefix + "cat";
    // "aaa...aaasat"
    string longWordSat = longPrefix + "sat";
    string text = str::stream() << longWordCat << " " << longWordSat;
    FTSIndexFormat::getKeys(spec, BSON("data" << text), &keys);

    // Hard-coded expected computed keys for future-proofing.
    std::set<string> expectedKeys;
    // cat
    expectedKeys.insert(longWordCat);
    // sat
    expectedKeys.insert(longWordSat);

    assertEqualsIndexKeys(expectedKeys, keys);
}

/**
 * Tests keys for long terms using text index version 2.
 * In version 2, long terms (longer than 32 characters)
 * are hashed with murmur3 and appended to the first 32
 * characters of the term to form the index key.
 */
TEST(FTSIndexFormat, LongWordTextIndexVersion2) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text")
                                                       << "textIndexVersion"
                                                       << 2))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    string longPrefix(1024U, 'a');
    // "aaa...aaacat"
    string longWordCat = longPrefix + "cat";
    // "aaa...aaasat"
    string longWordSat = longPrefix + "sat";
    // "aaa...aaamongerdbfts"
    string longWordMongoDBFts = longPrefix + "mongerdbfts";
    string text = str::stream() << longWordCat << " " << longWordSat << " " << longWordMongoDBFts;
    FTSIndexFormat::getKeys(spec, BSON("data" << text), &keys);

    // Hard-coded expected computed keys for future-proofing.
    std::set<string> expectedKeys;
    // cat
    expectedKeys.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab8e78455d827ebb87cbe87f392bf45f6");
    // sat
    expectedKeys.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaf2d6f58bb3b81b97e611ae7ccac6dea7");
    // mongerdbfts
    expectedKeys.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaae1d6b34f5d9c92acecd8cce32f747b27");

    assertEqualsIndexKeys(expectedKeys, keys);
}

/**
 * Tests keys for long terms using text index version 3.
 * In version 3, long terms (longer than 256 characters)
 * are hashed with md5 and appended to the first 224
 * characters of the term to form the index key.
 */
TEST(FTSIndexFormat, LongWordTextIndexVersion3) {
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << BSON("data"
                                                               << "text")
                                                       << "textIndexVersion"
                                                       << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    string longPrefix(1024U, 'a');
    // "aaa...aaacat"
    string longWordCat = longPrefix + "cat";
    // "aaa...aaasat"
    string longWordSat = longPrefix + "sat";
    string text = str::stream() << longWordCat << " " << longWordSat;
    FTSIndexFormat::getKeys(spec, BSON("data" << text), &keys);

    // Hard-coded expected computed keys for future-proofing.
    std::set<string> expectedKeys;
    // cat
    expectedKeys.insert(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa256a476d3197f1d31d1834fe91b9ef46");
    // sat
    expectedKeys.insert(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab8c685737a761255443de66dae5d7d0a");

    assertEqualsIndexKeys(expectedKeys, keys);
}

TEST(FTSIndexFormat, GetKeysWithLeadingEmptyArrayThrows) {
    BSONObj keyPattern = fromjson("{'a.b': 1, data: 'text'}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: {b: []}, data: 'foo'}");
    ASSERT_THROWS_CODE(FTSIndexFormat::getKeys(spec, objToIndex, &keys),
                       AssertionException,
                       ErrorCodes::CannotBuildIndexKeys);
}

TEST(FTSIndexFormat, GetKeysWithTrailingEmptyArrayThrows) {
    BSONObj keyPattern = fromjson("{data: 'text', 'a.b': 1}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: {b: []}, data: 'foo'}");
    ASSERT_THROWS_CODE(FTSIndexFormat::getKeys(spec, objToIndex, &keys),
                       AssertionException,
                       ErrorCodes::CannotBuildIndexKeys);
}

TEST(FTSIndexFormat, GetKeysWithLeadingSingleElementArrayThrows) {
    BSONObj keyPattern = fromjson("{'a.b': 1, data: 'text'}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: [{b: 9}], data: 'foo'}");
    ASSERT_THROWS_CODE(FTSIndexFormat::getKeys(spec, objToIndex, &keys),
                       AssertionException,
                       ErrorCodes::CannotBuildIndexKeys);
}

TEST(FTSIndexFormat, GetKeysWithTrailingSingleElementArrayThrows) {
    BSONObj keyPattern = fromjson("{data: 'text', 'a.b': 1}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: [{b: 9}], data: 'foo'}");
    ASSERT_THROWS_CODE(FTSIndexFormat::getKeys(spec, objToIndex, &keys),
                       AssertionException,
                       ErrorCodes::CannotBuildIndexKeys);
}

TEST(FTSIndexFormat, GetKeysWithMultiElementArrayThrows) {
    BSONObj keyPattern = fromjson("{'a.b': 1, 'a.c': 'text'}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: [{b: 9, c: 'foo'}, {b: 10, c: 'bar'}]}");
    ASSERT_THROWS_CODE(FTSIndexFormat::getKeys(spec, objToIndex, &keys),
                       AssertionException,
                       ErrorCodes::CannotBuildIndexKeys);
}

TEST(FTSIndexFormat, GetKeysWithPositionalPathAllowed) {
    BSONObj keyPattern = fromjson("{'a.0': 1, 'a.b': 'text'}");
    FTSSpec spec(assertGet(FTSSpec::fixSpec(BSON("key" << keyPattern << "textIndexVersion" << 3))));
    BSONObjSet keys = SimpleBSONObjComparator::kInstance.makeBSONObjSet();
    BSONObj objToIndex = fromjson("{a: [{b: 'foo'}, {b: 'bar'}]}");
    FTSIndexFormat::getKeys(spec, objToIndex, &keys);
    ASSERT_EQ(2U, keys.size());

    {
        BSONObj key = *(keys.begin());
        ASSERT_EQ(3, key.nFields());
        BSONObjIterator it{key};
        ASSERT_BSONELT_EQ(it.next(), fromjson("{'': {b: 'foo'}}").firstElement());
        ASSERT_BSONELT_EQ(it.next(), fromjson("{'': 'bar'}").firstElement());
    }

    {
        BSONObj key = *(++keys.begin());
        ASSERT_EQ(3, key.nFields());
        BSONObjIterator it{key};
        ASSERT_BSONELT_EQ(it.next(), fromjson("{'': {b: 'foo'}}").firstElement());
        ASSERT_BSONELT_EQ(it.next(), fromjson("{'': 'foo'}").firstElement());
    }
}
}  // namespace fts
}  // namespace monger
