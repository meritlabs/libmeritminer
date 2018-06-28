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
#ifndef MERIT_CUCKOO_MEAN_CUCKOO_H
#define MERIT_CUCKOO_MEAN_CUCKOO_H

#include "ctpl/ctpl.h"

#include <set>
#include <vector>

namespace merit
{
    namespace cuckoo
    {
        // Find proofsize-length cuckoo cycle in random graph
        bool FindCycle(
                const char* hex_header_hash,
                uint32_t hex_header_hash_len,
                uint8_t edgeBits,
                uint8_t proofSize,
                std::set<uint32_t>& cycle,
                size_t threads_number,
                ctpl::thread_pool&);
    }
}

#endif // MERIT_CUCKOO_MEAN_CUCKOO_H
