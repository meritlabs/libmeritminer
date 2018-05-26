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
    {
        assert(c);

        if(!c->stratum.connect(url, user, pass)) {
            BOOST_LOG_TRIVIAL(error) << "error connecting to stratum server: " << url; 
            return false;
        }
        if(!c->stratum.subscribe()) {
            BOOST_LOG_TRIVIAL(error) << "error subscribing to stratum server: " << url; 
            return false;
        }

        if(!c->stratum.authorize()) {
            BOOST_LOG_TRIVIAL(error) << "error authorize to stratum server: " << url; 
            return false;
        }

        c->submit_work_func = [c](const util::Work& w) {
            c->stratum.submit_work(w);
        };

        return true;
    }
    
    void init_logging()
    {
    }

    void run_stratum(Context* c)
    {
        assert(c);
        if(c->stratum_thread.joinable()) {
            stop_stratum(c);
        }

        c->stratum_thread = std::thread([c]() {
                c->stratum.run();
        });
    }

    void stop_stratum(Context* c)
    {
        assert(c);
        c->stratum.stop();
        c->stratum_thread.join();
    }

    void run_miner(Context* c, int workers, int threads_per_worker)
    {
        assert(c);
        using namespace std::chrono_literals;

        if(c->miner && c->miner->running()) {
            stop_miner(c);
        }
        assert(!c->miner);

        c->miner = std::make_unique<miner::Miner>(
                workers,
                threads_per_worker,
                c->submit_work_func);

        std::atomic<bool> started;
        c->mining_thread = std::thread([c, &started]() {
                c->miner->run();
        });

        //TODO: different logic depending on stratum vs solo
        c->collab_thread = std::thread([c]() {
                while(!c->miner->running()) {}
                while(c->miner->running()) {
                    auto j = c->stratum.get_job();
                    if(!j) { 
                        std::this_thread::sleep_for(50ms);
                        continue;
                    }
                    c->miner->submit_job(*j);
                }
        });
    }

    void stop_miner(Context* c)
    {
        assert(c);
        if(!c->miner) {
            return;
        }
        c->miner->stop();
        c->mining_thread.join();
        c->collab_thread.join();
        c->miner.reset();
    }
}


