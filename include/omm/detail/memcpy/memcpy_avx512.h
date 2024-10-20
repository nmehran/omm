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

#pragma once

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

#ifdef OMM_FULL_LIBRARY
#include "omm/detail/cpu_features.h"
#else
#pragma push_macro("G_L3_CACHE_SIZE")
#pragma push_macro("G_CACHE_LINE_SIZE")
#undef G_L3_CACHE_SIZE
#undef G_CACHE_LINE_SIZE

// IMPORTANT: Definitions below are only for standalone mode.
// When using the full library, these are ignored and values are auto-detected
// by cpu_features.h instead.

// L3 cache size: Typically varies between processors. Set to 32MB as a common value.
#define G_L3_CACHE_SIZE (32 * 1024 * 1024)  // 32MB

// Cache line size: Smallest data transfer unit between CPU cache and main memory. Typical for modern x86.
#define G_CACHE_LINE_SIZE 64  // Aligning to this can improve performance by reducing cache misses

#endif

namespace omm {

__attribute__((always_inline, hot, artificial, returns_nonnull, nonnull(1, 2)))
inline void *memcpy_avx512(void *__restrict dest, const void *__restrict src, std::size_t size) noexcept {
    // Fast path for small sizes: leverage compiler's built-in optimization
    if (__builtin_expect(size < G_L3_CACHE_SIZE, 1)) {
        return __builtin_memcpy(dest, src, size);
    }

    // AVX-512 uses 512-bit (64-byte) vectors
    static constexpr std::size_t ALIGNMENT = 64;
    static constexpr std::size_t UNROLL_FACTOR = 8;  // Unrolling factor, use default or adjust based on profiling
    static constexpr std::size_t BLOCK_SIZE = ALIGNMENT * UNROLL_FACTOR;
    // Prefetch two blocks ahead - adjust based on target hardware characteristics
    static constexpr std::size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;
    static constexpr std::size_t PREFETCH_COUNT = PREFETCH_DISTANCE / G_CACHE_LINE_SIZE;

    auto* __restrict dest_ptr = static_cast<uint8_t* __restrict>(dest);
    const auto* __restrict src_ptr = static_cast<const uint8_t* __restrict>(src);

    // Align destination to ALIGNMENT boundary for optimal streaming stores
    std::size_t initial_bytes = (ALIGNMENT - (reinterpret_cast<std::uintptr_t>(dest_ptr) & (ALIGNMENT - 1))) & (ALIGNMENT - 1);
    if (initial_bytes > 0) {
        __builtin_memcpy(dest_ptr, src_ptr, initial_bytes);
        dest_ptr += initial_bytes;
        src_ptr += initial_bytes;
        size -= initial_bytes;
    }

    // Use __m512i pointers for AVX-512 intrinsics
    auto* __restrict dest_vec = reinterpret_cast<__m512i* __restrict>(dest_ptr);
    const auto* __restrict src_vec = reinterpret_cast<const __m512i* __restrict>(src_ptr);
    // Compute size that's a multiple of BLOCK_SIZE for vectorized processing
    const std::size_t vector_size = size & ~(BLOCK_SIZE - 1);

    for (std::size_t i = 0; i < vector_size; i += BLOCK_SIZE) {
        // Prefetch data using NTA (Non-Temporal Access) hint to bypass cache for large transfers
        #pragma unroll(PREFETCH_COUNT)
        for (std::size_t p = 0; p < PREFETCH_DISTANCE; p += G_CACHE_LINE_SIZE) {
            _mm_prefetch(src_ptr + p, _MM_HINT_NTA);
        }
        // Unrolled AVX-512 loads and streaming stores to minimize cache interaction
        #pragma unroll(UNROLL_FACTOR)
        for (std::size_t p = 0; p < UNROLL_FACTOR; ++p) {
            _mm512_stream_si512(dest_vec++, _mm512_loadu_si512(src_vec++));
        }
        src_ptr += BLOCK_SIZE;
    }

    // Handle remaining bytes (< BLOCK_SIZE) with standard memcpy
    std::size_t remaining = size - vector_size;
    if (remaining > 0) {
        __builtin_memcpy(dest_vec, src_vec, remaining);
    }

    // Ensure all non-temporal (streaming) stores are visible
    _mm_sfence();

    return dest;
}

} // namespace omm
