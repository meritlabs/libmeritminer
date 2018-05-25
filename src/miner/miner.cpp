#include "miner/miner.hpp"

#include <crypto++/sha.h>
#include <iterator>

namespace merit
{
    namespace miner
    {
        void diff_to_target(std::array<uint32_t, 8>& target, double diff)
        {
            uint64_t m;
            int k;

            for (k = 7; k > 0 && diff > 1.0; k--)
                diff /= 4294967296.0;
            m = 4294901760.0 / diff;

            std::fill(target.begin(), target.end(), 0);
            target[k] = static_cast<uint32_t>(m);
            target[k + 1] = static_cast<uint32_t>(m >> 32);
        }

        Work work_from_job(const stratum::Job& j) 
        {
            Work w;
            w.jobid = j.id;

            auto xnonce2 = j.coinbase.begin() + j.xnonce2_start;
            auto xnonce2_size = std::distance(xnonce2, j.coinbase.end());
            w.xnonce2.resize(xnonce2_size);
            std::copy(xnonce2, j.coinbase.end(), w.xnonce2.begin());

            std::array<unsigned char, 64> merkle_root;

            //sha256 
            CryptoPP::SHA256{}.CalculateDigest(
                    merkle_root.data(), j.coinbase.data(), j.coinbase.size());

            for(const auto& m : j.merkle) {
                assert(m.size() == 32);
                std::copy(m.begin(), m.end(), merkle_root.data() + 32);
                CryptoPP::SHA256{}.CalculateDigest(
                        merkle_root.data(),
                        merkle_root.data(),
                        merkle_root.size());
            }

            //Create block header
            std::fill(w.data.begin(), w.data.end(), 0);
            w.data[0] = le32dec(j.version.data());
            for(int i = 0; i < 8; i++)
                w.data[1 + i] = le32dec(reinterpret_cast<const uint32_t *>(j.prevhash.data()) + i);
            for(int i = 0; i < 8; i++)
                w.data[9 + i] = be32dec(reinterpret_cast<const uint32_t *>(merkle_root.data()) + i);
            w.data[17] = le32dec(j.time.data());
            w.data[18] = le32dec(j.nbits.data());
            w.data[20] = (j.nedgebits << 24) | (1 << 23);
            w.data[31] = 0x00000288;

            diff_to_target(w.target, j.diff);

            return w;
        }
    }
}
