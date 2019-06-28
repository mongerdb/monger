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

#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <memory>

#include "monger/db/jsobj.h"
#include "monger/db/operation_context.h"
#include "monger/db/record_id.h"
#include "monger/db/storage/index_entry_comparison.h"

#pragma once

namespace monger {

class BSONObjBuilder;
class BucketDeletionNotification;
class SortedDataBuilderInterface;
struct ValidateResults;

/**
 * This enum is returned by any functions that could potentially insert special format onto disk. It
 * is a way to inform the callers to do something when special format exists on disk.
 * TODO SERVER-36385: Remove this enum in 4.4.
 */
enum SpecialFormatInserted { NoSpecialFormatInserted = 0, LongTypeBitsInserted = 1 };

/**
 * This is the uniform interface for storing indexes and supporting point queries as well as range
 * queries. The actual implementation is up to the storage engine. All the storage engines must
 * support an index key size up to the maximum document size.
 */
class SortedDataInterface {
public:
    virtual ~SortedDataInterface() {}

    //
    // Data changes
    //

    /**
     * Return a bulk builder for 'this' index.
     *
     * Implementations can assume that 'this' index outlives its bulk
     * builder.
     *
     * @param opCtx the transaction under which keys are added to 'this' index
     * @param dupsAllowed true if duplicate keys are allowed, and false
     *        otherwise
     *
     * @return caller takes ownership
     */
    virtual SortedDataBuilderInterface* getBulkBuilder(OperationContext* opCtx,
                                                       bool dupsAllowed) = 0;

    /**
     * Insert an entry into the index with the specified key and RecordId.
     *
     * @param opCtx the transaction under which the insert takes place
     * @param dupsAllowed true if duplicate keys are allowed, and false
     *        otherwise
     *
     * @return Status::OK() if the insert succeeded,
     *
     *         ErrorCodes::DuplicateKey if 'key' already exists in 'this' index
     *         at a RecordId other than 'loc' and duplicates were not allowed
     *
     *         SpecialFormatInserted::LongTypeBitsInserted if the key we've
     *         inserted has long typebits.
     */
    virtual StatusWith<SpecialFormatInserted> insert(OperationContext* opCtx,
                                                     const BSONObj& key,
                                                     const RecordId& loc,
                                                     bool dupsAllowed) = 0;

    /**
     * Remove the entry from the index with the specified key and RecordId.
     *
     * @param opCtx the transaction under which the remove takes place
     * @param dupsAllowed true if duplicate keys are allowed, and false
     *        otherwise
     */
    virtual void unindex(OperationContext* opCtx,
                         const BSONObj& key,
                         const RecordId& loc,
                         bool dupsAllowed) = 0;

    /**
     * Return ErrorCodes::DuplicateKey if there is more than one occurence of 'key' in this index,
     * and Status::OK() otherwise. This call is only allowed on a unique index, and will invariant
     * otherwise.
     *
     * @param opCtx the transaction under which this operation takes place
     */
    virtual Status dupKeyCheck(OperationContext* opCtx, const BSONObj& key) = 0;

    /**
     * Attempt to reduce the storage space used by this index via compaction. Only called if the
     * indexed record store supports compaction-in-place.
     */
    virtual Status compact(OperationContext* opCtx) {
        return Status::OK();
    }

    //
    // Information about the tree
    //

    /**
     * TODO: expose full set of args for testing?
     */
    virtual void fullValidate(OperationContext* opCtx,
                              long long* numKeysOut,
                              ValidateResults* fullResults) const = 0;

    virtual bool appendCustomStats(OperationContext* opCtx,
                                   BSONObjBuilder* output,
                                   double scale) const = 0;


    /**
     * Return the number of bytes consumed by 'this' index.
     *
     * @param opCtx the transaction under which this operation takes place
     *
     * @see IndexAccessMethod::getSpaceUsedBytes
     */
    virtual long long getSpaceUsedBytes(OperationContext* opCtx) const = 0;

    /**
     * Return true if 'this' index is empty, and false otherwise.
     */
    virtual bool isEmpty(OperationContext* opCtx) = 0;

    /**
     * Attempt to bring the entirety of 'this' index into memory.
     *
     * If the underlying storage engine does not support the operation,
     * returns ErrorCodes::CommandNotSupported
     *
     * @return Status::OK()
     */
    virtual Status touch(OperationContext* opCtx) const {
        return Status(ErrorCodes::CommandNotSupported,
                      "this storage engine does not support touch");
    }

    /**
     * Return the number of entries in 'this' index.
     *
     * The default implementation should be overridden with a more
     * efficient one if at all possible.
     */
    virtual long long numEntries(OperationContext* opCtx) const {
        long long x = -1;
        fullValidate(opCtx, &x, nullptr);
        return x;
    }

    /**
     * Navigates over the sorted data.
     *
     * A cursor is constructed with a direction flag with the following effects:
     *      - The direction that next() moves.
     *      - If a seek method hits an exact match on key, forward cursors will be positioned on
     *        the first value for that key, reverse cursors on the last.
     *      - If a seek method or restore does not hit an exact match, cursors will be
     *        positioned on the closest position *after* the query in the direction of the
     *        search.
     *      - The end position is on the "far" side of the query. In a forward cursor that means
     *        that it is the lowest value for the key if the end is exclusive or the first entry
     *        past the key if the end is inclusive or there are no exact matches.
     *
     * A cursor is tied to a transaction, such as the OperationContext or a WriteUnitOfWork
     * inside that context. Any cursor acquired inside a transaction is invalid outside
     * of that transaction, instead use the save and restore methods to reestablish the cursor.
     *
     * Any method other than the save methods may throw WriteConflict exception. If that
     * happens, the cursor may not be used again until it has been saved and successfully
     * restored. If next() or restore() throw a WCE the cursor's position will be the same as
     * before the call (strong exception guarantee). All other methods leave the cursor in a
     * valid state but with an unspecified position (basic exception guarantee). All methods
     * only provide the basic guarantee for exceptions other than WCE.
     *
     * Any returned unowned BSON is only valid until the next call to any method on this
     * interface. The implementations must assume that passed-in unowned BSON is only valid for
     * the duration of the call.
     *
     * Implementations may override any default implementation if they can provide a more
     * efficient implementation.
     */
    class Cursor {
    public:
        /**
         * Tells methods that return an IndexKeyEntry what part of the data the caller is
         * interested in.
         *
         * Methods returning an engaged optional<T> will only return null RecordIds or empty
         * BSONObjs if they have been explicitly left out of the request.
         *
         * Implementations are allowed to return more data than requested, but not less.
         */
        enum RequestedInfo {
            // Only usable part of the return is whether it is engaged or not.
            kJustExistance = 0,
            // Key must be filled in.
            kWantKey = 1,
            // Loc must be fulled in.
            kWantLoc = 2,
            // Both must be returned.
            kKeyAndLoc = kWantKey | kWantLoc,
        };

        virtual ~Cursor() = default;


        /**
         * Sets the position to stop scanning. An empty key unsets the end position.
         *
         * If next() hits this position, or a seek method attempts to seek past it they
         * unposition the cursor and return boost::none.
         *
         * Setting the end position should be done before seeking since the current position, if
         * any, isn't checked.
         */
        virtual void setEndPosition(const BSONObj& key, bool inclusive) = 0;

        /**
         * Moves forward and returns the new data or boost::none if there is no more data.
         * If not positioned, returns boost::none.
         */
        virtual boost::optional<IndexKeyEntry> next(RequestedInfo parts = kKeyAndLoc) = 0;

        //
        // Seeking
        //

        /**
         * Seeks to the provided key and returns current position.
         *
         * TODO consider removing once IndexSeekPoint has been cleaned up a bit. In particular,
         * need a way to specify use whole keyPrefix and nothing else and to support the
         * combination of empty and exclusive. Should also make it easier to construct for the
         * common cases.
         */
        virtual boost::optional<IndexKeyEntry> seek(const BSONObj& key,
                                                    bool inclusive,
                                                    RequestedInfo parts = kKeyAndLoc) = 0;

        /**
         * Seeks to the position described by seekPoint and returns the current position.
         *
         * NOTE: most implementations should just pass seekPoint to
         * IndexEntryComparison::makeQueryObject().
         */
        virtual boost::optional<IndexKeyEntry> seek(const IndexSeekPoint& seekPoint,
                                                    RequestedInfo parts = kKeyAndLoc) = 0;

        /**
         * Seeks to a key with a hint to the implementation that you only want exact matches. If
         * an exact match can't be found, boost::none will be returned and the resulting
         * position of the cursor is unspecified.
         */
        virtual boost::optional<IndexKeyEntry> seekExact(const BSONObj& key,
                                                         RequestedInfo parts = kKeyAndLoc) {
            auto kv = seek(key, true, kKeyAndLoc);
            if (kv && kv->key.woCompare(key, BSONObj(), /*considerFieldNames*/ false) == 0)
                return kv;
            return {};
        }

        //
        // Saving and restoring state
        //

        /**
         * Prepares for state changes in underlying data in a way that allows the cursor's
         * current position to be restored.
         *
         * It is safe to call save multiple times in a row.
         * No other method (excluding destructor) may be called until successfully restored.
         */
        virtual void save() = 0;

        /**
         * Prepares for state changes in underlying data without necessarily saving the current
         * state.
         *
         * The cursor's position when restored is unspecified. Caller is expected to seek
         * following the restore.
         *
         * It is safe to call saveUnpositioned multiple times in a row.
         * No other method (excluding destructor) may be called until successfully restored.
         */
        virtual void saveUnpositioned() {
            save();
        }

        /**
         * Recovers from potential state changes in underlying data.
         *
         * If the former position no longer exists, a following call to next() will return the
         * next closest position in the direction of the scan, if any.
         *
         * This handles restoring after either save() or saveUnpositioned().
         */
        virtual void restore() = 0;

        /**
         * Detaches from the OperationContext and releases any storage-engine state.
         *
         * It is only legal to call this when in a "saved" state. While in the "detached" state, it
         * is only legal to call reattachToOperationContext or the destructor. It is not legal to
         * call detachFromOperationContext() while already in the detached state.
         */
        virtual void detachFromOperationContext() = 0;

        /**
         * Reattaches to the OperationContext and reacquires any storage-engine state.
         *
         * It is only legal to call this in the "detached" state. On return, the cursor is left in a
         * "saved" state, so callers must still call restoreState to use this object.
         */
        virtual void reattachToOperationContext(OperationContext* opCtx) = 0;
    };

    /**
     * Returns an unpositioned cursor over 'this' index.
     *
     * Implementations can assume that 'this' index outlives all cursors it produces.
     */
    virtual std::unique_ptr<Cursor> newCursor(OperationContext* opCtx,
                                              bool isForward = true) const = 0;

    //
    // Index creation
    //

    virtual Status initAsEmpty(OperationContext* opCtx) = 0;
};

/**
 * A version-hiding wrapper around the bulk builder for the Btree.
 */
class SortedDataBuilderInterface {
public:
    virtual ~SortedDataBuilderInterface() {}

    /**
     * Adds 'key' to intermediate storage.
     *
     * 'key' must be > or >= the last key passed to this function (depends on _dupsAllowed).  If
     * this is violated an error Status (ErrorCodes::InternalError) will be returned.
     *
     * @return Status::OK() if addKey succeeded,
     *
     *         SpecialFormatInserted::LongTypeBitsInserted if we've inserted any
     *         key with long typebits.
     */
    virtual StatusWith<SpecialFormatInserted> addKey(const BSONObj& key, const RecordId& loc) = 0;

    /**
     * Do any necessary work to finish building the tree.
     *
     * The default implementation may be used if no commit phase is necessary because addKey
     * always leaves the tree in a valid state.
     *
     * This is called outside of any WriteUnitOfWork to allow implementations to split this up
     * into multiple units.
     *
     * @return SpecialFormatInserted::LongTypeBitsInserted if we've inserted any
     * key with long typebits.
     *
     * TODO SERVER-36385: Change the return type from SpecialFormatInserted back to "void" as that
     * was a hack introduced in SERVER-36280 for detecting long TypeBits in an edge case in one of
     * the unique index builder implementations.
     */
    virtual SpecialFormatInserted commit(bool mayInterrupt) {
        return SpecialFormatInserted::NoSpecialFormatInserted;
    }
};

}  // namespace monger
