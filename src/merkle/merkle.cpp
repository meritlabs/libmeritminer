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

#include <merit/merkle/merkle.hpp>

#include "merit/merkle/merkle.hpp"

namespace merit {

    namespace merkle {

        std::vector<std::string> MerkleTree::branches() {
            std::cout << "=== Trying to get branches... === " << std::endl;

            std::cout << "    == Before hexing: " << std::endl;
            for(const auto& s: steps){
                std::cout << "step: " << s << std::endl;
            }
            std::cout << "  That's all ==" << std::endl << std::endl;


            std::vector<std::string> branches;
            std::string tmp_hash;
            std::stringstream stream;

            for (const auto &step: steps) {
                std::string tmp{step};
                std::string step_hex;

                merit::util::to_hex(tmp.begin(), tmp.end(), step_hex);
                branches.push_back(step_hex);
            }

//            std::cout << "End loop and function stuff ===" << std::endl;

            return branches;
        }

        std::vector<const char *> MerkleTree::calculateSteps(const std::vector<char*> &hashes_list) {
            std::cout << "=== calculateSteps()" << std::endl;

            std::vector<const char *> steps{};
            std::vector<const char *> L{nullptr};
            for (const auto &el: hashes_list) {
                L.push_back(el);
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
                        char *join_res = new char[picosha2::k_digest_size];
                        merkleJoin(L[i], L[i+1], join_res);
                        std::cout << "!========= RESULT OF MERKLE JOIN: " << join_res << std::endl;
                        Ld.push_back(join_res);
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

        void MerkleTree::merkleJoin(const char *h1, const char *h2, char* dest){
            std::cout << " == merkleJoin == " << std::endl;
            std::cout << "  h1= " << h1 << std::endl;
            std::cout << "  h2= " << h2 << std::endl;

            std::string joined{h1};
            joined += h2;

            unsigned char buf[joined.size()];   // array to hold the result.
            std::copy(joined.begin(), joined.end(), buf);

//            std::cout << "   joined= " << joined << " | size= " << joined.size() << std::endl;
//            std::cout << "   buf= " << buf << std::endl;
//
//            for (int i = 0; i < 64; ++i) {
//                std::bitset<8> bitset{buf[i]};
//                std::cout << i << " := " << bitset << std::endl;
//            }

//            std::cout << "===   before hashing   ==="<< std::endl;

            auto res = double_hash(buf, joined.size());

//            std::cout << "===   after hashing   ==="<< std::endl;
//            std::cout << "   res=" << res << std::endl;
//
//            // cast from unsigned to the ordinary
            std::string final{reinterpret_cast<const char*>(res), 32};

//            std::cout << "   final=" << final << " with size=" << final.size() << std::endl;
//            std::cout << "   final.c_str() =" << final.c_str()<< std::endl;

            std::copy(final.begin(), final.end(), dest);
        }

        const unsigned char* double_hash(const unsigned char* str, unsigned int size){
            auto *result = new unsigned char[picosha2::k_digest_size];
            std::cout << "!!!double_hash!!!" << std::endl;
            merit::util::double_sha256(result, str, size);
            std::cout << "   result=" << result << std::endl;
            return result;
        }

    }
}

