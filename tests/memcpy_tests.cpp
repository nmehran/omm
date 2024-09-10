#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <random>
#include "omm/memcpy/memcpy.hpp"

class MemcpyTest : public ::testing::Test {
protected:
    std::vector<size_t> test_sizes;
    std::mt19937 gen;

    void SetUp() override {
        // Basic sizes
        test_sizes = {0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256};

        // Add some larger sizes
        for (size_t i = 9; i <= 20; ++i) {
            test_sizes.push_back(1ULL << i);  // 512, 1024, 2048, ..., 1MB
        }

        // Random number generator
        std::random_device rd;
        gen = std::mt19937(rd());
    }

    std::vector<char> generate_random_data(size_t size) {
        std::vector<char> data(size);
        std::uniform_int_distribution<> dis(0, 255);
        std::generate(data.begin(), data.end(), [&]() { return static_cast<char>(dis(gen)); });
        return data;
    }

    void test_memcpy_implementation(const std::function<void(void*, const void*, size_t)>& memcpy_func, const char* func_name) {
        for (size_t size : test_sizes) {
            SCOPED_TRACE("Size: " + std::to_string(size));

            auto src = generate_random_data(size);
            std::vector<char> dest(size);

            // Test the copy
            memcpy_func(dest.data(), src.data(), size);
            EXPECT_EQ(src, dest) << "Copy failed for " << func_name << " with size " << size;

            // Compare with std::memcpy
            std::vector<char> std_dest(size);
            std::memcpy(std_dest.data(), src.data(), size);
            EXPECT_EQ(std_dest, dest) << func_name << " differs from std::memcpy for size " << size;

            // Test unaligned source (if size > 1)
            if (size > 1) {
                dest.assign(size - 1, 0);
                memcpy_func(dest.data(), src.data() + 1, size - 1);
                EXPECT_EQ(0, std::memcmp(src.data() + 1, dest.data(), size - 1))
                                    << "Unaligned source copy failed for " << func_name << " with size " << (size - 1);
            }

            // Test unaligned destination (if size > 1)
            if (size > 1) {
                dest.assign(size, 0);
                memcpy_func(dest.data() + 1, src.data(), size - 1);
                EXPECT_EQ(0, std::memcmp(src.data(), dest.data() + 1, size - 1))
                                    << "Unaligned destination copy failed for " << func_name << " with size " << (size - 1);
            }
        }
    }
};

TEST_F(MemcpyTest, StandardMemcpy) {
    test_memcpy_implementation(omm::memcpy_standard, "omm::memcpy_standard");
}

TEST_F(MemcpyTest, AVX2Memcpy) {
    test_memcpy_implementation(omm::memcpy_avx2, "omm::memcpy_avx2");
}

#ifdef __AVX512F__
TEST_F(MemcpyTest, AVX512Memcpy) {
    test_memcpy_implementation(omm::memcpy_avx512, "omm::memcpy_avx512");
}
#endif

TEST_F(MemcpyTest, AutoMemcpy) {
    test_memcpy_implementation(omm::memcpy_auto, "omm::memcpy_auto");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}