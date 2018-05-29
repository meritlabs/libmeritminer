#ifndef MERITMINER_H
#define MERITMINER_H

#include <string>

namespace merit
{
    void test();
    struct Context;

    Context* create_context();
    void delete_context(Context*);

    bool connect_stratum(
            Context* c,
            const char* url,
            const char* user,
            const char* pass);

    void init_logging();
    bool run_stratum(Context*);
    void stop_stratum(Context*);

    bool run_miner(Context*, int workers, int threads_per_worker);
    void stop_miner(Context*);
    bool is_stratum_running(Context*);
    bool is_miner_running(Context*);
}
#endif //MERITMINER_H
