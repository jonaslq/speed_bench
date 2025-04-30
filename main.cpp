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
#include "loops.hpp"

// Benchmark configuration constants
struct BenchmarkConfig {
    static constexpr int DEFAULT_BENCHMARK_SECONDS = 10;
    static constexpr int SCALING_BENCHMARK_SECONDS = 5;
    static constexpr uint64_t BATCH_SIZE = 1000;
    static constexpr uint64_t MAX_UINT32 = (1ULL << 32) - 1;
    
    // SIMD configurations
    static constexpr int SSE_COUNTERS = 4;
    static constexpr int AVX2_COUNTERS = 8;
    static constexpr int AVX512_COUNTERS = 16;
    
    static int get_counters_per_thread(bool use_avx512, bool use_avx2, bool use_sse) {
        return use_avx512 ? AVX512_COUNTERS : 
               (use_avx2 ? AVX2_COUNTERS : 
               (use_sse ? SSE_COUNTERS : 1));
    }
};

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
    const uint64_t max_uint32 = (1ULL << 32) - 1; // 2^32 - 1 via bitskift
    
    for (int i = 0; i < thread_count; ++i) {
        uint64_t thread_iterations = loops_completed[i] * (max_uint32 + 1);
        total_loops += loops_completed[i];
        total_iterations += thread_iterations;
    }
    double duration = std::chrono::duration<double>(end - start).count();
    std::cout << "[" << label << "] Benchmark finished. Time: " << duration << "s" << std::endl;
    std::cout << "[" << label << "] Total loops: " << total_loops << std::endl;
    std::cout << "[" << label << "] Total iterations: " << format_large_number(total_iterations) << std::endl;
    std::cout << "[" << label << "] Iterations per second: " << format_large_number(uint64_t(total_iterations / duration)) << std::endl;
}

void simd_bench(int thread_count, const char* label, int seconds, bool use_sse, bool use_avx2, bool use_avx512) {
    const char* simd_type = use_avx512 ? "AVX-512" : (use_avx2 ? "AVX2" : "SSE");
    int counters_per_thread = BenchmarkConfig::get_counters_per_thread(use_avx512, use_avx2, use_sse);
    
    std::cout << "\n[" << label << "] Starting " << simd_type << " benchmark with " 
              << thread_count << " threads (" << (thread_count * counters_per_thread) 
              << " parallel counters) for " << seconds << " seconds." << std::endl;
    
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    std::vector<uint64_t> loops_completed(thread_count, 0);
    
    try {
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, i, use_sse, use_avx2, use_avx512]() {
                uint64_t local_loops = 0;
                while (!stop_flag.load(std::memory_order_relaxed)) {
                    if (use_avx512) {
                        avx512_loop_work();
                    } else if (use_avx2) {
                        avx2_loop_work();
                    } else {
                        sse_loop_work();
                    }
                    ++local_loops;
                }
                loops_completed[i] = local_loops;
            });
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        stop_flag.store(true, std::memory_order_seq_cst);
        
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration<double>(end - start).count();
        
        uint64_t total_loops = 0;
        uint64_t total_iterations = 0;
        
        for (int i = 0; i < thread_count; ++i) {
            total_loops += loops_completed[i];
            uint64_t remaining_loops = loops_completed[i];
            
            while (remaining_loops > 0) {
                uint64_t current_batch = std::min(remaining_loops, BenchmarkConfig::BATCH_SIZE);
                uint64_t batch_iterations = current_batch * (BenchmarkConfig::MAX_UINT32 + 1ULL) * counters_per_thread;
                total_iterations += batch_iterations;
                remaining_loops -= current_batch;
            }
        }
        
        uint64_t iterations_per_second = uint64_t(double(total_iterations) / duration);
        
        std::cout << "[" << label << "] Benchmark finished. Time: " << std::fixed << std::setprecision(2) 
                  << duration << "s" << std::endl;
        std::cout << "[" << label << "] Total loops: " << total_loops << std::endl;
        std::cout << "[" << label << "] Total iterations: " << format_large_number(total_iterations) << std::endl;
        std::cout << "[" << label << "] Iterations per second: " << format_large_number(iterations_per_second) << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in benchmark: " << e.what() << std::endl;
        stop_flag.store(true, std::memory_order_seq_cst);
        throw;
    }
}

void scaling_bench(const char* label, int seconds, bool use_sse = false, bool use_avx2 = false, bool use_avx512 = false) {
    // Använd dynamisk skalning baserat på tillgängliga kärnor
    std::vector<unsigned int> thread_counts;
    
    // Beräkna maximalt antal trådar baserat på hårdvaran
    unsigned int max_threads = std::thread::hardware_concurrency();
    
    // Bygg thread_counts vektorn dynamiskt
    for (unsigned int threads = 1; threads <= max_threads; threads *= 2) {
        thread_counts.push_back(threads);
    }
    // Om sista värdet inte är max_threads, lägg till det
    if (thread_counts.back() != max_threads) {
        thread_counts.push_back(max_threads);
    }
    
    std::cout << "\n[" << label << "] Running benchmark..." << std::endl;
    std::cout << "Testing with " << seconds << " seconds per thread count" << std::endl;
    
    if (use_sse || use_avx2 || use_avx512) {
        const char* simd_type = use_avx512 ? "AVX-512" : (use_avx2 ? "AVX2" : "SSE");
        int counters_per_thread = use_avx512 ? 16 : (use_avx2 ? 8 : 4);
        std::cout << "Using " << simd_type << " (" << counters_per_thread << " counters per thread)" << std::endl;
    }
    
    std::cout << "\nFormat: threads → iterations/second (scaling factor)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    uint64_t base_iterations = 0;
    
    for (unsigned int threads : thread_counts) {
        if (use_sse || use_avx2 || use_avx512) {
            simd_bench(threads, "Scale", seconds, use_sse, use_avx2, use_avx512);
        } else {
            generic_bench(threads, "Scale", seconds);
        }
        
        if (base_iterations == 0) {
            base_iterations = threads;
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
    bool scaling_test = false;
    bool use_sse = false;
    bool use_avx2 = false;
    bool use_avx512 = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: ./speed_bench [singlepass] [--avx2|--avx512|--sse] [--scaling] [--max-cores=N]" << std::endl;
            std::cout << "  No argument: Run multi-threaded throughput benchmark (10s)" << std::endl;
            std::cout << "  singlepass : Each physical core/thread counts from 4294967295 to 0 once" << std::endl;
            std::cout << "  --avx2    : Use AVX2 instructions (8 counters per thread)" << std::endl;
            std::cout << "  --avx512  : Use AVX-512 instructions (16 counters per thread)" << std::endl;
            std::cout << "  --sse     : Use SSE instructions (4 counters per thread)" << std::endl;
            std::cout << "  --scaling : Run scaling test (1,2,4,8,16,24,32 threads)" << std::endl;
            std::cout << "  --max-cores=N : Use at most N threads/cores" << std::endl;
            std::cout << "  -h, --help : Show this help message" << std::endl;
            return 0;
        } else if (std::strncmp(argv[i], "--max-cores=", 12) == 0) {
            max_cores = std::atoi(argv[i] + 12);
            if (max_cores < 1) {
                std::cout << "illegal value for --max-cores. Use --help for info" << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--avx2") == 0) {
            if (use_avx512 || use_sse) {
                std::cout << "Error: Only one SIMD type can be specified" << std::endl;
                return 1;
            }
            use_avx2 = true;
        } else if (std::strcmp(argv[i], "--avx512") == 0) {
            if (use_avx2 || use_sse) {
                std::cout << "Error: Only one SIMD type can be specified" << std::endl;
                return 1;
            }
            use_avx512 = true;
        } else if (std::strcmp(argv[i], "--sse") == 0) {
            if (use_avx2 || use_avx512) {
                std::cout << "Error: Only one SIMD type can be specified" << std::endl;
                return 1;
            }
            use_sse = true;
        } else if (std::strcmp(argv[i], "--scaling") == 0) {
            scaling_test = true;
        } else if (std::strcmp(argv[i], "singlepass") != 0) {
            std::cout << "illegal argument. Use --help for info" << std::endl;
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

    // Adjust thread counts for SIMD modes
    int counters_per_thread = use_avx512 ? 16 : (use_avx2 ? 8 : (use_sse ? 4 : 1));
    if (use_sse || use_avx2) {
        physical = (physical + counters_per_thread - 1) / counters_per_thread;
        logical = (logical + counters_per_thread - 1) / counters_per_thread;
    }
    // För AVX-512 behåller vi det faktiska antalet kärnor som detekterades
    
    if (max_cores > 0) {
        if (singlepass_mode) {
            if ((unsigned)max_cores < physical) physical = max_cores;
        } else {
            if ((unsigned)max_cores < physical) physical = max_cores;
            if ((unsigned)max_cores < logical) logical = max_cores;
        }
    }

    std::cout << "C++ speed benchmark starting." << std::endl;
    
    if (use_sse || use_avx2 || use_avx512) {
        const char* simd_type = use_avx512 ? "AVX-512" : (use_avx2 ? "AVX2" : "SSE");
        std::cout << "Using " << simd_type << " SIMD instructions ("
                  << counters_per_thread << " parallel counters per thread)" << std::endl;
    }
    
    std::cout << "Detected " << (physical * counters_per_thread)
              << " physical and " << (logical * counters_per_thread)
              << " logical processing units." << std::endl;

    if (scaling_test) {
        if (use_sse || use_avx2 || use_avx512) {
            scaling_bench("Scaling Test", 5, use_sse, use_avx2, use_avx512);
        } else {
            scaling_bench("Scaling Test", 5);
        }
    } else if (singlepass_mode) {
        std::cout << "Running in singlepass mode (one loop per physical core, parallel)" << std::endl;
        std::cout << "Each thread processes " << counters_per_thread 
                  << " counters from " << uint32_t(-1) << " to 0 once." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < physical; ++i) {
            threads.emplace_back([use_sse, use_avx2, use_avx512]() { 
                if (use_avx512) {
                    avx512_loop_work();
                } else if (use_avx2) {
                    avx2_loop_work();
                } else if (use_sse) {
                    sse_loop_work();
                } else {
                    optimized_loop_work();
                }
            });
        }
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        uint64_t total_counts = uint64_t(physical) * (uint64_t(uint32_t(-1)) + 1) * counters_per_thread;
        std::cout << "[Singlepass] All threads completed one loop." << std::endl;
        std::cout << "[Singlepass] Time elapsed: " << duration << "s" << std::endl;
        std::cout << "[Singlepass] Counted " << format_large_number(total_counts)
                  << " times in " << duration << "s" << std::endl;
        std::cout << "[Singlepass] Iterations per second: " 
                  << format_large_number(uint64_t(double(total_counts) / duration)) << std::endl;
    } else {
        if (use_sse || use_avx2 || use_avx512) {
            if (use_avx512) {
                // För AVX-512 utan scaling, använd maximalt tillgängliga kärnor
                simd_bench(logical, "SIMD AVX-512", 10, false, false, true);
            } else if (logical > physical) {
                simd_bench(physical, "SIMD Physical Cores", 10, use_sse, use_avx2, use_avx512);
                simd_bench(logical, "SIMD All Cores", 10, use_sse, use_avx2, use_avx512);
            } else {
                simd_bench(logical, "SIMD Benchmark", 10, use_sse, use_avx2, use_avx512);
            }
        } else {
            if (logical > physical) {
                generic_bench(physical, "Physical Cores Only", 10);
                generic_bench(logical, "Logical + Physical Cores", 10);
            } else {
                generic_bench(logical, "Benchmark", 10);
            }
        }
    }
    return 0;
}
