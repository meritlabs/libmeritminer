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
#ifndef MERIT_MINER_STRATUM_H
#define MERIT_MINER_STRATUM_H

#include <thread>
#include <random>
#include <mutex>
#include <array>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <thread>
#include <atomic>

#include "merit/util/util.hpp"
#include "merit/util/work.hpp"

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
            size_t xnonce2_size;
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
                bool run(bool solo_mining);
                void stop();
                bool connected() const;
                bool running() const;
                bool stopping() const;

                MaybeJob get_job();
                MaybeJob get_solo_job(const std::string& auth_token);

                unsigned int get_solo_job_id();

                void submit_work(const util::Work&);

            private:
                bool reconnect();
                bool send(const std::string&);
                bool recv(std::string&);
                bool recv_with_headers(std::string&);
                void cleanup();
                bool subscribe_resp();
                bool handle_command(const pt::ptree&, const std::string& res);
                bool mining_notify(const pt::ptree& params);
                bool mining_difficulty(const pt::ptree& params);
                bool mining_set_solo_job(const pt::ptree& params);
                pt::ptree convert_blocktemplate(const pt::ptree& params);
                bool client_reconnect(const pt::ptree& params);
                bool client_get_version(const pt::ptree& params);
                bool client_show_message(const pt::ptree& params, const pt::ptree& id);

            private:
                enum ConnState {
                    Disconnected,
                    Connecting,
                    Connected,
                    Subscribing,
                    Subscribed,
                    Authorizing,
                    Authorized,
                };
                std::atomic<ConnState> _state;

                enum RunState {
                    NotRunning,
                    Running,
                    Stopping
                };
                std::atomic<RunState> _run_state;

                std::string _agent;
                std::string _url;
                std::string _user;
                std::string _pass;
                std::string _session_id;
                std::string _host;
                std::string _port;
                util::bytes _sockbuf;

                unsigned int static _solo_job_id;

                std::atomic<double> _next_diff;
                mutable std::mutex _sock_mutex;
                mutable std::mutex _job_mutex;

                std::vector<unsigned char> _xnonce1;
                size_t _xnonce2_size;
                Job _job;
                bool _new_job;
                asio::io_service _service;
                asio::ip::tcp::socket _socket;
                std::random_device _rd;
                std::mt19937 _mt;
        };

        util::Work work_from_job(const stratum::Job&); 
    }

}
#endif
