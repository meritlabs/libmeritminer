#ifndef MERIT_MINER_STRATUM_H
#define MERIT_MINER_STRATUM_H

#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <atomic>

#include "util/util.hpp"
#include "util/work.hpp"

namespace pt = boost::property_tree;
namespace asio = boost::asio;

namespace merit
{
    namespace stratum
    {
        struct Job {
            std::string id;
            util::ubytes prevhash;
            util::ubytes coinbase;
            size_t coinbase1_size;
            size_t xnonce2_start;
            std::vector<util::ubytes> merkle;
            util::ubytes version;
            util::ubytes nbits;
            int nedgebits;
            util::ubytes time;
            bool clean = false;
            double diff = 0.0;
        };

        using MaybeJob = boost::optional<Job>;

        struct Client {
            public:

                Client();
                ~Client();

                void set_agent(
                        const std::string& software,
                        const std::string& version);

                bool connect(
                        const std::string& url, 
                        const std::string& user, 
                        const std::string& pass);

                void disconnect();
                bool subscribe();
                bool authorize();
                bool run();
                void stop();

                MaybeJob get_job();

                void submit_work(const util::Work&);

            private:
                bool send(const std::string&);
                bool recv(std::string&);
                void cleanup();
                bool subscribe_resp();
                bool handle_command(const std::string& res);
                bool mining_notify(const pt::ptree& params);
                bool mining_difficulty(const pt::ptree& params);
                bool client_reconnect(const pt::ptree& params);
                bool client_get_version(const pt::ptree& params);
                bool client_show_message(const pt::ptree& params, const pt::ptree& id);

            private:
                enum State {
                    Disconnected,
                    Connected,
                    Subscribed,
                    Authorized,
                    Method,
                } _state;

                std::string _agent;
                std::string _url;
                std::string _user;
                std::string _pass;
                std::string _session_id;
                std::string _host;
                std::string _port;
                util::bytes _sockbuf;

                std::atomic<double> _next_diff;
                mutable std::mutex _sock_mutex;
                mutable std::mutex _job_mutex;

                std::vector<unsigned char> _xnonce1;
                size_t _xnonce2_size;
                Job _job;
                bool _new_job;
                asio::io_service _service;
                asio::ip::tcp::socket _socket;
                std::atomic<bool> _running;
        };

        util::Work work_from_job(const stratum::Job&); 
    }

}
#endif
