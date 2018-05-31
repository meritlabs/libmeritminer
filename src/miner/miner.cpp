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
#include "miner/miner.hpp"
#include "cuckoo/mean_cuckoo.h"

#include<chrono>
#include<iostream>
#include<set>

namespace merit
{
    namespace miner
    {
        namespace 
        {
            const int CUCKOO_PROOF_SIZE = 42;
            const int MAX_STATS = 100;
        }

        Stat::Stat()
        {
            attempts = 0;
            cycles = 0;
            shares = 0;
        }

        Stat::Stat(const Stat& o) :
            start{o.start},
            end{o.end}
        {
            int a = o.attempts;
            int c = o.cycles;
            int s = o.shares;
            attempts = a;
            cycles = c;
            shares = s;
        }

        Stat& Stat::operator=(const Stat& o)
        {
            if(&o == this) return *this;
            int a = o.attempts;
            int c = o.cycles;
            int s = o.shares;
            attempts = a;
            cycles = c;
            shares = s;
            return *this;
        }

        double Stat::seconds() const
        {
            return std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        }

        double Stat::attempts_per_second() const
        {
            const double a = attempts;
            const double s = seconds();
            return s == 0 ? 0 : a / s;
        }

        double Stat::cycles_per_second() const
        {
            const double a = cycles;
            const double s = seconds();
            return s == 0 ? 0 : a / s;
        }

        double Stat::shares_per_second() const
        {
            const double a = shares;
            const double s = seconds();
            return s == 0 ? 0 : a / s;
        }

        Miner::Miner(
                int workers,
                int threads_per_worker,
                util::SubmitWorkFunc submit_work) :
            _submit_work{submit_work},
            _pool{(workers * threads_per_worker) + workers}
        {
            assert(workers >= 1);
            assert(threads_per_worker >= 1);

            _state = NotRunning;
            std::cerr << "info: " << "workers: " << workers << std::endl;
            std::cerr << "info: " << "threads per worker: " << threads_per_worker << std::endl;

            for(int i = 0; i < workers; i++) {
                _workers.emplace_back(i, threads_per_worker, _pool, *this);
            }
        }

        void Miner::submit_job(const stratum::Job& j)
        {
            auto w = stratum::work_from_job(j);
            {
                std::lock_guard<std::mutex> guard{_work_mutex};
                _next_work = w;
            }

            {
                std::lock_guard<std::mutex> sguard{_stat_mutex};
                if(_total_stats.start == std::chrono::system_clock::time_point{}) {
                    _total_stats.start = std::chrono::system_clock::now();
                }

                if(_current_stat.start == std::chrono::system_clock::time_point{}) {
                    _current_stat.start = std::chrono::system_clock::now();
                } else {
                    _total_stats.end = std::chrono::system_clock::now();
                    _current_stat.end = _total_stats.end;

                    auto current = _current_stat;

                    _stats.push_back(current);
                    if(_stats.size() > MAX_STATS) {
                        _stats.pop_front();
                    }

                    _current_stat.start = _current_stat.end;

                    const int a = current.attempts;
                    const int c = current.cycles;
                    const int s = current.shares;

                    _total_stats.attempts += a;
                    _total_stats.cycles += c;
                    _total_stats.shares += s;
                }
            }
        }

        void Miner::clear_job() {
            _next_work.reset();
        }

        void Miner::submit_work(const util::Work& w)
        {
            _submit_work(w);
        }

        void Miner::run()
        {
            std::cerr << "info: " << "starting workers..." << std::endl;
            using namespace std::chrono_literals;
            if(_state != NotRunning) {
                return;
            } 

            _state = Running;

            std::vector<std::future<void>> jobs;
            for(auto& worker : _workers) {
                jobs.push_back(_pool.push([&worker](int id){ worker.run(); }));
            }

            for(auto& j: jobs) { j.get();}
            _state = NotRunning;

            std::cerr << "info: " << "stopped workers." << std::endl;
        }

        void Miner::stop()
        {
            std::cerr << "info: " << "stopping workers..." << std::endl;
            _state = Stopping;
        }

        util::MaybeWork Miner::next_work() const
        {
            std::lock_guard<std::mutex> guard{_work_mutex};
            return _next_work;
        }

        int Miner::total_workers() const 
        {
            return _workers.size();
        }

        Miner::State Miner::state() const 
        {
            return _state;
        }

        bool Miner::running() const
        {
            return _state != NotRunning;
        }

        bool Miner::stopping() const
        {
            return _state == Stopping;
        }

        Stats Miner::stats() const
        {
            std::lock_guard<std::mutex> lock{_stat_mutex};
            return _stats;
        }

        Stat Miner::total_stats() const
        {
            return _total_stats;
        }

        const Stat& Miner::current_stat() const
        {
            return _current_stat;
        }

        Stat& Miner::current_stat()
        {
            return _current_stat;
        }

        Worker::Worker(
                int id,
                int threads,
                ctpl::thread_pool& pool,
                Miner& miner) :
            _state{NotRunning},
            _id{id},
            _threads{threads},
            _pool{pool},
            _miner{miner}
        {
        }

        Worker::Worker(const Worker& o) :
            _id{o._id},
            _threads{o._threads},
            _pool{o._pool},
            _miner{o._miner}
        {
            State s = o._state;
            _state = s;
        }

        int Worker::id()
        {
            return _id;
        }

        bool target_test(
                const std::array<uint32_t, 8>& hash, 
                const std::array<uint32_t, 8>& target)
        {
            for (int i = 7; i >= 0; i--) {
                if (hash[i] > target[i]) {
                    return false;
                }
                if (hash[i] < target[i]) {
                    return true;
                }
            }
            return true;
        }

        void Worker::run()
        {
            std::cerr << "info: " << "started worker: " << _id << std::endl;
            using namespace std::chrono_literals;
            util::Work prev_work;
            uint32_t n =  0xffffffffU / _miner.total_workers() * _id;
            uint32_t end_nonce = 0xffffffffU / _miner.total_workers() * (_id + 1) - 0x20;

            _state = Running;
            while(_miner.state() == Miner::Running)
            {
                auto work = _miner.next_work();

                if(!work) {
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                if(std::equal(
                            prev_work.data.begin(),
                            prev_work.data.begin()+19,
                            work->data.begin())) {
                    work->data[19] = ++n;

                } else {
                    std::cerr << "info: " << "(" << _id << ") got work: " << work->jobid << std::endl;
                    n =  0xffffffffU / _miner.total_workers() * _id;
                    work->data[19] = n;
                    prev_work = *work;
                }

                if(n > end_nonce) {
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                assert(work->data.size() > 16);

                auto bwork = work->data;
                for(int i = 0; i < bwork.size(); i++) {
                    be32enc(&bwork[i], work->data[i]);
                }

                std::array<unsigned char, 32> hash;
                util::double_sha256(
                        hash.data(),
                        reinterpret_cast<const unsigned char*>(bwork.data()),
                        81);
                std::reverse(hash.begin(), hash.end());

                std::string hex_header_hash;
                util::to_hex(hash, hex_header_hash);

                uint8_t proofsize = 42;
                std::set<uint32_t> cycle;

                uint8_t edgebits = work->data[20] >> 24;

                bool found = cuckoo::FindCycle(
                        hex_header_hash.data(),
                        hex_header_hash.size(),
                        edgebits,
                        CUCKOO_PROOF_SIZE,
                        cycle,
                        _threads,
                        _pool);

                auto& stat = _miner.current_stat();
                stat.attempts++;

                if(found) {
                    stat.cycles++;

                    std::copy(cycle.begin(), cycle.end(), work->cycle.begin());
                    std::array<uint32_t, 8> cycle_hash;
                    std::array<uint8_t, 1 + sizeof(uint32_t) * CUCKOO_PROOF_SIZE> cycle_with_size;
                    cycle_with_size[0] = CUCKOO_PROOF_SIZE;
                    std::copy(
                            reinterpret_cast<const uint8_t*>(work->cycle.data()),
                            reinterpret_cast<const uint8_t*>(work->cycle.data()) + sizeof(uint32_t) * work->cycle.size(),
                            cycle_with_size.begin()+1);

                    util::double_sha256(
                            reinterpret_cast<unsigned char*>(cycle_hash.data()),
                            cycle_with_size.data(),
                            cycle_with_size.size());

                    std::string cycle_hash_hex;
                    util::to_hex(cycle_hash, cycle_hash_hex);
                    std::cerr << "info: " << "(" << _id << ") found cycle: " << cycle_hash_hex << std::endl;
                    if(target_test(cycle_hash, work->target)) {
                        stat.shares++;
                        _miner.submit_work(*work);
                    }
                }
            }
            _state = NotRunning;
            std::cerr << "info: " << "worker " << _id << " stopped..." << std::endl;
        }
    }
}
