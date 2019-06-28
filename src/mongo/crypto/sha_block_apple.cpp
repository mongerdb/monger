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

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

#include "monger/crypto/sha1_block.h"
#include "monger/crypto/sha256_block.h"
#include "monger/crypto/sha512_block.h"

namespace monger {
using CDRinit = std::initializer_list<ConstDataRange>;

SHA1BlockTraits::HashType SHA1BlockTraits::computeHash(CDRinit input) {
    CC_SHA1_CTX ctx;
    CC_SHA1_Init(&ctx);
    for (const auto& range : input) {
        CC_SHA1_Update(&ctx, range.data(), range.length());
    }

    SHA1BlockTraits::HashType ret;
    static_assert(sizeof(ret) == CC_SHA1_DIGEST_LENGTH,
                  "SHA1 HashType size doesn't match expected digest output size");
    CC_SHA1_Final(ret.data(), &ctx);
    return ret;
}

SHA256BlockTraits::HashType SHA256BlockTraits::computeHash(CDRinit input) {
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    for (const auto& range : input) {
        CC_SHA256_Update(&ctx, range.data(), range.length());
    }

    SHA256BlockTraits::HashType ret;
    static_assert(sizeof(ret) == CC_SHA256_DIGEST_LENGTH,
                  "SHA256 HashType size doesn't match expected digest output size");
    CC_SHA256_Final(ret.data(), &ctx);
    return ret;
}

SHA512BlockTraits::HashType SHA512BlockTraits::computeHash(CDRinit input) {
    CC_SHA512_CTX ctx;
    CC_SHA512_Init(&ctx);
    for (const auto& range : input) {
        CC_SHA512_Update(&ctx, range.data(), range.length());
    }

    SHA512BlockTraits::HashType ret;
    static_assert(sizeof(ret) == CC_SHA512_DIGEST_LENGTH,
                  "SHA512 HashType size doesn't match expected digest output size");
    CC_SHA512_Final(ret.data(), &ctx);
    return ret;
}

void SHA1BlockTraits::computeHmac(const uint8_t* key,
                                  size_t keyLen,
                                  CDRinit input,
                                  SHA1BlockTraits::HashType* const output) {
    static_assert(sizeof(*output) == CC_SHA1_DIGEST_LENGTH,
                  "SHA1 HashType size doesn't match expected hmac output size");
    CCHmacContext ctx;
    CCHmacInit(&ctx, kCCHmacAlgSHA1, key, keyLen);
    for (const auto& range : input) {
        CCHmacUpdate(&ctx, range.data(), range.length());
    }
    CCHmacFinal(&ctx, output);
}

void SHA256BlockTraits::computeHmac(const uint8_t* key,
                                    size_t keyLen,
                                    CDRinit input,
                                    SHA256BlockTraits::HashType* const output) {
    static_assert(sizeof(*output) == CC_SHA256_DIGEST_LENGTH,
                  "SHA256 HashType size doesn't match expected hmac output size");
    CCHmacContext ctx;
    CCHmacInit(&ctx, kCCHmacAlgSHA256, key, keyLen);
    for (const auto& range : input) {
        CCHmacUpdate(&ctx, range.data(), range.length());
    }
    CCHmacFinal(&ctx, output);
}

void SHA512BlockTraits::computeHmac(const uint8_t* key,
                                    size_t keyLen,
                                    CDRinit input,
                                    SHA512BlockTraits::HashType* const output) {
    static_assert(sizeof(*output) == CC_SHA512_DIGEST_LENGTH,
                  "SHA512 HashType size doesn't match expected hmac output size");
    CCHmacContext ctx;
    CCHmacInit(&ctx, kCCHmacAlgSHA512, key, keyLen);
    for (const auto& range : input) {
        CCHmacUpdate(&ctx, range.data(), range.length());
    }
    CCHmacFinal(&ctx, output);
}

}  // namespace monger
