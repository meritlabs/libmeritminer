/*
 * Copyright (C) 2018 The Merit Foundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either vedit_refsion 3 of the License, or
 * (at your option) any later vedit_refsion.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * Botan library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than Botan. If you modify file(s) with
 * this exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do
 * so, delete this exception statement from your version. If you delete
 * this exception statement from all source files in the program, then
 * also delete it here.
 */

#ifndef MERIT_MERKLE_H
#define MERIT_MERKLE_H

#include <cctype>
#include <vector>
#include <cstdio>
#include <string>
#include <memory>

namespace merit {

    namespace merkle {

        template<typename T, char *(hash_func)(const T &), size_t hash_len>
        class MerkleNode {
        protected:

            std::unique_ptr<const MerkleNode> left_, right_;
            const char *hash_;
            const std::shared_ptr<T> value_;

            /**
             * Computes the hash of the children nodes' respective hashes.
             * In other words, given a node N, with children (N1, N2), whose hash values are,
             * respectively, H1 and H2, computes:
             *
             *     H = hash(H1 || H2)
             *
             * where `||` represents the concatenation operation.
             * If the `right_` descendant is missing, it will simply duplicate the `left_` node's hash.
             *
             * For a "leaf" node (both descendants missing), it will use the `hash_func()` to compute the
             * hash of the stored `value_`.
             */
            virtual const char *computeHash() const = 0;

        public:

            /**
             * Builds a "leaf" node, with no descendants and a `value` that will be copied and stored.
             * We also compute the hash (`hash_func()`) of the value and store it in this node.
             *
             * We assume ownership of the pointer returned by the `hash_func()` which we assume has been
             * freshly allocated, and will be released upon destruction of this node.
             */
            MerkleNode(const T &value) : value_(new T(value)), left_(nullptr), right_(nullptr) {
                hash_ = hash_func(value);
            }

            /**
             * Creates an intermediate node, storing the descendants as well as computing the compound hash.
             */
            MerkleNode(const MerkleNode *left,
                       const MerkleNode *right) :
                    left_(left), right_(right), value_(nullptr) {
            }

            /**
             * Deletes the memory pointed to by the `hash_` pointer: if your `hash_func()` and/or the
             * `computeHash()` method implementation do not allocate this memory (or you do not wish to
             * free the allocated memory) remember to override this destructor's behavior too.
             */
            virtual ~MerkleNode() {
                if (hash_) delete[](hash_);
            }

            /**
             * Recursively validate the subtree rooted in this node: if executed at the root of the tree,
             * it will validate the entire tree.
             */
            virtual bool validate() const;

            /**
             * The length of the hash, also part of the template instantiation (`hash_len`).
             *
             * @see hash_len
             */
            size_t len() const { return hash_len; }

            /**
             * Returns the buffer containing the hash value, of `len()` bytes length.
             *
             * @see len()
             */
            const char *hash() const { return hash_; }

            bool hasChildren() const {
                return left_ || right_;
            }

            const MerkleNode *left() const { return left_.get(); }

            const MerkleNode *right() const { return right_.get(); }
        };
    }
}

#endif //MERIT_MERKLE_H
