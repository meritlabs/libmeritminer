/*
 * Copyright (c) 2013-2018 John Tromp
 * Copyright (c) 2018 Jiri Vadura - photon
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

#include "cuda_profiler_api.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "device_functions.h"
#include "exceptions.h"
#include "merit/nvml/nvml.h"
#include <xmmintrin.h>
#include <algorithm>
#include <stdio.h>
#include <stdint.h>
#include <atomic>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <fstream>
#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <merit/nvml/nvml.h>
#include <memory>

#ifdef _WIN32
#  define WINDOWS_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u32 node_t;
typedef u64 nonce_t;

#define MAXDEVICES 16
#define MAXPATHLEN 8192
#define MAXPROOFLENGTH 84
#define BIGEPS 5 / 64
#define TRIMFRAC256 184
#define BKTGRAN 32

const int CTHREADS = 1024;

template <class offset_t, uint8_t EDGEBITS_IN, uint8_t XBITS_IN>
struct Params {
    // prepare params for algorithm
    const static u32 XBITS = XBITS_IN;
    const static u32 NX = 1 << XBITS;
    const static u32 EDGEBITS = EDGEBITS_IN;
    const static u32 NEDGES = (offset_t)1 << EDGEBITS;
    const static u32 EDGEMASK = NEDGES - 1;
    const static u32 NODEBITS = EDGEBITS + 1;
    const static offset_t NNODES = (offset_t)1 << NODEBITS;
    const static u32 NODEMASK = NNODES - 1;

    const static u32 IDXSHIFT = 10;
    const static u32 CUCKOO_SIZE = NNODES >> IDXSHIFT;
    const static u32 CUCKOO_MASK = CUCKOO_SIZE - 1;
    const static u32 KEYBITS = 64-NODEBITS;
    const static u64 KEYMASK = (1LL << KEYBITS) - 1;
    const static u64 MAXDRIFT = 1LL << (KEYBITS - IDXSHIFT);

    const static u32 GABLOCKS = 512;
    const static u32 GATPB = 64;
    const static u32 GBBLOCKS = 32*BKTGRAN;
    const static u32 GBTPB = 64;
    const static u32 TRBLOCKS = 4096;
    const static u16 TRTPB = 1024;
    const static u16 TLBLOCKS = 4096;
    const static u16 TLTPB = 1024;
    const static u16 RBLOCKS = 512;
    const static u16 RTPB = 256;
};

struct SipKeys
{
    unsigned long long k0;
    unsigned long long k1;
    unsigned long long k2;
    unsigned long long k3;
};

// const auto DUCK_SIZE_A = 130LL;
// const auto DUCK_SIZE_B = 85LL;
const auto DUCK_SIZE_A = 30LL;
const auto DUCK_SIZE_B = 20LL;

const auto DUCK_A_EDGES = (DUCK_SIZE_A * 1024LL);
const auto DUCK_A_EDGES_64 = (DUCK_A_EDGES * 64LL);

const auto DUCK_B_EDGES = (DUCK_SIZE_B * 1024LL);
const auto buffer_size = DUCK_SIZE_A * 1024 * 4096 * 8;
const auto buffer_size_2 = DUCK_SIZE_B * 1024 * 4096 * 8;
const auto indexes_size = 128 * 128 * 4;

const int BKTMASK4K = (4096-1);


#define ROTL(x,b) ( ((x) << (b)) | ( (x) >> (64 - (b))) )
#define SIPROUND \
    do { \
        v0 += v1; v2 += v3; v1 = ROTL(v1,13); \
        v3 = ROTL(v3,16); v1 ^= v0; v3 ^= v2; \
        v0 = ROTL(v0,32); v2 += v1; v0 += v3; \
        v1 = ROTL(v1,17);   v3 = ROTL(v3,21); \
        v1 ^= v2; v3 ^= v0; v2 = ROTL(v2,32); \
    } while(0)


template<int EDGEMASK>
__device__  node_t dipnode(
        const u64 v0i,
        const u64 v1i,
        const u64 v2i,
        const u64 v3i,
        const  nonce_t nce,
        const  u32 uorv) {

    u64 nonce = 2 * nce + uorv;
    u64 v0 = v0i, v1 = v1i, v2 = v2i, v3 = v3i ^ nonce;
    SIPROUND; SIPROUND;
    v0 ^= nonce;
    v2 ^= 0xff;
    SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return (v0 ^ v1 ^ v2  ^ v3) & EDGEMASK;
}

template<int EDGEMASK>
node_t host_dipnode(
        const u64 v0i,
        const u64 v1i,
        const u64 v2i,
        const u64 v3i,
        const  nonce_t nce,
        const  u32 uorv) {

    u64 nonce = 2 * nce + uorv;
    u64 v0 = v0i, v1 = v1i, v2 = v2i, v3 = v3i ^ nonce;
    SIPROUND; SIPROUND;
    v0 ^= nonce;
    v2 ^= 0xff;
    SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return (v0 ^ v1 ^ v2  ^ v3) & EDGEMASK;
}

__device__ ulonglong4 Pack4edges(
        const uint2 e1,
        const  uint2 e2,
        const  uint2 e3,
        const  uint2 e4)
{
    u64 r1 = (((u64)e1.y << 32) | ((u64)e1.x));
    u64 r2 = (((u64)e2.y << 32) | ((u64)e2.x));
    u64 r3 = (((u64)e3.y << 32) | ((u64)e3.x));
    u64 r4 = (((u64)e4.y << 32) | ((u64)e4.x));
    return make_ulonglong4(r1, r2, r3, r4);
}

    template <class P>
__global__  void FluffyRecovery(
        const u64 v0i,
        const u64 v1i,
        const u64 v2i,
        const u64 v3i,
        uint8_t proof_size,
        ulonglong4 * buffer,
        int * indexes,
        u64* recovery)
{
    const int gid = blockDim.x * blockIdx.x + threadIdx.x;
    const int lid = threadIdx.x;

    __shared__ u32 nonces[MAXPROOFLENGTH];

    if (lid < proof_size) nonces[lid] = 0;

    __syncthreads();

    for (int i = 0; i < 1024 * 4; i++)
    {
        u64 nonce = gid * (1024 * 4) + i;
        if(nonce > P::NEDGES){
            break;
        }

        u64 u = dipnode<P::EDGEMASK>(v0i, v1i, v2i, v3i, nonce, 0) << 1;
        u64 v = dipnode<P::EDGEMASK>(v0i, v1i, v2i, v3i, nonce, 1) << 1 | 1;

        u64 a = u | (v << 32);
        u64 b = v | (u << 32);

        for (int i = 0; i < proof_size; i++)
        {
            if ((recovery[i] == a) || (recovery[i] == b))
                nonces[i] = nonce;
        }
    }

    __syncthreads();

    if (lid < proof_size)
    {
        if (nonces[lid] > 0)
            indexes[lid] = nonces[lid];
    }
}


    template<class P>
__global__  void FluffySeed2A(
        const u64 v0i,
        const u64 v1i,
        const u64 v2i,
        const u64 v3i,
        ulonglong4 * __restrict__ buffer,
        int *  __restrict__ indexes)
{
    const int gid = blockDim.x * blockIdx.x + threadIdx.x;
    const int lid = threadIdx.x;

    __shared__ uint2 tmp[P::NX][16];
    __shared__ int counters[P::NX];

    counters[lid] = 0;

    __syncthreads();

    u64 nonce    = (u64)gid * P::NEDGES / (blockDim.x * gridDim.x);
    u64 endnonce = (u64)(gid +1) * P::NEDGES / (blockDim.x * gridDim.x);
    for (; nonce < endnonce; nonce++) {

        uint2 hash;

        hash.x = dipnode<P::EDGEMASK>(v0i, v1i, v2i, v3i, nonce, 0);
        hash.y = dipnode<P::EDGEMASK>(v0i, v1i, v2i, v3i, nonce, 1);

        int bucket = hash.x & (64 - 1);

        __syncthreads();

        int counter = min((int)(atomicAdd(counters + bucket, 1)), (int)14);

        tmp[bucket][counter] = hash;

        __syncthreads();

        {
            int idx = min(16, counters[lid]);

            if (idx >= 8) {
                int new_count = idx - 8;
                counters[lid] = new_count;

                {
                    int cnt = min(
                            static_cast<int>(atomicAdd(indexes + lid, 8)), 
                            static_cast<int>(DUCK_A_EDGES_64 - 8));

                    {
                        buffer[(lid * DUCK_A_EDGES_64 + cnt) / 4] =
                            Pack4edges(tmp[lid][0], tmp[lid][1], tmp[lid][2], tmp[lid][3]);

                        buffer[(lid * DUCK_A_EDGES_64 + cnt + 4) / 4] =
                            Pack4edges(tmp[lid][4], tmp[lid][5], tmp[lid][6], tmp[lid][7]);
                    }
                }

                for (int t = 0; t < new_count; t++) {
                    tmp[lid][t] = tmp[lid][t + 8];
                }

            }
        }
    }

    __syncthreads();

    {
        int idx = min(15, counters[lid]);

        if (idx >  0) {
            int cnt = min( (int)atomicAdd(indexes + lid, 4), (int)(DUCK_A_EDGES_64 - 4));

            buffer[(lid * DUCK_A_EDGES_64 + cnt) / 4] = Pack4edges(
                    tmp[lid][0],
                    idx > 1 ? tmp[lid][1] : make_uint2(0, 0),
                    idx > 2 ? tmp[lid][2] : make_uint2(0, 0),
                    idx > 3 ? tmp[lid][3] : make_uint2(0, 0));
        }

        if (idx > 4) {
            int cnt = min(
                    static_cast<int>(atomicAdd(indexes + lid, 4)),
                    static_cast<int>(DUCK_A_EDGES_64 - 4));

            buffer[(lid * DUCK_A_EDGES_64 + cnt) / 4] = Pack4edges(
                    tmp[lid][4],
                    idx > 5 ? tmp[lid][5] : make_uint2(0, 0),
                    idx > 6 ? tmp[lid][6] : make_uint2(0, 0),
                    idx > 7 ? tmp[lid][7] : make_uint2(0, 0));
        }
    }

}

__global__  void FluffySeed2B(
        const  uint2 *__restrict__  source,
        ulonglong4 *__restrict__  destination,
        const  int *__restrict__  src_indexes,
        int *__restrict__  dest_indexes,
        int start_block)
{
    const int lid = threadIdx.x;
    const int group = blockIdx.x;

    __shared__ uint2 tmp[64][15];
    __shared__ int counters[64];

    counters[lid] = 0;

    __syncthreads();

    const int offset_mem = start_block * DUCK_A_EDGES_64;
    const int my_bucket = group / BKTGRAN;
    const int micro_block_no = group % BKTGRAN;

    const int bucket_edges = min(
            src_indexes[my_bucket + start_block],
            (int)(DUCK_A_EDGES_64));

    const int micro_block_edge_count = DUCK_A_EDGES_64 / BKTGRAN;
    const int loops = micro_block_edge_count / 64;

    for (int i = 0; i < loops; i++)
    {
        int edge_index = (micro_block_no * micro_block_edge_count) + (64 * i) + lid;

        if (edge_index < bucket_edges)
        {
            uint2 edge = source[offset_mem + (my_bucket * DUCK_A_EDGES_64) + edge_index];

            if (edge.x == 0 && edge.y == 0) continue;

            int bucket = (edge.x >> 6) & (64 - 1);

            __syncthreads();

            int counter = min((int)(atomicAdd(counters + bucket, 1)), (int)15);

            tmp[bucket][counter] = edge;

            __syncthreads();

            int idx = min(16, counters[lid]);

            if (idx >= 8) {
                int new_count = (idx - 8);
                counters[lid] = new_count;

                {
                    int cnt = min(
                            (int)atomicAdd(dest_indexes + start_block * 64 + my_bucket * 64 + lid, 8), 
                            static_cast<int>(DUCK_A_EDGES - 8));

                    {
                        destination[((my_bucket * 64 + lid) * DUCK_A_EDGES + cnt) / 4] =
                            Pack4edges(tmp[lid][0], tmp[lid][1], tmp[lid][2], tmp[lid][3]);
                        destination[((my_bucket * 64 + lid) * DUCK_A_EDGES + cnt + 4) / 4] =
                            Pack4edges(tmp[lid][4], tmp[lid][5], tmp[lid][6], tmp[lid][7]);
                    }
                }

                for (int t = 0; t < new_count; t++) {
                    tmp[lid][t] = tmp[lid][t + 8];
                }

            }
        }
    }

    __syncthreads();

    {
        int idx = min(16, counters[lid]);

        if (idx > 0)
        {
            int cnt = min(
                    (int)atomicAdd(dest_indexes + start_block * 64 + my_bucket * 64 + lid, 4), 
                    (int)(DUCK_A_EDGES - 4));

            destination[((my_bucket * 64 + lid) * DUCK_A_EDGES + cnt) / 4] = Pack4edges(
                    tmp[lid][0],
                    idx > 1 ? tmp[lid][1] : make_uint2(0, 0),
                    idx > 2 ? tmp[lid][2] : make_uint2(0, 0),
                    idx > 3 ? tmp[lid][3] : make_uint2(0, 0));
        }
        if (idx > 4)
        {
            int cnt = min(
                    (int)atomicAdd(dest_indexes + start_block * 64 + my_bucket * 64 + lid, 4), 
                    (int)(DUCK_A_EDGES - 4));

            destination[((my_bucket * 64 + lid) * DUCK_A_EDGES + cnt) / 4] = Pack4edges(
                    tmp[lid][4],
                    idx > 5 ? tmp[lid][5] : make_uint2(0, 0),
                    idx > 6 ? tmp[lid][6] : make_uint2(0, 0),
                    idx > 7 ? tmp[lid][7] : make_uint2(0, 0));
        }
    }
}

__device__ __forceinline__  void Increase2bCounter(u32 * ecounters, const int bucket)
{
    int word = bucket >> 5;
    unsigned char bit = bucket & 0x1F;
    u32 mask = 1 << bit;

    u32 old = atomicOr(ecounters + word, mask) & mask;

    if (old > 0)
        atomicOr(ecounters + word + 4096, mask);
}

__device__ __forceinline__  bool Read2bCounter(u32 * ecounters, const int bucket)
{
    int word = bucket >> 5;
    unsigned char bit = bucket & 0x1F;
    u32 mask = 1 << bit;

    return (ecounters[word + 4096] & mask) > 0;
}

    template<class P, int BKTINSIZE, int BKTOUTSIZE>
__global__   void FluffyRound(
        const uint2 *__restrict__  source,
        uint2 *__restrict__  destination,
        const int *__restrict__  src_indexes,
        int * __restrict__ dest_indexes)
{

    const int lid = threadIdx.x;
    const int group = blockIdx.x;

    __shared__ u32 ecounters[8*CTHREADS];

    const int edges_in_bucket = min(src_indexes[group], BKTINSIZE);
    const int loops = (edges_in_bucket + CTHREADS) / CTHREADS;

    ecounters[lid] = 0;
    ecounters[lid + CTHREADS] = 0;
    ecounters[lid + (CTHREADS * 2)] = 0;
    ecounters[lid + (CTHREADS * 3)] = 0;
    ecounters[lid + (CTHREADS * 4)] = 0;
    ecounters[lid + (CTHREADS * 5)] = 0;
    ecounters[lid + (CTHREADS * 6)] = 0;
    ecounters[lid + (CTHREADS * 7)] = 0;

    __syncthreads();

    for (int i = 0; i < loops; i++) {
        const int lindex = (i * CTHREADS) + lid;

        if (lindex < edges_in_bucket) {
            const int index = (BKTINSIZE * group) + lindex;

            uint2 edge = source[index];

            if (edge.x == 0 && edge.y == 0) continue;

            Increase2bCounter(ecounters, (edge.x & P::EDGEMASK) >> 12);
        }
    }

    __syncthreads();

    for (int i = 0; i < loops; i++) {
        const int lindex = (i * CTHREADS) + lid;

        if (lindex < edges_in_bucket) {
            const int index = (BKTINSIZE * group) + lindex;

            uint2 edge = source[index];

            if (edge.x == 0 && edge.y == 0) continue;

            if (Read2bCounter(ecounters, (edge.x & P::EDGEMASK) >> 12))
            {
                const int bucket = edge.y & BKTMASK4K;
                const int bkt_idx = min(
                        atomicAdd(dest_indexes + bucket, 1),
                        BKTOUTSIZE - 1);

                destination[(bucket * BKTOUTSIZE) + bkt_idx] =
                    make_uint2(edge.y, edge.x);
            }
        }
    }

}


__global__   void /*Magical*/FluffyTail/*Pony*/(
        const uint2 * source,
        uint2 * destination,
        const int * src_indexes,
        int * dest_indexes)
{
    const int lid = threadIdx.x;
    const int group = blockIdx.x;

    int my_edges = src_indexes[group];
    __shared__ int dest_idx;

    if (lid == 0) {
        dest_idx = atomicAdd(dest_indexes, my_edges);
    }

    __syncthreads();

    if (lid < my_edges) {
        destination[dest_idx + lid] = source[group * DUCK_B_EDGES + lid];
    }
}

std::vector<u64> buffer_h;
std::vector<int*> buffer_a;
std::vector<int*> buffer_b;
std::vector<int*> indexes_e;
std::vector<int*> indexes_e2;
std::vector<u64*> recovery;

    template <class P>
bool TrimEdges(
        const SipKeys& k,
        int* buffer_a,
        int* buffer_b,
        int* indexes_e,
        int* indexes_e2)
{
    FluffySeed2A<P> << < P::GABLOCKS, P::GATPB >> > (
            k.k0, k.k1, k.k2, k.k3,
            (ulonglong4 *)buffer_a,
            (int *)indexes_e2);

    cudaDeviceSynchronize();

    FluffySeed2B << < P::GBBLOCKS, P::GBTPB >> > (
            (const uint2 *)buffer_a,
            (ulonglong4 *)buffer_b,
            (const int *)indexes_e2,
            (int *)indexes_e,
            0);

    cudaMemcpy(
            buffer_a,
            buffer_b,
            buffer_size / 2,
            cudaMemcpyDeviceToDevice);

    FluffySeed2B << < P::GBBLOCKS, P::GBTPB >> > (
            (const uint2 *)buffer_a,
            (ulonglong4 *)buffer_b,
            (const int *)indexes_e2,
            (int *)indexes_e,
            32);

    cudaMemcpy(
            &((char *)buffer_a)[buffer_size / 2],
            buffer_b,
            buffer_size / 2, cudaMemcpyDeviceToDevice);


    cudaMemset(indexes_e2, 0, indexes_size);
    FluffyRound<P, DUCK_A_EDGES, DUCK_B_EDGES> << < P::TRBLOCKS, P::TRTPB >> > (
            (const uint2 *)buffer_a,
            (uint2 *)buffer_b,
            (const int *)indexes_e,
            (int *)indexes_e2);

    cudaDeviceSynchronize();

    for (int i = 0; i < 80; i++)
    {
        cudaMemset(indexes_e, 0, indexes_size);
        FluffyRound<P, DUCK_B_EDGES, DUCK_B_EDGES> << < P::TRBLOCKS, P::TRTPB >> > (
                (const uint2 *)buffer_b,
                (uint2 *)buffer_a,
                (const int *)indexes_e2,
                (int *)indexes_e);

        cudaMemset(indexes_e2, 0, indexes_size);
        FluffyRound<P, DUCK_B_EDGES, DUCK_B_EDGES> << < P::TRBLOCKS, P::TRTPB >> > (
                (const uint2 *)buffer_a,
                (uint2 *)buffer_b,
                (const int *)indexes_e,
                (int *)indexes_e2);
    }

    cudaMemset(indexes_e, 0, indexes_size);
    cudaDeviceSynchronize();

    FluffyTail << < P::TLBLOCKS, P::TLTPB >> > (
            (const uint2 *)buffer_b,
            (uint2 *)buffer_a,
            (const int *)indexes_e2,
            (int *)indexes_e);

    return true;
}

template<class P>
class CuckooHash {
    public:
        std::vector<u64> cuckoo;

        CuckooHash() : cuckoo(P::CUCKOO_SIZE) {
        }

        void set(node_t u, node_t v) {
            u64 niew = (u64)u << P::NODEBITS | v;
            for (node_t ui = u >> P::IDXSHIFT; ui < P::CUCKOO_SIZE ; ui = (ui+1) & P::CUCKOO_MASK) {
                u64 old = cuckoo[ui];
                if (old == 0 || (old >> P::NODEBITS) == (u & P::KEYMASK)) {
                    cuckoo[ui] = niew;
                    return;
                }
            }
        }
        node_t operator[](node_t u) const {
            for (node_t ui = u >> P::IDXSHIFT; ui < P::CUCKOO_SIZE; ui = (ui+1) & P::CUCKOO_MASK) {
                u64 cu = cuckoo[ui];
                if (!cu)
                    return 0;
                if ((cu >> P::NODEBITS) == (u & P::KEYMASK)) {
                    assert(((ui - (u >> P::IDXSHIFT)) & P::CUCKOO_MASK) < P::MAXDRIFT);
                    return (node_t)(cu & P::NODEMASK);
                }
            }
        }
};

template<class P>
u32 Path(CuckooHash<P> &cuckoo, u32 u, u32 *us) {
    u32 nu, u0 = u;
    for (nu = 0; u; u = cuckoo[u]) {
        if (nu >= MAXPATHLEN) {
            while (nu-- && us[nu] != u) ;
            if (~nu) {
                printf("illegal %4d-cycle from node %d\n", MAXPATHLEN-nu, u0);
                exit(0);
            }
            printf("maximum path length exceeded\n");
            return 0; // happens once in a million runs or so; signal trouble
        }
        us[nu++] = u;
    }
    return nu;
}

using Edge = std::pair<node_t, node_t>;

/*
   This function remains here because it provides a clear algorithm how the
   nonces are recovered from the edges. The Solution function after this one
   executes the same algorithm except on the GPU which is much much faster.
 */
template <class P>
void SolutionSlow(
        const SipKeys& keys,
        std::set<uint32_t >& nonces,
        node_t* us, u32 nu,
        node_t* vs, u32 nv) {

    std::set<Edge> cycle;
    cycle.insert(Edge{*us, *vs});

    while (nu--) {
        Edge e{us[(nu+1)&~1], us[nu|1]};
        cycle.insert(e); // u's in even position; v's in odd
    }

    while (nv--) {
        Edge e{vs[nv|1], vs[(nv+1)&~1]};
        cycle.insert(e); // u's in odd position; v's in even
    }

    for (u64 nonce = 0; nonce < P::NEDGES; nonce++) {
        u64 u = host_dipnode<P::EDGEMASK>(keys.k0, keys.k1, keys.k2, keys.k3, nonce, 0) << 1;
        u64 v = host_dipnode<P::EDGEMASK>(keys.k0, keys.k1, keys.k2, keys.k3, nonce, 1) << 1 | 1;

        Edge e{u,v};

        if (cycle.find(e) != cycle.end()) {
            nonces.insert(nonce);
            cycle.erase(e);
        }
    }
}

template <class P>
void Solution(
        uint8_t proof_size,
        const SipKeys& keys,
        std::set<uint32_t >& nonces,
        node_t* us, u32 nu,
        node_t* vs, u32 nv,
        int* buffer_a,
        int* indexes_e2,
        u64* recovery) {

    u64 solution_edges[MAXPROOFLENGTH];
    u32 host_nonces[MAXPROOFLENGTH];

    int i = 0;
    solution_edges[i] = (u64)*us | ((u64)(*vs) << 32); 
    i++;
    while (nu--) {
        solution_edges[i] = (u64)us[(nu+1)&~1] | ((u64)us[nu|1] << 32);
        i++;
    }

    while (nv--) {
        solution_edges[i] = (u64)vs[nv|1] | ((u64)vs[(nv+1)&~1] << 32);
        i++;
    }

    assert(i == proof_size);
    cudaMemcpy(recovery, solution_edges, proof_size * 8, cudaMemcpyHostToDevice);
    cudaDeviceSynchronize();

    cudaMemset(indexes_e2, 0, indexes_size);
    FluffyRecovery<P> << < P::RBLOCKS, P::RTPB >> >(
            keys.k0, keys.k1, keys.k2, keys.k3,
            proof_size,
            (ulonglong4 *)buffer_a,
            (int *)indexes_e2,
            recovery);
    cudaDeviceSynchronize();
    cudaMemcpy(host_nonces, indexes_e2, proof_size * 8, cudaMemcpyDeviceToHost);
    for(int j = 0; j < i; j++) {
        nonces.insert(host_nonces[j]);
    }
}

using Cycle = std::set<uint32_t>;
using Cycles = std::vector<Cycle>;

template <class P>
bool FindCycles(
        const SipKeys& keys,
        Cycles& cycles, 
        u64* edges,
        const u32 size,
        uint8_t proof_size,
        int* buffer_a,
        int* indexes_e2,
        u64* recovery) {

    assert(proof_size <= MAXPROOFLENGTH);

    CuckooHash<P> cuckoo;
    node_t us[MAXPATHLEN], vs[MAXPATHLEN];

    for (u32 i = 0; i < size; i++) {
        u32 uxyz = edges[i] >> 32;  u32 vxyz = edges[i] & 0xffffffff;
        const u32 u0 = uxyz << 1, v0 = (vxyz << 1) | 1;
        if (u0) {
            u32 nu = Path(cuckoo, u0, us), nv = Path(cuckoo, v0, vs);
            if (!nu-- || !nv--) {
                return false; // drop edge causing trouble
            }

            if (us[nu] == vs[nv]) {
                const u32 min = nu < nv ? nu : nv;
                for (nu -= min, nv -= min; us[nu] != vs[nv]; nu++, nv++) ;
                const u32 len = nu + nv + 1;

                if (len == proof_size) {
                    Cycle cycle;
                    Solution<P>(
                            proof_size,
                            keys,
                            cycle,
                            us,
                            nu,
                            vs,
                            nv,
                            buffer_a,
                            indexes_e2,
                            recovery);
                    cycles.emplace_back(cycle);
                }
            } else if (nu < nv) {
                while (nu--) {
                    cuckoo.set(us[nu+1], us[nu]);
                }
                cuckoo.set(u0, v0);
            } else {
                while (nv--) {
                    cuckoo.set(vs[nv+1], vs[nv]);
                }
                cuckoo.set(v0, u0);
            }
        }
    }
    return !cycles.empty();
}

int CudaDevices()
{
    int count = 0;
    cudaGetDeviceCount(&count);
    return count;
}

const size_t BUFFER_H_SIZE = 150000;
int SetupKernelBuffers() {
    const int count = CudaDevices();

    if(!buffer_a.empty()) {
        return count;
    }

    assert(buffer_h.empty());
    assert(buffer_a.empty());
    assert(buffer_b.empty());
    assert(indexes_e.empty());
    assert(indexes_e2.empty());
    assert(recovery.empty());

    buffer_h.resize(count * BUFFER_H_SIZE);
    buffer_a.resize(count, nullptr);
    buffer_b.resize(count, nullptr);
    indexes_e.resize(count, nullptr);
    indexes_e2.resize(count, nullptr);
    recovery.resize(count, nullptr);

    return count;
}

using Cycle = std::set<uint32_t>;

template <class offset_t, uint8_t EDGEBITS, uint8_t XBITS>
struct Run
{
    using P = Params<offset_t, EDGEBITS, XBITS>;

    bool operator()(
            Cycles& cycles,
            uint64_t sip_k0, uint64_t sip_k1,
            uint8_t proof_size,
            int device)
    {
        assert(device >= 0);
        assert(device < buffer_a.size());

        u32 host_a[256 * 256];

        size_t free_device_mem = 0;
        size_t total_device_mem = 0;

        std::ostringstream err_msg;

        cudaError_t status = cudaSetDevice(device);
        if (status != cudaSuccess) {
            err_msg << "An error occurred while trying to set the CUDA device: ";
            err_msg << cudaGetErrorString(status);
            throw CudaSetDeviceException(err_msg.str());
        }

        if(buffer_a[device] == nullptr) {
            cudaMemGetInfo(&free_device_mem, &total_device_mem);

            status = cudaMalloc((void**)&buffer_a[device], buffer_size);
            if (status != cudaSuccess) {
                err_msg << "An error while allocating memory for buffer_a: ";
                err_msg << cudaGetErrorString(status);

                throw CudaMemoryAllocationException(err_msg.str());
            }
        }

        if(buffer_b[device] == nullptr) {
            status = cudaMalloc((void**)&buffer_b[device], buffer_size_2);
            if (status != cudaSuccess) {
                err_msg << "An error while allocating memory for buffer_b: ";
                err_msg << cudaGetErrorString(status);

                throw CudaMemoryAllocationException(err_msg.str());
            }
        }

        if(indexes_e[device] == nullptr) {
            status = cudaMalloc((void**)&indexes_e[device], indexes_size);
            if (status != cudaSuccess) {
                err_msg << "An error while allocating memory for indexes_e: ";
                err_msg << cudaGetErrorString(status);

                throw CudaMemoryAllocationException(err_msg.str());
            }
        }

        if(indexes_e2[device] == nullptr) {
            status = cudaMalloc((void**)&indexes_e2[device], indexes_size);
            if (status != cudaSuccess) {
                err_msg << "An error while allocating memory for indexes_e2: ";
                err_msg << cudaGetErrorString(status);

                throw CudaMemoryAllocationException(err_msg.str());
            }
        }

        if(recovery[device] == nullptr) {
            status = cudaMalloc((void**)&recovery[device], proof_size*8);
            if (status != cudaSuccess) {
                err_msg << "An error while allocating memory for recovery: ";
                err_msg << cudaGetErrorString(status);

                throw CudaMemoryAllocationException(err_msg.str());
            }
        }

        SipKeys keys {
            sip_k0 ^ 0x736f6d6570736575ULL,
                   sip_k1 ^ 0x646f72616e646f6dULL,
                   sip_k0 ^ 0x6c7967656e657261ULL,
                   sip_k1 ^ 0x7465646279746573ULL
        };

        cudaMemset(indexes_e[device], 0, indexes_size);
        cudaMemset(indexes_e2[device], 0, indexes_size);

        cudaDeviceSynchronize();

        if(!TrimEdges<P>(
                    keys,
                    buffer_a[device],
                    buffer_b[device],
                    indexes_e[device],
                    indexes_e2[device])) {
            return false;
        }

        cudaMemcpy(host_a, indexes_e[device], 64 * 64 * 4, cudaMemcpyDeviceToHost);

        int pos = host_a[0];

        if(pos > 0 && pos < 500000) {
            cudaMemcpy(
                    &buffer_h[device*BUFFER_H_SIZE],
                    &((u64*)buffer_a[device])[0],
                    pos * 8,
                    cudaMemcpyDeviceToHost);
        }

        cudaDeviceSynchronize();

        if(pos > 0 && pos < 500000) {
            return FindCycles<P>(
                    keys,
                    cycles,
                    &buffer_h[device*BUFFER_H_SIZE],
                    pos,
                    proof_size,
                    buffer_a[device],
                    indexes_e2[device],
                    recovery[device]);
        }
        return false;
    }
};

size_t CudaGetFreeMemory(int device){
    size_t free, total;

    cudaSetDevice(device);
    cudaDeviceReset();

    cudaMemGetInfo(&free, &total);
    return free;
}

namespace nvml = merit::nvml;

std::unique_ptr<nvml::nvml_handle, int (*)(nvml::nvml_handle *)> initNVML(){
    auto nvml = std::unique_ptr<nvml::nvml_handle, int (*)(nvml::nvml_handle *)>(nvml::nvml_create(), nvml::nvml_destroy);

    if (nvml == nullptr)
        std::cerr << "Failed to initialize NVML" << std::endl;

    return nvml;
}


std::vector<merit::GPUInfo> GPUsInfo()
{
    std::vector<merit::GPUInfo> res{};

    // Initialize NVML library
    auto nvml = initNVML();

    nvml::nvmlDevice_t device;
    int devices = CudaDevices();

    for (int index = 0; index < devices; index ++) {
        merit::GPUInfo item{};
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, index);
        item.id = index;
        item.title = prop.name;
        item.total_memory = prop.totalGlobalMem;

        // Get device
        auto nvmlres = nvml->nvmlDeviceGetHandleByIndex(index, &device);
        if (nvml::NVML_SUCCESS != nvmlres)
            std::cerr << "Failed to get handle for device " << index << " " << nvml->nvmlErrorString(nvmlres) << std::endl;

        // Temperature
        unsigned int temp;
        nvmlres = nvml->nvmlDeviceGetTemperature(device, 0, &temp);
        if (nvml::NVML_SUCCESS != nvmlres){
            std::cerr << "Failed to get temperature of device" << index << " " << nvml->nvmlErrorString(nvmlres) << std::endl;
            item.temperature = -1;
        } else {
            item.temperature = temp;
        }

        // GPU cores and memory utilization
        nvml::nvmlUtilization_t gpuUtil;
        nvmlres = nvml->nvmlDeviceGetUtilizationRates(device, &gpuUtil);
        if (nvml::NVML_SUCCESS != nvmlres){
            std::cerr << "Failed to get utilization of device " << index << " : " << nvml->nvmlErrorString(nvmlres) << std::endl;
            item.gpu_util = -1;
            item.memory_util = -1;
        } else {
            item.gpu_util = gpuUtil.gpu;
            item.memory_util = gpuUtil.memory;
        }

        // Fan speed
        unsigned int speed;
        nvmlres = nvml->nvmlDeviceGetFanSpeed(device, &speed);
        if (nvml::NVML_SUCCESS != nvmlres){
            std::cerr << "Failed to get fan speed of device " <<  index <<  " : " << nvml->nvmlErrorString(nvmlres) << std::endl;
            item.fan_speed = -1;
        } else {
            item.fan_speed = speed;
        }

        // add device info to array
        res.push_back(item);
    }

    return res;
}


bool FindCyclesOnCudaDevice(
        uint64_t sip_k0, uint64_t sip_k1,
        uint8_t edgebits,
        uint8_t proof_size,
        Cycles& cycles,
        int device)
{
    switch (edgebits) {
        case 16:
            return Run<uint32_t, 16u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 17:
            return Run<uint32_t, 17u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 18:
            return Run<uint32_t, 18u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 19:
            return Run<uint32_t, 19u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 20:
            return Run<uint32_t, 20u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 21:
            return Run<uint32_t, 21u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 22:
            return Run<uint32_t, 22u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 23:
            return Run<uint32_t, 23u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 24:
            return Run<uint32_t, 24u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 25:
            return Run<uint32_t, 25u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 26:
            return Run<uint32_t, 26u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 27:
            return Run<uint32_t, 27u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 28:
            return Run<uint32_t, 28u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 29:
            return Run<uint32_t, 29u, 6u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 30:
            return Run<uint64_t, 30u, 8u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        case 31:
            return Run<uint64_t, 31u, 8u>{}(cycles, sip_k0, sip_k1, proof_size, device);
        default:
            std::stringstream e;
            e << __func__ << ": Edgebits equal to " << edgebits << " is not supported";
            throw std::runtime_error(e.str());
    }
}
