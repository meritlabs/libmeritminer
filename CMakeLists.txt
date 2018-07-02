# Copyright (c) 2017-2018 The Merit Foundation developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

cmake_minimum_required(VERSION 2.8.11)
project(libmeritminer LANGUAGES C CXX CUDA)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

set(CMAKE_CXX_STANDARD 14)

#setup boost
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system program_options thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
add_library(boost_system STATIC IMPORTED)
set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${Boost_SYSTEM_LIBRARY_RELEASE}")
add_library(boost_program_options STATIC IMPORTED)
set_target_properties(boost_program_options PROPERTIES IMPORTED_LOCATION "${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE}")
add_library(boost_thread STATIC IMPORTED)
set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${Boost_THREAD_LIBRARY_RELEASE}")

include_directories(include)
include_directories(src/)
include_directories(src/crypto)
include_directories(src/cuckoo)
include_directories(src/blake2)
include_directories(src/stratum)
include_directories(src/miner)
include_directories(src/util)
include_directories(src/PicoSHA2)

find_package(CUDA)
if(CUDA_FOUND)
    add_definitions(-DCUDA_ENABLED)

    link_directories(${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES})
    include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    add_library(cuda STATIC IMPORTED)
    set_target_properties(cuda PROPERTIES IMPORTED_LOCATION ${CUDA_cudart_static_LIBRARY})

    add_library(meritminer STATIC
        src/public.cpp
        src/cuckoo/gpu/kernel.cu
        src/cuckoo/gpu/exceptions.h
        src/cuckoo/mean_cuckoo.cpp
        src/blake2/blake2b-ref.c
        src/stratum/stratum.cpp
        src/miner/miner.cpp
        src/util/util.cpp)
else()
    add_library(meritminer STATIC 
        src/public.cpp
        src/cuckoo/mean_cuckoo.cpp
        src/blake2/blake2b-ref.c
        src/stratum/stratum.cpp
        src/miner/miner.cpp
        src/util/util.cpp)
endif()

target_compile_features(meritminer PUBLIC cxx_std_14)
set_target_properties(meritminer PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(FAT_LIB ${CMAKE_BINARY_DIR}/libfatmeritminer.a)

if(CMAKE_HOST_WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(FAT_LIB ${CMAKE_BINARY_DIR}/libfatmeritminer.lib)
    add_custom_target(combined
        COMMAND lib.exe /OUT:${FAT_LIB} $<TARGET_FILE:meritminer> $<TARGET_FILE:boost_system> $<TARGET_FILE:boost_program_options> $<TARGET_FILE:boost_thread> $<TARGET_FILE:cuda>
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS meritminer boost_system boost_program_options boost_thread)
    add_library(fatmeritminer STATIC IMPORTED GLOBAL)
    add_dependencies(fatmeritminer combined)
    set_target_properties(fatmeritminer PROPERTIES IMPORTED_LOCATION ${FAT_LIB})
else()
    set(FAT_LIB ${CMAKE_BINARY_DIR}/libfatmeritminer.a)
    add_custom_target(combined
            COMMAND ar -x $<TARGET_FILE:meritminer>
            COMMAND ar -x $<TARGET_FILE:boost_system>
            COMMAND ar -x $<TARGET_FILE:boost_program_options>
            COMMAND ar -x $<TARGET_FILE:boost_thread>
            COMMAND ar -x $<TARGET_FILE:cuda>
            COMMAND ar -qcs ${FAT_LIB} *.cu.o *.o
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS meritminer boost_system boost_program_options boost_thread)
    add_library(fatmeritminer STATIC IMPORTED GLOBAL)
    add_dependencies(fatmeritminer combined)
    set_target_properties(fatmeritminer PROPERTIES IMPORTED_LOCATION ${FAT_LIB})
endif()

add_executable(merit-minerd src/minerd.cpp)

if(CMAKE_HOST_WIN32)
	target_link_libraries(merit-minerd fatmeritminer)
else()
	target_link_libraries(merit-minerd fatmeritminer pthread rt dl)
endif()

set_target_properties(meritminer PROPERTIES PUBLIC_HEADER "include/merit/miner.hpp")

install(TARGETS merit-minerd meritminer
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            PUBLIC_HEADER DESTINATION include/merit
            ARCHIVE DESTINATION lib)
install(FILES ${FAT_LIB} DESTINATION lib)
include(CPack)

