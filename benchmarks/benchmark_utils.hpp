#ifndef OMM_BENCHMARK_UTILS_HPP
#define OMM_BENCHMARK_UTILS_HPP

#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <set>
#include <array>
#include <cstddef>
#include <iostream>
#include <cstdlib>
#include <thread>

#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
#endif

namespace omm {
namespace benchmark {

/**
 * @brief Gets a color-coded benchmark name and cycles through colors.
 * @param name The benchmark name.
 * @param resetColor Whether to reset color after the name.
 * @return Color-coded benchmark name.
 */
std::string GetColoredBenchmarkName(const std::string& name, bool resetColor = false) {
    static const std::array<const char*, 4> colors = {
            "\033[32m", // Green
            "\033[34m", // Blue
            "\033[35m", // Magenta
            "\033[36m"  // Cyan
    };

    static size_t colorIndex = 0;

    const char* colorCode = colors[colorIndex];
    colorIndex = (colorIndex + 1) % colors.size();

    std::string coloredName = colorCode + name;
    if (resetColor) {
        coloredName += "\033[0m";
    }
    return coloredName;
}

/**
 * @brief Custom reporter that filters out specified benchmark aggregates.
 *
 * Inherits from benchmark::ConsoleReporter and allows selective filtering
 * of benchmark aggregates (e.g., median, stddev, cv) from the output.
 */
class FilteredReporter : public ::benchmark::ConsoleReporter {
private:
    std::set<std::string> filtered_aggregates;

public:
    /**
     * @brief Constructor with default filter for median, stddev, and cv.
     * @param aggregates_to_filter Vector of aggregate names to filter out.
     */
    explicit FilteredReporter(const std::vector<std::string>& aggregates_to_filter =
            {"median", "stddev", "cv"})
            : filtered_aggregates(aggregates_to_filter.begin(), aggregates_to_filter.end()) {}

    /**
     * @brief Overridden ReportRuns method to filter aggregates.
     * @param reports Vector of benchmark runs to report.
     */
    void ReportRuns(const std::vector<Run>& reports) override {
        std::vector<Run> filtered_reports;
        filtered_reports.reserve(reports.size());  // Optimize for performance

        for (const auto& run : reports) {
            if (filtered_aggregates.find(run.aggregate_name) == filtered_aggregates.end()) {
                filtered_reports.push_back(run);
            }
        }

        ConsoleReporter::ReportRuns(filtered_reports);
    }

    /**
     * @brief Adds an aggregate to the filter.
     * @param aggregate Name of the aggregate to filter out.
     */
    void AddFilter(const std::string& aggregate) {
        filtered_aggregates.insert(aggregate);
    }

    /**
     * @brief Removes an aggregate from the filter.
     * @param aggregate Name of the aggregate to stop filtering.
     * @return True if the aggregate was found and removed, false otherwise.
     */
    bool RemoveFilter(const std::string& aggregate) {
        return filtered_aggregates.erase(aggregate) > 0;
    }

    /**
     * @brief Clears all filters, allowing all aggregates to be reported.
     */
    void ClearFilters() {
        filtered_aggregates.clear();
    }
};

/**
 * @brief Pins the current thread to the specified CPU core.
 * @param core_id The ID of the CPU core to pin the thread to.
 */
void PinToCore(int core_id) {
    #ifdef __linux__
        if (core_id < 0) return;  // Do nothing if core_id is negative

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
            std::cerr << "Failed to pin thread to core " << core_id << std::endl;
        }
    #else
        std::cerr << "PinToCore is only implemented for Linux" << std::endl;
    #endif
}

/**
 * @brief Allocates memory using huge pages.
 * @param size The size of memory to allocate in bytes.
 * @return Pointer to the allocated memory, or nullptr if allocation failed.
 */
//void* AllocateHugePages(std::size_t size) {
//    // Not Implemented Yet
//}

/**
 * @brief Frees memory allocated with huge pages.
 * @param ptr Pointer to the memory to free.
 * @param size The size of the memory block to free.
 */
//void FreeHugePages(void* ptr, std::size_t size) {
//    // Not Implemented Yet
//}

/**
 * @brief Flushes the CPU cache.
 */
//void FlushCache() {
//    // Not Implemented Yet
//}

} // namespace benchmark
} // namespace omm

#endif // OMM_BENCHMARK_UTILS_HPP