#pragma once

#include <cstddef>
#include <cstring>

// Include specialized implementations of memcpy for different CPU architectures
#include "omm/detail/cpu_features.h"

#ifdef __AVX512F__
#include "omm/detail/memcpy/memcpy_avx512.h"
#endif
#ifdef __AVX2__
#include "omm/detail/memcpy/memcpy_avx2.h"
#endif

namespace omm {

namespace detail {

// Function pointer type for memcpy implementations
using MemcpyFunc = void* (*)(void*, const void*, std::size_t);

// Selects the optimal memcpy implementation based on available CPU features.
// Called once at program startup to initialize the best_memcpy function pointer.
MemcpyFunc initialize_best_memcpy() {
    #ifdef __AVX512F__
    if (cpu_supports_avx512f()) return memcpy_avx512;
    #endif
    #ifdef __AVX2__
    if (cpu_supports_avx2()) return memcpy_avx2;
    #endif
    return std::memcpy;
}

// Global variable to store the best memcpy implementation
// This is initialized once when the program starts
static const MemcpyFunc best_memcpy = initialize_best_memcpy();

} // namespace detail

// Inline memcpy function with a fast path for small sizes
__attribute__((always_inline, hot, artificial, returns_nonnull, nonnull(1, 2)))
inline void* memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) noexcept {
    // Use builtin_memcpy for sizes up to the L3 cache size for performance
    if (__builtin_expect(n < G_L3_CACHE_SIZE, 1)) {
        return __builtin_memcpy(dest, src, n);
    }
    return detail::best_memcpy(dest, src, n);
}

} // namespace omm