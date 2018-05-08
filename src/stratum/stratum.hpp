#ifndef MERIT_MINER_STRATUM_H
#define MERIT_MINER_STRATUM_H

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
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
            int merkle_count = 0;
            unsigned char **merkle = nullptr;
            std::array<unsigned char,4> version;
            std::array<unsigned char,4> nbites;
            std::array<unsigned char,4> nedgebits;
            std::array<unsigned char,4> ntime;
            bool clean = false;
            double diff = 0.0;
        };

        struct CurlOptions
        {
            bool protocol = false;
            std::string cert;
            std::string proxy;
            long proxy_type = 0;
        };

        struct Client {
            public:

                ~Client();
                bool connect(
                        const std::string& url, 
                        const std::string& user, 
                        const std::string& pass,
                        const CurlOptions& opts);

                void disconnect();
                bool subscribe();
                bool authorize();

            private:
                bool send(const std::string&);
                bool recv(std::string&);
                void initbuffers();
                void cleanup();

            private:
                std::string _url;
                std::string _user;
                std::string _pass;
                CurlOptions _opts;
                std::string _curl_url;
                std::string _session_id;
                bytes _sockbuf;
                CURL *_curl = nullptr;
                char _curl_err_str[CURL_ERROR_SIZE];
                curl_socket_t _sock;

                double _next_diff;
                std::mutex _sock_mutex;
                std::mutex _work_mutex;

                std::vector<unsigned char> _xnonce1;
                size_t _xnonce2_size;
                Job _job;
                pthread_mutex_t _work_lock;
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
