# Copyright (c) 2017-2018 The Merit Foundation developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

cmake_minimum_required(VERSION 2.8.11)
project(libmeritminer)

#setup boost
set(Boost_USE_STATIC_LIBS on)
find_package(Boost COMPONENTS system program_options filesystem regex thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(include)

ADD_LIBRARY(meritminer SHARED src/miner.cpp)

ADD_EXECUTABLE(meritminertest src/test.cpp)
TARGET_LINK_LIBRARIES(meritminertest meritminer)
set_target_properties(meritminer PROPERTIES PUBLIC_HEADER "include/merit/miner.hpp")

install(TARGETS meritminertest meritminer
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            PUBLIC_HEADER DESTINATION include/merit
            ARCHIVE DESTINATION lib)
include(CPack)
 
