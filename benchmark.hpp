#pragma once
#include <string>
#include "loops.hpp"
#include "cpu_features.hpp"

namespace benchmark {

void generic_bench(int thread_count, const char* label, int seconds);
void simd_bench(int thread_count, const char* label, int seconds, bool use_sse, bool use_avx2, bool use_avx512);
void scaling_bench(const char* label, int seconds, bool use_sse = false, bool use_avx2 = false, bool use_avx512 = false);
std::string format_large_number(uint64_t number);
unsigned get_physical_cores_linux();

}