#ifndef OMM_MEMCPY_HPP
#define OMM_MEMCPY_HPP

#include <cstddef>
#include <type_traits>
#include <memory>
#include <cstring>
#include <functional>
#include <immintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

namespace omm {

// Forward declarations
    enum class MemcpyImpl;

    namespace detail {
        // Type aliases
        using MemcpyFunc = void (*)(void*, const void*, std::size_t);

        // CPU feature detection
        bool cpu_supports_avx512f();
        bool cpu_supports_avx2();

        // Implementation functions
        void memcpy_avx512(void* dst, const void* src, std::size_t size);
        void memcpy_avx2(void* dst, const void* src, std::size_t size);
        void standard_memcpy(void* dst, const void* src, std::size_t size);

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
        const void* get_source_pointer(const T& src);

        template<typename T>
        void* get_dest_pointer(T& dst);

        template<typename T>
        std::size_t get_size(const T& container, std::size_t size);

        // Best implementation selection
        extern MemcpyFunc best_memcpy_impl;
        void initialize_best_memcpy_impl();

        // Initialization
        struct Initializer {
            Initializer();
        };
        extern Initializer initializer;
    } // namespace detail

// Public API

/**
 * @brief Enum for explicitly selecting a memcpy implementation
 */
    enum class MemcpyImpl {
        Auto,    ///< Automatically select the best available implementation
        AVX512,  ///< Use AVX-512 implementation
        AVX2,    ///< Use AVX2 implementation
        Standard ///< Use standard memcpy implementation
    };

/**
 * @brief Main memcpy function template
 *
 * @tparam Dst Destination type
 * @tparam Src Source type
 * @param dst Destination object
 * @param src Source object
 * @param size Size of data to copy (in bytes)
 * @param impl Specific implementation to use (default: Auto)
 */
    template<typename Dst, typename Src>
    void memcpy(Dst&& dst, const Src& src, std::size_t size = 0, MemcpyImpl impl = MemcpyImpl::Auto);

// Non-template wrapper functions
    void memcpy_auto(void* dst, const void* src, std::size_t size);
    void memcpy_avx512(void* dst, const void* src, std::size_t size);
    void memcpy_avx2(void* dst, const void* src, std::size_t size);
    void memcpy_standard(void* dst, const void* src, std::size_t size);
    void memcpy_wrapper(void* dst, const void* src, std::size_t size);

/**
 * @brief Get a function pointer to a specific memcpy implementation
 *
 * @param impl Desired implementation (default: Auto)
 * @return std::function<void(void*, const void*, std::size_t)> Function pointer to the selected implementation
 */
    std::function<void(void*, const void*, std::size_t)> get_memcpy_function(MemcpyImpl impl = MemcpyImpl::Auto);

} // namespace omm

// Include implementation details
#include "memcpy_impl.hpp"

#endif // OMM_MEMCPY_HPP