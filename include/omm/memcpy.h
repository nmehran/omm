#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

// Include specialized implementations of memcpy for different CPU architectures
#include "omm/detail/cpu_features.h"
#include "omm/detail/memcpy/memcpy_avx512.h"
#include "omm/detail/memcpy/memcpy_avx2.h"

// Define DEBUG macro. Uncomment the next line to enable debug output.
// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << "[DEBUG] " << __func__ << ": " << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

namespace omm {

namespace detail {

// Function pointer type for memcpy implementations
using MemcpyFunc = void* (*)(void*, const void*, std::size_t);

// Function to determine the best memcpy implementation at runtime
MemcpyFunc initialize_best_memcpy() {
    if (detail::cpu_supports_avx512f()) {
        DEBUG_PRINT("AVX-512F enabled at compile-time");
        return memcpy_avx512;
    } else if (detail::cpu_supports_avx2()) {
        DEBUG_PRINT("AVX2 enabled at compile-time");
        return memcpy_avx2;
    } else {
        DEBUG_PRINT("AVX2 not enabled at compile-time");
        return std::memcpy;
    }
}

// Global variable to store the best memcpy implementation
// This is initialized once when the program starts
static const MemcpyFunc best_memcpy = initialize_best_memcpy();

} // namespace detail

// Inline memcpy function with a fast path for small sizes
__attribute__((always_inline, nonnull(1, 2)))
inline void* memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) noexcept {
    // Use builtin_memcpy for sizes up to the L3 cache size for performance
    if (__builtin_expect(n < G_L3_CACHE_SIZE, 1)) {
        return __builtin_memcpy(dest, src, n);
    }
    return detail::best_memcpy(dest, src, n);
}

} // namespace omm