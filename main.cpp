#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>

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

void debug_bench() {
    std::cout << "\n[Debug] Starting simple sequential benchmark..." << std::endl;
    uint64_t total_iterations = 2ull * (uint64_t(uint32_t(-1)) + 1);
    std::cout << "[Debug] Will count up to " << format_large_number(total_iterations) << " numbers" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "[Debug] Starting loop 1..." << std::endl;
    optimized_loop_work();
    auto mid = std::chrono::high_resolution_clock::now();
    std::cout << "[Debug] Loop 1 completed in " << std::chrono::duration<double>(mid - start).count() << "s" << std::endl;
    std::cout << "[Debug] Starting loop 2..." << std::endl;
    optimized_loop_work();
    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(end - start).count();
    double loop2_time = std::chrono::duration<double>(end - mid).count();
    std::cout << "[Debug] Loop 2 completed in " << loop2_time << "s" << std::endl;
    std::cout << "[Debug] Benchmark finished. Total time elapsed: " << total_time << "s" << std::endl;
    std::cout << "[Debug] Average time per loop: " << (total_time / 2) << "s" << std::endl;
    std::cout << "[Debug] Total numbers counted: " << format_large_number(total_iterations) << std::endl;
    std::cout << "[Debug] Numbers per second: " << std::scientific << (total_iterations / total_time) << std::endl;
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
                volatile uint32_t count = 0xFFFFFFFF;
#ifdef ASM_OPTIMIZED_LOOP
                asm volatile (
                    "1: sub $1, %[cnt]\n\t"
                    "jnz 1b\n\t"
                    : [cnt] "+r" (count)
                    :
                    : "cc"
                );
#else
                while (count != 0) {
                    --count;
                }
#endif
                local_final = count;
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
        if (i < 3) {
            std::cout << "   Thread " << i << ": " << loops_completed[i] << " loops, " << thread_iterations << " total" << std::endl;
        }
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
    bool singlepass_mode = argc > 1 && std::strcmp(argv[1], "singlepass") == 0;
    unsigned logical = std::thread::hardware_concurrency();
    unsigned physical = logical / 2 > 0 ? logical / 2 : 1; // Simple estimation
    std::cout << "C++ speed benchmark starting." << std::endl;
    std::cout << "Detected " << logical << " logical cores and ~" << physical << " physical cores." << std::endl;
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
        std::cout << "[Singlepass] Counted " << total_counts << " times in " << duration << "s" << std::endl;
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
