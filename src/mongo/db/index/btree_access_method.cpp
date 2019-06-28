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

#include "monger/db/index/btree_access_method.h"

#include <vector>

#include "monger/base/status.h"
#include "monger/base/status_with.h"
#include "monger/db/catalog/index_catalog_entry.h"
#include "monger/db/jsobj.h"
#include "monger/db/keypattern.h"

namespace monger {

using std::vector;

// Standard Btree implementation below.
BtreeAccessMethod::BtreeAccessMethod(IndexCatalogEntry* btreeState,
                                     std::unique_ptr<SortedDataInterface> btree)
    : AbstractIndexAccessMethod(btreeState, std::move(btree)) {
    // The key generation wants these values.
    vector<const char*> fieldNames;
    vector<BSONElement> fixed;

    BSONObjIterator it(_descriptor->keyPattern());
    while (it.more()) {
        BSONElement elt = it.next();
        fieldNames.push_back(elt.fieldName());
        fixed.push_back(BSONElement());
    }

    _keyGenerator = std::make_unique<BtreeKeyGenerator>(
        fieldNames, fixed, _descriptor->isSparse(), btreeState->getCollator());
}

void BtreeAccessMethod::doGetKeys(const BSONObj& obj,
                                  BSONObjSet* keys,
                                  BSONObjSet* multikeyMetadataKeys,
                                  MultikeyPaths* multikeyPaths) const {
    _keyGenerator->getKeys(obj, keys, multikeyPaths);
}

}  // namespace monger
