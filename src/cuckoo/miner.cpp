// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2016 John Tromp

#include "cuckoo/miner.h"
#include "cuckoo/mean_cuckoo.h"

#include <assert.h>
#include <numeric>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

namespace merit
{
    namespace cuckoo
    {
        /**
         * Single Pro
         */
        bool SingleProofAttempt(
                const char* hex_header_hash,
                size_t hex_header_hash_len,
                unsigned int nBits,
                uint8_t edgeBits,
                std::set<uint32_t>& cycle,
                int proof_size,
                size_t nThreads,
                ctpl::thread_pool& pool)
        {
            assert(cycle.empty());

            bool cycleFound =
                FindCycle(
                        hex_header_hash,
                        hex_header_hash_len,
                        edgeBits,
                        proof_size,
                        cycle,
                        nThreads, pool);

            if (cycleFound) {
                return true;
            }

            cycle.clear();

            return false;
        }
    }
}
