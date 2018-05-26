#ifndef MERIT_MINER_MINER_H
#define MERIT_MINER_MINER_H

#include <array>
#include <atomic>
#include <thread>
#include "util/util.hpp"
#include "stratum/stratum.hpp"

#include <boost/optional.hpp>

#include "ctpl/ctpl.h"

namespace merit
{
    namespace miner
    {

        using MaybeStratumJob = boost::optional<stratum::Job>;
        class Miner;
        class Worker
        {
            public:
                Worker(const Worker& o);
                Worker(int id, int threads, ctpl::thread_pool&, Miner&);
            public:

                int id();
                void run();
                void stop();

            private:
                int _id;
                int _threads;
                ctpl::thread_pool& _pool;
                Miner& _miner;
                std::atomic<bool> _running;
        };

        using Workers = std::vector<Worker>;

        class Miner
        {
            public:
                Miner(
                        int workers,
                        int threads_per_worker,
                        util::SubmitWorkFunc submit_work);

            public:
                void submit_job(const stratum::Job&);
                void submit_work(const util::Work&);

                void run();
                void stop();

                util::MaybeWork next_work() const;

                int total_workers() const;
                bool running() const;

            private:
                ctpl::thread_pool _pool;
                util::MaybeWork _next_work;
                util::SubmitWorkFunc _submit_work;
                std::atomic<bool> _running;
                Workers _workers;
                mutable std::mutex _work_mutex;
        };



    }
}
#endif
