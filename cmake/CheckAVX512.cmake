include(CheckCXXSourceRuns)

set(AVX512_CODE
        "#include <immintrin.h>
    int main() {
        __m512i a = _mm512_set1_epi32(1);
        __m512i b = _mm512_set1_epi32(2);
        __m512i c = _mm512_add_epi32(a, b);
        return _mm512_extract_epi32(c, 0) == 3 ? 0 : 1;
    }")

set(CMAKE_REQUIRED_FLAGS "-mavx512f")
check_cxx_source_runs("${AVX512_CODE}" AVX512_SUPPORTED)
set(CMAKE_REQUIRED_FLAGS "")