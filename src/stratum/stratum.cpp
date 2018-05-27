#include "stratum/stratum.hpp"

#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <sstream>
#include <iterator>


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

            const std::string PACKAGE_NAME = "meritminer";
            const std::string PACKAGE_VERSION = "0.0.1";
            const std::string USER_AGENT = PACKAGE_NAME + "/" + PACKAGE_VERSION;
        }

        Client::Client() :
            _state{Disconnected},
            _agent{USER_AGENT},
            _socket{_service},
            _new_job{false}
        {
        }

        Client::~Client() 
        {
            disconnect();
        }

        void Client::set_agent(
                const std::string& software,
                const std::string& version)
        {
            _agent = software + "/" + version;
        }

        bool Client::connect(
                    const std::string& iurl, 
                    const std::string& iuser, 
                    const std::string& ipass)
        {
            if(_state == Connected) {
                disconnect();
            }

            _url = iurl;
            _user = iuser;
            _pass = ipass;

            auto host_pos = _url.find("://") + 3; 
            auto port_pos = _url.find(":", 14) + 1;
            _host = _url.substr(host_pos, port_pos - host_pos - 1);
            _port = _url.substr(port_pos);

            BOOST_LOG_TRIVIAL(info) << "host: " << _host;
            BOOST_LOG_TRIVIAL(info) << "port: " << _port;

            asio::ip::tcp::resolver resolver{_service};
            asio::ip::tcp::resolver::query query{_host, _port};
            auto endpoints = resolver.resolve(query);

            boost::asio::connect(_socket, endpoints);
            return true;
        }

        void Client::disconnect()
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
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
            BOOST_LOG_TRIVIAL(error) << "error parsing json: " << e.what();
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
            BOOST_LOG_TRIVIAL(info) << "notify: " << j.id << " time: " << *time << " nbits: " << *nbits << " edgebits: " << j.nedgebits << " prevhash: " << *prevhash;

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
            BOOST_LOG_TRIVIAL(info) << "difficulty: " << *diff;
            return true;
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

            disconnect();
            return true;
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
                BOOST_LOG_TRIVIAL(info) << "message: " << *msg;
            }

            return true;
        }

        bool Client::handle_command(const std::string& res)
        {
            pt::ptree val;
            if(!parse_json(res, val)) {
                BOOST_LOG_TRIVIAL(error) << "error parsing stratum response";
                return false;
            }

            auto id = val.get_child_optional("id");
            auto method = val.get_optional<std::string>("method");
            if(!method) {
                return true;
            }

            auto params = val.get_child_optional("params");
            if(!params) {
                BOOST_LOG_TRIVIAL(error) << "unable to get params from response";
                return false;
            }

            if(*method == "mining.notify") {
                if(!mining_notify(*params)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to set mining.notify";
                    return false;
                }
            } else if(*method == "mining.set_difficulty") {
                if(!mining_difficulty(*params)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to set mining.difficulty";
                    return false;
                }
            } else if(*method == "client.reconnect") {
                if(!client_reconnect(*params)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to execute client.reconnect";
                    return false;
                }
            } else if(*method == "client.get_version") {
                if(!id || !client_get_version(*id)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to execute client.get_version";
                    return false;
                }
            } else if(*method == "client.show_message") {
                if(!id) { return true; }

                if(!client_show_message(*params, *id)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to execute client.show_message";
                    return false;
                }
            }

            return true;
        }

        bool Client::authorize()
        {
            std::stringstream req;
            req << "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"" << _user << "\", \"" << _pass << "\"]}";
            if (!send(req.str()))
            {
                BOOST_LOG_TRIVIAL(error) << "error sending authorize request";
                return false;
            }

            return true;
        }

        bool Client::run()
        {
            _running = true;
            while (_running) {
                std::string res;
                if(!recv(res)) {
                    BOOST_LOG_TRIVIAL(error) << "error receiving";
                    return false;
                }
                if(!handle_command(res)) {
                    break;
                }
            }
            return true;
        }

        void Client::stop()
        {
            _running = false;
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
                BOOST_LOG_TRIVIAL(error) << "Error submitting work: " << req.str();
            } else {
                BOOST_LOG_TRIVIAL(info) << "submitted work: " << req.str();
            }
        }

        bool Client::subscribe()
        {
            std::stringstream req;
            if (!_session_id.empty()) {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << _agent << "\", \"" << _session_id << "\"]}";
            } else {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << _agent << "\"]}";
            }

            if (!send(req.str())) {
                BOOST_LOG_TRIVIAL(error) << "subscribe failed" << std::endl;
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

            pt::ptree resp;
            if(!parse_json(resp_line, resp)) {
                BOOST_LOG_TRIVIAL(error) << "error parsing response: " << resp_line;
                return false;
            }

            auto result = resp.get_child_optional("result");
            if(!result) {
                auto err = resp.get_optional<std::string>("error");
                if(err) {
                    BOOST_LOG_TRIVIAL(error) << "subscribe error : " << *err;
                } else {
                    BOOST_LOG_TRIVIAL(error) << "unknown subscribe error";
                }
                return false;
            }

            if(result->size() < 3) {
                BOOST_LOG_TRIVIAL(error) << "not enough values in response";
                return false;
            }

            if(!find_session_id(*result, _session_id)) {
                BOOST_LOG_TRIVIAL(error) << "failed to find the session id";
                return false;
            }

            auto res = result->begin();
            res++;

            auto xnonce1 = res->second.get_value_optional<std::string>();
            if(!xnonce1) {
                BOOST_LOG_TRIVIAL(error) << "invalid extranonce";
                return false;
            }

            res++;
            auto xnonce2_size = res->second.get_value_optional<int>();
            if(!xnonce2_size) {
                BOOST_LOG_TRIVIAL(error) << "cannot parse extranonce size";
                return false;
            }

            _xnonce2_size = *xnonce2_size;

            if (_xnonce2_size < 0 || _xnonce2_size > 100) {
                BOOST_LOG_TRIVIAL(error) << "invalid extranonce2 size";
                return false;
            }

            if(!util::parse_hex(*xnonce1, _xnonce1)) {
                BOOST_LOG_TRIVIAL(error) << "error parsing extranonce1";
            }
            _next_diff = 1.0;

            return true;

        }

        bool Client::send(const std::string& message)
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            boost::system::error_code error;
            asio::write(_socket, boost::asio::buffer(message), error);
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
                        BOOST_LOG_TRIVIAL(error) << "error receiving data: " << error << std::endl;
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

            if (_sockbuf.size() > size + 1) {
                std::copy(_sockbuf.begin() + size + 1, _sockbuf.end(), _sockbuf.begin());
                _sockbuf.resize(_sockbuf.size() - size - 1);
            } else {
                _sockbuf.clear();
            }

            return true;
        }

        void diff_to_target(std::array<uint32_t, 8>& target, double diff)
        {
            int k;

            for (k = 7; k > 0 && diff > 1.0; k--) {
                diff /= 4294967296.0;
            }

            uint64_t m = 4294901760.0 / diff;

            std::fill(target.begin(), target.end(), 0);
            target[k] = static_cast<uint32_t>(m);
            target[k + 1] = static_cast<uint32_t>(m >> 32);
        }

        util::Work work_from_job(const stratum::Job& j) 
        {
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
