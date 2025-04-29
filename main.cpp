#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <set>
#include <fstream>
#include <cstdlib>
#include <immintrin.h>
#include <cpuid.h>

#if defined(__x86_64__) || defined(_M_X64)
#define ASM_OPTIMIZED_LOOP
#endif

std::string format_large_number(uint64_t number) {
    std::ostringstream oss;
    if (number >= 1000000000) {
        oss << std::fixed << std::setprecision(1) << (number / 1e9) << " billion";
    } else if (number >= 1000000) {
        oss << std::fixed << std::setprecision(1) << (number / 1e6) << " million";
    } else if (number >= 1000) {
        oss << std::fixed << std::setprecision(1) << (number / 1e3) << " thousand";
    } else {
        oss << number;
    }
    return oss.str();
}

// Optimized loop with inline assembler (x86_64)
void optimized_loop_work() {
#ifdef ASM_OPTIMIZED_LOOP
    volatile uint32_t count = 0xFFFFFFFF;
    asm volatile (
        "1: sub $1, %[cnt]\n\t"
        "jnz 1b\n\t"
        : [cnt] "+r" (count)
        :
        : "cc"
    );
#else
    volatile uint32_t count = 0xFFFFFFFF;
    while (count != 0) {
        --count;
    }
#endif
}

enum class SimdType {
    NONE,
    SSE42,
    AVX2,
    AVX512
};

SimdType check_simd_support() {
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    unsigned int eax7 = 0, ebx7 = 0, ecx7 = 0, edx7 = 0;
    
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return SimdType::NONE;
    }
    
    // Check SSE4.2 first (bit 20 in ECX)
    if (!(ecx & bit_SSE4_2)) {
        return SimdType::NONE;
    }
    
    // For AVX, we need both the AVX bit (bit 28 in ECX) 
    // and the OSXSAVE bit (bit 27 in ECX) to be set
    bool has_avx = (ecx & (bit_AVX | bit_OSXSAVE)) == (bit_AVX | bit_OSXSAVE);
    
    if (!has_avx) {
        return SimdType::SSE42;
    }
    
    // Get CPU features from leaf 7
    __cpuid_count(7, 0, eax7, ebx7, ecx7, edx7);
    
    // Check AVX512F (bit 16), AVX512DQ (bit 17), AVX512BW (bit 30), AVX512VL (bit 31) in EBX
    if ((ebx7 & bit_AVX512F) && (ebx7 & bit_AVX512DQ) && 
        (ebx7 & bit_AVX512BW) && (ebx7 & bit_AVX512VL)) {
        return SimdType::AVX512;
    }
    
    // Check AVX2 (bit 5 in EBX from leaf 7)
    if (ebx7 & bit_AVX2) {
        return SimdType::AVX2;
    }
    
    return SimdType::SSE42;
}

// Number of 32-bit integers processed per SIMD operation
constexpr int SSE_INT32_COUNT = 4;    // 128-bit
constexpr int AVX2_INT32_COUNT = 8;   // 256-bit
constexpr int AVX512_INT32_COUNT = 16; // 512-bit

void simd_loop_work_sse42() {
    alignas(16) volatile __m128i count = _mm_set1_epi32(0xFFFFFFFF);
    const __m128i one = _mm_set1_epi32(1);
    const __m128i zero = _mm_setzero_si128();
    
    while (_mm_movemask_epi8(_mm_cmpeq_epi32(count, zero)) != 0xFFFF) {
        count = _mm_sub_epi32(count, one);
    }
}

void simd_loop_work_avx2() {
    alignas(32) volatile __m256i count = _mm256_set1_epi32(0xFFFFFFFF);
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i zero = _mm256_setzero_si256();
    
    while (_mm256_movemask_epi8(_mm256_cmpeq_epi32(count, zero)) != -1) {
        count = _mm256_sub_epi32(count, one);
    }
}

void simd_loop_work_avx512() {
    alignas(64) volatile __m512i count = _mm512_set1_epi32(0xFFFFFFFF);
    const __m512i one = _mm512_set1_epi32(1);
    const __m512i zero = _mm512_setzero_si512();
    __mmask16 mask;
    
    do {
        count = _mm512_sub_epi32(count, one);
        mask = _mm512_cmpeq_epi32_mask(count, zero);
    } while (mask != 0xFFFF);
}

void simd_loop_work_avx2_asm() {
    alignas(32) __m256i count = _mm256_set1_epi32(0xFFFFFFFF);
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i zero = _mm256_setzero_si256();
    int mask;
    
    do {
        count = _mm256_sub_epi32(count, one);
        mask = _mm256_movemask_epi8(_mm256_cmpeq_epi32(count, zero));
    } while (mask != -1);
}

void simd_loop_work_avx512_asm() {
    alignas(64) __m512i count = _mm512_set1_epi32(0xFFFFFFFF);
    const __m512i one = _mm512_set1_epi32(1);
    __mmask16 mask;
    
    do {
        count = _mm512_sub_epi32(count, one);
        mask = _mm512_cmpeq_epi32_mask(count, _mm512_setzero_si512());
    } while (mask != 0xFFFF);
}

void simd_bench(int thread_count, const char* label, int seconds, SimdType simd_type) {
    int counters_per_thread = 
        (simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
        (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
        (simd_type == SimdType::SSE42) ? SSE_INT32_COUNT : 1;
    
    const char* simd_name = 
        (simd_type == SimdType::AVX512) ? "AVX-512" :
        (simd_type == SimdType::AVX2) ? "AVX2" :
        "SSE4.2";
    
    std::cout << "\n[" << label << "] Starting " << simd_name
              << " benchmark with " << thread_count 
              << " threads (" << (thread_count * counters_per_thread) 
              << " parallel counters) for " << seconds << " seconds." << std::endl;
    
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;
    std::vector<uint64_t> loops_completed(thread_count, 0);
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i, simd_type]() {
            uint64_t local_loops = 0;
            while (!stop_flag.load(std::memory_order_relaxed)) {
                switch (simd_type) {
                    case SimdType::AVX512:
                        simd_loop_work_avx512_asm(); // Using assembly version
                        break;
                    case SimdType::AVX2:
                        simd_loop_work_avx2_asm();   // Using assembly version
                        break;
                    default:
                        simd_loop_work_sse42();
                        break;
                }
                ++local_loops;
            }
            loops_completed[i] = local_loops;
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop_flag.store(true, std::memory_order_seq_cst);
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    uint64_t total_loops = 0;
    double total_iterations = 0.0;
    const double max_uint32 = double(uint64_t(1) << 32);  // 2^32, equals uint32_t(-1) + 1
    
    for (int i = 0; i < thread_count; ++i) {
        uint64_t thread_loops = loops_completed[i];
        total_loops += thread_loops;
        // Use double arithmetic to avoid overflow
        total_iterations += double(thread_loops) * max_uint32 * double(counters_per_thread);
    }
    
    double duration = std::chrono::duration<double>(end - start).count();
    double iterations_per_second = total_iterations / duration;
    
    std::cout << "[" << label << "] Benchmark finished. Time: " << duration << "s" << std::endl;
    std::cout << "[" << label << "] Total loops: " << total_loops << std::endl;
    std::cout << "[" << label << "] Total iterations: " << std::scientific 
              << std::setprecision(3) << total_iterations << std::endl;
    std::cout << "[" << label << "] Iterations per second: " << std::scientific 
              << std::setprecision(3) << iterations_per_second << std::endl;
}

unsigned get_physical_cores_linux() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo) return 0;
    std::set<std::pair<int, int>> core_ids;
    std::string line;
    int physical_id = -1, core_id = -1;
    while (std::getline(cpuinfo, line)) {
        if (line.find("physical id") != std::string::npos) {
            physical_id = std::stoi(line.substr(line.find(":") + 1));
        } else if (line.find("core id") != std::string::npos) {
            core_id = std::stoi(line.substr(line.find(":") + 1));
        } else if (line.empty() && physical_id != -1 && core_id != -1) {
            core_ids.emplace(physical_id, core_id);
            physical_id = core_id = -1;
        }
    }
    // For last processor
    if (physical_id != -1 && core_id != -1) {
        core_ids.emplace(physical_id, core_id);
    }
    return core_ids.empty() ? 0 : core_ids.size();
}

void generic_bench(int thread_count, const char* label, int seconds) {
    std::cout << "\n[" << label << "] Starting benchmark with " << thread_count << " threads for " << seconds << " seconds." << std::endl;
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;
    std::vector<uint64_t> loops_completed(thread_count, 0);
    std::vector<uint32_t> final_count(thread_count, 0);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            uint64_t local_loops = 0;
            uint32_t local_final = 0;
            while (!stop_flag.load(std::memory_order_relaxed)) {
                optimized_loop_work();
                local_final = 0; // always zero after loop
                ++local_loops;
            }
            loops_completed[i] = local_loops;
            final_count[i] = local_final;
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop_flag.store(true, std::memory_order_seq_cst);
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t total_loops = 0, total_iterations = 0;
    for (int i = 0; i < thread_count; ++i) {
        uint64_t thread_iterations = loops_completed[i] * (uint64_t(uint32_t(-1)) + 1);
        total_loops += loops_completed[i];
        total_iterations += thread_iterations;
    }
    double duration = std::chrono::duration<double>(end - start).count();
    std::cout << "[" << label << "] Benchmark finished. Time: " << duration << "s" << std::endl;
    std::cout << "[" << label << "] Total loops: " << total_loops << std::endl;
    std::cout << "[" << label << "] Total iterations: " << format_large_number(total_iterations) << std::endl;
    std::cout << "[" << label << "] Iterations per second: " << format_large_number(uint64_t(total_iterations / duration)) << std::endl;
}

void singlepass_bench(int thread_count) {
    std::cout << "\n[Singlepass] Running one loop per physical core (" << thread_count << " threads)..." << std::endl;
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([]() {
            optimized_loop_work();
        });
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    std::cout << "[Singlepass] All threads completed one loop." << std::endl;
    std::cout << "[Singlepass] Time elapsed: " << duration << "s" << std::endl;
}

void scaling_bench(const char* label, int seconds, bool use_simd, SimdType simd_type) {
    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 24, 32};
    
    std::cout << "\n[" << label << "] Running scaling benchmark..." << std::endl;
    std::cout << "Testing with " << seconds << " seconds per thread count" << std::endl;
    
    if (use_simd) {
        const char* simd_name = 
            (simd_type == SimdType::AVX512) ? "AVX-512" :
            (simd_type == SimdType::AVX2) ? "AVX2" :
            "SSE4.2";
        int counters_per_thread = 
            (simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
            (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
            SSE_INT32_COUNT;
        std::cout << "Using " << simd_name << " assembly implementation ("
                  << counters_per_thread << " counters per thread)" << std::endl;
    }
    
    std::cout << "\nFormat: threads → iterations/second (scaling factor)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    uint64_t base_iterations = 0;
    
    for (int threads : thread_counts) {
        if (use_simd) {
            simd_bench(threads, "Scale", seconds, simd_type);
        } else {
            generic_bench(threads, "Scale", seconds);
        }
        
        // Extract iterations/second from last line of output for scaling calculation
        if (base_iterations == 0) {
            base_iterations = threads; // First run is our baseline
        } else {
            double scaling = double(threads) / base_iterations;
            std::cout << "Scaling factor vs single thread: " << std::fixed 
                      << std::setprecision(2) << scaling << "x" << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    int max_cores = -1;
    bool simd_mode = false;
    bool scaling_test = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: ./speed_bench [singlepass] [--simd] [--scaling] [--max-cores=N]" << std::endl;
            std::cout << "  No argument: Run multi-threaded throughput benchmark (10s)" << std::endl;
            std::cout << "  singlepass : Each physical core/thread counts from 4294967295 to 0 once" << std::endl;
            std::cout << "  --simd    : Use SIMD instructions (AVX-512, AVX2, or SSE4.2)" << std::endl;
            std::cout << "  --scaling : Run scaling test (1,2,4,8,16 threads)" << std::endl;
            std::cout << "  --max-cores=N : Use at most N threads/cores" << std::endl;
            std::cout << "  -h, --help : Show this help message" << std::endl;
            return 0;
        } else if (std::strncmp(argv[i], "--max-cores=", 12) == 0) {
            max_cores = std::atoi(argv[i] + 12);
            if (max_cores < 1) {
                std::cout << "illegal value for --max-cores. Use --help for info" << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--simd") == 0) {
            simd_mode = true;
        } else if (std::strcmp(argv[i], "--scaling") == 0) {
            scaling_test = true;
        } else if (std::strcmp(argv[i], "singlepass") != 0) {
            std::cout << "illegal argument. Use --help for info" << std::endl;
            return 1;
        }
    }
    
    // Check for SIMD support early if requested
    SimdType simd_type = SimdType::NONE;
    if (simd_mode) {
        simd_type = check_simd_support();
        if (simd_type == SimdType::NONE) {
            std::cout << "Error: No SIMD support detected on this CPU (requires at least SSE4.2)" << std::endl;
            return 1;
        }
    }
    
    bool singlepass_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "singlepass") == 0) {
            singlepass_mode = true;
            break;
        }
    }
    
    unsigned logical = std::thread::hardware_concurrency();
    unsigned physical = 1;
#if defined(__linux__)
    physical = get_physical_cores_linux();
    if (physical == 0) {
        physical = logical / 2 > 0 ? logical / 2 : 1;
    }
#else
    physical = logical / 2 > 0 ? logical / 2 : 1;
#endif

    // Justera antalet trådar för SIMD baserat på instruktionsset
    if (simd_mode) {
        int counters_per_thread = 
            (simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
            (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
            SSE_INT32_COUNT;
            
        physical = (physical + counters_per_thread - 1) / counters_per_thread;
        logical = (logical + counters_per_thread - 1) / counters_per_thread;
    }

    if (max_cores > 0) {
        if (singlepass_mode) {
            if ((unsigned)max_cores < physical) physical = max_cores;
        } else {
            if ((unsigned)max_cores < physical) physical = max_cores;
            if ((unsigned)max_cores < logical) logical = max_cores;
        }
    }

    std::cout << "C++ speed benchmark starting." << std::endl;
    if (simd_mode) {
        const char* simd_name = 
            (simd_type == SimdType::AVX512) ? "AVX-512" :
            (simd_type == SimdType::AVX2) ? "AVX2" :
            "SSE4.2";
        
        int counters_per_thread = 
            (simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
            (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
            SSE_INT32_COUNT;
            
        std::cout << "Using " << simd_name << " SIMD instructions ("
                  << counters_per_thread << " parallel counters per thread)" << std::endl;
    }
    
    int counters_per_thread = simd_mode ? 
        ((simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
         (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
         SSE_INT32_COUNT) : 1;
    
    std::cout << "Detected " << (physical * counters_per_thread)
              << " physical and " << (logical * counters_per_thread)
              << " logical processing units." << std::endl;

    if (scaling_test) {
        scaling_bench("Scaling Test", 5, simd_mode, simd_type); // Using 5 seconds per test to keep total time reasonable
    } else if (singlepass_mode) {
        std::cout << "Running in singlepass mode (one loop per physical core, parallel)" << std::endl;
        std::cout << "Each thread counts from " << uint32_t(-1) << " to 0 once." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < physical; ++i) {
            threads.emplace_back([simd_mode, simd_type]() { 
                if (simd_mode) {
                    switch (simd_type) {
                        case SimdType::AVX512:
                            simd_loop_work_avx512();
                            break;
                        case SimdType::AVX2:
                            simd_loop_work_avx2();
                            break;
                        default:
                            simd_loop_work_sse42();
                            break;
                    }
                } else {
                    optimized_loop_work(); 
                }
            });
        }
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        uint64_t total_counts = uint64_t(physical) * (uint64_t(uint32_t(-1)) + 1);
        if (simd_mode) {
            total_counts *= 
                (simd_type == SimdType::AVX512) ? AVX512_INT32_COUNT :
                (simd_type == SimdType::AVX2) ? AVX2_INT32_COUNT :
                SSE_INT32_COUNT;
        }
        std::cout << "[Singlepass] All threads completed one loop." << std::endl;
        std::cout << "[Singlepass] Time elapsed: " << duration << "s" << std::endl;
        std::cout << "[Singlepass] Counted " << total_counts << " (That's roughly " 
                  << format_large_number(total_counts) << ") times in " << duration << "s" << std::endl;
    } else if (simd_mode) {
        if (logical > physical) {
            simd_bench(physical, "SIMD Physical Cores", 10, simd_type);
            simd_bench(logical, "SIMD All Cores", 10, simd_type);
        } else {
            simd_bench(logical, "SIMD Benchmark", 10, simd_type);
        }
    } else {
        if (logical > physical) {
            generic_bench(physical, "Physical Cores Only", 10);
            generic_bench(logical, "Logical + Physical Cores", 10);
        } else {
            generic_bench(logical, "Benchmark", 10);
        }
    }
    return 0;
}
