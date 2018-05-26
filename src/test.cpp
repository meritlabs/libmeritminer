#include <merit/miner.hpp>

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

int main(int argc, char** argv) 
{
    merit::init_logging();
    std::unique_ptr<merit::Context, decltype(&merit::delete_context)> c{
        merit::create_context(), &merit::delete_context};
    if(!merit::connect_stratum(c.get(),"stratum+tcp://testnet.pool.merit.me:3333", "max", "foo")) {
        std::cerr << "Error connecting" << std::endl;
        return 1;
    }
    merit::run_stratum(c.get());
    merit::run_miner(c.get(), 2 ,2);
    while(true) { 
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    return 0;
}
