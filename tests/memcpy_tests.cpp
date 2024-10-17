#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <random>
#include <array>
#include <algorithm>
#include <iostream>
#include "omm/memcpy.h"

using MemcpyFunc = void *(*)(void*, const void*, std::size_t);

class MemcpyTest : public ::testing::TestWithParam<std::pair<MemcpyFunc, const char*>> {
protected:
    std::vector<size_t> test_sizes;
    std::mt19937 gen{42}; // Fixed seed for reproducibility
    std::array<size_t, 8> alignments{};

    void SetUp() override {
        // Test sizes focusing on L3 cache boundary
        test_sizes = {
                G_L3_CACHE_SIZE,
                G_L3_CACHE_SIZE + 1,
                G_L3_CACHE_SIZE * 2,
                G_L3_CACHE_SIZE * 2 + 1023,
        };

        // Generate alignment offsets (0 to 56 bytes, step 8)
        for (size_t i = 0; i < alignments.size(); ++i) {
            alignments[i] = i * 8;
        }

        std::random_device rd;
        gen = std::mt19937(rd());
    }

    // Generate random data for testing
    std::vector<char> generate_random_data(size_t size) {
        std::vector<char> data(size);
        std::uniform_int_distribution<> dis(0, 255);
        std::generate(data.begin(), data.end(), [&]() { return static_cast<char>(dis(gen)); });
        return data;
    }

    // Report the first mismatch found in a memory comparison
    static void report_mismatch(const char* src, const char* dest, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            if (src[i] != dest[i]) {
                ADD_FAILURE() << "Mismatch at byte " << i << ": expected "
                              << static_cast<int>(static_cast<unsigned char>(src[i]))
                              << ", got " << static_cast<int>(static_cast<unsigned char>(dest[i]));
                return;
            }
        }
    }

    // Test non-overlapping memory copy
    static void test_non_overlapping_copy(void *(*memcpy_func)(void*, const void*, size_t), const char* func_name) {
        constexpr size_t size = 1024;
        std::vector<char> src(size);
        std::vector<char> dest(size);
        std::iota(src.begin(), src.end(), 0);  // Fill with 0, 1, 2, ...

        memcpy_func(dest.data(), src.data(), size);

        EXPECT_TRUE(std::equal(src.begin(), src.end(), dest.begin()))
                            << "Non-overlapping copy failed for " << func_name;

        if (!std::equal(src.begin(), src.end(), dest.begin())) {
            for (size_t i = 0; i < size; ++i) {
                if (src[i] != dest[i]) {
                    std::cout << "Mismatch at index " << i << ": src = " << static_cast<int>(src[i])
                              << ", dest = " << static_cast<int>(dest[i]) << std::endl;
                    break;
                }
            }
        }
    }

    // Test overlapping memory copy
    static void test_overlapping_copy(void *(*memcpy_func)(void*, const void*, size_t), const char* func_name) {
        constexpr size_t size = 1024;
        std::vector<char> buffer(size * 2);
        std::iota(buffer.begin(), buffer.end(), 0);  // Fill with 0, 1, 2, ...

        // Test forward overlapping copy
        std::vector<char> expected = buffer;
        std::memmove(expected.data() + 100, expected.data(), size);

        memcpy_func(buffer.data() + 100, buffer.data(), size);

        EXPECT_TRUE(std::equal(expected.begin(), expected.end(), buffer.begin()))
                            << "Forward overlapping copy failed for " << func_name;

        if (!std::equal(expected.begin(), expected.end(), buffer.begin())) {
            report_first_mismatch(expected, buffer, "Forward");
        }

        // Test backward overlapping copy
        std::iota(buffer.begin(), buffer.end(), 0);
        expected = buffer;
        std::memmove(expected.data(), expected.data() + 100, size);

        memcpy_func(buffer.data(), buffer.data() + 100, size);

        EXPECT_TRUE(std::equal(expected.begin(), expected.end(), buffer.begin()))
                            << "Backward overlapping copy failed for " << func_name;

        if (!std::equal(expected.begin(), expected.end(), buffer.begin())) {
            report_first_mismatch(expected, buffer, "Backward");
        }
    }

private:
    // Helper function to report the first mismatch in overlapping copy tests
    static void report_first_mismatch(const std::vector<char>& expected, const std::vector<char>& actual, const char* direction) {
        for (size_t i = 0; i < expected.size(); ++i) {
            if (expected[i] != actual[i]) {
                std::cout << direction << " mismatch at index " << i << ": expected = "
                          << static_cast<int>(expected[i]) << ", actual = "
                          << static_cast<int>(actual[i]) << std::endl;
                break;
            }
        }
    }
};

// Test various sizes and alignments
TEST_P(MemcpyTest, VariousSizesAndAlignments) {
    auto [memcpy_func, func_name] = GetParam();

    for (size_t size : test_sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto src = generate_random_data(size + G_CACHE_LINE_SIZE);
        std::vector<char> dest(size + G_CACHE_LINE_SIZE, 0);

        for (size_t src_align : alignments) {
            for (size_t dest_align : alignments) {
                SCOPED_TRACE("Src alignment: " + std::to_string(src_align) + ", Dest alignment: " + std::to_string(dest_align));

                size_t copy_size = size - std::max(src_align, dest_align);

                memcpy_func(dest.data() + dest_align, src.data() + src_align, copy_size);

                if (std::memcmp(src.data() + src_align, dest.data() + dest_align, copy_size) != 0) {
                    report_mismatch(src.data() + src_align, dest.data() + dest_align, copy_size);
                    ADD_FAILURE() << "Copy failed for " << func_name << " with size " << copy_size;
                }

                // Verify no overflow
                EXPECT_EQ(0, dest[dest_align + copy_size]) << "Overflow detected in destination";

                std::fill(dest.begin(), dest.end(), 0);
            }
        }
    }
}

// Test small sizes
TEST_P(MemcpyTest, SmallSizes) {
    auto [memcpy_func, func_name] = GetParam();

    std::vector<size_t> small_sizes = {0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256,
                                       1023, 1024, 1025,
                                       G_L3_CACHE_SIZE / 2,
                                       G_L3_CACHE_SIZE - 1,
                                       G_L3_CACHE_SIZE};

    for (size_t size : small_sizes) {
        SCOPED_TRACE("Small size: " + std::to_string(size));

        auto src = generate_random_data(size);
        std::vector<char> dest(size, 0);

        memcpy_func(dest.data(), src.data(), size);

        if (std::memcmp(src.data(), dest.data(), size) != 0) {
            report_mismatch(src.data(), dest.data(), size);
            ADD_FAILURE() << "Small size copy failed for " << func_name << " with size " << size;
        }
    }
}

// Test non-overlapping copy
TEST_P(MemcpyTest, NonOverlappingCopy) {
    auto [memcpy_func, func_name] = GetParam();
    test_non_overlapping_copy(memcpy_func, func_name);
}

// Test overlapping copy
TEST_P(MemcpyTest, OverlappingCopy) {
    auto [memcpy_func, func_name] = GetParam();
    test_overlapping_copy(memcpy_func, func_name);
}

// Instantiate the test suite for different memcpy implementations
INSTANTIATE_TEST_SUITE_P(
        MemcpyTests,
        MemcpyTest,
        ::testing::Values(
                std::make_pair(std::memcpy, "std::memcpy"),
                std::make_pair(omm::memcpy_avx2, "omm::memcpy_avx2"),
                std::make_pair(omm::memcpy, "omm::memcpy")
        )
);

// Conditionally compile AVX-512 tests
#ifdef __AVX512F__
INSTANTIATE_TEST_SUITE_P(
    MemcpyTestsAVX512,
    MemcpyTest,
    ::testing::Values(
        std::make_pair(omm::memcpy_avx512, "omm::memcpy_avx512")
    )
);
#endif

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}