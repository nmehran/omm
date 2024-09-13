#ifndef OMM_MEMCPY_H
#define OMM_MEMCPY_H

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>

#include "omm/detail/cpu_features.h"

// Include specialized implementations
#ifdef __AVX512F__
#include "memcpy_avx512.h"
#endif

#ifdef __AVX2__
#include "omm/detail/memcpy/memcpy_avx2.h"
#endif

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

inline void standard_memcpy_impl(void* dst, const void* src, std::size_t size) {
    std::memcpy(dst, src, size);
}

// Best implementation selection
inline MemcpyFunc get_best_memcpy_impl() {
#ifdef __AVX512F__
    if (cpu_supports_avx512f()) {
        return memcpy_avx512;
    }
#endif
#ifdef __AVX2__
    if (cpu_supports_avx2()) {
        return memcpy_avx2;
    }
#endif
    return memcpy_standard;
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
            memcpy_avx512(d, s, copy_size);
#else
            memcpy_standard(d, s, copy_size);
#endif
            break;
        case MemcpyImpl::AVX2:
#ifdef __AVX2__
            memcpy_avx2(d, s, copy_size);
#else
            memcpy_standard(d, s, copy_size);
#endif
            break;
        case MemcpyImpl::Standard:
            memcpy_standard(d, s, copy_size);
            break;
        case MemcpyImpl::Auto:
        default:
            detail::get_best_memcpy_impl()(d, s, copy_size);
            break;
    }
}

inline void memcpy_auto(void* dst, const void* src, std::size_t size) {
    detail::get_best_memcpy_impl()(dst, src, size);
}

inline void memcpy_standard(void* dst, const void* src, std::size_t size) {
    detail::standard_memcpy_impl(dst, src, size);
}

inline std::function<void(void*, const void*, std::size_t)> get_memcpy_function(MemcpyImpl impl) {
    switch(impl) {
        case MemcpyImpl::AVX512:
#ifdef __AVX512F__
            return memcpy_avx512;
#else
            return memcpy_standard;
#endif
        case MemcpyImpl::AVX2:
#ifdef __AVX2__
            return memcpy_avx2;
#else
            return memcpy_standard;
#endif
        case MemcpyImpl::Standard:
            return memcpy_standard;
        case MemcpyImpl::Auto:
        default:
            return memcpy_auto;
    }
}

} // namespace omm

#endif // OMM_MEMCPY_H