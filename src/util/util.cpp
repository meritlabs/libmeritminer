#include "util/util.hpp"

#include "PicoSHA2/picosha2.h"

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
