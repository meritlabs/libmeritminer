#include "stratum.hpp"

#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/log/trivial.hpp>

#include <iostream>
#include <sstream>

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
            _socket{_service}
        {
        }

        Client::~Client() 
        {
            disconnect();
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
            auto host = _url.substr(host_pos, port_pos - host_pos - 1);
            auto port = _url.substr(port_pos);

            BOOST_LOG_TRIVIAL(info) << "HOST: " << host;
            BOOST_LOG_TRIVIAL(info) << "PORT: " << port;

            asio::ip::tcp::resolver resolver{_service};
            asio::ip::tcp::resolver::query query{host, port};
            auto endpoints = resolver.resolve(query);

            boost::asio::connect(_socket, endpoints);
            return true;
        }

        void Client::disconnect()
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            _socket.close();
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

        template<class C>
        bool parse_hex(const std::string& s, C& res)
        {
            std::stringstream tobin;
            std::istringstream ss(s);
            for(int i = 0; i < s.size(); i+=2)
            {
                unsigned char byte;
                tobin << std::hex << s[i] << s;
                tobin >> byte;
                res.push_back( byte );
            }
            return true;
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

            if(!parse_hex(*prevhash, _job.prevhash)) { return false;}
            if(!parse_hex(*version, _job.version)) { return false;}
            if(!parse_hex(*nbits, _job.nbits)) { return false;}
            if(!parse_hex(*time, _job.time)) { return false;}

            _job.nedgebits = *edgebits;

            _job.coinbase1_size = coinbase1->size()/2;

            if(!parse_hex(*coinbase1, _job.coinbase)) { return false; } 
            _job.coinbase.insert(_job.coinbase.end(), _xnonce1.begin(), _xnonce1.end());

            const auto xnonce2_start = _job.coinbase.size();

            _job.coinbase.insert(_job.coinbase.end(), _xnonce2_size, 0);
            if(!parse_hex(*coinbase2, _job.coinbase)) { return false; } 

            _job.xnonce2 = _job.coinbase.begin();
            std::advance(_job.xnonce2, xnonce2_start);

            _job.id = *job_id;

            //TODO fill merkle tree
            _job.merkle.clear();
            for(const auto& hex : merkle_array) {
                std::string s = hex.second.get_value<std::string>();
                ubytes bin;
                if(!parse_hex(s, bin)) {
                    return false;
                }

                _job.merkle.push_back(bin);
            }

            _job.diff = _next_diff;
            _job.clean = *is_clean;

            return true;
        }

        bool Client::mining_difficulty(const pt::ptree& params)
        {
            return true;
        }

        bool Client::client_reconnect(const pt::ptree& params)
        {
            return true;
        }

        bool Client::client_get_version(const pt::ptree& params)
        {
            return true;
        }

        bool Client::client_show_message(const pt::ptree& params)
        {
            return true;
        }

        bool Client::handle_command(const std::string& res)
        {
            pt::ptree val;
            if(!parse_json(res, val)) {
                BOOST_LOG_TRIVIAL(error) << "error parsing stratum response";
                return false;
            }

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
            } else if(*method == "mining.difficulty") {
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
                if(!client_get_version(*params)) {
                    BOOST_LOG_TRIVIAL(error) << "unable to execute client.get_version";
                    return false;
                }
            } else if(*method == "client.show_message") {
                if(!client_show_message(*params)) {
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
                BOOST_LOG_TRIVIAL(debug) << res;

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

        bool Client::subscribe()
        {
            std::stringstream req;
            if (!_session_id.empty()) {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << USER_AGENT << "\", \"" << _session_id << "\"]}";
            } else {
                req << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" << USER_AGENT << "\"]}";
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

            BOOST_LOG_TRIVIAL(info) << "session id: " << _session_id;

            auto res = result->begin();
            res++;

            auto xnonce1 = res->second.get_value_optional<std::string>();
            if(!xnonce1) {
                BOOST_LOG_TRIVIAL(error) << "invalid extranonce";
                return false;
            }

            BOOST_LOG_TRIVIAL(info) << "xnonce1: " << *xnonce1;

            res++;
            auto xnonce2_size = res->second.get_value_optional<int>();
            if(!xnonce2_size) {
                BOOST_LOG_TRIVIAL(error) << "cannot parse extranonce size";
                return false;
            }

            _xnonce2_size = *xnonce2_size;

            BOOST_LOG_TRIVIAL(info) << "xnonce2 size: " << *xnonce2_size;

            if (_xnonce2_size < 0 || _xnonce2_size > 100) {
                BOOST_LOG_TRIVIAL(error) << "invalid extranonce2 size";
                return false;
            }

            if(!parse_hex(*xnonce1, _xnonce1)) {
                BOOST_LOG_TRIVIAL(error) << "error parsing extranonce1";
            }
            _next_diff = 1.0;
            BOOST_LOG_TRIVIAL(info) << "nextdiff: " << _next_diff;

            return true;

        }

        bool Client::send(const std::string& message)
        {
            BOOST_LOG_TRIVIAL(debug) << "SEND: " << message;
            std::lock_guard<std::mutex> guard{_sock_mutex};
            boost::system::error_code error;
            asio::write(_socket, boost::asio::buffer(message), error);
            return !error;
        }

        bool has_line_ending(const bytes& b)
        {
            return std::find(b.begin(), b.end(), '\n') != b.end();
        }

        bool Client::recv(std::string& message)
        {
            if (!has_line_ending(_sockbuf)) {
                auto start = std::chrono::system_clock::now();

                std::chrono::duration<double> duration;
                do {
                    bytes s(BUFFER_SIZE, 0);
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

            BOOST_LOG_TRIVIAL(debug) << "RECV: " << message;
            return true;
        }
    }
}
