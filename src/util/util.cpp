/*
 * Copyright (C) 2018-2021 The Merit Foundation
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
#include "merit/util/util.hpp"

#include <array>
#include "merit/PicoSHA2/picosha2.h"
#include <array>

namespace merit
{
    namespace util
    {
        void double_sha256(
                unsigned char* digest,
                const unsigned char* data,
                size_t len)
        {
			
            std::array<unsigned char, picosha2::k_digest_size> d;
            picosha2::hash256(data, data+len, d.begin(), d.end());
            picosha2::hash256(d.begin(), d.end(), digest, digest+picosha2::k_digest_size);
        }
    }
}
