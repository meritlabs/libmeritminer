#include "stratum.hpp"

#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/log/trivial.hpp>

#if defined(WIN32)
#include <mstcpip.h>
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

#include <iostream>
#include <sstream>

namespace pt = boost::property_tree;

namespace merit
{
    namespace stratum
    {
        const size_t BUFFER_SIZE = 2048;
        const size_t RECV_SIZE = (BUFFER_SIZE - 4);

        const std::string PACKAGE_NAME = "meritminer";
        const std::string PACKAGE_VERSION = "0.0.1";
        const std::string USER_AGENT = PACKAGE_NAME + "/" + PACKAGE_VERSION;


        int keepalive_callback(
                void*,
                curl_socket_t fd,
                curlsocktype)
        {
            int keepalive = 1;
            int keepcnt = 3;
            int keepidle = 50;
            int keepintvl = 50;
#ifndef WIN32
            if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive))) {
                return 1;
            }

#ifdef __APPLE_CC__
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &tcp_keepintvl, sizeof(tcp_keepintvl))) {
                return 1;
            }
#endif

#ifdef __linux
            if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl))) {
                return 1;
            }
            if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt))) {
                return 1;
            }
            if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle))) {
                return 1;
            }
#endif

#else 
            tcp_keepalive k;
            k.onoff = 1;
            k.keepalivetime = tcp_keepidle * 1000;
            k.keepaliveinterval = tcp_keepintvl * 1000;

            DWORD out;
            if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &k, sizeof(k), nullptr, 0, &out, nullptr, nullptr)) {
                return 1;
            }
#endif
            return 0;
        }

        curl_socket_t grab_callback(
                void *client,
                curlsocktype,
                curl_sockaddr *addr)
        {
            assert(client);
            assert(addr);

            curl_socket_t *sock = reinterpret_cast<curl_socket_t*>(client);
            *sock = socket(addr->family, addr->socktype, addr->protocol);
            return *sock;
        }

        Client::~Client() 
        {
            disconnect();
        }

        void Client::initbuffers()
        {
            if (!_sockbuf.empty()) {
                _sockbuf.resize(BUFFER_SIZE);
                _sockbuf[0] = '\0';
            }
        }

        void Client::cleanup()
        {
            if (_curl) {
                curl_easy_cleanup(_curl);
                _curl = nullptr;
            }
            _sockbuf.clear();
        }

        bool Client::connect(
                    const std::string& iurl, 
                    const std::string& iuser, 
                    const std::string& ipass,
                    const CurlOptions& iopts)
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            _url = iurl;
            _user = iuser;
            _pass = ipass;
            _opts = iopts;

            cleanup();

            _curl = curl_easy_init();

            if (!_curl) {
                BOOST_LOG_TRIVIAL(error) << "CURL failed to initialize";
                return false;
            }

            initbuffers();

            _curl_url = "http" + _url.substr(11);

            if (_opts.protocol) {
                curl_easy_setopt(_curl, CURLOPT_VERBOSE, 1);
            }

            if (!_opts.cert.empty()) {
                curl_easy_setopt(_curl, CURLOPT_CAINFO, _opts.cert.c_str());
            }

            if (!_opts.proxy.empty()) {
                curl_easy_setopt(_curl, CURLOPT_PROXY, _opts.proxy.c_str());
                curl_easy_setopt(_curl, CURLOPT_PROXYTYPE, _opts.proxy_type);
            }

            curl_easy_setopt(_curl, CURLOPT_URL, _curl_url.c_str());
            curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, &_curl_err_str[0]);
            curl_easy_setopt(_curl, CURLOPT_FRESH_CONNECT, 1);
            curl_easy_setopt(_curl, CURLOPT_CONNECTTIMEOUT, 35);
            curl_easy_setopt(_curl, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(_curl, CURLOPT_TCP_NODELAY, 1);
            curl_easy_setopt(_curl, CURLOPT_HTTPPROXYTUNNEL, 1);
            curl_easy_setopt(_curl, CURLOPT_CONNECT_ONLY, 1);

            curl_easy_setopt(_curl, CURLOPT_SOCKOPTFUNCTION, keepalive_callback);
            curl_easy_setopt(_curl, CURLOPT_OPENSOCKETFUNCTION, grab_callback);
            curl_easy_setopt(_curl, CURLOPT_OPENSOCKETDATA, &_sock);

            if (curl_easy_perform(_curl)) {
                BOOST_LOG_TRIVIAL(error) << "Stratum connection failed: " << _curl_err_str;
                cleanup();
                return false;
            }

            BOOST_LOG_TRIVIAL(info) << "Connected to: " << _url;
            return true;
        }

        void Client::disconnect()
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            cleanup();
        }

        bool is_socket_full(curl_socket_t sock, int timeout)
        {
            struct timeval tv;
            fd_set rd;

            FD_ZERO(&rd);
            FD_SET(sock, &rd);
            tv.tv_usec = 0;
            tv.tv_sec = timeout;

            return select(sock + 1, &rd, NULL, NULL, &tv) > 0;
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

        bool parse_hex(const std::string& s, ubytes& res)
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

        bool Client::handle_auth_resp(const std::string& res)
        {
            //TODO: handle auth response
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

            while (true) {
                std::string res;
                if(!recv(res)) {
                    return true;
                }
                BOOST_LOG_TRIVIAL(debug) << res;

                if(!handle_auth_resp(res)) {
                    break;
                }
            }

            return true;
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

            if (!is_socket_full(_sock, 30)) {
                BOOST_LOG_TRIVIAL(error) << "subscribe timed out" << std::endl;
                return false;
            }

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
            std::lock_guard<std::mutex> guard{_sock_mutex};
            if(!_curl) {
                return false;
            }

            BOOST_LOG_TRIVIAL(debug) << "SEND: " << message;

            size_t sent = 0;
            auto size = message.size();
            while(size > 0) {
                fd_set w;
                FD_ZERO(&w);
                FD_SET(_sock, &w);
		        timeval timeout{0, 0};

                if (select(_sock + 1, NULL, &w, NULL, &timeout) < 1) {
                    return false;
                }

                size_t n = 0;
                CURLcode rc = curl_easy_send(_curl, message.data() + sent, size, &n);
                if (rc != CURLE_OK && rc != CURLE_AGAIN) {
                    n = 0;
                }
                sent += n;
                size -=n;
            }
            assert(sent == message.size());
            return true;
        }

        bool has_line_ending(const bytes& b)
        {
            return std::find(b.begin(), b.end(), '\n') != b.end();
        }

        bool blocked()
        {
#ifdef WIN32
            return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
            return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
        }

        bool Client::recv(std::string& message)
        {
            assert(_curl);

            if (!has_line_ending(_sockbuf)) {
                auto start = std::chrono::system_clock::now();

                if (!is_socket_full(_sock, 60)) {
                    BOOST_LOG_TRIVIAL(error) << "recieve timed out" << std::endl;
                    return false;
                }

                std::chrono::duration<double> duration;
                do {
                    bytes s(BUFFER_SIZE, 0);
                    size_t n;

                    CURLcode rc = curl_easy_recv(_curl, s.data(), RECV_SIZE, &n);
                    if (rc == CURLE_OK && !n) {
                        BOOST_LOG_TRIVIAL(error) << "recieved no data" << std::endl;
                        return false;
                    }
                    if (rc != CURLE_OK) {
                        if (rc != CURLE_AGAIN || !is_socket_full(_sock, 1)) {
                            BOOST_LOG_TRIVIAL(error) << "error recieving data" << std::endl;
                            return false;
                        }
                    } else {
                        _sockbuf.insert(_sockbuf.end(), s.data(), s.data()+n);

                    }

                    auto end = std::chrono::system_clock::now();
                    duration = end - start;

                } while (duration.count() < 60 && !has_line_ending(_sockbuf));
            }

            auto nl = std::find(_sockbuf.begin(), _sockbuf.end(), '\n');
            if (nl == _sockbuf.end()) {
                BOOST_LOG_TRIVIAL(error) << "failed to parse the response" << std::endl;
                return false;
            }

            auto size = std::distance(_sockbuf.begin(), nl);
            message.resize(size);
            std::copy(_sockbuf.begin(), nl, message.begin());

            if (_sockbuf.size() > size + 1) {
                std::copy(_sockbuf.begin() + size + 1, _sockbuf.end(), _sockbuf.begin());
                _sockbuf.resize(_sockbuf.size() - size + 1);
            } else {
                _sockbuf.clear();
            }

            BOOST_LOG_TRIVIAL(debug) << "RECV: " << message;
            return true;
        }
    }
}
