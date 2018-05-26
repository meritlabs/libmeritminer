
#ifndef MERIT_MINER_WORK_H
#define MERIT_MINER_WORK_H

#include "util/util.hpp"

#include <string>
#include <array>
#include <functional>

#include <boost/optional.hpp>

namespace merit
{
    namespace util
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

        using MaybeWork = boost::optional<Work>;
        using SubmitWorkFunc = std::function<void(const Work&)> ;
    }
}
#endif
