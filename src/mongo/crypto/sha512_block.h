/**
 *    Copyright (C) 2019-present MongerDB, Inc.
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

#pragma once

#include "monger/crypto/hash_block.h"

#include "monger/util/make_array_type.h"

namespace monger {

/**
 * A Traits type for adapting HashBlock to sha512 hashes.
 */
struct SHA512BlockTraits {
    using HashType = MakeArrayType<std::uint8_t, 64, SHA512BlockTraits>;

    static constexpr StringData name = "SHA512Block"_sd;

    static HashType computeHash(std::initializer_list<ConstDataRange> input);

    static void computeHmac(const uint8_t* key,
                            size_t keyLen,
                            std::initializer_list<ConstDataRange> input,
                            HashType* const output);

    static HashType computeHmac(const uint8_t* key,
                                size_t keyLen,
                                std::initializer_list<ConstDataRange> input);
};

using SHA512Block = HashBlock<SHA512BlockTraits>;

}  // namespace monger
