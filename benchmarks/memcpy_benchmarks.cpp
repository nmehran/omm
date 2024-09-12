#include <benchmark/benchmark.h>
#include <cstring>
#include "omm/memcpy/memcpy.hpp"
#include "benchmark_utils.hpp"

// === Constants ===

constexpr size_t KB = 1024;
constexpr size_t MB = 1024 * KB;
constexpr size_t GB = 1024 * MB;

constexpr size_t MIN_ALLOCATION = 1 * MB;
constexpr size_t MAX_ALLOCATION = 8 * GB;

constexpr uint16_t REPETITIONS = 5;
constexpr uint16_t CPU_NUM = 0;

// === Benchmark Fixture ===

class MemcpyBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        size = state.range(0);
        src = new char[size];
        dest = new char[size];

        // Initialize memory
        std::memset(src, 1, size);  // Fill source with 1's
        std::memset(dest, 0, size);  // Fill destination with 0's

        omm::benchmark::PinToCore(CPU_NUM);  // Pin to specified CPU core
    }

    void TearDown(const benchmark::State&) override {
        delete[] src;
        delete[] dest;
    }

protected:
    size_t size{0};
    char* src{nullptr};
    char* dest{nullptr};
};

// === Benchmark Functions ===

BENCHMARK_DEFINE_F(MemcpyBenchmark, StandardMemcpy)(benchmark::State& state) {
    for (auto _ : state) {
        std::memcpy(dest, src, size);
        benchmark::DoNotOptimize(src);
        benchmark::DoNotOptimize(dest);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(size));
}

BENCHMARK_DEFINE_F(MemcpyBenchmark, AVX2_Memcpy)(benchmark::State& state) {
    for (auto _ : state) {
        omm::memcpy_avx2(dest, src, size);
        benchmark::DoNotOptimize(src);
        benchmark::DoNotOptimize(dest);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(size));
}

// === Benchmark Configuration ===

#define CONFIGURE_BENCHMARK(func_name) \
    BENCHMARK_REGISTER_F(MemcpyBenchmark, func_name) \
        ->Name(omm::benchmark::GetColoredBenchmarkName(#func_name)) \
        ->Range(MIN_ALLOCATION, MAX_ALLOCATION) \
        ->RangeMultiplier(2) \
        ->Repetitions(REPETITIONS) \
        ->MinTime(20.0) \
        ->Unit(benchmark::kMillisecond) \
        ->UseRealTime() \
        ->MeasureProcessCPUTime() \
        ->ReportAggregatesOnly(true)

CONFIGURE_BENCHMARK(StandardMemcpy);
CONFIGURE_BENCHMARK(AVX2_Memcpy);

// === Main Function ===

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);

    omm::benchmark::FilteredReporter filtered_reporter({"mean", "stddev", "cv"});
    benchmark::RunSpecifiedBenchmarks(&filtered_reporter);

    return 0;
}