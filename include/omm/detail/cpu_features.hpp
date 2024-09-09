#ifndef OMM_CPU_FEATURES_HPP
#define OMM_CPU_FEATURES_HPP

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <iostream>

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

// Global variables for cache sizes
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

    std::cout << "Max CPUID leaf: " << max_leaf << std::endl;

    // Try CPUID leaf 0x80000006 for L1 and L2 cache info
    #if defined(_MSC_VER)
    __cpuid(cpu_info, 0x80000005);
    std::uint32_t l1d_size = (cpu_info[2] >> 24) * 1024;  // L1 data cache size in bytes
    std::uint32_t l1i_size = (cpu_info[3] >> 24) * 1024;  // L1 instruction cache size in bytes

    __cpuid(cpu_info, 0x80000006);
    std::uint32_t l2_size = (cpu_info[2] >> 16) * 1024;   // L2 cache size in bytes
    std::uint32_t l3_size = ((cpu_info[3] >> 18) & 0x3FFF) * 512 * 1024;  // L3 cache size in bytes
    #elif defined(__GNUC__) || defined(__clang__)
    __get_cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
    std::uint32_t l1d_size = (ecx >> 24) * 1024;  // L1 data cache size in bytes
    std::uint32_t l1i_size = (edx >> 24) * 1024;  // L1 instruction cache size in bytes

    __get_cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    std::uint32_t l2_size = (ecx >> 16) * 1024;   // L2 cache size in bytes
    std::uint32_t l3_size = ((edx >> 18) & 0x3FFF) * 512 * 1024;  // L3 cache size in bytes
    #endif

    std::cout << "Detected cache sizes: "
              << "L1D: " << l1d_size
              << ", L1I: " << l1i_size
              << ", L2: " << l2_size
              << ", L3: " << l3_size << " bytes" << std::endl;

    if (l1d_size > 0) cache_info.push_back({l1d_size, 64, 0, 1});  // Assume 64-byte line size for L1D
    if (l1i_size > 0) cache_info.push_back({l1i_size, 64, 0, 2});  // Assume 64-byte line size for L1I
    if (l2_size > 0) cache_info.push_back({l2_size, 64, 0, 3});    // Assume 64-byte line size for L2
    if (l3_size > 0) cache_info.push_back({l3_size, 64, 0, 3});    // Assume 64-byte line size for L3

    return cache_info;
}

inline void initialize_cache_sizes() {
    auto cache_info = detect_cache_sizes();
    for (const auto& cache : cache_info) {
        switch (cache.type) {
            case 1: // Data cache
                if (G_L1_CACHE_SIZE == 0) {
                    G_L1_CACHE_SIZE = cache.size;
                    G_CACHE_LINE_SIZE = cache.line_size;
                }
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

    std::cout << "Initialized cache sizes: "
              << "L1: " << G_L1_CACHE_SIZE
              << ", L2: " << G_L2_CACHE_SIZE
              << ", L3: " << G_L3_CACHE_SIZE
              << ", Line: " << G_CACHE_LINE_SIZE << " bytes" << std::endl;
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
bool cpu_supports_avx512f();

/**
 * @brief Checks if the CPU supports AVX2 instructions.
 * @return true if AVX2 is supported, false otherwise.
 */
bool cpu_supports_avx2();

/**
 * @brief Retrieves the CPU's vendor ID string.
 * @return A string containing the CPU vendor ID.
 */
const char* get_cpu_vendor_id();

/**
 * @brief Retrieves the CPU's brand string.
 * @return A string containing the CPU brand information.
 */
const char* get_cpu_brand_string();

/**
 * @brief Retrieves the CPU's feature flags.
 * @return A 64-bit unsigned integer representing the CPU feature flags.
 */
std::uint64_t get_cpu_features();

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
CPUInfo get_cpu_info();


} // namespace omm::detail


#endif // OMM_CPU_FEATURES_HPP