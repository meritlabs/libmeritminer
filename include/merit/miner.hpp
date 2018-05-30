#ifndef MERITMINER_H
#define MERITMINER_H

#include <string>
#include <vector>

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

    void disconnect_stratum(Context* c);

    void init_logging();
    bool run_stratum(Context*);
    void stop_stratum(Context*);

    bool run_miner(Context*, int workers, int threads_per_worker);
    void stop_miner(Context*);
    bool is_stratum_running(Context*);
    bool is_miner_running(Context*);
    bool is_stratum_stopping(Context*);
    bool is_miner_stopping(Context*);
    int number_of_cores();

    struct MinerStat
    {
        int64_t start;
        int64_t end;
        double seconds;
        double shares_per_second;
        double cycles_per_second;
        double attempts_per_second;
        int attempts;
        int cycles;
        int shares;
    };

    using StatHistory = std::vector<MinerStat>;
    struct MinerStats
    {
        MinerStat total;
        MinerStat current;
        StatHistory history;
    };

    MinerStats get_miner_stats(Context*);
}
#endif //MERITMINER_H
