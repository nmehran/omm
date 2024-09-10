#include <benchmark/benchmark.h>
#include <memory>
#include <cstring>
#include "omm/memcpy/memcpy.hpp"
#include "benchmark_utils.hpp"

// === Constants ===

constexpr size_t KB = 1024;
constexpr size_t MB = 1024 * KB;
constexpr size_t GB = 1024 * MB;

constexpr size_t MIN_ALLOCATION = 1 * MB;
constexpr size_t MAX_ALLOCATION = 4 * GB;

constexpr uint16_t REPETITIONS = 5;
constexpr uint16_t CPU_NUM = 0;

// === Benchmark Functions ===

static void BM_StandardMemcpy(benchmark::State& state) {
    omm::benchmark::PinToCore(CPU_NUM);  // Adjust core number as needed
    const size_t size = state.range(0);
    auto src = std::make_unique<char[]>(size);
    auto dest = std::make_unique<char[]>(size);
    benchmark::DoNotOptimize(src.get());
    benchmark::DoNotOptimize(dest.get());

    std::memset(src.get(), 'A', size);  // Initialize source buffer

    for (auto _ : state) {
        std::memcpy(dest.get(), src.get(), size);
        benchmark::DoNotOptimize(src.get());
        benchmark::DoNotOptimize(dest.get());
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(size));
}

static void BM_AVX2_Memcpy(benchmark::State& state) {
    omm::benchmark::PinToCore(CPU_NUM);  // Adjust core number as needed
    const size_t size = state.range(0);
    auto src = std::make_unique<char[]>(size);
    auto dest = std::make_unique<char[]>(size);
    benchmark::DoNotOptimize(src.get());
    benchmark::DoNotOptimize(dest.get());

    std::memset(src.get(), 'A', size);  // Initialize source buffer

    for (auto _ : state) {
        omm::memcpy_avx2(dest.get(), src.get(), size);
        benchmark::DoNotOptimize(src.get());
        benchmark::DoNotOptimize(dest.get());
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(size));
}

// === Benchmark Configuration ===

#define CONFIGURE_BENCHMARK(benchmark_func) \
    BENCHMARK(benchmark_func) \
        ->RangeMultiplier(2)->Range(MIN_ALLOCATION, MAX_ALLOCATION) \
        ->Repetitions(REPETITIONS) \
        ->MinTime(1.0) \
        ->Unit(benchmark::kMillisecond) \
        ->ReportAggregatesOnly(true) \
        ->Name(omm::benchmark::GetColoredBenchmarkName(#benchmark_func))

CONFIGURE_BENCHMARK(BM_StandardMemcpy);
CONFIGURE_BENCHMARK(BM_AVX2_Memcpy);

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);

    omm::benchmark::FilteredReporter report_mean_only({"median", "stddev", "cv"});
    benchmark::RunSpecifiedBenchmarks(&report_mean_only);

    return 0;
}