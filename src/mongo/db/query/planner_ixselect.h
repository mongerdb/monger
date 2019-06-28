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

#include "monger/db/matcher/expression_array.h"
#include "monger/db/query/canonical_query.h"
#include "monger/db/query/index_entry.h"
#include "monger/db/query/query_solution.h"
#include "monger/stdx/unordered_set.h"

namespace monger {

class CollatorInterface;

/**
 * Methods for determining what fields and predicates can use indices.
 */
class QueryPlannerIXSelect {
public:
    /**
     * Return all the fields in the tree rooted at 'node' that we can use an index on
     * in order to answer the query.
     */
    static void getFields(const MatchExpression* node, stdx::unordered_set<std::string>* out);

    /**
     * Similar to other getFields() method, but with 'prefix' argument which is a path prefix to be
     * prepended to any fields mentioned in predicates encountered.
     *
     * Public for testing.
     */
    static void getFields(const MatchExpression* node,
                          std::string prefix,
                          stdx::unordered_set<std::string>* out);

    /**
     * Finds all indices that correspond to the hinted index. Matches the index both by name and by
     * key pattern.
     */
    static std::vector<IndexEntry> findIndexesByHint(const BSONObj& hintedIndex,
                                                     const std::vector<IndexEntry>& allIndices);

    /**
     * Finds all indices prefixed by fields we have predicates over.  Only these indices are
     * useful in answering the query.
     */
    static std::vector<IndexEntry> findRelevantIndices(
        const stdx::unordered_set<std::string>& fields, const std::vector<IndexEntry>& allIndices);

    /**
     * Determine how useful all of our relevant 'indices' are to all predicates in the subtree
     * rooted at 'node'.  Affixes a RelevantTag to all predicate nodes which can use an index.
     *
     * 'prefix' is a path prefix that should be prepended to any path (certain array operators
     * imply a path prefix).
     *
     * For an index to be useful to a predicate, the index must be compatible (see above).
     *
     * If an index is compound but not prefixed by a predicate's path, it's only useful if
     * there exists another predicate that 1. will use that index and 2. is related to the
     * original predicate by having an AND as a parent.
     */
    static void rateIndices(MatchExpression* node,
                            std::string prefix,
                            const std::vector<IndexEntry>& indices,
                            const CollatorInterface* collator);

    /**
     * Amend the RelevantTag lists for all predicates in the subtree rooted at 'node' to remove
     * invalid assignments to text and geo indices.
     *
     * See the body of this function and the specific stripInvalidAssignments functions for details.
     */
    static void stripInvalidAssignments(MatchExpression* node,
                                        const std::vector<IndexEntry>& indices);

    /**
     * In some special cases, we can strip most of the index assignments from the tree early
     * on. Specifically, if we find an AND which has a child tagged for equality over a
     * single-field unique index, then all other predicate-to-index assignments can be
     * stripped off the subtree rooted at 'node'.
     *
     * This is used to ensure that we always favor key-value lookup plans over any
     * more complex plan.
     *
     * Example:
     *   Suppose you have match expression OR (AND (a==1, b==2), AND (c==3, d==4)).
     *   There are indices on fields, 'a', 'b', 'c', and 'd'. The index on 'd' is
     *   the only unique index.
     *
     *   This code will find that the subtree AND (c==3, d==4) can be answered by
     *   looking up the value of 'd' in the unique index. Since no better plan than
     *   a single key lookup is ever available, all assignments in this subtree
     *   are stripped, except for the assignment of d==4 to the unique 'd' index.
     *
     *   Stripping the assignment for 'c' causes the planner to generate just two
     *   possible plans:
     *     1) an OR of an index scan over 'a' and an index scan over 'd'
     *     2) an OR of an index scan over 'b' and an index scan over 'd'
     */
    static void stripUnneededAssignments(MatchExpression* node,
                                         const std::vector<IndexEntry>& indices);

    /**
     * Given a list of IndexEntries and fields used by a query's match expression, return a list
     * "expanded" indexes (where the $** indexes in the given list have been expanded).
     */
    static std::vector<IndexEntry> expandIndexes(const stdx::unordered_set<std::string>& fields,
                                                 std::vector<IndexEntry> relevantIndices);

    /**
     * Check if this match expression is a leaf and is supported by a wildcard index.
     */
    static bool nodeIsSupportedByWildcardIndex(const MatchExpression* queryExpr);

    /*
     * Return true if the given match expression can use a sparse index, false otherwise. This will
     * not traverse the children of the given match expression.
     */
    static bool nodeIsSupportedBySparseIndex(const MatchExpression* queryExpr, bool isInElemMatch);

    /**
     * Some types of matches are not supported by any type of index. If this function returns
     * false, then 'queryExpr' is definitely not supported for any type of index. If the function
     * returns true then 'queryExpr' may (or may not) be supported by some index.
     */
    static bool logicalNodeMayBeSupportedByAnIndex(const MatchExpression* queryExpr);

private:
    /**
     * Used to keep track of if any $elemMatch predicates were encountered when walking a
     * MatchExpression tree. The presence of an outer $elemMatch can impact whether an index is
     * applicable for an inner MatchExpression. For example, the NOT expression in
     * {a: {$elemMatch: {b: {$ne: null}}} can only use an "a.b" index if that path is not multikey
     * on "a.b". Because of the $elemMatch, it's okay to use the "a.b" index if the path is multikey
     * on "a".
     */
    struct ElemMatchContext {
        ArrayMatchingMatchExpression* innermostParentElemMatch{nullptr};
        StringData fullPathToParentElemMatch{""_sd};
    };

    /**
     * Return true if the index key pattern field 'keyPatternElt' (which belongs to 'index' and is
     * at position 'keyPatternIndex' in the index's keyPattern) can be used to answer the predicate
     * 'node'. When 'node' is a sub-tree of a larger MatchExpression, 'fullPathToNode' is the path
     * traversed to get to this node, otherwise it is empty.
     *
     * For example, {field: "hashed"} can only be used with sets of equalities.
     *              {field: "2d"} can only be used with some geo predicates.
     *              {field: "2dsphere"} can only be used with some other geo predicates.
     */
    static bool _compatible(const BSONElement& keyPatternElt,
                            const IndexEntry& index,
                            std::size_t keyPatternIndex,
                            MatchExpression* node,
                            StringData fullPathToNode,
                            const CollatorInterface* collator,
                            const ElemMatchContext& elemMatchContext);

    static void _rateIndices(MatchExpression* node,
                             std::string prefix,
                             const std::vector<IndexEntry>& indices,
                             const CollatorInterface* collator,
                             const ElemMatchContext& elemMatchContext);

    /**
     * Amend the RelevantTag lists for all predicates in the subtree rooted at 'node' to remove
     * invalid assignments to text indexes.
     *
     * A predicate on a field from a compound text index with a non-empty index prefix
     * (e.g. pred {a: 1, b: 1} on index {a: 1, b: 1, c: "text"}) is only considered valid to
     * assign to the text index if it is a direct child of an AND with the following properties:
     * - it has a TEXT child
     * - for every index prefix component, it has an EQ child on that component's path
     *
     * Note that compatible() enforces the precondition that only EQ nodes are considered
     * relevant to text index prefixes.
     * If there is a relevant compound text index with a non-empty "index prefix" (e.g. the
     * prefix {a: 1, b: 1} for the index {a: 1, b: 1, c: "text"}), amend the RelevantTag(s)
     * created above to remove assignments to the text index where the query does not have
     * predicates over each indexed field of the prefix.
     *
     * This is necessary because text indices do not obey the normal rules of sparseness, in
     * that they generate no index keys for documents without indexable text data in at least
     * one text field (in fact, text indices ignore the sparse option entirely).  For example,
     * given the text index {a: 1, b: 1, c: "text"}:
     *
     * - Document {a: 1, b: 6, c: "hello world"} generates 2 index keys
     * - Document {a: 1, b: 7, c: {d: 1}} generates 0 index keys
     * - Document {a: 1, b: 8} generates 0 index keys
     *
     * As a result, the query {a: 1} *cannot* be satisfied by the text index {a: 1, b: 1, c:
     * "text"}, since documents without indexed text data would not be returned by the query.
     * rateIndices() above will eagerly annotate the pred {a: 1} as relevant to the text index;
     * those annotations get removed here.
     */
    static void stripInvalidAssignmentsToTextIndexes(MatchExpression* node,
                                                     const std::vector<IndexEntry>& indices);

    /**
     * For V1 2dsphere indices we ignore the sparse option.  As such we can use an index
     * like {nongeo: 1, geo: "2dsphere"} to answer queries only involving nongeo.
     *
     * For V2 2dsphere indices also ignore the sparse flag but indexing behavior as compared to
     * V1 is different.  If all of the geo fields are missing from the document we do not index
     * it.  As such we cannot use V2 sparse indices unless we have a predicate over a geo
     * field.
     *
     * 2dsphere indices V2 are "geo-sparse."  That is, if there aren't any geo-indexed fields in
     * a document it won't be indexed.  As such we can't use an index like {foo:1, geo:
     * "2dsphere"} to answer a query on 'foo' if the index is V2 as it will not contain the
     * document {foo:1}.
     *
     * We *can* use it to answer a query on 'foo' if the predicate on 'foo' is AND-related to a
     * predicate on every geo field in the index.
     */
    static void stripInvalidAssignmentsTo2dsphereIndices(MatchExpression* node,
                                                         const std::vector<IndexEntry>& indices);

    /**
     * This function strips RelevantTag assignments to expanded 'wildcard' indexes, in cases where
     * the assignment is incompatible with the query.
     *
     * Specifically, if the query has a TEXT node with both 'text' and 'wildcard' indexes present,
     * then the 'wildcard' index will mark itself as relevant to the '_fts' path reported by the
     * TEXT node. We therefore remove any such misassigned 'wildcard' tags here.
     */
    static void stripInvalidAssignmentsToWildcardIndexes(MatchExpression* root,
                                                         const std::vector<IndexEntry>& indices);

    /**
     * This function strips RelevantTag assignments to partial indices, where the assignment is
     * incompatible with the index's filter expression.
     *
     * For example, suppose there exists a partial index in 'indices' with key pattern {a: 1} and
     * filter expression {f: {$exists: true}}.  If 'node' is {a: 1}, this function would strip the
     * EQ predicate's assignment to the partial index (because if it did not, plans that use this
     * index would miss documents that don't satisfy the filter expression).  On the other hand, if
     * 'node' is {a: 1, f: 1}, then the partial index could be used, and so this function would not
     * strip the assignment.
     *
     * Special note about OR clauses: if 'node' contains a leaf with an assignment to a partial
     * index inside an OR, this function will look both inside and outside the OR clause in an
     * attempt to find predicates that could satisfy the partial index, but these predicates must be
     * wholly contained either inside or outside.
     *
     * To illustrate, given a partial index {a: 1} with filter expression {f: true, g: true}, the
     * assignment of the "a" predicate would not be stripped for either of the following
     * expressions:
     * - {f: true, g: true, $or: [{a: 0}, {a: 1}]}
     * - {$or: [{a: 1, f: true, g: true}, {_id: 1}]}
     *
     * However, the assignment of the "a" predicate would be stripped in the following expression:
     * - {f: true, $or: [{a: 1, g: true}, {_id: 1}]}
     *
     * For the last case, the assignment is stripped is because the {f: true} predicate and the
     * {g: true} predicate are both needed for the {a: 1} predicate to be compatible with the
     * partial index, but the {f: true} predicate is outside the OR while the {g: true} predicate is
     * contained within the OR.
     */
    static void stripInvalidAssignmentsToPartialIndices(MatchExpression* node,
                                                        const std::vector<IndexEntry>& indices);

    static bool notEqualsNullCanUseIndex(const IndexEntry& index,
                                         const BSONElement& keyPatternElt,
                                         std::size_t keyPatternIndex,
                                         const ElemMatchContext& elemMatchContext);
};

}  // namespace monger
