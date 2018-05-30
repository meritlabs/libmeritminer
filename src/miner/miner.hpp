#ifndef MERIT_MINER_MINER_H
#define MERIT_MINER_MINER_H

#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <deque>
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
                enum State {Running, NotRunning};

                Worker(const Worker& o);
                Worker(int id, int threads, ctpl::thread_pool&, Miner&);

            public:

                int id();
                void run();
                State state() const;

            private:
                std::atomic<State> _state;
                int _id;
                int _threads;
                ctpl::thread_pool& _pool;
                Miner& _miner;
        };

        using Workers = std::vector<Worker>;

        struct Stat
        {
            std::chrono::system_clock::time_point start;
            std::chrono::system_clock::time_point end;
            std::atomic<int> attempts;
            std::atomic<int> cycles;
            std::atomic<int> shares;

            Stat();
            Stat(const Stat&);
            Stat& operator=(const Stat&);

            double seconds() const;
            double attempts_per_second() const;
            double cycles_per_second() const;
            double shares_per_second() const;
        };

        using Stats = std::deque<Stat>;

        class Miner
        {
            public:
                enum State {Running, Stopping, NotRunning};

                Miner(
                        int workers,
                        int threads_per_worker,
                        util::SubmitWorkFunc submit_work);

            public:
                void submit_job(const stratum::Job&);
                void submit_work(const util::Work&);

                void run();
                void stop();
                State state() const;
                bool running() const;
                bool stopping() const;


                util::MaybeWork next_work() const;

                int total_workers() const;

                //Stats
                Stats stats() const;
                Stat total_stats() const;
                const Stat& current_stat() const;
                Stat& current_stat();

            private:
                std::atomic<State> _state;
                ctpl::thread_pool _pool;
                util::MaybeWork _next_work;
                util::SubmitWorkFunc _submit_work;
                Workers _workers;
                Stats _stats;
                Stat _total_stats;
                Stat _current_stat;;
                mutable std::mutex _work_mutex;
                mutable std::mutex _stat_mutex;
        };



    }
}
#endif
