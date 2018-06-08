# Copyright (c) 2017-2018 The Merit Foundation developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

cmake_minimum_required(VERSION 2.8.11)
project(libmeritminer)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

#setup boost
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system program_options thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
add_library(boost_system STATIC IMPORTED)
set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "/usr/local/lib/libboost_system.a")
add_library(boost_program_options STATIC IMPORTED)
set_target_properties(boost_program_options PROPERTIES IMPORTED_LOCATION "/usr/local/lib/libboost_program_options.a")
add_library(boost_thread STATIC IMPORTED)
set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "/usr/local/lib/libboost_thread.a")

include_directories(include)
include_directories(src/)
include_directories(src/crypto)
include_directories(src/cuckoo)
include_directories(src/blake2)
include_directories(src/stratum)
include_directories(src/miner)
include_directories(src/util)
include_directories(src/PicoSHA2)
add_definitions(-fPIC)

add_library(meritminer STATIC 
    src/public.cpp
    src/cuckoo/mean_cuckoo.cpp
    src/blake2/blake2b-ref.c
    src/stratum/stratum.cpp
    src/miner/miner.cpp
    src/util/util.cpp)

set(FAT_LIB ${CMAKE_BINARY_DIR}/libfatmeritminer.a)
add_custom_target(combined
        COMMAND ar -x $<TARGET_FILE:meritminer>
        COMMAND ar -x $<TARGET_FILE:boost_system>
        COMMAND ar -x $<TARGET_FILE:boost_program_options>
        COMMAND ar -x $<TARGET_FILE:boost_thread>
        COMMAND ar -qcs ${FAT_LIB} *.o
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS meritminer boost_system boost_program_options boost_thread)
add_library(fatmeritminer STATIC IMPORTED GLOBAL)
add_dependencies(fatmeritminer combined)
set_target_properties(fatmeritminer PROPERTIES IMPORTED_LOCATION ${FAT_LIB})

add_executable(merit-minerd src/minerd.cpp)
target_link_libraries(merit-minerd fatmeritminer pthread)
set_target_properties(meritminer PROPERTIES PUBLIC_HEADER "include/merit/miner.hpp")

install(TARGETS merit-minerd meritminer
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            PUBLIC_HEADER DESTINATION include/merit
            ARCHIVE DESTINATION lib)
install(FILES ${FAT_LIB} DESTINATION lib)
include(CPack)
 
