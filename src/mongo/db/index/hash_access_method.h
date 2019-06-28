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

#include <string>

#include "monger/base/status.h"
#include "monger/db/hasher.h"  // For HashSeed.
#include "monger/db/index/index_access_method.h"
#include "monger/db/index/index_descriptor.h"
#include "monger/db/jsobj.h"

namespace monger {

class CollatorInterface;

/**
 * This is the access method for "hashed" indices.
 */
class HashAccessMethod : public AbstractIndexAccessMethod {
public:
    HashAccessMethod(IndexCatalogEntry* btreeState, std::unique_ptr<SortedDataInterface> btree);

private:
    /**
     * Fills 'keys' with the keys that should be generated for 'obj' on this index.
     *
     * This function ignores the 'multikeyPaths' and 'multikeyMetadataKeys' pointers because hashed
     * indexes don't support tracking path-level multikey information.
     */
    void doGetKeys(const BSONObj& obj,
                   BSONObjSet* keys,
                   BSONObjSet* multikeyMetadataKeys,
                   MultikeyPaths* multikeyPaths) const final;

    // Only one of our fields is hashed.  This is the field name for it.
    std::string _hashedField;

    // _seed defaults to zero.
    HashSeed _seed;

    // _hashVersion defaults to zero.
    int _hashVersion;

    BSONObj _missingKey;

    // Null if this index orders strings according to the simple binary compare. If non-null,
    // represents the collator used to generate index keys for indexed strings.
    const CollatorInterface* _collator;
};

}  // namespace monger
