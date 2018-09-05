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

        std::vector<const char *> MerkleTree::calculateSteps(const std::vector<std::string> &hashes_list) {
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

        const char *MerkleTree::merkleJoin(const char *h1, const char *h2){
            std::string joined = h1;
            joined.append(h2);

            return double_hash(joined);
        }

        const char* double_hash(const std::string& str){
            auto *result = new unsigned char[str.size()];
            merit::util::double_sha256(result, reinterpret_cast<const unsigned char *>(str.c_str()), str.size());
            return reinterpret_cast<const char *>(result);
        }

    }
}

