#include "miner/miner.hpp"
#include "cuckoo/mean_cuckoo.h"
#include <boost/log/trivial.hpp>

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

            _running = false;
            BOOST_LOG_TRIVIAL(info) << "workers: " << workers;
            BOOST_LOG_TRIVIAL(info) << "threads per worker: " << threads_per_worker;

            for(int i = 0; i < workers; i++) {
                _workers.emplace_back(i, threads_per_worker, _pool, *this);
            }
        }

        void Miner::submit_job(const stratum::Job& j)
        {
            auto w = stratum::work_from_job(j);
            std::lock_guard<std::mutex> guard{_work_mutex};
            _next_work = w;
        }

        void Miner::submit_work(const util::Work& w)
        {
            _submit_work(w);
        }

        void Miner::run()
        {
            BOOST_LOG_TRIVIAL(info) << "starting workers...";
            assert(!_running);
            using namespace std::chrono_literals;
            _running = true;

            std::vector<std::future<void>> jobs;
            for(auto& worker : _workers) {
                jobs.push_back(_pool.push([&worker](int id){ worker.run(); }));
            }

            for(auto& j: jobs) { j.get();}

            BOOST_LOG_TRIVIAL(info) << "stopped workers.";
        }

        void Miner::stop()
        {
            BOOST_LOG_TRIVIAL(info) << "stopping workers...";
            _running = false;
            for(auto& w: _workers) { w.stop(); } 
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

        bool Miner::running() const 
        {
            return _running;
        }

        Worker::Worker(
                int id,
                int threads,
                ctpl::thread_pool& pool,
                Miner& miner) :
            _id{id},
            _threads{threads},
            _pool{pool},
            _miner{miner}
        {
            _running = false;
        }

        Worker::Worker(const Worker& o) :
            _id{o._id},
            _threads{o._threads},
            _pool{o._pool},
            _miner{o._miner}
        {
            _running = o._running ? true : false;
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
            BOOST_LOG_TRIVIAL(info) << "started worker: " << _id;
            using namespace std::chrono_literals;
            _running = true;
            util::Work prev_work;
            uint32_t n =  0xffffffffU / _miner.total_workers() * _id;
            uint32_t end_nonce = 0xffffffffU / _miner.total_workers() * (_id + 1) - 0x20;

            while(_running)
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
                    BOOST_LOG_TRIVIAL(info) << "(" << _id << ") got work: " << work->jobid;
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

                if(found) {
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
                    BOOST_LOG_TRIVIAL(info) << "(" << _id << ") found cycle: " << cycle_hash_hex;
                    if(target_test(cycle_hash, work->target)) {
                        _miner.submit_work(*work);
                    }
                }

            }
        }

        void Worker::stop()
        {
            _running = false;

        }
    }
}
