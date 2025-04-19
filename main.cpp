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

int main(int argc, char* argv[]) {
    int max_cores = -1;
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: ./speed_bench [singlepass] [--max-cores=N]" << std::endl;
            std::cout << "  No argument: Run multi-threaded throughput benchmark (10s)" << std::endl;
            std::cout << "  singlepass : Each physical core/thread counts from 4294967295 to 0 once" << std::endl;
            std::cout << "  --max-cores=N : Use at most N threads/cores" << std::endl;
            std::cout << "  -h, --help : Show this help message" << std::endl;
            return 0;
        } else if (std::strncmp(argv[i], "--max-cores=", 12) == 0) {
            max_cores = std::atoi(argv[i] + 12);
            if (max_cores < 1) {
                std::cout << "illegal value for --max-cores. Use --help for info" << std::endl;
                return 1;
            }
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
    if (max_cores > 0) {
        if (singlepass_mode) {
            if ((unsigned)max_cores < physical) physical = max_cores;
        } else {
            if ((unsigned)max_cores < physical) physical = max_cores;
            if ((unsigned)max_cores < logical) logical = max_cores;
        }
    }
    std::cout << "C++ speed benchmark starting." << std::endl;
    std::cout << "Detected " << physical << " physical cores and " << logical << " logical cores." << std::endl;
    if (singlepass_mode) {
        std::cout << "Running in singlepass mode (one loop per physical core, parallel)" << std::endl;
        std::cout << "Each thread counts from " << uint32_t(-1) << " to 0 once." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < physical; ++i) {
            threads.emplace_back([]() { optimized_loop_work(); });
        }
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        uint64_t total_counts = uint64_t(physical) * (uint64_t(uint32_t(-1)) + 1);
        std::cout << "[Singlepass] All threads completed one loop." << std::endl;
        std::cout << "[Singlepass] Time elapsed: " << duration << "s" << std::endl;
        std::cout << "[Singlepass] Counted " << total_counts << " (That's roughly " << format_large_number(total_counts) << ") times in " << duration << "s" << std::endl;
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
