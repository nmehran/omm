//#ifndef OMM_MEMCPY_IMPL_HPP
//#define OMM_MEMCPY_IMPL_HPP
//
//#include "omm/detail/cpu_features.hpp"
//#include "memcpy.hpp"
//#include <immintrin.h>
//#include <cstring>
//#include <algorithm>
//
//namespace omm {
//    namespace detail {
//
//        // Helper functions implementations
//        template<typename T>
//        const void* get_source_pointer(const T& src) {
//            if constexpr (std::is_pointer_v<T>) {
//                return static_cast<const void*>(src);
//            } else if constexpr (has_data<T>::value) {
//                return static_cast<const void*>(src.data());
//            } else {
//                return static_cast<const void*>(&src);
//            }
//        }
//
//        template<typename T>
//        void* get_dest_pointer(T& dst) {
//            if constexpr (std::is_pointer_v<T>) {
//                return static_cast<void*>(dst);
//            } else if constexpr (has_data<T>::value) {
//                return static_cast<void*>(dst.data());
//            } else {
//                return static_cast<void*>(&dst);
//            }
//        }
//
//        template<typename T>
//        std::size_t get_size(const T& container, std::size_t size) {
//            if constexpr (has_size<T>::value) {
//                return container.size() * sizeof(typename T::value_type);
//            } else {
//                return size;
//            }
//        }
//
//        // Memcpy implementation functions
//        inline void memcpy_avx512(void* dst, const void* src, std::size_t size) {
//            // TODO: Implement AVX-512 memcpy
//            // This is a placeholder implementation
//            std::memcpy(dst, src, size);
//        }
//
//        inline void memcpy_avx2(void* dst, const void* src, std::size_t size) {
//            char* d = reinterpret_cast<char*>(dst);
//            const char* s = reinterpret_cast<const char*>(src);
//
//            // For small sizes, use standard memcpy
//            if (size <= G_L3_CACHE_SIZE) {
//                __builtin_memcpy(dst, src, size);
//                return;
//            }
//
//            // Handle initial unaligned bytes
//            size_t initial_bytes = (32 - (reinterpret_cast<uintptr_t>(d) & 31)) & 31;
//            if (initial_bytes > 0) {
//                __builtin_memcpy(d, s, initial_bytes);
//                d += initial_bytes;
//                s += initial_bytes;
//                size -= initial_bytes;
//            }
//
//            constexpr size_t BLOCK_SIZE = 512;
//            constexpr size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;
//
//            size_t vec_size = size & ~(BLOCK_SIZE - 1);
//            const char* s_end = s + vec_size;
//
//            while (s < s_end) {
//                _mm_prefetch(s, _MM_HINT_T0);
//                for (size_t i = 0; i < PREFETCH_DISTANCE; i += G_CACHE_LINE_SIZE) {
//                    _mm_prefetch(s + i, _MM_HINT_NTA);
//                }
//
//                for (size_t i = 0; i < BLOCK_SIZE; i += 32) {
//                    __m256i v = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(s + i));
//                    _mm256_stream_si256(reinterpret_cast<__m256i*>(d + i), v);
//                }
//
//                s += BLOCK_SIZE;
//                d += BLOCK_SIZE;
//            }
//
//            size_t remaining = size - vec_size;
//            if (remaining > 0) {
//                __builtin_memcpy(d, s, remaining);
//            }
//
//            // Ensure all non-temporal stores are visible
//            _mm_sfence();
//        }
//
//        inline void standard_memcpy(void* dst, const void* src, std::size_t size) {
//            std::memcpy(dst, src, size);
//        }
//
//        inline void optimized_memcpy(void* dst, const void* src, std::size_t size) {
//            if (size < G_L3_CACHE_SIZE) {
//                standard_memcpy(dst, src, size);
//            } else {
//                memcpy_avx2(dst, src, size);
//            }
//        }
//
//        // Best implementation selection
//        MemcpyFunc best_memcpy_impl = standard_memcpy;
//
//        inline void initialize_best_memcpy_impl() {
//            if (cpu_supports_avx512f()) {
//                best_memcpy_impl = memcpy_avx512;
//            } else if (cpu_supports_avx2()) {
//                best_memcpy_impl = optimized_memcpy;
//            } else {
//                best_memcpy_impl = standard_memcpy;
//            }
//        }
//
//        // Initialization
//        inline Initializer::Initializer() {
//            initialize_best_memcpy_impl();
//        }
//
//        extern Initializer initializer;
//
//    } // namespace detail
//
//    // Public API implementation
//    template<typename Dst, typename Src>
//    void memcpy(Dst&& dst, const Src& src, std::size_t size, MemcpyImpl impl) {
//        void* d = detail::get_dest_pointer(dst);
//        const void* s = detail::get_source_pointer(src);
//        std::size_t copy_size = detail::get_size(src, size);
//
//        switch(impl) {
//            case MemcpyImpl::AVX512:
//                detail::memcpy_avx512(d, s, copy_size);
//                break;
//            case MemcpyImpl::AVX2:
//                detail::memcpy_avx2(d, s, copy_size);
//                break;
//            case MemcpyImpl::Standard:
//                detail::standard_memcpy(d, s, copy_size);
//                break;
//            case MemcpyImpl::Auto:
//            default:
//                detail::best_memcpy_impl(d, s, copy_size);
//                break;
//        }
//    }
//
//    inline void memcpy_auto(void* dst, const void* src, std::size_t size) {
//        memcpy(dst, src, size, MemcpyImpl::Auto);
//    }
//
//    inline void memcpy_avx512(void* dst, const void* src, std::size_t size) {
//        memcpy(dst, src, size, MemcpyImpl::AVX512);
//    }
//
//    inline void memcpy_avx2(void* dst, const void* src, std::size_t size) {
//        memcpy(dst, src, size, MemcpyImpl::AVX2);
//    }
//
//    inline void memcpy_standard(void* dst, const void* src, std::size_t size) {
//        memcpy(dst, src, size, MemcpyImpl::Standard);
//    }
//
//    inline std::function<void(void*, const void*, std::size_t)> get_memcpy_function(MemcpyImpl impl) {
//        switch(impl) {
//            case MemcpyImpl::AVX512:
//                return memcpy_avx512;
//            case MemcpyImpl::AVX2:
//                return memcpy_avx2;
//            case MemcpyImpl::Standard:
//                return memcpy_standard;
//            case MemcpyImpl::Auto:
//            default:
//                return memcpy_auto;
//        }
//    }
//
//} // namespace omm
//
//#endif // OMM_MEMCPY_IMPL_HPP