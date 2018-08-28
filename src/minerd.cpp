/*
 * Copyright (C) 2018 The Merit Foundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either vedit_refsion 3 of the License, or
 * (at your option) any later vedit_refsion.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give 
 * permission to link the code of portions of this program with the 
 * Botan library under certain conditions as described in each 
 * individual source file, and distribute linked combinations 
 * including the two.
 *
 * You must obey the GNU General Public License in all respects for 
 * all of the code used other than Botan. If you modify file(s) with 
 * this exception, you may extend this exception to your version of the 
 * file(s), but you are not obligated to do so. If you do not wish to do 
 * so, delete this exception statement from your version. If you delete 
 * this exception statement from all source files in the program, then 
 * also delete it here.
 */
#include <merit/miner.hpp>

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <thread>
#include <utility>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::pair<int, int> determine_utilization(int cores)
{
    return cores & 1 ? std::make_pair(cores, 1) : std::make_pair(cores / 2, 2);
}

int main(int argc, char** argv) 
{
    merit::init();

    po::options_description desc("Allowed options");
    std::string url;
    std::vector<int> gpu_devices;
    std::string address;
    desc.add_options()
        ("help", "show the help message")
        ("infogpu", "show the info about GPU in your system")
        ("url", po::value<std::string>(&url)->default_value("stratum+tcp://pool.merit.me:3333"), "The stratum pool url")
        ("address", po::value<std::string>(&address), "The address to send mining rewards to.")
        ("gpu", po::value<std::vector<int>>(&gpu_devices)->multitoken(), "Index of GPU device to use in mining(can use multiple times). For more info check --infogpu")
        ("cores", po::value<int>()->default_value(merit::number_of_cores()), "The number of CPU cores to use.");


    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << std::endl;;
        return 1;
    }

    if (vm.count("infogpu")) {
        auto info = merit::gpus_info();
        std::cout << "GPU info:" << std::endl;
        for(const auto &item: info){
            std::cout << "Device number: " << item.id << std::endl;
            std::cout << "Total memory: " << item.total_memory << std::endl;
            std::cout << "Title: " << item.title << std::endl;
            std::cout << "Temperature: " << item.temperature << std::endl;
            std::cout << "GPU util: " << item.gpu_util << std::endl;
            std::cout << "Memory util: " << item.memory_util << std::endl;
            std::cout << "Fan speed: " << item.fan_speed << std::endl << std::endl;
        }

        return 1;
    }

    if(address.empty()) {
        std::cerr << "forgot to set your reward address. use --address" << std::endl;
        return 1;
    }

    // Validate input GPU device indexes
    auto info = merit::gpus_info();
    for(const auto& device: gpu_devices){
        if(device >= info.size()){
            std::cerr << "There is no GPU device with index = " << device << ". Please check available GPU devices by using --infogpu argument." << std::endl;
            return 1;
        }
    }

    int cores;
    cores = vm["cores"].as<int>();
    cores = std::max(0, cores);
    auto utilization = determine_utilization(cores);

    std::unique_ptr<merit::Context, decltype(&merit::delete_context)> c{
        merit::create_context(), &merit::delete_context};

    merit::set_agent(c.get(), "merit-minerd", "0.4");

    if(!merit::connect_stratum(c.get(), url.c_str(), address.c_str(), "")) {
        std::cerr << "Error connecting" << std::endl;
        return 1;
    }
    merit::run_stratum(c.get());
    merit::run_miner(c.get(), utilization.first ,utilization.second, gpu_devices);

    int prev_graphs = 0;
    while(true) { 
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(5s);

        auto stats = merit::get_miner_stats(c.get());
        auto graphs = stats.total.attempts + stats.current.attempts;
        auto cycles = stats.total.cycles + stats.current.cycles;
        auto shares = stats.total.shares + stats.current.shares;
        auto graphps = stats.total.attempts_per_second;
        auto cyclesps = stats.total.cycles_per_second;
        auto sharesps = stats.total.shares_per_second;
        if(graphs > prev_graphs) {
            std::cout << "graphs: " << graphs << " cycles: " << cycles << " shares: " << shares;
            if(stats.total.attempts > 0) {
                std::cout << " graphs/s: " << graphps << " cycles/s: " << cyclesps << " shares/s: " << sharesps << std::endl;
            }
            std::cout << std::endl;
        }
        prev_graphs = graphs;
    }

    return 0;
}
