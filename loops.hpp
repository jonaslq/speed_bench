#pragma once

#include <array>
#include <cstdint>
#include <immintrin.h>

#if defined(__x86_64__) || defined(_M_X64)
#define ASM_OPTIMIZED_LOOP
#endif

// Standard optimization loop
void optimized_loop_work();

// SIMD implementations
void sse_loop_work();
void avx2_loop_work();
void avx512_loop_work();