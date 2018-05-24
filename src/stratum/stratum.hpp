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

namespace pt = boost::property_tree;
namespace asio = boost::asio;

namespace merit
{
    namespace stratum
    {
        using ubytes = std::vector<unsigned char>;
        using bytes = std::vector<char>;

        struct Job {
            std::string id;
            ubytes prevhash;
            ubytes coinbase;
            size_t coinbase1_size;
            ubytes::iterator xnonce2;
            std::vector<ubytes> merkle;
            ubytes version;
            ubytes nbits;
            int nedgebits;
            ubytes time;
            bool clean = false;
            double diff = 0.0;
        };

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
                bytes _sockbuf;

                std::atomic<double> _next_diff;
                std::mutex _sock_mutex;
                std::mutex _work_mutex;

                std::vector<unsigned char> _xnonce1;
                size_t _xnonce2_size;
                Job _job;
                pthread_mutex_t _work_lock;
                asio::io_service _service;
                asio::ip::tcp::socket _socket;
                std::atomic<bool> _running;
        };

        bool stratum_socket_full(struct stratum_ctx *sctx, int timeout);
        bool stratum_send_line(struct stratum_ctx *sctx, char *s);
        char *stratum_recv_line(struct stratum_ctx *sctx);
        bool stratum_connect(struct stratum_ctx *sctx, const char *url);
        void stratum_disconnect(struct stratum_ctx *sctx);
        bool stratum_subscribe(struct stratum_ctx *sctx);
        bool stratum_authorize(struct stratum_ctx *sctx, const char *user, const char *pass);
        bool stratum_handle_method(struct stratum_ctx *sctx, const char *s);
    }
}
#endif
