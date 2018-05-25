#ifndef MERIT_MINER_MINER_H
#define MERIT_MINER_MINER_H

#include <array>
#include "util/util.hpp"
#include "stratum/stratum.hpp"

namespace merit
{
    namespace miner
    {
        struct Work
        {
            std::string jobid;
            std::array<uint32_t, 32> data;
            std::array<uint32_t, 8> target;
            std::array<uint32_t, 42> cycle;

            int height;
            std::string txs;
            std::string invites;
            std::string referrals;
            std::string workid;

            util::ubytes xnonce2;
        };

        Work work_from_job(const stratum::Job&); 
    }
}
#endif
