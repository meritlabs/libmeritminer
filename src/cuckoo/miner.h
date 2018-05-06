// Copyright (c) 2017-2018 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MERIT_CUCKOO_MINER_H
#define MERIT_CUCKOO_MINER_H

#include "ctpl/ctpl.h"
#include <set>
#include <vector>

namespace merit 
{
    namespace cuckoo
    {
        /**
         * Find cycle for block that satisfies the proof-of-work requirement
         * specified by block hash with advanced edge trimming and matrix solver
         */
        bool SingleProofAttempt(
                const char* hex_header_hash,
                size_t hex_header_hash_len,
                unsigned int nBits,
                uint8_t edgeBits,
                std::set<uint32_t>& cycle,
                int proof_size,
                size_t threads,
                ctpl::thread_pool& pool);
    }
}

#endif // MERIT_CUCKOO_MINER_H
