#include "util/util.hpp"

#include <crypto++/sha.h>

namespace merit
{
    namespace util
    {
        void double_sha256(
                unsigned char* digest,
                const unsigned char* data,
                size_t len)
        {
            CryptoPP::SHA256 hash;
            std::array<unsigned char, CryptoPP::SHA256::DIGESTSIZE> d;
            CryptoPP::SHA256{}.CalculateDigest(d.data(), data, len);
            std::array<unsigned char, CryptoPP::SHA256::DIGESTSIZE> d2;
            CryptoPP::SHA256{}.CalculateDigest(d2.data(), d.data(), d.size());
            std::copy(d2.begin(), d2.end(), digest);
        }
    }
}
