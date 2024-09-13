#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <algorithm>
#include <x86intrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#include <fstream>
#include <sstream>

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
#define G_L1_CACHE_SIZE (omm::detail::G_CACHE_SIZES[omm::detail::L1_CACHE])
#define G_L2_CACHE_SIZE (omm::detail::G_CACHE_SIZES[omm::detail::L2_CACHE])
#define G_L3_CACHE_SIZE (omm::detail::G_CACHE_SIZES[omm::detail::L3_CACHE])
#define G_CACHE_LINE_SIZE (omm::detail::G_CACHE_SIZES[omm::detail::CACHE_LINE])

/**
 * @brief Detects cache sizes at runtime using `lscpu` command.
 * @return Vector of CacheInfo for all detected cache levels.
 */
std::vector<CacheInfo> detect_cache_sizes() {
    std::vector<CacheInfo> cache_info(4);  // Cache info for L1d, L1i, L2, L3

    auto parse_size = [](const std::string& str) -> std::uint32_t {
        std::istringstream iss(str);
        float value;
        std::string unit;
        if (!(iss >> value >> unit)) return 0;
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
        if (unit == "kib" || unit == "kb" || unit == "k") return static_cast<std::uint32_t>(value * 1024);
        if (unit == "mib" || unit == "mb" || unit == "m") return static_cast<std::uint32_t>(value * 1024 * 1024);
        return static_cast<std::uint32_t>(value);
    };

#ifdef _WIN32
    // Windows implementation
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* buffer = nullptr;
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &bufferSize);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)malloc(bufferSize);
    GetLogicalProcessorInformationEx(RelationCache, buffer, &bufferSize);

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* ptr = buffer;
    for (DWORD i = 0; i < bufferSize; i += ptr->Size, ptr = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)((char*)ptr + ptr->Size)) {
        if (ptr->Relationship == RelationCache) {
            CACHE_RELATIONSHIP* cache = &ptr->Cache;
            if (cache->Level <= 3) {
                cache_info[cache->Level - 1].size = cache->CacheSize;
                cache_info[cache->Level - 1].line_size = cache->LineSize;
                cache_info[cache->Level - 1].type = cache->Level;
            }
        }
    }
    free(buffer);

#elif defined(__APPLE__)
    // MacOS implementation
    std::array<std::string, 4> cache_names = {"l1dcachesize", "l1icachesize", "l2cachesize", "l3cachesize"};
    std::array<std::string, 4> line_size_names = {"l1dcachelinesize", "l1icachelinesize", "l2cachelinesize", "l3cachelinesize"};

    for (size_t i = 0; i < cache_names.size(); ++i) {
        uint64_t size = 0;
        size_t size_len = sizeof(size);
        if (sysctlbyname(cache_names[i].c_str(), &size, &size_len, nullptr, 0) == 0) {
            cache_info[i].size = static_cast<std::uint32_t>(size);
        }

        uint64_t line_size = 0;
        size_t line_size_len = sizeof(line_size);
        if (sysctlbyname(line_size_names[i].c_str(), &line_size, &line_size_len, nullptr, 0) == 0) {
            cache_info[i].line_size = static_cast<std::uint32_t>(line_size);
        }

        cache_info[i].type = i + 1;
    }

#else
    // Linux implementation
    std::array<std::string, 4> cache_names = {"L1d", "L1i", "L2", "L3"};

    FILE* pipe = popen("lscpu", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            for (size_t i = 0; i < cache_names.size(); ++i) {
                if (line.find(cache_names[i] + " cache:") == 0) {
                    cache_info[i].size = parse_size(line.substr(line.find(':') + 2));
                }
                if (line.find(cache_names[i] + " cache line size:") == 0) {
                    cache_info[i].line_size = parse_size(line.substr(line.find(':') + 2));
                }
            }
        }
        pclose(pipe);
    }
#endif

    // Set default values if sizes are not detected
    for (size_t i = 0; i < cache_info.size(); ++i) {
        if (cache_info[i].size == 0) cache_info[i].size = DEFAULT_L1_CACHE_SIZE;
        if (cache_info[i].line_size == 0) cache_info[i].line_size = DEFAULT_CACHE_LINE_SIZE;
        if (cache_info[i].type == 0) cache_info[i].type = i + 1;

//         std::cout << "Cache " << i+1 << ": " << cache_info[i].size << " bytes, Line Size: "
//                   << cache_info[i].line_size << " bytes" << std::endl; // DEBUG
    }

    return cache_info;
}

/**
 * @brief Initializes the global cache size variables.
 */
inline void initialize_cache_sizes() {
    auto cache_info = detect_cache_sizes();

    for (const auto& cache : cache_info) {
        switch (cache.type) {
            case 1: // L1 Data cache
                G_L1_CACHE_SIZE = cache.size;
                G_CACHE_LINE_SIZE = cache.line_size;
                break;
            case 2: // L1 Instruction cache
                // We don't store instruction cache size separately
                break;
            case 3: // L2 cache
                G_L2_CACHE_SIZE = cache.size;
                break;
            case 4: // L3 cache
                G_L3_CACHE_SIZE = cache.size;
                break;
        }
    }

    // Ensure we have values for all cache levels
    G_L1_CACHE_SIZE = G_L1_CACHE_SIZE > 0 ? G_L1_CACHE_SIZE : DEFAULT_L1_CACHE_SIZE;
    G_L2_CACHE_SIZE = G_L2_CACHE_SIZE > 0 ? G_L2_CACHE_SIZE : DEFAULT_L2_CACHE_SIZE;
    G_L3_CACHE_SIZE = G_L3_CACHE_SIZE > 0 ? G_L3_CACHE_SIZE : DEFAULT_L3_CACHE_SIZE;
    G_CACHE_LINE_SIZE = G_CACHE_LINE_SIZE > 0 ? G_CACHE_LINE_SIZE : DEFAULT_CACHE_LINE_SIZE;
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
