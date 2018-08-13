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

    void set_agent(
            Context* c,
            const char* software,
            const char* version);

    bool connect_stratum(
            Context* c,
            const char* url,
            const char* user,
            const char* pass);

    void set_reserve_pools(Context* c, const std::vector<std::string>& pools);

    void disconnect_stratum(Context* c);
    bool is_stratum_connected(Context* c);

    void init();
    bool run_stratum(Context*);
    void stop_stratum(Context*);

    struct GPUInfo {
        size_t id;
        std::string title;
        long long int total_memory;
        int temperature;
        int gpu_util;
        int memory_util;
        int fan_speed;
    };

    bool run_miner(Context*, int workers, int threads_per_worker, const std::vector<int>& gpu_devices);
    void stop_miner(Context*);
    bool is_stratum_running(Context*);
    bool is_miner_running(Context*);
    bool is_stratum_stopping(Context*);
    bool is_miner_stopping(Context*);
    int number_of_cores();
    int number_of_gpus();
    size_t free_memory_on_gpu(int device);
    std::vector<GPUInfo> gpus_info();

    struct MinerStat
    {
        int64_t start;
        int64_t end;
        double seconds;
        double attempts_per_second;
        double cycles_per_second;
        double shares_per_second;
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
