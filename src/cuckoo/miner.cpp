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

#include "cuckoo/miner.h"
#include "cuckoo/mean_cuckoo.h"

#include <assert.h>
#include <numeric>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

namespace merit
{
    namespace cuckoo
    {
        /**
         * Single Pro
         */
        bool SingleProofAttempt(
                const char* hex_header_hash,
                size_t hex_header_hash_len,
                unsigned int nBits,
                uint8_t edgeBits,
                std::set<uint32_t>& cycle,
                int proof_size,
                size_t nThreads,
                ctpl::thread_pool& pool)
        {
            assert(cycle.empty());

            bool cycleFound =
                FindCycle(
                        hex_header_hash,
                        hex_header_hash_len,
                        edgeBits,
                        proof_size,
                        cycle,
                        nThreads, pool);

            if (cycleFound) {
                return true;
            }

            cycle.clear();

            return false;
        }
    }
}
