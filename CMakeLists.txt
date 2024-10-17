cmake_minimum_required(VERSION 3.15...3.30.3)

project(OMM VERSION 1.0.0 LANGUAGES CXX)

# Basic C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Build Configuration ===

# Define the main library as an INTERFACE library
add_library(omm INTERFACE)

# Set include directories for the INTERFACE library
target_include_directories(omm INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Specify C++ standard features
target_compile_features(omm INTERFACE cxx_std_20)

# Set optimization flags for Release configuration
target_compile_options(omm INTERFACE
        $<$<CONFIG:Release>:
        -O3                           # Optimize for speed
        -mtune=native                 # Tune for native architecture, but don't limit instruction set
        -flto                         # Enable link-time optimization
        -funroll-loops                # Unroll loops for performance
        -fomit-frame-pointer          # Omit frame pointer for optimized code
        -fipa-pta                     # Inter-procedural pointer aliasing analysis
        -fno-rtti                     # Disable runtime type information
        -ffast-math                   # Enable fast math optimizations
        -fstrict-aliasing             # Enforce strict aliasing rules
        -fvisibility=hidden           # Reduce symbol visibility
        -falign-loops=64              # Align loops to 64-byte boundaries
        -pipe                         # Use pipes for communication between stages
        -fopt-info-vec                # Provide vectorization optimization info
        -mavx2                        # Support AVX2
        >
)

# Enable AVX-512 if supported
if(AVX512_SUPPORTED)
    message(STATUS "AVX-512 is supported and will be enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512vl")
    target_compile_definitions(omm INTERFACE AVX512_SUPPORTED)
else()
    message(STATUS "AVX-512 is not supported and will not be enabled")
endif()

# Set CMake policy to handle new optimizations
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

# Enable IPO/LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO/LTO not supported: ${error}")
endif()

# Options for building tests and benchmarks
option(OMM_BUILD_TESTS "Build OMM tests" OFF)
option(OMM_BUILD_BENCHMARKS "Build OMM benchmarks" OFF)

include(FetchContent)

# === Example ===

# Auto-detect example sources
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

# Create example executables
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE omm)
endforeach()

# === Tests ===

if(OMM_BUILD_TESTS)
    message(STATUS "Building tests")

    enable_testing()

    # Google Test
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Auto-detect test sources
    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    # Create test executables
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE omm GTest::gtest_main)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
else ()
    message(STATUS "Not building tests")
endif()

# === Benchmarks ===

if(OMM_BUILD_BENCHMARKS)
    message(STATUS "Building benchmarks")

    # Google Benchmark
    FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.0
    )
    FetchContent_MakeAvailable(benchmark)

    # Auto-detect benchmark sources
    file(GLOB BENCHMARK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp")

    # Create benchmark executables
    foreach(BENCHMARK_SOURCE ${BENCHMARK_SOURCES})
        get_filename_component(BENCHMARK_NAME ${BENCHMARK_SOURCE} NAME_WE)
        add_executable(${BENCHMARK_NAME} ${BENCHMARK_SOURCE})
        target_link_libraries(${BENCHMARK_NAME} PRIVATE
                omm
                benchmark::benchmark
                benchmark::benchmark_main
        )
        target_include_directories(${BENCHMARK_NAME} PRIVATE ${benchmark_INCLUDE_DIRS})
    endforeach()
else ()
    message(STATUS "Not building benchmarks")
endif()

# === Installation ===

include(GNUInstallDirs)
install(TARGETS omm
        EXPORT omm_targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/omm DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate and install package configuration files
include(CMakePackageConfigHelpers)
configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OMMConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/OMMConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OMM
)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/OMMConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/OMMConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/OMMConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OMM
)

# Export targets
install(EXPORT omm_targets
        FILE OMMTargets.cmake
        NAMESPACE OMM::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OMM
)
