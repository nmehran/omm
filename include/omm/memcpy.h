#pragma once

#include <cstddef>
#include <cstring>

#include "omm/detail/cpu_features.h"

// Include specialized implementations of memcpy for different CPU architectures
#ifdef __AVX512F__
#include "omm/detail/memcpy/memcpy_avx512.h"
#endif

#ifdef __AVX2__
#include "omm/detail/memcpy/memcpy_avx2.h"
#endif

namespace omm {

class MemcpyDispatcher {
private:
    // Selects the most suitable memcpy implementation based on CPU capabilities.
    static void* get_best_memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) noexcept {
    #ifdef __AVX512F__
        // Use AVX-512 optimized memcpy if available
        if (cpu_supports_avx512f()) {
            return memcpy_avx512(dest, src, n);
        }
    #endif

    #ifdef __AVX2__
        // Use AVX2 optimized memcpy if available
        if (detail::cpu_supports_avx2()) {
            return memcpy_avx2(dest, src, n);
        }
    #endif

        // Default to standard memcpy if no specialized support
        return std::memcpy(dest, src, n);
    }

public:
    // Public memcpy function that dispatches to the best implementation
    static void* memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) noexcept {
        return get_best_memcpy(dest, src, n);
    }
};

// Inline alias to the dispatcher’s memcpy function with a fast path for small sizes.
// Uses built-in memcpy for small transfers to optimize performance for typical small-copy operations.
__attribute__((nonnull(1, 2)))
inline void* memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) noexcept {
    // Use built-in memcpy for sizes up to the L3 cache size for performance
    return (n < G_L3_CACHE_SIZE) ? __builtin_memcpy(dest, src, n)
                                 : MemcpyDispatcher::memcpy(dest, src, n);
}

} // namespace omm
