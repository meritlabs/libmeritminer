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
    void run_stratum(Context*);
    void stop_stratum(Context*);

    void run_miner(Context*, int workers, int threads_per_worker);
    void stop_miner(Context*);
}
#endif //MERITMINER_H
