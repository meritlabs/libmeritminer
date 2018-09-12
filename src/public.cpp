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
#include "merit/miner.hpp"
#include "merit/stratum/stratum.hpp"
#include "merit/miner/miner.hpp"

#include <iostream>

//forward declare SetupBuffers which is in kernel.cu
int SetupKernelBuffers();

namespace merit {

    struct Context {
        stratum::Client stratum;
        std::unique_ptr<miner::Miner> miner;
        util::SubmitWorkFunc submit_work_func;

        std::thread stratum_thread;
        std::thread mining_thread;
        std::thread collab_thread;
    };

    Context *create_context() {
        return new Context;
    }

    void delete_context(Context *c) {
        if (c) { delete c; }
    }

    void set_agent(Context *c, const char *software, const char *version) {
        assert(c);
        c->stratum.set_agent(software, version);
    }

    bool connect_stratum(
            Context *c,
            const char *url,
            const char *user,
            const char *pass)
    try {
        assert(c);

        std::cerr << "info: " << "connecting to: " << url << " with user = " << user  << " pass = " << pass << std::endl;
        if (!c->stratum.connect(url, user, pass)) {
            std::cerr << "error: " << "error connecting to stratum server: " << url << std::endl;
            return false;
        }

        std::cerr << "info: " << "subscribing to: " << url << std::endl;
        if (!c->stratum.subscribe()) {
            std::cerr << "error: " << "error subscribing to stratum server: " << url << std::endl;
            return false;
        }

        std::cerr << "info: " << "authorizing as: " << user << std::endl;
        if (!c->stratum.authorize()) {
            std::cerr << "error: " << "error authorize to stratum server: " << url << std::endl;
            return false;
        }

        c->submit_work_func = [c](const util::Work &w) {
            c->stratum.submit_work(w);
        };

        std::cerr << "info: " << "connected to: " << url << std::endl;
        return true;
    }
    catch (std::exception &e) {
        std::cerr << "error: " << "error connecting to stratum server: " << e.what() << std::endl;
        c->stratum.disconnect();
        return false;
    }

    bool connect_solo_stratum(
            Context *c,
            const char *url,
            const char *user,
            const char *pass)
    {
        try {
            assert(c);

            std::cerr << "info: " << "connecting to: " << url << " with user = " << user << " pass = " << pass
                      << std::endl;
            if (!c->stratum.connect(url, user, pass)) {
                std::cerr << "error: " << "error connecting to stratum server: " << url << std::endl;
                return false;
            }

//        c->submit_work_func = [c](const util::Work &w) {
//            c->stratum.submit_work(w);
//        };

            std::cerr << "info: " << "connected to: " << url << std::endl;

            return true;
        } catch (std::exception &e) {
            std::cerr << "error: " << "error connecting to stratum server: " << e.what() << std::endl;
            c->stratum.disconnect();
            return false;
        }
    }

    void disconnect_stratum(Context *c) {
        assert(c);
        c->stratum.disconnect();
    }

    bool is_stratum_connected(Context *c) {
        assert(c);
        return c->stratum.connected();
    }

    void init() {
        ::SetupKernelBuffers();
    }

    bool run_stratum(Context *c, bool solo_mining) {
        assert(c);
        if (c->stratum.running()) {
            stop_stratum(c);
            return false;
        }

        if (c->stratum_thread.joinable()) {
            c->stratum_thread.join();
        }

        c->stratum_thread = std::thread([c, solo_mining]() {
            try {
                c->stratum.run(solo_mining);
            } catch (std::exception &e) {
                std::cerr << "error: " << "error running stratum: " << e.what() << std::endl;
            }
            c->stratum.disconnect();
            std::cerr << "info: " << "stopped stratum." << std::endl;
        });

        return true;
    }

    void stop_stratum(Context *c) {
        assert(c);
        std::cerr << "info: " << "stopping stratum..." << std::endl;
        c->stratum.stop();
    }

    bool run_miner(Context *c, int workers, int threads_per_worker,
                   const std::vector<int> &gpu_devices, bool solo_mining,
                   const std::string& auth_token)
    try {
        assert(c);
        using namespace std::chrono_literals;

        if (c->miner && c->miner->running()) {
            stop_miner(c);
            return false;
        }

        c->miner.reset();

        std::cerr << "info: " << "setting up miner..." << std::endl;
        c->miner = std::make_unique<miner::Miner>(
                workers,
                threads_per_worker,
                gpu_devices,
                c->submit_work_func);

        std::cerr << "info: " << "starting miner..." << std::endl;
        if (c->mining_thread.joinable()) {
            c->mining_thread.join();
        }
        c->mining_thread = std::thread([c]() {
            try {
                c->miner->run();
            } catch (std::exception &e) {
                c->miner->stop();
                std::cerr << "error: " << e.what() << std::endl;

                return false;
            }
        });

        //TODO: different logic depending on stratum vs solo
        std::cerr << "info: " << "starting collab thread..." << std::endl;
        if (c->collab_thread.joinable()) {
            c->collab_thread.join();
        }

        if (solo_mining) {
            std::cout << "info: " << "Starting solo mining..." << std::endl;

            c->collab_thread = std::thread([c, auth_token]() {
                while (c->miner->state() != miner::Miner::Running) {}

                std::cout << "Miner is Running and ready to start getting job and then mine it" << std::endl;

                // TODO: fix this when implementing server support for solo mining
//                c->stratum.stop(); // disconnect from server

                while (c->miner->running())
                    try {
                        c->stratum.get_solo_job(auth_token);

                        // get block
                        auto j = c->stratum.get_job();
                        while(!j.is_initialized()){
                            std::this_thread::sleep_for(1s);
                            j = c->stratum.get_job();
                        }

                        // submit and then finding cycles starts
                        c->miner->submit_job(*j);

                        std::this_thread::sleep_for(10s); // just for testing

                        // if good cycle, then submit block

                        // again...

//                        std::cout << "Clearing the job" << std::endl;
//                        c->miner->clear_job();
                    } catch (std::exception &e) {
                        std::cerr << "error: " << "error getting job for solo mining: " << e.what() << std::endl;
                        std::this_thread::sleep_for(50ms);
                    }
            });
        } else {
            c->collab_thread = std::thread([c]() {
                while (c->miner->state() != miner::Miner::Running) {}
                while (c->miner->running())
                    try {
                        auto j = c->stratum.get_job();
                        if (!j) {
                            if (!c->stratum.connected()) {
                                c->miner->clear_job();
                            }
                            std::this_thread::sleep_for(50ms);
                            continue;
                        }

                        c->miner->submit_job(*j);

                    } catch (std::exception &e) {
                        std::cerr << "error: " << "error getting job: " << e.what() << std::endl;
                        std::this_thread::sleep_for(50ms);
                    }
            });
            return true;
        }
    }
    catch (std::exception &e) {
        std::cerr << "error: " << "error starting miners: " << e.what() << std::endl;
        return false;
    }

    void stop_miner(Context *c) {
        assert(c);
        if (!c->miner) {
            return;
        }
        c->miner->stop();
    }

    bool is_stratum_running(Context *c) {
        assert(c);
        return c->stratum.running();
    }

    bool is_miner_running(Context *c) {
        assert(c);
        return c->miner && c->miner->running();
    }

    bool is_stratum_stopping(Context *c) {
        assert(c);
        return c->stratum.stopping();
    }

    bool is_miner_stopping(Context *c) {
        assert(c);
        return c->miner && c->miner->stopping();
    }

    int number_of_cores() {
        return std::thread::hardware_concurrency();
    }

    int number_of_gpus() {
        return miner::GpuDevices();
    }

    size_t free_memory_on_gpu(int device) {
        return miner::CudaGetFreeMemory(device);
    };


    MinerStat to_public_stat(const miner::Stat &s) {
        return {
                s.start.time_since_epoch().count(),
                s.end.time_since_epoch().count(),
                s.seconds(),
                s.attempts_per_second(),
                s.cycles_per_second(),
                s.shares_per_second(),
                s.attempts,
                s.cycles,
                s.shares
        };
    }

    MinerStats get_miner_stats(Context *c) {
        assert(c);
        if (!c->miner) return {};

        auto total = c->miner->total_stats();
        auto history = c->miner->stats();
        auto current = c->miner->current_stat();

        MinerStats s;
        s.total = to_public_stat(total);
        s.current = to_public_stat(current);

        s.history.resize(history.size());
        std::transform(
                history.begin(),
                history.end(),
                s.history.begin(),
                to_public_stat);

        return s;
    }

    std::vector<merit::GPUInfo> gpus_info() {
        return miner::GPUInfo();
    };


}


