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

namespace merit
{
    namespace stratum
    {
        const size_t BUFFER_SIZE = 2048;
        const size_t RECV_SIZE = (BUFFER_SIZE - 4);

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
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &tcp_keepintvl, sizeof(tcp_keepintvl)))
                return 1;
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

        bool Client::subscribe()
        {
            return true;
        }

        bool Client::send(const std::string& message)
        {
            std::lock_guard<std::mutex> guard{_sock_mutex};
            if(!_curl) {
                return false;
            }

            BOOST_LOG_TRIVIAL(debug) << message;

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

                size_t n;
                CURLcode rc = curl_easy_send(_curl, message.data() + sent, size, &n);
                if (rc != CURLE_OK) {
                    if (rc != CURLE_AGAIN) {
			            n = 0;
                    }
                }
                sent += n;
                size -=n;
            }
            return true;
        }

        bool has_line_ending(const bytes& b)
        {
            return std::find(b.begin(), b.end(), '\n') != b.end();
        }

        bool Client::recv(std::string& message)
        {
            assert(_curl);

        }
    }
}
