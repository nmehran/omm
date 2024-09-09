#ifndef OMM_MEMCPY_IMPL_HPP
#define OMM_MEMCPY_IMPL_HPP

#include "memcpy.hpp"
#include <immintrin.h>

namespace omm {
    namespace detail {

// CPU feature detection implementations
#if defined(_MSC_VER)
        inline void cpuid(int cpu_info[4], int info_type) {
    __cpuidex(cpu_info, info_type, 0);
}
#elif defined(__GNUC__) || defined(__clang__)
        inline void cpuid(int cpu_info[4], int info_type) {
            __cpuid_count(info_type, 0, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
        }
#endif

        inline bool cpu_supports_avx512f() {
            int cpu_info[4];
            cpuid(cpu_info, 7);
            return (cpu_info[1] & (1 << 16)) != 0; // Check bit 16 of EBX for AVX-512F
        }

        inline bool cpu_supports_avx2() {
            int cpu_info[4];
            cpuid(cpu_info, 7);
            return (cpu_info[1] & (1 << 5)) != 0; // Check bit 5 of EBX for AVX2
        }

// Helper functions implementations
        template<typename T>
        const void* get_source_pointer(const T& src) {
            if constexpr (std::is_pointer_v<T>) {
                return static_cast<const void*>(src);
            } else if constexpr (has_data<T>::value) {
                return static_cast<const void*>(src.data());
            } else {
                return static_cast<const void*>(&src);
            }
        }

        template<typename T>
        void* get_dest_pointer(T& dst) {
            if constexpr (std::is_pointer_v<T>) {
                return static_cast<void*>(dst);
            } else if constexpr (has_data<T>::value) {
                return static_cast<void*>(dst.data());
            } else {
                return static_cast<void*>(&dst);
            }
        }

        template<typename T>
        std::size_t get_size(const T& container, std::size_t size) {
            if constexpr (has_size<T>::value) {
                return container.size() * sizeof(typename T::value_type);
            } else {
                return size;
            }
        }

// Memcpy implementation functions
        inline void memcpy_avx512(void* dst, const void* src, std::size_t size) {
//            char* d = reinterpret_cast<char*>(dst);
//            const char* s = reinterpret_cast<const char*>(src);
//
//            // For small sizes, use pointer instructions
//            if (size <= 64) {
//                for (size_t i = 0; i < size; ++i) {
//                    *d++ = *s++;
//                }
//                return;
//            }
//
//            // Handle initial unaligned bytes
//            size_t initial_bytes = (64 - (reinterpret_cast<uintptr_t>(d) & 63)) & 63;
//            for (size_t i = 0; i < initial_bytes; ++i) {
//                *d++ = *s++;
//            }
//            size -= initial_bytes;
//
//            constexpr size_t BLOCK_SIZE = 512;
//            constexpr size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;
//            constexpr size_t CACHE_LINE_SIZE = 64;
//
//            size_t vec_size = size & ~(BLOCK_SIZE - 1);
//            const char* s_end = s + vec_size;
//
//            // Set up 512-bit aligned pointers for the main loop
//            const __m512i* s512 = reinterpret_cast<const __m512i*>(s);
//            __m512i* d512 = reinterpret_cast<__m512i*>(d);
//
//            while (s < s_end) {
//                // Prefetch
//                _mm_prefetch(s, _MM_HINT_T0);
//                for (size_t i = 0; i < PREFETCH_DISTANCE; i += CACHE_LINE_SIZE) {
//                    _mm_prefetch(s + i, _MM_HINT_NTA);
//                }
//
//                // Copy BLOCK_SIZE bytes
//                for (size_t i = 0; i < BLOCK_SIZE; i += 64) {
//                    __m512i v = _mm512_loadu_si512(s512);
//                    _mm512_stream_si512(d512, v);
//                    ++s512;
//                    ++d512;
//                }
//
//                s += BLOCK_SIZE;
//                d += BLOCK_SIZE;
//            }
//
//            // Handle remaining bytes with pointer instructions
//            size_t remaining = size - vec_size;
//            for (size_t i = 0; i < remaining; ++i) {
//                *d++ = *s++;
//            }
//
//            // Ensure all non-temporal stores are visible
//            _mm_sfence();
        }

        inline void memcpy_avx2(void* dst, const void* src, std::size_t size) {
            char* d = reinterpret_cast<char*>(dst);
            const char* s = reinterpret_cast<const char*>(src);

            // For small sizes, use pointer instructions
            if (size <= 32) {
                for (size_t i = 0; i < size; ++i) {
                    *d++ = *s++;
                }
                return;
            }

            // Handle initial unaligned bytes
            size_t initial_bytes = (32 - (reinterpret_cast<uintptr_t>(d) & 31)) & 31;
            for (size_t i = 0; i < initial_bytes; ++i) {
                *d++ = *s++;
            }
            size -= initial_bytes;

            constexpr size_t BLOCK_SIZE = 256;
            constexpr size_t PREFETCH_DISTANCE = 2 * BLOCK_SIZE;
            constexpr size_t CACHE_LINE_SIZE = 64;

            size_t vec_size = size & ~(BLOCK_SIZE - 1);
            const char* s_end = s + vec_size;

            // Set up 256-bit aligned pointers for the main loop
            const __m256i* s256 = reinterpret_cast<const __m256i*>(s);
            __m256i* d256 = reinterpret_cast<__m256i*>(d);

            while (s < s_end) {
                // Prefetch
                _mm_prefetch(s, _MM_HINT_T0);
                for (size_t i = 0; i < PREFETCH_DISTANCE; i += CACHE_LINE_SIZE) {
                    _mm_prefetch(s + i, _MM_HINT_NTA);
                }

                // Copy BLOCK_SIZE bytes
                for (size_t i = 0; i < BLOCK_SIZE; i += 32) {
                    __m256i v = _mm256_loadu_si256(s256);
                    _mm256_stream_si256(d256, v);
                    ++s256;
                    ++d256;
                }

                s += BLOCK_SIZE;
                d += BLOCK_SIZE;
            }

            // Handle remaining bytes with pointer instructions
            size_t remaining = size - vec_size;
            for (size_t i = 0; i < remaining; ++i) {
                *d++ = *s++;
            }

            // Ensure all non-temporal stores are visible
            _mm_sfence();
        }

        inline void standard_memcpy(void* dst, const void* src, std::size_t size) {
            std::memcpy(dst, src, size);
        }

// Best implementation selection
        MemcpyFunc best_memcpy_impl = standard_memcpy;

        void initialize_best_memcpy_impl() {
            if (cpu_supports_avx512f()) {
                best_memcpy_impl = memcpy_avx512;
            } else if (cpu_supports_avx2()) {
                best_memcpy_impl = memcpy_avx2;
            } else {
                best_memcpy_impl = standard_memcpy;
            }
        }

// Initialization
        Initializer initializer;

        Initializer::Initializer() {
            initialize_best_memcpy_impl();
        }

    } // namespace detail

// Public API implementation
    template<typename Dst, typename Src>
    void memcpy(Dst&& dst, const Src& src, std::size_t size, MemcpyImpl impl) {
        void* d = detail::get_dest_pointer(dst);
        const void* s = detail::get_source_pointer(src);
        std::size_t copy_size = detail::get_size(src, size);

        switch(impl) {
            case MemcpyImpl::AVX512:
                detail::memcpy_avx512(d, s, copy_size);
                break;
            case MemcpyImpl::AVX2:
                detail::memcpy_avx2(d, s, copy_size);
                break;
            case MemcpyImpl::Standard:
                detail::standard_memcpy(d, s, copy_size);
                break;
            case MemcpyImpl::Auto:
            default:
                detail::best_memcpy_impl(d, s, copy_size);
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

    inline void memcpy_wrapper(void* dst, const void* src, std::size_t size) {
        memcpy(dst, src, size, MemcpyImpl::Auto);
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
                return memcpy_wrapper;
        }
    }

} // namespace omm

#endif // OMM_MEMCPY_IMPL_HPP