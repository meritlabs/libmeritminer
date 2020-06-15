/*
 * Copyright (C) 2018-2020 The Merit Foundation
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
#ifndef MERIT_MINER_MINER_H
#define MERIT_MINER_MINER_H

#include <array>
#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <deque>
#include "merit/util/util.hpp"
#include "merit/stratum/stratum.hpp"
#include "merit/miner.hpp"
#include "merit/ctpl/ctpl.h"

#include <boost/optional.hpp>


namespace merit
{
    namespace miner
    {
        int GpuDevices();
        std::vector<merit::GPUInfo> GPUInfo();
        size_t CudaGetFreeMemory(int device);

        using MaybeStratumJob = boost::optional<stratum::Job>;
        class Miner;
        class Worker
        {
            public:
                enum State {Running, NotRunning};

                Worker(const Worker& o);
                Worker(int id, int threads, bool gpu_device, ctpl::thread_pool&, Miner&);

            public:

                int id();
                void run();
                State state() const;

            private:
                std::atomic<State> _state;
                int _id;
                int _threads;
                bool _gpu_device;
                ctpl::thread_pool& _pool;
                Miner& _miner;
        };

        using Workers = std::vector<Worker>;

        struct Stat
        {
            std::chrono::high_resolution_clock::time_point start;
            std::chrono::high_resolution_clock::time_point end;
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
                        const std::vector<int>& gpu_devices,
                        util::SubmitWorkFunc submit_work);
                ~Miner();

            public:
                void submit_job(const stratum::Job&);
                void submit_work(const util::Work&);
                void clear_job();

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
                void wait_for_jobs();

            private:
                std::atomic<State> _state;
                ctpl::thread_pool _pool;
                util::MaybeWork _next_work;
                util::SubmitWorkFunc _submit_work;
                Workers _workers;
                std::vector<std::future<void>> _jobs;
                Stats _stats;
                Stat _total_stats;
                Stat _current_stat;;
                mutable std::mutex _work_mutex;
                mutable std::mutex _stat_mutex;
        };



    }
}
#endif
