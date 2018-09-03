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
#include <list>
#include <vector>
#include <cstdio>
#include <string>
#include <memory>
#include <string.h>
#include <bitset>
#include <iostream>

#include "merit/PicoSHA2/picosha2.h"
#include "merit/util/util.hpp"

namespace merit {

    namespace merkle {

//        template<typename T, char *(hash_func)(const T &), size_t hash_len>
//        class MerkleNode {
//        protected:
//
//            std::unique_ptr<const MerkleNode> left_, right_;
//            const char *hash_;
//            const std::shared_ptr<T> value_;
//
//            /**
//             * Computes the hash of the children nodes' respective hashes.
//             * In other words, given a node N, with children (N1, N2), whose hash values are,
//             * respectively, H1 and H2, computes:
//             *
//             *     H = hash(H1 || H2)
//             *
//             * where `||` represents the concatenation operation.
//             * If the `right_` descendant is missing, it will simply duplicate the `left_` node's hash.
//             *
//             * For a "leaf" node (both descendants missing), it will use the `hash_func()` to compute the
//             * hash of the stored `value_`.
//             */
//            virtual const char *computeHash() const = 0;
//
//        public:
//
//            /**
//             * Builds a "leaf" node, with no descendants and a `value` that will be copied and stored.
//             * We also compute the hash (`hash_func()`) of the value and store it in this node.
//             *
//             * We assume ownership of the pointer returned by the `hash_func()` which we assume has been
//             * freshly allocated, and will be released upon destruction of this node.
//             */
//            MerkleNode(const T &value) : value_(new T(value)), left_(nullptr), right_(nullptr) {
//                hash_ = hash_func(value);
//            }
//
//            /**
//             * Creates an intermediate node, storing the descendants as well as computing the compound hash.
//             */
//            MerkleNode(const MerkleNode *left,
//                       const MerkleNode *right) :
//                    left_(left), right_(right), value_(nullptr) {
//            }
//
//            /**
//             * Deletes the memory pointed to by the `hash_` pointer: if your `hash_func()` and/or the
//             * `computeHash()` method implementation do not allocate this memory (or you do not wish to
//             * free the allocated memory) remember to override this destructor's behavior too.
//             */
//            virtual ~MerkleNode() {
//                if (hash_) delete[](hash_);
//            }
//
//            /**
//             * Recursively validate the subtree rooted in this node: if executed at the root of the tree,
//             * it will validate the entire tree.
//             */
//            virtual bool validate() const;
//
//            /**
//             * The length of the hash, also part of the template instantiation (`hash_len`).
//             *
//             * @see hash_len
//             */
//            size_t len() const { return hash_len; }
//
//            /**
//             * Returns the buffer containing the hash value, of `len()` bytes length.
//             *
//             * @see len()
//             */
//            const char *hash() const { return hash_; }
//
//            bool hasChildren() const {
//                return left_ || right_;
//            }
//
//            const MerkleNode *left() const { return left_.get(); }
//
//            const MerkleNode *right() const { return right_.get(); }
//        };

        template<const char *(hash_func)(const std::string&)>
        class MerkleTree {
        public:
            explicit MerkleTree(const std::vector<std::string> &hashes_list) {
                steps = calculateSteps(hashes_list);
            }

            std::vector<std::string> branches() {
                std::cout << "=== Trying to get branches..." << std::endl;

                std::vector<std::string> branches;
                std::string tmp_hash;
                std::stringstream stream;

                std::cout << "before foreach loop" << std::endl;

                for (const auto &step: steps) {
                    std::cout << "looping..." << std::endl;
                    std::string tmp{step};
                    std::cout << "Step: " << step << " : " << tmp << std::endl;
                    std::string step_hex;

                    merit::util::to_hex(tmp.begin(), tmp.end(), step_hex);

                    std::cout << "Step HEX: " << step_hex << std::endl;

//                    std::bitset<picosha2::k_digest_size*8> version_bits(step_binary);
//                    stream << std::hex << version_bits.to_ullong();
//                    branches.push_back(stream.str()); stream.str("");
                    branches.push_back(step_hex);
                }

                std::cout << "End loop and function stuff ===" << std::endl;


                return branches;
            }

        private:
            std::vector<const char *> steps;

            std::vector<const char *> calculateSteps(const std::vector<std::string> &hashes_list) {
                std::cout << "=== calculateSteps()" << std::endl;

                std::vector<const char *> steps{};
                std::vector<const char *> L{nullptr};
                for (const auto &el: hashes_list) {
                    L.push_back(el.c_str());
                }

                int startL = 2;
                unsigned long Ll = L.size();

                if (Ll > 1)
                {
                    std::cout << "Calculating steps.." << std::endl;
                    while (true)
                    {
                        std::cout << "   while loop..." << std::endl;

                        if(Ll == 1)
                            break;

                        steps.push_back(L[1]);

                        if(Ll % 2 == 1)
                            L.push_back(L[L.size() - 1]);

                        std::vector<const char*> Ld;

                        for (int i = startL; i < Ll; i += 2) {
                            Ld.push_back(merkleJoin(L[i], L[i+1]));
                        }

                        L.clear(); L.push_back(nullptr);
                        for(const auto& el: Ld)
                            L.push_back(el);

                        Ll = L.size();

                        std::cout << "   end of while loop" << std::endl;

                    }
                }

                std::cout << " end of Calculating steps function ===" << std::endl;

                return steps;
            }

            const char* merkleJoin(const char* h1, const char* h2){
                std::string joined = h1;
                joined.append(h2);

                return hash_func(joined);
            }
        };

        const char *double_hash(const std::string &str);

//        class DoubleSHA256StringMerkleNode : public MerkleNode<std::string, double_hash, picosha2::k_digest_size> {
//        public:
//            DoubleSHA256StringMerkleNode(const std::string &value) : MerkleNode(value) {}
//
//            DoubleSHA256StringMerkleNode(const DoubleSHA256StringMerkleNode *left,
//                                         const DoubleSHA256StringMerkleNode *right) : MerkleNode(left, right) {}
//
//        protected:
//            const char *computeHash() const override;
//        };

        // Recursive implementation of the build algorithm.
//        template<typename NodeType>
//        const NodeType *build_(NodeType *nodes[], size_t len) {
//
//            if (len == 1) return new NodeType(nodes[0], nullptr);
//            if (len == 2) return new NodeType(nodes[0], nodes[1]);
//
//            size_t half = len % 2 == 0 ? len / 2 : len / 2 + 1;
//            return new NodeType(build_(nodes, half), build_(nodes + half, len - half));
//        }
//
//        template<typename T, typename NodeType>
//        const NodeType *build(const std::list<T> &values) {
//
//            NodeType *leaves[values.size()];
//            int i = 0;
//            for (auto value : values) {
//                leaves[i++] = new NodeType(value);
//            }
//
//            return build_(leaves, values.size());
//        };
//
//        template<typename T, typename NodeType>
//        const NodeType *build(const std::vector<T> &values) {
//            std::list<T> lst(values.begin(), values.end());
//
//            return build<T, NodeType>(lst);
//        };


    }
}

#endif //MERIT_MERKLE_H
