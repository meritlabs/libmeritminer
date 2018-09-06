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
#include <cstring>
#include <string>
#include <memory>
#include <string.h>
#include <bitset>
#include <iostream>

#include "merit/PicoSHA2/picosha2.h"
#include "merit/util/util.hpp"

namespace merit {

    namespace merkle {

        class MerkleTree {
        public:
            explicit MerkleTree(const std::vector<char *> &hashes_list) {
                steps = calculateSteps(hashes_list);
            }

            std::vector<std::string> branches();

        private:
            std::vector<const char *> steps;

            /**
             * As input takes array of bytes
             * So, before use this function you have to convert hex string into byte array(const char*)
             */
            std::vector<const char *> calculateSteps(const std::vector<char *> &hashes_list);
            void merkleJoin(const char* h1, const char* h2, char* dest);
        };

        const unsigned char *double_hash(const unsigned char* str, unsigned int size);

    }
}

#endif //MERIT_MERKLE_H
