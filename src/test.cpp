#include <merit/miner.hpp>

#include <iostream>

int main(int argc, char** argv) 
{
    merit::init_logging();
    merit::test();
    if(!merit::connect_stratum("stratum+tcp://127.0.0.1", "maxim", "foo")) {
        std::cerr << "Error connecting" << std::endl;
        return 1;
    }
    while(true) {}
    return 0;
}
