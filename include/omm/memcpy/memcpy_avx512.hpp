/**
 * Copyright 2024-present OMM Project Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * File authors:
 *   - Nima Mehrani <nm@gradientdynamics.com>
 */

#ifndef OMM_MEMCPY_AVX512_HPP
#define OMM_MEMCPY_AVX512_HPP

#include <cstddef>
#include <immintrin.h>
#include <cstdint>

#ifdef OMM_FULL_LIBRARY
#include "omm/detail/cpu_features.hpp"
#else
// Redefine macros for standalone use (suppress warnings)
#pragma push_macro("G_L3_CACHE_SIZE")
#pragma push_macro("G_CACHE_LINE_SIZE")
#undef G_L3_CACHE_SIZE
#undef G_CACHE_LINE_SIZE
#define G_L3_CACHE_SIZE 32 * 1024 * 1024 // 32MB
#define G_CACHE_LINE_SIZE 64

#endif

namespace omm {

inline void memcpy_avx512(void* dst, const void* src, std::size_t size) {
    // For sizes smaller than or equal to L3 cache size, use builtin memcpy
    if (size <= G_L3_CACHE_SIZE) {
        __builtin_memcpy(dst, src, size);
        return;
    }

    char* d = reinterpret_cast<char*>(dst);
    const char* s = reinterpret_cast<const char*>(src);

    // Handle initial unaligned bytes
    size_t initial_bytes = (64 - (reinterpret_cast<uintptr_t>(d) & 63)) & 63;
    if (initial_bytes > 0) {
        __builtin_memcpy(d, s, initial_bytes);
        d += initial_bytes;
        s += initial_bytes;
        size -= initial_bytes;
    }

    const size_t CACHE_LINE_1X = G_CACHE_LINE_SIZE;
    const size_t CACHE_LINE_4X = G_CACHE_LINE_SIZE * 4;
    const size_t BLOCK_SIZE = CACHE_LINE_4X;
    const size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;

    size_t vec_size = size & ~(BLOCK_SIZE - 1);
    const char* s_end = s + vec_size;

    while (s < s_end) {
        for (size_t i = G_CACHE_LINE_SIZE; i < PREFETCH_DISTANCE; i += G_CACHE_LINE_SIZE) {
            _mm_prefetch(s + i, _MM_HINT_NTA);
        }

        for (size_t i = 0; i < BLOCK_SIZE; i += 64) {
            __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(s + i));
            _mm512_stream_si512(reinterpret_cast<__m512i*>(d + i), v);
        }

        s += BLOCK_SIZE;
        d += BLOCK_SIZE;
    }

    size_t remaining = size - vec_size;
    if (remaining > 0) {
        __builtin_memcpy(d, s, remaining);
    }

    // Ensure all non-temporal stores are visible
    _mm_sfence();
}

} // namespace omm

#endif // OMM_MEMCPY_AVX512_HPP