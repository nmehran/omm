#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <atomic>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#include <fstream>
#include <sstream>
#else
#error "Unsupported compiler"
#endif

// Define DEBUG macro. Uncomment the next line to enable debug output.
// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << "[DEBUG] " << __func__ << ": " << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

namespace omm::detail {

/**
 * @brief Checks if the CPU supports AVX-512F instructions.
 * @return true if AVX-512F is supported, false otherwise.
 */
inline bool cpu_supports_avx512f() {
    #if defined(__AVX512F__)
        #if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
            bool supported = __builtin_cpu_supports("avx512f");
            DEBUG_PRINT("AVX-512F compile-time check passed, runtime check: " << (supported ? "supported" : "not supported"));
            return supported;
        #else
            DEBUG_PRINT("AVX-512F compile-time check passed, but no runtime check available");
            return false;  // Safe fallback if runtime check is unavailable
        #endif
    #else
        DEBUG_PRINT("AVX-512F not enabled at compile-time");
        return false;  // AVX-512F not enabled at compile-time
    #endif
}

/**
 * @brief Checks if the CPU supports AVX2 instructions.
 * @return true if AVX2 is supported, false otherwise.
 */
inline bool cpu_supports_avx2() {
    #if defined(__AVX2__)
        #if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
            bool supported = __builtin_cpu_supports("avx2");
            DEBUG_PRINT("AVX2 compile-time check passed, runtime check: " << (supported ? "supported" : "not supported"));
            return supported;
        #else
            DEBUG_PRINT("AVX2 compile-time check passed, but no runtime check available");
            return false;  // Safe fallback if runtime check is unavailable
        #endif
    #else
        DEBUG_PRINT("AVX2 not enabled at compile-time");
        return false;  // AVX2 not enabled at compile-time
    #endif
}

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

    class CacheSizeManager {
    public:
        static CacheSizeManager& instance() {
            static CacheSizeManager instance;
            return instance;
        }

        const std::array<std::uint32_t, NUM_CACHE_SIZES>& get_cache_sizes() {
            std::call_once(init_flag_, &CacheSizeManager::initialize, this);
            return cache_sizes_;
        }

    private:
        CacheSizeManager() = default;
        ~CacheSizeManager() = default;
        CacheSizeManager(const CacheSizeManager&) = delete;
        CacheSizeManager& operator=(const CacheSizeManager&) = delete;

        void initialize() {
            auto detected_sizes = detect_cache_sizes();
            cache_sizes_[L1_CACHE] = detected_sizes[0].size > 0 ? detected_sizes[0].size : DEFAULT_L1_CACHE_SIZE;
            cache_sizes_[L2_CACHE] = detected_sizes[2].size > 0 ? detected_sizes[2].size : DEFAULT_L2_CACHE_SIZE;
            cache_sizes_[L3_CACHE] = detected_sizes[3].size > 0 ? detected_sizes[3].size : DEFAULT_L3_CACHE_SIZE;
            cache_sizes_[CACHE_LINE] = detected_sizes[0].line_size > 0 ? detected_sizes[0].line_size : DEFAULT_CACHE_LINE_SIZE;
        }

        /**
         * @brief Detects cache sizes at runtime using platform-specific methods.
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
            }

            return cache_info;
        }

        std::array<std::uint32_t, NUM_CACHE_SIZES> cache_sizes_;
        std::once_flag init_flag_;
    };

// Convenience macros for accessing cache sizes
#define G_L1_CACHE_SIZE (omm::detail::CacheSizeManager::instance().get_cache_sizes()[omm::detail::L1_CACHE])
#define G_L2_CACHE_SIZE (omm::detail::CacheSizeManager::instance().get_cache_sizes()[omm::detail::L2_CACHE])
#define G_L3_CACHE_SIZE (omm::detail::CacheSizeManager::instance().get_cache_sizes()[omm::detail::L3_CACHE])
#define G_CACHE_LINE_SIZE (omm::detail::CacheSizeManager::instance().get_cache_sizes()[omm::detail::CACHE_LINE])

/**
 * @brief Struct to hold comprehensive CPU information.
 */
    struct CPUInfo {
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
        const auto& sizes = CacheSizeManager::instance().get_cache_sizes();
        return {
                sizes[L1_CACHE],
                sizes[L2_CACHE],
                sizes[L3_CACHE],
                sizes[CACHE_LINE]
        };
    }

} // namespace omm::detail