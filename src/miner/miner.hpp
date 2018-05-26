#ifndef MERIT_MINER_MINER_H
#define MERIT_MINER_MINER_H

#include <array>
#include <atomic>
#include <thread>
#include "util/util.hpp"
#include "stratum/stratum.hpp"

#include <boost/optional.hpp>

namespace merit
{
    namespace miner
    {

        class Worker
        {
            public:
        };

        using MaybeStratumJob = boost::optional<stratum::Job>;

        class Miner
        {
            public:
                Miner(
                        int wokers,
                        int threads_per_worker,
                        util::SubmitWorkFunc submit_work);

            public:
                void submit_job(const stratum::Job&);
                void run();
                void stop();
                

            private:

                MaybeStratumJob _next_stratum_job;
                util::SubmitWorkFunc _submit_work;
        };

    }
}
#endif
