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
#include "merit/stratum/stratum.hpp"

#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>

#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
#include <winsock2.h>
#include <mstcpip.h>
#endif


namespace pt = boost::property_tree;

namespace merit
{
    namespace stratum
    {
        namespace
        {
            const size_t MAX_ALLOC_SIZE = 2*1024*1024;
            const size_t BUFFER_SIZE = 2048;
            const size_t RECV_SIZE = (BUFFER_SIZE - 4);
            const int CKEEPALIVE = 1;
            const int CTCP_KEEPCNT = 3;
            const int CTCP_KEEPIDLE = 50;
            const int CTCP_KEEPINTVL = 30;

            const std::string PACKAGE_NAME = "libmeritminer";
            const std::string PACKAGE_VERSION = "0.0.1";
            const std::string USER_AGENT = PACKAGE_NAME + "/" + PACKAGE_VERSION;
        }

        Client::Client() :
            _state{Disconnected},
            _run_state{NotRunning},
            _agent{USER_AGENT},
            _socket{_service},
            _new_job{false},
            _mt{_rd()}
        {
        }

        unsigned int Client::_solo_job_id = 0;

        Client::~Client()
        {
            disconnect();
        }

        void Client::set_agent(
                const std::string& software,
                const std::string& version)
        {
            _agent = software + "/" + version;
            std::cerr << "setting agent to: " << _agent << std::endl;
        }

        bool set_socket_opts(asio::ip::tcp::socket& sock)
        {

#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
            struct tcp_keepalive vals;
            vals.onoff = 1;
            vals.keepalivetime = CTCP_KEEPIDLE * 1000;
            vals.keepaliveinterval = CTCP_KEEPINTVL * 1000;
            DWORD outputBytes;
            if (WSAIoctl(
                        sock.native_handle(),
                        SIO_KEEPALIVE_VALS,
                        &vals,
                        sizeof(vals),
                        NULL, 0, &outputBytes, NULL, NULL)) {
                std::cerr << "error: " << "error setting keepalive" << std::endl;
                return false;
            }
#else
            if (setsockopt(
                        sock.native_handle(),
                        SOL_SOCKET,
                        SO_KEEPALIVE,
                        &CKEEPALIVE,
                        sizeof(CKEEPALIVE))) {
                std::cerr << "error: " << "error setting keepalive" << std::endl;
                return false;
            }
#ifdef __linux
            if (setsockopt(
                        sock.native_handle(),
                        SOL_TCP,
                        TCP_KEEPCNT,
                        &CTCP_KEEPCNT,
                        sizeof(CTCP_KEEPCNT))) {
                std::cerr << "error: " << "error setting keepcnt" << std::endl;
                return false;
            }
            if (setsockopt(
                        sock.native_handle(),
                        SOL_TCP,
                        TCP_KEEPIDLE,
                        &CTCP_KEEPIDLE,
                        sizeof(CTCP_KEEPIDLE))) {
                std::cerr << "error: " << "error setting keepidle" << std::endl;
                return false;
            }
            if (setsockopt(
                        sock.native_handle(),
                        SOL_TCP,
                        TCP_KEEPINTVL,
                        &CTCP_KEEPINTVL,
                        sizeof(CTCP_KEEPINTVL))) {
                std::cerr << "error: " << "error setting keepintvl" << std::endl;
                return false;
            }
#endif
#ifdef __APPLE_CC__
            if (setsockopt(
                        sock.native_handle(),
                        IPPROTO_TCP,
                        TCP_KEEPALIVE,
                        &CTCP_KEEPINTVL,
                        sizeof(CTCP_KEEPINTVL))) {
                std::cerr << "error: " << "error setting keepintvl" << std::endl;
                return false;
            }
#endif
#endif
            return true;
        }

        bool Client::connect(
                    const std::string& iurl,
                    const std::string& iuser,
                    const std::string& ipass)
        try
        {
            if(_state != Disconnected) {
                disconnect();
            }
            _state = Connecting;

            _url = iurl;
            _user = iuser;
            _pass = ipass;

            auto host_pos = _url.find("://") + 3;
            auto port_pos = _url.find(":", 14) + 1;
            _host = _url.substr(host_pos, port_pos - host_pos - 1);
            _port = _url.substr(port_pos);

            std::cerr << "info: " << "host: " << _host << std::endl;
            std::cerr << "info: " << "port: " << _port << std::endl;


            asio::ip::tcp::resolver resolver{_service};
            asio::ip::tcp::resolver::query query{_host, _port};
            auto endpoints = resolver.resolve(query);

            boost::system::error_code e;
            boost::asio::connect(_socket, endpoints, e);

            if(e) {
                std::cout << "Error: " << e << std::endl;
                disconnect();
                return false;
            }

            if(!set_socket_opts(_socket)) {
                std::cout << "Error while setting socket options" << std::endl;
                return false;
            }

            _state = Connected;
            return true;
        }
        catch(std::exception& e)
        {
            _state = Disconnected;
            throw;
        }

        void Client::disconnect()
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            _sockbuf.clear();
            _next_diff = 0.0;
            _xnonce1.clear();
            _xnonce2_size = 0;
            _job = Job{};
            _new_job = false;

            _socket.close();
            _state = Disconnected;
        }

        bool parse_json(const std::string& s, pt::ptree& r)
        try
        {
            std::stringstream ss;
            ss << s;

            pt::read_json(ss, r);
            return true;
        }
        catch(std::exception& e)
        {
            std::cerr << "error: " << "error parsing json: " << e.what() << std::endl;
            return false;
        }

        bool find_session_id(const pt::ptree& p, std::string& session_id)
        {
            bool mining_notify_found = false;
            for(const auto& arr : p) {

                if(arr.second.empty()) {
                    continue;
                }

                for(const auto& itm : arr.second) {
                    auto s = itm.second.get_value<std::string>();
                    if(mining_notify_found) {
                        session_id = s;
                        return true;
                    }

                    if(s == "mining.notify") {
                        mining_notify_found = true;
                    }
                }
            }
            return false;
        }

        bool Client::mining_notify(const pt::ptree& params)
        {
            auto v = params.begin();

            auto job_id = v->second.get_value_optional<std::string>(); v++;
            if(!job_id) { return false; }
            std::cout << "job_id: " << typeid(job_id).name() << std::endl;

            auto prevhash = v->second.get_value_optional<std::string>(); v++;
            if(!prevhash) { return false; }

            auto coinbase1 = v->second.get_value_optional<std::string>(); v++;
            if(!coinbase1) { return false; }

            auto coinbase2 = v->second.get_value_optional<std::string>(); v++;
            if(!coinbase2) { return false; }

            auto merkle_array = v->second; v++;

            auto version = v->second.get_value_optional<std::string>(); v++;
            if(!version) { return false; }

            auto nbits = v->second.get_value_optional<std::string>(); v++;
            if(!nbits) { return false; }

            auto edgebits = v->second.get_value_optional<int>(); v++;
            if(!edgebits) { return false; }

            auto time = v->second.get_value_optional<std::string>(); v++;
            if(!time) { return false; }

            auto is_clean = v->second.get_value_optional<bool>(); v++;
            if(!is_clean) { return false; }

            if(prevhash->size() != 64) { return false; }
            if(version->size() != 8) { return false; }
            if(nbits->size() != 8) { return false; }
            if(time->size() != 8) { return false; }

            std::lock_guard<std::mutex> guard{_job_mutex};
            Job j;

            if(!util::parse_hex(*prevhash, j.prevhash)) { return false;}
            if(!util::parse_hex(*version, j.version)) { return false;}
            if(!util::parse_hex(*nbits, j.nbits)) { return false;}
            if(!util::parse_hex(*time, j.time)) { return false;}

            j.nedgebits = *edgebits;

            j.coinbase1_size = coinbase1->size()/2;

            if(!util::parse_hex(*coinbase1, j.coinbase)) { return false; }
            j.coinbase.insert(j.coinbase.end(), _xnonce1.begin(), _xnonce1.end());

            j.xnonce2_start = j.coinbase.size();
            j.xnonce2_size = _xnonce2_size;

            j.coinbase.insert(j.coinbase.end(), _xnonce2_size, 0);
            if(!util::parse_hex(*coinbase2, j.coinbase)) { return false; }

            j.id = *job_id;

            for(const auto& hex : merkle_array) {
                std::string s = hex.second.get_value<std::string>();
                util::ubytes bin;
                if(!util::parse_hex(s, bin)) {
                    return false;
                }

                j.merkle.push_back(bin);
            }

            j.diff = _next_diff;
            j.clean = *is_clean;
            _new_job = true;
            std::cerr << "info: " << "notify: " << j.id << " time: " << *time << " nbits: " << *nbits << " edgebits: " << j.nedgebits << " prevhash: " << *prevhash << std::endl;

            _job = j;

            return true;
        }

        bool Client::mining_difficulty(const pt::ptree& params)
        {
            auto v = params.begin();
            auto diff = v->second.get_value_optional<double>();
            if(!diff || *diff == 0) {
                return false;
            }

            _next_diff = *diff;
            std::cerr << "info: " << "difficulty: " << *diff << std::endl;
            return true;
        }

        bool Client::mining_set_solo_job(const pt::ptree& params)
        {
            auto res = convert_blocktemplate(params);

            auto job_id = res.get<int>("id");
            auto prevhash = res.get<std::string>("prevhash");
            auto coinbase1 = res.get<std::string>("coinbase1");
            auto coinbase2 = res.get<std::string>("coinbase2");
            auto merkle_array = std::vector<int>{};
            auto version = res.get<std::string>("version");
            auto nbits = res.get<std::string>("bits");
            auto edgebits = res.get<int>("edgebits");
            auto time = res.get<std::string>("time");
            auto is_clean = res.get<bool>("is_clean");

            if(prevhash.size() != 64) { return false; }
            if(version.size() != 8) { return false; } // problem
            if(nbits.size() != 8) { return false; }
            if(time.size() != 8) { return false; } // problem

            std::lock_guard<std::mutex> guard{_job_mutex};
            Job j;

            if(!util::parse_hex(prevhash, j.prevhash)) { return false;}
            if(!util::parse_hex(version, j.version)) { return false;}
            if(!util::parse_hex(nbits, j.nbits)) { return false;}
            if(!util::parse_hex(time, j.time)) { return false;}

            j.nedgebits = edgebits;

            j.coinbase1_size = coinbase1.size()/2;

            if(!util::parse_hex(coinbase1, j.coinbase)) { return false; }
            j.coinbase.insert(j.coinbase.end(), _xnonce1.begin(), _xnonce1.end());

            j.xnonce2_start = j.coinbase.size();
            j.xnonce2_size = _xnonce2_size;

            j.coinbase.insert(j.coinbase.end(), _xnonce2_size, 0);
            if(!util::parse_hex(coinbase2, j.coinbase)) { return false; }

            j.id = job_id;

//            for(const auto& hex : merkle_array) {
//                std::string s = hex.second.get_value<std::string>();
//                util::ubytes bin;
//                if(!util::parse_hex(s, bin)) {
//                    return false;
//                }
//
//                j.merkle.push_back(bin);
//            }

            j.diff = _next_diff;
            j.clean = is_clean;
            _new_job = true;
            std::cerr << "info: " << "new solo job: " << j.id << " time: " << time << " nbits: " << nbits << " edgebits: " << j.nedgebits << " prevhash: " << prevhash << std::endl;

            _job = j;

            return true;

        }

        pt::ptree Client::convert_blocktemplate(const pt::ptree& params)
        {
            pt::ptree res{};
            std::string tmp;
            std::stringstream stream;

            res.add("id", get_solo_job_id());

            std::string prevhash;
            util::to_hex(params.get<std::string>("result.previousblockhash"), prevhash);
            res.add("prevhash", prevhash);

            // TODO: fix this
            res.add("coinbase1", "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff1d03693e0503366c1708");
            res.add("coinbase2", "0b2f4d65726974506f6f6c2fffffffff160aca9a3b000000001976a9144bfac9c3a88aab06ae1dcdf59a22eef6dbab361b88acd2665703000000001976a91429b3b93cf44ffa4f14415ca00e4b7cd0a44e0b2988ac07c36003000000001976a9145cdcaad0a61feeff8c474d50daf79885bd08694188acc8d3ef02000000001976a914c5d1ce565e99547c2bf056c996b5b8409356b27d88acca231603000000001976a9144904ee33f9345b6532ade7e043f6f61e8c69e3b388acafc79703000000001976a9142c930dff417ce135d133a0d6b4bd91b27885390788acc1916f03000000001976a914d7552b88da89bce65f8e8f6825391413e03d7b7488acfeb95b03000000001976a914a6080afe276ac0f5ae29926cf3fd67539dd0dbd888ac28bf5103000000001976a9145aeb58b3a35dc7282e15ff71a6b21ee1c21021c088ac03ac3d03000000001976a9145fab2bf7f998f84bf085e44a7edb61868361a1cf88acf50e4203000000001976a91401a2071430bdd5f5264bf9970ec0cc2649cbe88988ac0e4c6a02000000001976a914dbc5297edb42d494a7de6d6e126306b83f3b805288ac397f8302000000001976a91470e932c20dc49f72eb669a6a3ad7fe026175708988aca64e3603000000001976a9149b1b860efdb47e2dd0f9504326d5217beedbba5488ac6842ad02000000001976a914efe006f514be4e91118c835966909ddabd5a633a88ac98155c02000000001976a914e5f369d2151e8591766912d406a8f884d7690cd188ac6018b702000000001976a914a5f97cb5333d20ac335431dba391d2e8b09cc33e88ac8eb6b702000000001976a91492f32bed913273f7a11dc26a21ad3c3675732d0e88ac79626502000000001976a914c82be3807dfb9f02a2996bc6ad0b5ca29297fca888ace2f38802000000001976a9143fd2058fb6faeb7090798692d1f51ad44e46960b88acc7831d03000000001976a914ee34a60e8fe4223c0ad57600b7d70713c73c216e88ac0000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf900000000");


            // TODO: looks like it's wrong
            uint32_t version = static_cast<uint32_t>(atoi(params.get<std::string>("result.version").c_str())); // get, convert to BigEndian, then to HexString
            stream << std::hex << version;
            res.add("version", version);
            stream.clear();

            res.add("bits", params.get<std::string>("result.bits"));

            res.add("edgebits", params.get<std::string>("result.edgebits"));

            uint32_t time = static_cast<uint32_t>(atoi(params.get<std::string>("result.curtime").c_str()));
            stream << std::hex << time;
            res.add("time", std::string(stream.str()));
            stream.clear();

            res.add("is_clean", true);

            std::cout << "=== CONVERTING BLOCKTEMPLATE ===" << std::endl;
            std::ostringstream oss;
            pt::write_json(oss, res);
            std::cout << oss.str() << std::endl;

            return res;
        }

        bool Client::client_reconnect(const pt::ptree& params)
        {
            auto v = params.begin();
            auto host = v->second.get_value_optional<std::string>(); v++;
            if(!host) {
                return false;
            }

            auto port_string = v->second.get_value_optional<std::string>();

            if(port_string) {
                _port = *port_string;
            } else {
                auto port_int = v->second.get_value_optional<int>();
                if(!port_int) {
                    return false;
                }
                _port = boost::lexical_cast<std::string>(*port_int);
            }

            return reconnect();
        }

        bool Client::client_get_version(const pt::ptree& id)
        {
            pt::ptree req;
            pt::ptree err;
            req.put_child("id", id);
            req.put_child("error", err);
            req.put("result", _agent);

            std::stringstream s;
            pt::write_json(s, req, false);
            return send(s.str());
        }

        bool Client::client_show_message(const pt::ptree& params, const pt::ptree& id)
        {
            auto v = params.begin();
            auto msg = v->second.get_value_optional<std::string>(); v++;
            if(msg) {
                std::cerr << "info: " << "message: " << *msg << std::endl;
            }

            return true;
        }

        bool Client::handle_command(const pt::ptree& val, const std::string& res)
        {
            auto id = val.get_child_optional("id");
            auto method = val.get_optional<std::string>("method");
            if(!method) {
                return true;
            }

            if(*method == "mining.get_solo_job"){
                if(!mining_set_solo_job(val)){
                    std::cerr << "error: " << "unable to set mining.set_solo_job" << std::endl;
                    return false;
                }

                return true;
            }

            auto params = val.get_child_optional("params");
            if(!params) {
                std::cerr << "error: " << "unable to get params from response" << std::endl;
                return false;
            }

            if(*method == "mining.notify") {
                if(!mining_notify(*params)) {
                    std::cerr << "error: " << "unable to set mining.notify" << std::endl;
                    return false;
                }
            } else if(*method == "mining.set_difficulty") {
                if(!mining_difficulty(*params)) {
                    std::cerr << "error: " << "unable to set mining.difficulty" << std::endl;
                    return false;
                }
            } else if(*method == "client.reconnect") {
                if(!client_reconnect(*params)) {
                    std::cerr << "error: " << "unable to execute client.reconnect" << std::endl;
                    return false;
                }
            } else if(*method == "client.get_version") {
                if(!id || !client_get_version(*id)) {
                    std::cerr << "error: " << "unable to execute client.get_version" << std::endl;
                    return false;
                }
            } else if(*method == "client.show_message") {
                if(!id) { return true; }

                if(!client_show_message(*params, *id)) {
                    std::cerr << "error: " << "unable to execute client.show_message" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "unknown method: '" << *method << "' message: " << res << std::endl;
            }

            return true;
        }

        bool Client::authorize()
        {
            _state = Authorizing;
            std::stringstream req;
            req << "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"" << _user << "\", \"" << _pass << "\"]}";
            std::cout << "=== Authorizing: " << req.str() << std::endl;
            if (!send(req.str()))
            {
                std::cerr << "error: " << "error sending authorize request" << std::endl;
                return false;
            }

            _state = Authorized;
            return true;
        }

        bool Client::reconnect()
        {
            using namespace std::chrono_literals;
            disconnect();
            bool connected = false;
            auto min_reconnect_time = 50ms;
            int tries = 1;
            while(_run_state == Running && !connected) {
                try {
                    connected = connect(_url, _user, _pass);
                    if(connected) {
                        connected = subscribe();
                        if(connected) {
                            connected = authorize();
                        }
                    }
                }
                catch(std::exception e) {
                    std::cerr << "error: " << "error reconnecting: " << e.what() << std::endl;
                }

                if(!connected) {
                    //exponential backoff
                    int k = std::pow(2, tries) - 1;
                    std::uniform_int_distribution<int> dist{1, k};

                    auto t = min_reconnect_time * dist(_mt);
                    tries++;

                    std::cerr << "error: " << "error connecting, reconnecting in " << t.count() << "ms..." << std::endl;
                    std::this_thread::sleep_for(t);
                }
            }

            return true;
        }

        bool Client::run(bool solo_mining)
        {
            _run_state = Running;
            while (_run_state == Running)
            try {
                std::string res;
                if((solo_mining && !recv_with_headers(res)) || (!solo_mining && !recv(res))) {
                    std::cerr << "error: " << "error receiving" << std::endl;
                    throw std::runtime_error("error receiving");
                }
                if(_run_state == Running && _state == Disconnected) {
                    std::cerr << "error: disconnected: " << std::endl;
                    throw std::runtime_error("disconnected.");
                }

                pt::ptree val;
                if(!parse_json(res, val)) {
                    _sockbuf.clear();
                    std::cerr << "error parsing stratum response: " << res << std::endl;
                    continue;
                }

                if(solo_mining){
                    val.put("method", val.get<std::string>("id"));
                }

                std::ostringstream oss;
                pt::write_json(oss, val);
                std::cout << "JSON: " << oss.str() << std::endl;

                if(!handle_command(val, res)) {
                    continue;
                }

            } catch(std::exception& e) {
                if(!reconnect()) {
                    std::cerr << "error: " << "failed to reconnect" << std::endl;
                    return false;
                } else if(_run_state == Running) {
                    std::cerr << "info: reconnected!" << std::endl;
                }
            }

            _run_state = NotRunning;
            std::cerr << "info: stratum stopped." << std::endl;

            return true;
        }

        void Client::stop()
        {
            _run_state = Stopping;
        }

        bool Client::connected() const
        {
            return _state != Disconnected;
        }

        bool Client::running() const
        {
            return _run_state != NotRunning;
        }

        bool Client::stopping() const
        {
            return _run_state == Stopping;
        }

        MaybeJob Client::get_job()
        {
            std::lock_guard<std::mutex> guard{_job_mutex};

            if(!_new_job) {
                return MaybeJob{};
            }

            _new_job = false;
            return _job;
        }

        MaybeJob Client::get_solo_job(const std::string& auth_token)
        {
            std::stringstream req;
            req << "POST / HTTP/1.1\n" <<
                   "Content-Type: text/plain\n" <<
                   "Authorization: Basic "<< auth_token << "\n" <<
                   "Accept: */*\n" <<
                   "Content-Length: " << 97 + _user.size() << "\n\n" <<
                   "{\"method\": \"getblocktemplate\", \"jsonrpc\": \"2.0\", \"params\": [{}, \""<< _user <<"\"], \"id\": \"mining.get_solo_job\"}";

            std::cout << "=== req: " << req.str() << std::endl;

            if(!send(req.str())) {
                std::cerr << "error: " << "Error getting blocktemplate for mining: " << req.str() << std::endl;
                disconnect();
            } else {
                std::cout << "info: " << "getting blocktemplate for " << _user << std::endl;
            }

            return _job;
        }

        void Client::submit_work(const util::Work& w)
        {
            std::string xnonce2_hex;
            util::to_hex(w.xnonce2, xnonce2_hex);

            uint32_t ntime;
            le32enc(&ntime, w.data[17]);

            uint32_t nonce;
            le32enc(&nonce, w.data[19]);

            std::string ntime_hex;
            util::to_hex(
                    reinterpret_cast<const unsigned char*>(&ntime),
                    reinterpret_cast<const unsigned char*>(&ntime)+4,
                    ntime_hex);

            std::string nonce_hex;
            util::to_hex(
                    reinterpret_cast<const unsigned char*>(&nonce),
                    reinterpret_cast<const unsigned char*>(&nonce) + 4,
                    nonce_hex);
            std::stringstream cycle;
            for(int i = 0; i < w.cycle.size(); i++) {
                if(i > 0) { cycle << ","; }
                cycle << std::hex << w.cycle[i];
            }

            std::stringstream req;
            req << "{\"method\": \"mining.submit\", \"params\": ["
                << "\"" << _user << "\","
                << "\"" << w.jobid << "\","
                << "\"" << xnonce2_hex << "\","
                << "\"" << ntime_hex << "\","
                << "\"" << nonce_hex << "\","
                << "\"" << cycle.str() << "\"], \"id\":4}";

            if(!send(req.str())) {
                std::cerr << "error: " << "Error submitting work: " << req.str() << std::endl;
                disconnect();
            } else {
                std::cerr << "info: " << "submitted work: " << req.str() << std::endl;
            }
        }

        bool Client::subscribe()
        {
            _state = Subscribing;
            std::stringstream req;
            if (!_session_id.empty()) {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << _agent << "\", \"" << _session_id << "\"]}";
            } else {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << _agent << "\"]}";
            }

            if (!send(req.str())) {
                std::cerr << "error: " << "subscribe failed" << std::endl;
                return false;
            }
            return subscribe_resp();
        }

        bool Client::subscribe_resp()
        {
            std::string resp_line;
            if(!recv(resp_line)) {
                return false;
            }

            std::cout << "=== Another response(subscribe_resp): " << resp_line << std::endl;

            pt::ptree resp;
            if(!parse_json(resp_line, resp)) {
                std::cerr << "error: " << "error parsing response: " << resp_line << std::endl;
                return false;
            }

            auto result = resp.get_child_optional("result");
            if(!result) {
                auto err = resp.get_optional<std::string>("error");
                if(err) {
                    std::cerr << "error: " << "subscribe error : " << *err << std::endl;
                } else {
                    std::cerr << "error: " << "unknown subscribe error" << std::endl;
                }
                return false;
            }

            if(result->size() < 3) {
                std::cerr << "error: " << "not enough values in response" << std::endl;
                return false;
            }

            if(!find_session_id(*result, _session_id)) {
                std::cerr << "error: " << "failed to find the session id" << std::endl;
                return false;
            }

            auto res = result->begin();
            res++;

            auto xnonce1 = res->second.get_value_optional<std::string>();
            if(!xnonce1) {
                std::cerr << "error: " << "invalid extranonce" << std::endl;
                return false;
            }

            res++;
            auto xnonce2_size = res->second.get_value_optional<int>();
            if(!xnonce2_size) {
                std::cerr << "error: " << "cannot parse extranonce size" << std::endl;
                return false;
            }

            _xnonce2_size = *xnonce2_size;

            if (_xnonce2_size < 0 || _xnonce2_size > 100) {
                std::cerr << "error: " << "invalid extranonce2 size" << std::endl;
                return false;
            }

            _xnonce1.clear();
            if(!util::parse_hex(*xnonce1, _xnonce1)) {
                std::cerr << "error: " << "error parsing extranonce1" << std::endl;
            }
            _next_diff = 1.0;

            _state = Subscribed;
            return true;
        }

        bool Client::send(const std::string& message)
        {
            auto message_with_nl = message + "\n";
            std::lock_guard<std::mutex> guard{_sock_mutex};
            std::cout << "=== message to send: " << message_with_nl << std::endl;
            boost::system::error_code error;
            asio::write(_socket, boost::asio::buffer(message_with_nl), error);
            return !error;
        }

        bool has_line_ending(const util::bytes& b)
        {
            return std::find(b.begin(), b.end(), '\n') != b.end();
        }

        bool Client::recv(std::string& message)
        {
            if (!has_line_ending(_sockbuf)) {
                auto start = std::chrono::system_clock::now();

                std::chrono::duration<double> duration;
                do {
                    util::bytes s(BUFFER_SIZE, 0);
                    boost::system::error_code error;
                    auto len = _socket.read_some(asio::buffer(s), error);
                    if(error && error != boost::asio::error::eof) {
                        std::cerr << "error: " << "error receiving data: " << error << std::endl;
                        return false;
                    }

                    _sockbuf.insert(_sockbuf.end(), s.data(), s.data()+len);

                    auto end = std::chrono::system_clock::now();
                    duration = end - start;

                } while (duration.count() < 60 && !has_line_ending(_sockbuf));
            }

            auto nl = std::find(_sockbuf.begin(), _sockbuf.end(), '\n');
            if (nl == _sockbuf.end()) {
                return true;
            }

            auto size = std::distance(_sockbuf.begin(), nl);
            message.resize(size);
            std::copy(_sockbuf.begin(), nl, message.begin());

            std::cout << "=== receive message: " << message << " ===" << std::endl;

            if (_sockbuf.size() > size + 1) {
                std::copy(_sockbuf.begin() + size + 1, _sockbuf.end(), _sockbuf.begin());
                _sockbuf.resize(_sockbuf.size() - size - 1);
            } else {
                _sockbuf.clear();
            }

            return true;
        }

        bool Client::recv_with_headers(std::string& res)
        {
            // Skip 4 lines with HTTP headers + blank line
            recv(res); recv(res); recv(res); recv(res); recv(res);

            return recv(res);
        }

        unsigned int Client::get_solo_job_id()
        {
            // return and increment(so all ids will be different)
            return _solo_job_id++;
        }



        void diff_to_target(std::array<uint32_t, 8>& target, double diff)
        {
            int k;

            for (k = 7; k > 0 && diff > 1.0; k--) {
                diff /= 4294967296.0;
            }

            uint64_t m = 2147450880.0 / diff;

            std::fill(target.begin(), target.end(), 0);
            target[k] = static_cast<uint32_t>(m);
            target[k + 1] = static_cast<uint32_t>(m >> 32);
        }

        util::Work work_from_job(const stratum::Job& a)
        {
            auto j = a;
            util::Work w;
            w.jobid = j.id;

            auto xnonce2 = j.coinbase.begin() + j.xnonce2_start;
            auto xnonce2_end = xnonce2+j.xnonce2_size;
            w.xnonce2.resize(j.xnonce2_size);
            std::copy(xnonce2, xnonce2_end, w.xnonce2.begin());

            std::array<unsigned char, 64> merkle_root;
            std::fill(merkle_root.begin(), merkle_root.end(), 0);

            //sha256
            util::double_sha256(merkle_root.data(), j.coinbase.data(), j.coinbase.size());

            for(const auto& m : j.merkle) {
                assert(m.size() == 32);
                std::copy(m.begin(), m.end(), merkle_root.begin() + 32);
                util::double_sha256(merkle_root.data(), merkle_root.data(), merkle_root.size());
            }

            //Create block header
            std::fill(w.data.begin(), w.data.end(), 0);
            w.data[0] = le32dec(j.version.data());
            for(int i = 0; i < 8; i++)
                w.data[1 + i] = le32dec(reinterpret_cast<const uint32_t *>(j.prevhash.data()) + i);
            for(int i = 0; i < 8; i++)
                w.data[9 + i] = be32dec(reinterpret_cast<const uint32_t *>(merkle_root.data()) + i);
            w.data[17] = le32dec(j.time.data());
            w.data[18] = le32dec(j.nbits.data());
            w.data[20] = (j.nedgebits << 24) | (1 << 23);
            w.data[31] = 0x00000288;

            diff_to_target(w.target, j.diff);

            return w;
        }
    }
}
