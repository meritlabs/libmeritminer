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

#include "merit/merkle/merkle.hpp"

namespace merit {

    namespace merkle {

        template<typename T, char *(hash_func)(const T &), size_t hash_len>
        bool MerkleNode<T, hash_func, hash_len>::validate() const {
            // If either child is not valid, the entire subtree is invalid too.
            if (left_ && !left_->validate()) {
                return false;
            }
            if (right_ && !right_->validate()) {
                return false;
            }

            std::unique_ptr<const char> computedHash(hasChildren() ? computeHash() : hash_func(*value_));
            return memcmp(hash_, computedHash.get(), len()) == 0;
        }

        const char *DoubleSHA256StringMerkleNode::computeHash() const {
            if(!right_)
                return left_.get()->hash();

            return (std::string(left_.get()->hash()) + std::string(right_.get()->hash())).c_str();
        }

        char* double_hash(const std::string& str){
            auto *result = new unsigned char[str.size()];
            merit::util::double_sha256(result, reinterpret_cast<const unsigned char *>(str.c_str()), str.size());
            return reinterpret_cast<char *>(result);
        }

        // Recursive implementation of the build algorithm.
        template<typename NodeType>
        const NodeType *build_(NodeType *nodes[], size_t len) {

            if (len == 1) return new NodeType(nodes[0], nullptr);
            if (len == 2) return new NodeType(nodes[0], nodes[1]);

            size_t half = len % 2 == 0 ? len / 2 : len / 2 + 1;
            return new NodeType(build_(nodes, half), build_(nodes + half, len - half));
        }

        template<typename T, typename NodeType>
        const NodeType *build(const std::list<T> &values) {

            NodeType *leaves[values.size()];
            int i = 0;
            for (auto value : values) {
                leaves[i++] = new NodeType(value);
            }

            return build_(leaves, values.size());
        };

        template<typename T, typename NodeType>
        const NodeType *build(const std::vector<T> &values) {
            std::list<T> lst(values.begin(), values.end());

            return build<T, NodeType>(lst);
        };
    }
}

