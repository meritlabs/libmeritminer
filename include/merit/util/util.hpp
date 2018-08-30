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
#ifndef MERIT_MINER_UTIL_H
#define MERIT_MINER_UTIL_H

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <boost/algorithm/hex.hpp>

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#if !HAVE_DECL_BE32DEC
static inline uint32_t be32dec(const void *pp)
{
    const uint8_t *p = (uint8_t const *)pp;
    return ((uint32_t)(p[3]) + ((uint32_t)(p[2]) << 8) +
            ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24));
}
#endif

#if !HAVE_DECL_LE32DEC
static inline uint32_t le32dec(const void *pp)
{
    const uint8_t *p = (uint8_t const *)pp;
    return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
            ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}
#endif


#if !HAVE_DECL_BE32ENC
static inline void be32enc(void *pp, uint32_t x)
{
    uint8_t *p = (uint8_t *)pp;
    p[3] = x & 0xff;
    p[2] = (x >> 8) & 0xff;
    p[1] = (x >> 16) & 0xff;
    p[0] = (x >> 24) & 0xff;
}
#endif

#if !HAVE_DECL_LE32ENC
static inline void le32enc(void *pp, uint32_t x)
{
    uint8_t *p = (uint8_t *)pp;
    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
    p[2] = (x >> 16) & 0xff;
    p[3] = (x >> 24) & 0xff;
}
#endif

#if defined(__linux__)
// Linux 
#include <endian.h>    // for htole32/64

#elif defined(__APPLE__)
// macOS
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)

#elif (defined(_WIN16) || defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
// Windows
#include <winsock2.h>

#if BYTE_ORDER == LITTLE_ENDIAN

#define htole32(x) (x)
#define htole64(x) (x)
#elif BYTE_ORDER == BIG_ENDIAN

#define htole32(x) __builtin_bswap32(x)
#define htole64(x) __builtin_bswap64(x)

#else

#error byte order not supported

#endif

#define __BYTE_ORDER    BYTE_ORDER
#define __BIG_ENDIAN    BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __PDP_ENDIAN    PDP_ENDIAN

#else
// Nothing matched
#error platform not supported

#endif

namespace merit
{
    namespace util
    {
        using ubytes = std::vector<unsigned char>;
        using bytes = std::vector<char>;

        template<class C>
        bool parse_hex(const std::string& s, C& res)
        try
        {
            boost::algorithm::unhex(s.begin(), s.end(), std::back_inserter(res));
            return true;
        }
        catch(...)
        {
            return false;
        }

        template<class I>
        bool parse_hex_in(const std::string& s, I& it)
        try
        {
            boost::algorithm::unhex(s.begin(), s.end(), it);
            return true;
        }
        catch(...)
        {
            return false;
        }

        template<class C>
        void to_hex(const C& bin, std::string& res)
        {
            boost::algorithm::hex_lower(bin.begin(), bin.end(), std::back_inserter(res));
        }

        template<class I>
        void to_hex(const I& begin, const I& end, std::string& res)
        {
            boost::algorithm::hex_lower(begin, end, std::back_inserter(res));
        }

        bool reverse_hex_string(const std::string& str, std::string& res)
        {
            std::string tmp;

            try
            {
                util::parse_hex(str, tmp);
                tmp.reserve();
                util::to_hex(tmp, res);

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void double_sha256(
                unsigned char* digest,
                const unsigned char* data,
                size_t len);
    }
}
#endif
