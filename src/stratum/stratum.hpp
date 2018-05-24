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

namespace pt = boost::property_tree;
namespace asio = boost::asio;

namespace merit
{
    namespace stratum
    {
        using ubytes = std::vector<unsigned char>;
        using bytes = std::vector<char>;

        struct Job {
            std::string job_id;
            std::array<unsigned char,32> prevhash;
            size_t coinbase_size = 0;
            ubytes coinbase;
            ubytes xnonce2;
            std::vector<bytes> merkel;
            std::array<unsigned char,4> version;
            std::array<unsigned char,4> nbites;
            std::array<unsigned char,4> nedgebits;
            std::array<unsigned char,4> ntime;
            bool clean = false;
            double diff = 0.0;
        };

        struct Client {
            public:

                Client();
                ~Client();
                bool connect(
                        const std::string& url, 
                        const std::string& user, 
                        const std::string& pass);

                void disconnect();
                bool subscribe();
                bool authorize();

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
                bool client_show_message(const pt::ptree& params);

            private:
                enum State {
                    Disconnected,
                    Connected,
                    Subscribed,
                    Authorized,
                    Method,
                } _state;

                std::string _url;
                std::string _user;
                std::string _pass;
                std::string _session_id;
                bytes _sockbuf;

                double _next_diff;
                std::mutex _sock_mutex;
                std::mutex _work_mutex;

                std::vector<unsigned char> _xnonce1;
                size_t _xnonce2_size;
                Job _job;
                pthread_mutex_t _work_lock;
                asio::io_service _service;
                asio::ip::tcp::socket _socket;
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
