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

        std::string HexCharToBin( char c ) {
            switch( c ) {
                case '0'    : return "0000";
                case '1'    : return "0001";
                case '2'    : return "0010";
                case '3'    : return "0011";
                case '4'    : return "0100";
                case '5'    : return "0101";
                case '6'    : return "0110";
                case '7'    : return "0111";
                case '8'    : return "1000";
                case '9'    : return "1001";
                case 'A'    : return "1010";
                case 'a'    : return "1010";
                case 'B'    : return "1011";
                case 'b'    : return "1011";
                case 'C'    : return "1100";
                case 'c'    : return "1100";
                case 'D'    : return "1101";
                case 'd'    : return "1101";
                case 'E'    : return "1110";
                case 'e'    : return "1110";
                case 'F'    : return "1111";
                case 'f'    : return "1111";
            }
        }


        std::string HexStrToBin(const std::string & hs) {
            std::string bin;
            for(auto c : hs) {
                bin += HexCharToBin(c);
            }
            return bin;
        }

        std::vector<char> ReverseByteOrder(const std::string& binary){
            std::vector<char> result;

            for (unsigned int i = 0; i < 8; ++i) { // binary.size() / 32
                std::bitset<32> tmp_32{binary.substr(i*32, (i+1) * 32)};
                auto endian_32 = htonl(static_cast<uint32_t>(tmp_32.to_ulong()));

                std::bitset<32> tmp_32_{endian_32};
                std::cout << " ||| function: " << tmp_32_.to_string() << std::endl;

                for (int j = 0; j < 4; j++)
                    result.push_back(static_cast<char &&>(endian_32 >> (j * 8)));
            }

//            std::reverse(result.begin(), result.end());

            return result;
        }

    }
}
