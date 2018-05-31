#include "merit/miner.hpp"
#include "stratum/stratum.hpp"
#include "miner/miner.hpp"

#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace merit
{

    struct Context
    {
        stratum::Client stratum;
        std::unique_ptr<miner::Miner> miner;
        util::SubmitWorkFunc submit_work_func;

        std::thread stratum_thread;
        std::thread mining_thread;
        std::thread collab_thread;
    };

    Context* create_context()
    {
        return new Context;
    }

    void delete_context(Context* c) 
    {
        if(c) { delete c;}
    }

    bool connect_stratum(
            Context* c,
            const char* url,
            const char* user,
            const char* pass)
    try
    {
        assert(c);

        BOOST_LOG_TRIVIAL(info) << "connecting to: " << url; 
        if(!c->stratum.connect(url, user, pass)) {
            BOOST_LOG_TRIVIAL(error) << "error connecting to stratum server: " << url; 
            return false;
        }

        BOOST_LOG_TRIVIAL(info) << "subscribing to: " << url; 
        if(!c->stratum.subscribe()) {
            BOOST_LOG_TRIVIAL(error) << "error subscribing to stratum server: " << url; 
            return false;
        }

        BOOST_LOG_TRIVIAL(info) << "authorizing as: " << user; 
        if(!c->stratum.authorize()) {
            BOOST_LOG_TRIVIAL(error) << "error authorize to stratum server: " << url; 
            return false;
        }

        c->submit_work_func = [c](const util::Work& w) {
            c->stratum.submit_work(w);
        };

        BOOST_LOG_TRIVIAL(info) << "connected to: " << url; 
        return true;
    }
    catch(std::exception& e)
    {
        BOOST_LOG_TRIVIAL(error) << "error connecting to stratum server: " << e.what(); 
        c->stratum.disconnect();
        return false;
    }

    void disconnect_stratum(Context* c)
    {
        assert(c);
        c->stratum.disconnect();
    }

    bool is_stratum_connected(Context* c)
    {
        assert(c);
        return c->stratum.connected();
    }
    
    void init_logging()
    {
    }

    bool run_stratum(Context* c)
    {
        assert(c);
        if(c->stratum.running()) {
            stop_stratum(c);
            return false;
        }

        if(c->stratum_thread.joinable()) {
            c->stratum_thread.join();
        }

        c->stratum_thread = std::thread([c]() {
                try {
                    c->stratum.run();
                } catch(std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "error running stratum: " << e.what(); 
                }
                c->stratum.disconnect();
                BOOST_LOG_TRIVIAL(info) << "stopped stratum."; 
        });

        return true;
    }

    void stop_stratum(Context* c)
    {
        assert(c);
        BOOST_LOG_TRIVIAL(info) << "stopping stratum..."; 
        c->stratum.stop();
    }

    bool run_miner(Context* c, int workers, int threads_per_worker)
    try
    {
        assert(c);
        using namespace std::chrono_literals;

        if(c->miner && c->miner->running()) {
            stop_miner(c);
            return false;
        }

        c->miner.reset();

        BOOST_LOG_TRIVIAL(info) << "setting up miner..."; 
        c->miner = std::make_unique<miner::Miner>(
                workers,
                threads_per_worker,
                c->submit_work_func);

        BOOST_LOG_TRIVIAL(info) << "starting miner..."; 
        if(c->mining_thread.joinable()) {
            c->mining_thread.join();
        }
        c->mining_thread = std::thread([c]() {
                try {
                    c->miner->run();
                } catch(std::exception& e) {
                    c->miner->stop();
                    BOOST_LOG_TRIVIAL(error) << "error running miner: " << e.what(); 
                }
        });

        //TODO: different logic depending on stratum vs solo
        BOOST_LOG_TRIVIAL(info) << "starting collab thread..."; 
        if(c->collab_thread.joinable()) {
            c->collab_thread.join();
        }
        c->collab_thread = std::thread([c]() {
                while(c->miner->state() != miner::Miner::Running) {}
                while(c->miner->running()) 
                try {
                    auto j = c->stratum.get_job();
                    if(!j) { 
                        if(!c->stratum.connected()) {
                            c->miner->clear_job();
                        }
                        std::this_thread::sleep_for(50ms);
                        continue;
                    }

                    c->miner->submit_job(*j);

                } catch(std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "error getting job: " << e.what(); 
                    std::this_thread::sleep_for(50ms);
                }
        });
        return true;
    }
    catch(std::exception& e)
    {
        BOOST_LOG_TRIVIAL(error) << "error starting miners: " << e.what(); 
        return false;
    }

    void stop_miner(Context* c)
    {
        assert(c);
        if(!c->miner) {
            return;
        }
        c->miner->stop();
    }

    bool is_stratum_running(Context* c)
    {
        assert(c);
        return c->stratum.running();
    }

    bool is_miner_running(Context* c)
    {
        assert(c);
        return c->miner && c->miner->running();
    }

    bool is_stratum_stopping(Context* c)
    {
        assert(c);
        return c->stratum.stopping();
    }

    bool is_miner_stopping(Context* c)
    {
        assert(c);
        return c->miner  && c->miner->stopping();
    }

    int number_of_cores()
    {
        return std::thread::hardware_concurrency();
    }

    MinerStat to_public_stat(const miner::Stat& s)
    {
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

    MinerStats get_miner_stats(Context* c)
    {
        assert(c);
        if(!c->miner) return {};

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
}


