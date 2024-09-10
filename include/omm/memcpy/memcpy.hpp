#ifndef OMM_MEMCPY_HPP
#define OMM_MEMCPY_HPP

#include <cstddef>
#include <type_traits>
#include <memory>
#include <cstring>
#include <functional>

#ifdef __AVX512F__
#include <immintrin.h>
#elif defined(__AVX2__)
#include <immintrin.h>
#endif

#include "omm/detail/cpu_features.hpp"

namespace omm {

// Public API

enum class MemcpyImpl {
    Auto,
    AVX512,
    AVX2,
    Standard
};

template<typename Dst, typename Src>
void memcpy(Dst&& dst, const Src& src, std::size_t size = 0, MemcpyImpl impl = MemcpyImpl::Auto);

void memcpy_auto(void* dst, const void* src, std::size_t size);
void memcpy_avx512(void* dst, const void* src, std::size_t size);
void memcpy_avx2(void* dst, const void* src, std::size_t size);
void memcpy_standard(void* dst, const void* src, std::size_t size);

std::function<void(void*, const void*, std::size_t)> get_memcpy_function(MemcpyImpl impl = MemcpyImpl::Auto);

namespace detail {

// Implementation details

using MemcpyFunc = void (*)(void*, const void*, std::size_t);

// Helper type traits and functions
template<typename T, typename = void>
struct has_data : std::false_type {};

template<typename T>
struct has_data<T, std::void_t<decltype(std::declval<T>().data())>> : std::true_type {};

template<typename T, typename = void>
struct has_size : std::false_type {};

template<typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

template<typename T>
inline const void* get_source_pointer(const T& src) {
    if constexpr (std::is_pointer_v<T>) {
        return static_cast<const void*>(src);
    } else if constexpr (has_data<T>::value) {
        return static_cast<const void*>(src.data());
    } else {
        return static_cast<const void*>(&src);
    }
}

template<typename T>
inline void* get_dest_pointer(T& dst) {
    if constexpr (std::is_pointer_v<T>) {
        return static_cast<void*>(dst);
    } else if constexpr (has_data<T>::value) {
        return static_cast<void*>(dst.data());
    } else {
        return static_cast<void*>(&dst);
    }
}

template<typename T>
inline std::size_t get_size(const T& container, std::size_t size) {
    if constexpr (has_size<T>::value) {
        return container.size() * sizeof(typename T::value_type);
    } else {
        return size;
    }
}

// Implementation functions
#ifdef __AVX512F__
inline void memcpy_avx512_impl(void* dst, const void* src, std::size_t size) {
    char* d = reinterpret_cast<char*>(dst);
    const char* s = reinterpret_cast<const char*>(src);

    // For small sizes, use standard memcpy
    if (size <= G_L3_CACHE_SIZE) {
        __builtin_memcpy(dst, src, size);
        return;
    }

    // Handle initial unaligned bytes
    size_t initial_bytes = (64 - (reinterpret_cast<uintptr_t>(d) & 63)) & 63;
    if (initial_bytes > 0) {
        __builtin_memcpy(d, s, initial_bytes);
        d += initial_bytes;
        s += initial_bytes;
        size -= initial_bytes;
    }

    constexpr size_t BLOCK_SIZE = 256;
    constexpr size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;

    size_t vec_size = size & ~(BLOCK_SIZE - 1);
    const char* s_end = s + vec_size;

    while (s < s_end) {
        _mm_prefetch(s, _MM_HINT_T0);
        for (size_t i = 0; i < PREFETCH_DISTANCE; i += G_CACHE_LINE_SIZE) {
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
#elif defined(__AVX2__)
inline void memcpy_avx2_impl(void* dst, const void* src, std::size_t size) {
    char* d = reinterpret_cast<char*>(dst);
    const char* s = reinterpret_cast<const char*>(src);

    // For small sizes, use standard memcpy
    if (size <= G_L3_CACHE_SIZE) {
        __builtin_memcpy(dst, src, size);
        return;
    }

    // Handle initial unaligned bytes
    size_t initial_bytes = (32 - (reinterpret_cast<uintptr_t>(d) & 31)) & 31;
    if (initial_bytes > 0) {
        __builtin_memcpy(d, s, initial_bytes);
        d += initial_bytes;
        s += initial_bytes;
        size -= initial_bytes;
    }

    constexpr size_t BLOCK_SIZE = 256;
    constexpr size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;

    size_t vec_size = size & ~(BLOCK_SIZE - 1);
    const char* s_end = s + vec_size;

    while (s < s_end) {
        _mm_prefetch(s, _MM_HINT_T0);
        for (size_t i = 0; i < PREFETCH_DISTANCE; i += G_CACHE_LINE_SIZE) {
            _mm_prefetch(s + i, _MM_HINT_NTA);
        }

        for (size_t i = 0; i < BLOCK_SIZE; i += 32) {
            __m256i v = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(s + i));
            _mm256_stream_si256(reinterpret_cast<__m256i*>(d + i), v);
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
#endif

inline void standard_memcpy_impl(void* dst, const void* src, std::size_t size) {
    std::memcpy(dst, src, size);
}

// Best implementation selection
inline MemcpyFunc get_best_memcpy_impl() {
#ifdef __AVX512F__
    if (cpu_supports_avx512f()) {
        return memcpy_avx512_impl;
    }
#endif
#ifdef __AVX2__
    if (omm::detail::cpu_supports_avx2()) {
        return memcpy_avx2_impl;
    }
#endif
    return standard_memcpy_impl;
}

} // namespace detail

// Public API implementation
template<typename Dst, typename Src>
inline void memcpy(Dst&& dst, const Src& src, std::size_t size, MemcpyImpl impl) {
    void* d = detail::get_dest_pointer(dst);
    const void* s = detail::get_source_pointer(src);
    std::size_t copy_size = detail::get_size(src, size);

    switch(impl) {
        case MemcpyImpl::AVX512:
#ifdef __AVX512F__
            detail::memcpy_avx512_impl(d, s, copy_size);
#else
            detail::standard_memcpy_impl(d, s, copy_size);
#endif
            break;
        case MemcpyImpl::AVX2:
#ifdef __AVX2__
            detail::memcpy_avx2_impl(d, s, copy_size);
#else
            detail::standard_memcpy_impl(d, s, copy_size);
#endif
            break;
        case MemcpyImpl::Standard:
            detail::standard_memcpy_impl(d, s, copy_size);
            break;
        case MemcpyImpl::Auto:
        default:
            detail::get_best_memcpy_impl()(d, s, copy_size);
            break;
    }
}

inline void memcpy_auto(void* dst, const void* src, std::size_t size) {
    memcpy(dst, src, size, MemcpyImpl::Auto);
}

inline void memcpy_avx512(void* dst, const void* src, std::size_t size) {
    memcpy(dst, src, size, MemcpyImpl::AVX512);
}

inline void memcpy_avx2(void* dst, const void* src, std::size_t size) {
    memcpy(dst, src, size, MemcpyImpl::AVX2);
}

inline void memcpy_standard(void* dst, const void* src, std::size_t size) {
    memcpy(dst, src, size, MemcpyImpl::Standard);
}

inline std::function<void(void*, const void*, std::size_t)> get_memcpy_function(MemcpyImpl impl) {
    switch(impl) {
        case MemcpyImpl::AVX512:
            return memcpy_avx512;
        case MemcpyImpl::AVX2:
            return memcpy_avx2;
        case MemcpyImpl::Standard:
            return memcpy_standard;
        case MemcpyImpl::Auto:
        default:
            return memcpy_auto;
    }
}

} // namespace omm

#endif // OMM_MEMCPY_HPP