// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2017 John Tromp
// Copyright (c) 2017-2018 The Merit Foundation developers

#ifndef MERIT_CUCKOO_MEAN_CUCKOO_H
#define MERIT_CUCKOO_MEAN_CUCKOO_H

#include "ctpl/ctpl.h"

#include <set>
#include <vector>

namespace merit
{
    namespace cuckoo
    {
        // Find proofsize-length cuckoo cycle in random graph
        bool FindCycle(
                const char* hex_header_hash,
                size_t hex_header_hash_len,
                uint8_t edgeBits,
                uint8_t proofSize,
                std::set<uint32_t>& cycle,
                size_t threads_number,
                ctpl::thread_pool&);
    } 
}

#endif // MERIT_CUCKOO_MEAN_CUCKOO_H
