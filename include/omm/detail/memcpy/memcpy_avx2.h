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

#ifndef OMM_MEMCPY_AVX2_H
#define OMM_MEMCPY_AVX2_H

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
// Redefine macros for standalone use (auto-detected by cpu_features.h, if using full library)
#define G_L3_CACHE_SIZE 32 * 1024 * 1024 // 32MB
#define G_CACHE_LINE_SIZE 64

#endif

namespace omm {

inline void memcpy_avx2(void* dest, const void* src, std::size_t size) noexcept {
    // Fast path for small sizes: leverage compiler's built-in optimization
    if (size <= G_L3_CACHE_SIZE) {
        __builtin_memcpy(dest, src, size);
        return;
    }

    // AVX2 uses 256-bit (32-byte) vectors
    static constexpr std::size_t ALIGNMENT = 32;
    static constexpr std::size_t UNROLL_FACTOR = 8;
    static constexpr std::size_t BLOCK_SIZE = ALIGNMENT * UNROLL_FACTOR;
    // Prefetch two blocks ahead - adjust based on target hardware characteristics
    static constexpr std::size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;
    static constexpr std::size_t PREFETCH_COUNT = PREFETCH_DISTANCE / G_CACHE_LINE_SIZE;

    auto* dest_ptr = static_cast<uint8_t*>(dest);
    const auto* src_ptr = static_cast<const uint8_t*>(src);

    // Align destination to ALIGNMENT boundary for optimal streaming stores
    std::size_t initial_bytes = (ALIGNMENT - (reinterpret_cast<std::uintptr_t>(dest_ptr) & ALIGNMENT - 1)) & ALIGNMENT - 1;
    if (initial_bytes > 0) {
        __builtin_memcpy(dest_ptr, src_ptr, initial_bytes);
        dest_ptr += initial_bytes;
        src_ptr += initial_bytes;
        size -= initial_bytes;
    }

    // Use __m256i pointers for AVX2 intrinsics
    auto* dest_vec = reinterpret_cast<__m256i*>(dest_ptr);
    const auto* src_vec = reinterpret_cast<const __m256i*>(src_ptr);
    // Compute size that's a multiple of BLOCK_SIZE for vectorized processing
    const std::size_t vector_size = size & ~(BLOCK_SIZE - 1);

    for (std::size_t i = 0; i < vector_size; i += BLOCK_SIZE) {
        // Prefetch data using NTA (Non-Temporal Access) hint to bypass cache for large transfers
        #pragma unroll(PREFETCH_COUNT)
        for (std::size_t p = 0; p < PREFETCH_DISTANCE; p += G_CACHE_LINE_SIZE) {
            _mm_prefetch(src_ptr + p, _MM_HINT_NTA);
        }
        // Unrolled AVX2 loads and streaming stores to minimize cache interaction
        #pragma unroll(UNROLL_FACTOR)
        for (std::size_t p = 0; p < UNROLL_FACTOR; ++p) {
            _mm256_stream_si256(dest_vec++, _mm256_loadu_si256(src_vec++));
        }
        src_ptr += BLOCK_SIZE;
    }

    // Handle remaining bytes (< BLOCK_SIZE) with standard memcpy
    std::size_t remaining = size - vector_size;
    if (remaining > 0) {
        __builtin_memcpy(dest_vec, src_ptr, remaining);
    }

    // Ensure all non-temporal (streaming) stores are visible
    _mm_sfence();
}

} // namespace omm

#endif // OMM_MEMCPY_AVX2_H
