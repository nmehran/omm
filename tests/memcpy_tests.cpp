#include <gtest/gtest.h>
#include <random>
#include <memory>
#include <cstring>
#include "omm/memcpy/memcpy.hpp"

class MemcpyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up any common resources for tests
    }

    void TearDown() override {
        // Clean up any common resources after tests
    }

    bool test_memcpy_implementation(std::function<void(void*, const void*, size_t)> memcpy_func, const char* func_name) {
        const size_t sizes[] = {1, 15, 16, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024, 1025, 2047, 2048, 2049, 4095, 4096, 4097, 8191, 8192, 8193, 16383, 16384, 16385};

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t size : sizes) {
            auto src = std::make_unique<char[]>(size);
            auto dest = std::make_unique<char[]>(size);

            // Fill source with random data
            for (size_t i = 0; i < size; ++i) {
                src[i] = static_cast<char>(dis(gen));
            }

            // Clear destination
            std::memset(dest.get(), 0, size);

            // Perform the copy
            memcpy_func(dest.get(), src.get(), size);

            // Verify the copy
            if (std::memcmp(src.get(), dest.get(), size) != 0) {
                std::cout << "Error in " << func_name << " for size " << size << std::endl;
                return false;
            }
        }

        return true;
    }
};

TEST_F(MemcpyTest, StandardMemcpy) {
EXPECT_TRUE(test_memcpy_implementation(std::memcpy, "std::memcpy"));
}

TEST_F(MemcpyTest, AVX2Memcpy) {
EXPECT_TRUE(test_memcpy_implementation(omm::memcpy_avx2, "omm::memcpy_avx2"));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}