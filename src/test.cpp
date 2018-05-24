#include <merit/miner.hpp>

#include <iostream>

int main(int argc, char** argv) 
{
    merit::init_logging();
    merit::test();
    if(!merit::connect_stratum("stratum+tcp://testnet.pool.merit.me:3333", "max", "foo")) {
        std::cerr << "Error connecting" << std::endl;
        return 1;
    }
    merit::run_stratum();
    return 0;
}
