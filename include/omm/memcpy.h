#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

// Include specialized implementations of memcpy for different CPU architectures
#include "omm/detail/cpu_features.h"
#include "omm/detail/memcpy/memcpy_avx512.h"
#include "omm/detail/memcpy/memcpy_avx2.h"

namespace omm {

namespace detail {

// Function pointer type for memcpy implementations
using MemcpyFunc = void* (*)(void*, const void*, std::size_t);

// Function to determine the best memcpy implementation, once, at runtime
MemcpyFunc initialize_best_memcpy() {
    if (detail::cpu_supports_avx512f()) {
        return memcpy_avx512;
    } else if (detail::cpu_supports_avx2()) {
        return memcpy_avx2;
    } else {
        return std::memcpy;
    }
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