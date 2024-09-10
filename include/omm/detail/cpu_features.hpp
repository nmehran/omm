#ifndef OMM_CPU_FEATURES_HPP
#define OMM_CPU_FEATURES_HPP

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <x86intrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#else
#error "Unsupported compiler"
#endif

namespace omm::detail {

/**
 * @brief Stores information about a CPU cache level.
 */
struct CacheInfo {
    std::uint32_t size;
    std::uint32_t line_size;
    std::uint32_t associativity;
    std::uint32_t type;
};

/**
 * @brief Enum for indexing cache sizes.
 */
enum CACHE_SIZES {
    L1_CACHE,
    L2_CACHE,
    L3_CACHE,
    CACHE_LINE,
    NUM_CACHE_SIZES
};

// Default fallback values for cache sizes
constexpr std::uint32_t DEFAULT_L1_CACHE_SIZE = 32 * 1024;
constexpr std::uint32_t DEFAULT_L2_CACHE_SIZE = 256 * 1024;
constexpr std::uint32_t DEFAULT_L3_CACHE_SIZE = 8 * 1024 * 1024;
constexpr std::uint32_t DEFAULT_CACHE_LINE_SIZE = 64;

// Global array for storing detected cache sizes
inline std::array<std::uint32_t, NUM_CACHE_SIZES> G_CACHE_SIZES = {0};

// Convenience macros for accessing cache sizes
#define G_L1_CACHE_SIZE (G_CACHE_SIZES[L1_CACHE])
#define G_L2_CACHE_SIZE (G_CACHE_SIZES[L2_CACHE])
#define G_L3_CACHE_SIZE (G_CACHE_SIZES[L3_CACHE])
#define G_CACHE_LINE_SIZE (G_CACHE_SIZES[CACHE_LINE])

/**
 * @brief Detects cache sizes at runtime using CPUID.
 * @return Vector of CacheInfo for all detected cache levels.
 */
std::vector<CacheInfo> detect_cache_sizes() {
    std::vector<CacheInfo> cache_info;

    #if defined(_MSC_VER)
    int cpu_info[4];
    #elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    #endif

    // Check max CPUID leaf
    #if defined(_MSC_VER)
    __cpuid(cpu_info, 0);
    int max_leaf = cpu_info[0];
    #elif defined(__GNUC__) || defined(__clang__)
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    unsigned int max_leaf = eax;
    #endif

//    std::cout << "Max CPUID leaf: " << max_leaf << std::endl; // DEBUG

    // Detect cache sizes using CPUID
    #if defined(_MSC_VER)
    __cpuid(cpu_info, 0x80000005);
    std::uint32_t l1d_size = (cpu_info[2] >> 24) * 1024;
    std::uint32_t l1i_size = (cpu_info[3] >> 24) * 1024;

    __cpuid(cpu_info, 0x80000006);
    std::uint32_t l2_size = (cpu_info[2] >> 16) * 1024;
    std::uint32_t l3_size = ((cpu_info[3] >> 18) & 0x3FFF) * 512 * 1024;
    #elif defined(__GNUC__) || defined(__clang__)
    __get_cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
    std::uint32_t l1d_size = (ecx >> 24) * 1024;
    std::uint32_t l1i_size = (edx >> 24) * 1024;

    __get_cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    std::uint32_t l2_size = (ecx >> 16) * 1024;
    std::uint32_t l3_size = ((edx >> 18) & 0x3FFF) * 512 * 1024;
    #endif

    // Use fallback values if detection fails
    l1d_size = (l1d_size > 0) ? l1d_size : DEFAULT_L1_CACHE_SIZE;
    l2_size = (l2_size > 0) ? l2_size : DEFAULT_L2_CACHE_SIZE;
    l3_size = (l3_size > 0) ? l3_size : DEFAULT_L3_CACHE_SIZE;

    // Adjust L3 cache size (divide by 2 as it's reported as total across all cores)
    l3_size /= 2;

//    std::cout << "Detected cache sizes: "
//              << "L1D: " << l1d_size
//              << ", L1I: " << l1i_size
//              << ", L2: " << l2_size
//              << ", L3: " << l3_size << " bytes" << std::endl; // DEBUG

    cache_info.push_back({l1d_size, DEFAULT_CACHE_LINE_SIZE, 0, 1});
    cache_info.push_back({l1i_size, DEFAULT_CACHE_LINE_SIZE, 0, 2});
    cache_info.push_back({l2_size, DEFAULT_CACHE_LINE_SIZE, 0, 3});
    cache_info.push_back({l3_size, DEFAULT_CACHE_LINE_SIZE, 0, 3});

    return cache_info;
}

/**
 * @brief Initializes the global cache size variables.
 */
inline void initialize_cache_sizes() {
    auto cache_info = detect_cache_sizes();
    for (const auto& cache : cache_info) {
        switch (cache.type) {
            case 1: // Data cache
                G_L1_CACHE_SIZE = cache.size;
                G_CACHE_LINE_SIZE = cache.line_size;
                break;
            case 2: // Instruction cache
                // We don't store instruction cache size separately
                break;
            case 3: // Unified cache
                if (G_L2_CACHE_SIZE == 0) {
                    G_L2_CACHE_SIZE = cache.size;
                } else if (G_L3_CACHE_SIZE == 0) {
                    G_L3_CACHE_SIZE = cache.size;
                }
                break;
        }
    }

    // Ensure we have values for all cache levels
    G_L1_CACHE_SIZE = G_L1_CACHE_SIZE > 0 ? G_L1_CACHE_SIZE : DEFAULT_L1_CACHE_SIZE;
    G_L2_CACHE_SIZE = G_L2_CACHE_SIZE > 0 ? G_L2_CACHE_SIZE : DEFAULT_L2_CACHE_SIZE;
    G_L3_CACHE_SIZE = G_L3_CACHE_SIZE > 0 ? G_L3_CACHE_SIZE : DEFAULT_L3_CACHE_SIZE;
    G_CACHE_LINE_SIZE = G_CACHE_LINE_SIZE > 0 ? G_CACHE_LINE_SIZE : DEFAULT_CACHE_LINE_SIZE;

//    std::cout << "Initialized cache sizes: "
//              << "L1: " << G_L1_CACHE_SIZE
//              << ", L2: " << G_L2_CACHE_SIZE
//              << ", L3: " << G_L3_CACHE_SIZE
//              << ", Line: " << G_CACHE_LINE_SIZE << " bytes" << std::endl; // DEBUG
}

/**
 * @brief Struct to ensure cache sizes are initialized.
 */
struct CacheSizeInitializer {
    CacheSizeInitializer() {
        initialize_cache_sizes();
    }
};

// Global instance to ensure initialization
inline CacheSizeInitializer g_cache_size_initializer;

/**
 * @brief Checks if the CPU supports AVX-512F instructions.
 * @return true if AVX-512F is supported, false otherwise.
 */

inline bool cpu_supports_avx512f() {
    #if defined(__AVX512F__) && defined(__GNUC__) && !defined(__clang__)
        // GCC defines __AVX512F__ when -mavx512f is used, but we still need to check at runtime
        return __builtin_cpu_supports("avx512f");
    #elif defined(__AVX512F__) && (defined(__clang__) || defined(_MSC_VER))
        // Clang and MSVC define __AVX512F__ only if the CPU supports it
        return true;
    #else
        return false;
    #endif
}

/**
 * @brief Checks if the CPU supports AVX2 instructions.
 * @return true if AVX2 is supported, false otherwise.
 */
inline bool cpu_supports_avx2() {
    #if defined(__AVX2__) && defined(__GNUC__) && !defined(__clang__)
        // GCC defines __AVX2__ when -mavx2 is used, but we still need to check at runtime
        return __builtin_cpu_supports("avx2");
    #elif defined(__AVX2__) && (defined(__clang__) || defined(_MSC_VER))
        // Clang and MSVC define __AVX2__ only if the CPU supports it
        return true;
    #else
        return false;
    #endif
}

/**
 * @brief Retrieves the CPU's vendor ID string.
 * @return A string containing the CPU vendor ID.
 */
inline const char* get_cpu_vendor_id() {
    static char vendor[13] = {0};
    #if defined(_MSC_VER)
    int cpu_info[4];
    __cpuid(cpu_info, 0);
    *reinterpret_cast<int*>(vendor) = cpu_info[1];
    *reinterpret_cast<int*>(vendor + 4) = cpu_info[3];
    *reinterpret_cast<int*>(vendor + 8) = cpu_info[2];
    #elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    *reinterpret_cast<int*>(vendor) = ebx;
    *reinterpret_cast<int*>(vendor + 4) = edx;
    *reinterpret_cast<int*>(vendor + 8) = ecx;
    #endif
    return vendor;
}

/**
 * @brief Retrieves the CPU's brand string.
 * @return A string containing the CPU brand information.
 */
inline const char* get_cpu_brand_string() {
    static char brand[49] = {0};
    #if defined(_MSC_VER)
    int cpu_info[4];
    __cpuid(cpu_info, 0x80000000);
    if (cpu_info[0] >= 0x80000004) {
        __cpuid(cpu_info, 0x80000002);
        std::memcpy(brand, cpu_info, sizeof(cpu_info));
        __cpuid(cpu_info, 0x80000003);
        std::memcpy(brand + 16, cpu_info, sizeof(cpu_info));
        __cpuid(cpu_info, 0x80000004);
        std::memcpy(brand + 32, cpu_info, sizeof(cpu_info));
    }
    #elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        __get_cpuid(0x80000002, &eax, &ebx, &ecx, &edx);
        std::memcpy(brand, &eax, 16);
        __get_cpuid(0x80000003, &eax, &ebx, &ecx, &edx);
        std::memcpy(brand + 16, &eax, 16);
        __get_cpuid(0x80000004, &eax, &ebx, &ecx, &edx);
        std::memcpy(brand + 32, &eax, 16);
    }
    #endif
    return brand;
}

/**
 * @brief Retrieves the CPU's feature flags.
 * @return A 64-bit unsigned integer representing the CPU feature flags.
 */
inline std::uint64_t get_cpu_features() {
    std::uint64_t features = 0;
    #if defined(_MSC_VER)
    int cpu_info[4];
    __cpuid(cpu_info, 1);
    features = static_cast<std::uint64_t>(cpu_info[2]) << 32 | cpu_info[3];
    #elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        features = static_cast<std::uint64_t>(ecx) << 32 | edx;
    }
    #endif
    return features;
}

/**
 * @brief Struct to hold comprehensive CPU information.
 */
struct CPUInfo {
    const char* vendor_id;
    const char* brand_string;
    std::uint64_t feature_flags;
    std::uint32_t l1_cache_size;
    std::uint32_t l2_cache_size;
    std::uint32_t l3_cache_size;
    std::uint32_t cache_line_size;
};

/**
 * @brief Retrieves comprehensive CPU information.
 * @return A CPUInfo struct containing various CPU details.
 */
inline CPUInfo get_cpu_info() {
    return {
        get_cpu_vendor_id(),
        get_cpu_brand_string(),
        get_cpu_features(),
        G_L1_CACHE_SIZE,
        G_L2_CACHE_SIZE,
        G_L3_CACHE_SIZE,
        G_CACHE_LINE_SIZE
    };
}

} // namespace omm::detail

#endif // OMM_CPU_FEATURES_HPP