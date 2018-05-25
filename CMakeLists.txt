# Copyright (c) 2017-2018 The Merit Foundation developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

cmake_minimum_required(VERSION 2.8.11)
project(libmeritminer)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

#setup crypto++
find_package(CryptoPP REQUIRED)

#setup boost
set(Boost_USE_STATIC_LIBS on)
find_package(Boost COMPONENTS log system program_options filesystem regex thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

include_directories(include)
include_directories(src/)
include_directories(src/crypto)
include_directories(src/cuckoo)
include_directories(src/blake2)
include_directories(src/stratum)
include_directories(src/miner)
include_directories(src/util)

add_library(meritminer SHARED 
    src/public.cpp
    src/cuckoo/miner.cpp
    src/cuckoo/mean_cuckoo.cpp
    src/blake2/blake2b-ref.c
    src/stratum/stratum.cpp
    src/miner/miner.cpp
    src/util/util.cpp)

add_executable(meritminertest src/test.cpp)
target_link_libraries(meritminertest meritminer ${Boost_LIBRARIES} cryptopp)
set_target_properties(meritminer PROPERTIES PUBLIC_HEADER "include/merit/miner.hpp")

install(TARGETS meritminertest meritminer
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            PUBLIC_HEADER DESTINATION include/merit
            ARCHIVE DESTINATION lib)
include(CPack)
 
