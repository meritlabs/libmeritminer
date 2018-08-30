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
#include "merit/miner/miner.hpp"
#include "merit/cuckoo/mean_cuckoo.h"
#include "merit/crypto/siphash.h"
#include "merit/blake2/blake2.h"

#include <chrono>
#include <iostream>
#include <set>


using Cycle = std::set<uint32_t>;
using Cycles = std::vector<Cycle>;

#ifdef CUDA_ENABLED

bool FindCyclesOnCudaDevice(
        uint64_t sip_k0, uint64_t sip_k1,
        uint8_t edgebits,
        uint8_t proof_size,
        Cycles& cycles,
        int device);

int CudaDevices();

std::vector<merit::GPUInfo> GPUsInfo();

size_t CudaGetFreeMemory(int device);

#else
int CudaDevices() { return 0;}

std::vector<merit::GPUInfo> GPUsInfo(){
    return std::vector<merit::GPUInfo>();
};

size_t CudaGetFreeMemory(int device) {
    return 0;
}

int SetupKernelBuffers() { 
    return 0;
}

#endif

namespace merit
{
    namespace miner
    {
        namespace
        {
            const int CUCKOO_PROOF_SIZE = 42;
            const int MAX_STATS = 100;

            bool work_same(const util::Work& a, const util::Work& b)
            {
                return std::equal(
                        a.data.begin(),
                        a.data.begin()+19,
                        b.data.begin());
            }
        }

        int GpuDevices()
        {
            return ::CudaDevices();
        }

        std::vector<merit::GPUInfo> GPUInfo()
        {
            return ::GPUsInfo();
        }

        size_t CudaGetFreeMemory(int device)
        {
            return ::CudaGetFreeMemory(device);
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
            const double c = cycles;
            const double s = seconds();
            return s == 0 ? 0 : c / s;
        }

        double Stat::shares_per_second() const
        {
            const double h = shares;
            const double s = seconds();
            return s == 0 ? 0 : h / s;
        }

        Miner::Miner(
                int workers,
                int threads_per_worker,
                const std::vector<int>& gpu_devices,
                util::SubmitWorkFunc submit_work) :
            _submit_work{submit_work},
            _pool{static_cast<int>((workers * threads_per_worker) + workers + gpu_devices.size())}
        {
//            gpu_devices = std::min(gpu_devices, GpuDevices());

            assert(workers >= 0);
            assert(threads_per_worker >= 0);

            _state = NotRunning;
            std::cerr << "info: " << "workers: " << workers << std::endl;
            std::cerr << "info: " << "threads per worker: " << threads_per_worker << std::endl;
            std::cerr << "info: " << "gpu devices: " << gpu_devices.size() << std::endl;

            for(int i = 0; i < workers; i++) {
                _workers.emplace_back(i, threads_per_worker, false, _pool, *this);
            }

            for(int i = 0; i < gpu_devices.size(); i++) {
                _workers.emplace_back(gpu_devices[i], threads_per_worker, true, _pool, *this);
            }
        }

        void Miner::submit_job(const stratum::Job& j)
        {
            std::cout << "submit_job" << std::endl;

            auto w = stratum::work_from_job(j);
            util::MaybeWork prev_work;
            {
                std::lock_guard<std::mutex> guard{_work_mutex};
                prev_work = _next_work;
                _next_work = w;
            }

            {
                std::lock_guard<std::mutex> sguard{_stat_mutex};
                if(_total_stats.start == std::chrono::high_resolution_clock::time_point{}) {
                    _total_stats.start = std::chrono::high_resolution_clock::now();
                } else {
                    if(!_next_work || !prev_work || work_same(*prev_work, *_next_work)) {
                        return;
                    }

                    _total_stats.end = std::chrono::high_resolution_clock::now();
                    _current_stat.end = _total_stats.end;

                    auto current = _current_stat;

                    _current_stat.attempts = 0;
                    _current_stat.cycles = 0;
                    _current_stat.shares = 0;

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
                jobs.push_back(_pool.push(
                            [&worker](int id){ 
                                try {
                                    worker.run(); 
                                } catch( std::exception& e) {
                                    std::cerr << "mining worker " << id << " error: " << e.what() << std::endl;
                                }
                            }));
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
                bool gpu_device,
                ctpl::thread_pool& pool,
                Miner& miner) :
            _state{NotRunning},
            _id{id},
            _threads{threads},
            _gpu_device{gpu_device},
            _pool{pool},
            _miner{miner}
        {
        }

        Worker::Worker(const Worker& o) :
            _id{o._id},
            _threads{o._threads},
            _gpu_device{o._gpu_device},
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
                std::cout << "in miner cycle" << std::endl;
                auto work = _miner.next_work();

                if(!work) {
                    std::cout << "There is no work!" << std::endl;
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                if(work_same(prev_work, *work)) {
                    work->data[19] = ++n;
                } else {
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
                Cycles cycles;

                uint8_t edgebits = work->data[20] >> 24;

#if CUDA_ENABLED
                bool found = false;
                if(!_gpu_device) {
                    found = cuckoo::FindCycles(
                            hex_header_hash.data(),
                            hex_header_hash.size(),
                            edgebits,
                            CUCKOO_PROOF_SIZE,
                            cycles,
                            _threads,
                            _pool);
                } else {
                    crypto::siphash_keys keys;
                    char hdrkey[32];
                    blake2b(
                            reinterpret_cast<void*>(hdrkey),
                            sizeof(hdrkey),
                            reinterpret_cast<const void*>(hex_header_hash.data()),
                            hex_header_hash.size(), 0, 0);
                    crypto::setkeys(&keys, hdrkey);

                    found = FindCyclesOnCudaDevice(
                            keys.k0, keys.k1,
                            edgebits,
                            CUCKOO_PROOF_SIZE,
                            cycles,
                            _id);
                }
#else
                bool found = cuckoo::FindCycles(
                        hex_header_hash.data(),
                        hex_header_hash.size(),
                        edgebits,
                        CUCKOO_PROOF_SIZE,
                        cycles,
                        _threads,
                        _pool);
#endif

                auto& stat = _miner.current_stat();
                stat.attempts++;

                if(found) {
                    std::cout << "CYCLE FOUND" << std::endl;

                    stat.cycles+=cycles.size();

                    int idx = 0;
                    for(const auto& cycle: cycles) {
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
                        std::cerr << "info: " << "(" << _id << ") found cycle (" << idx << "): " << cycle_hash_hex << std::endl;
                        if(target_test(cycle_hash, work->target)) {
                            stat.shares++;
                            _miner.submit_work(*work);
                        }
                        idx++;
                    }
                }
            }
            _state = NotRunning;
            std::cerr << "info: " << "worker " << _id << " stopped..." << std::endl;
        }
    }
}
