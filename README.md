# C++ Speed Benchmark

This project is a simple C++ benchmark tool that measures how fast your CPU can execute highly optimized counting loops, both single-threaded and multi-threaded. It uses inline assembly and SIMD intrinsics for maximum performance on x86_64 systems.

## Features

- Runs a tight counting loop in each thread to stress the CPU.
- Automatically detects logical and physical CPU cores.
- Reports total iterations, loops, and iterations per second.
- Supports multi-threaded benchmarking and a 'singlepass' mode for latency measurement.
- Includes support for SIMD instructions (SSE, AVX2, AVX-512).

## What the Benchmark Does

The benchmark measures the performance of your CPU by executing highly optimized counting loops. It uses SIMD instructions (SSE, AVX2, AVX-512) and multi-threading to stress the CPU and evaluate its throughput. Each thread processes counters in parallel, decrementing them from their maximum value (4294967295) to zero. The benchmark reports the total iterations, loops, and iterations per second, providing insights into the CPU's computational capabilities.

## Usage

1. Build the project:
   ```sh
   make
   ```
2. Run the benchmark (multi-threaded throughput test):
   ```sh
   ./speed_bench
   ```
3. Run the singlepass (latency) test, where each physical core counts from 4294967295 to 0 once:
   ```sh
   ./speed_bench singlepass
   ```
4. Run with specific SIMD instructions (e.g., AVX2):
   ```sh
   ./speed_bench --avx2
   ```
5. Run a scaling test to measure performance across different thread counts:
   ```sh
   ./speed_bench --scaling
   ```
6. Limit the number of cores used:
   ```sh
   ./speed_bench --max-cores=N
   ```

## Output

- **Default mode:** Prints the number of loops and iterations completed by each thread in 10 seconds, as well as the total iterations per second for your system.
- **Singlepass mode:** Each physical core/thread counts from 4294967295 to 0 once, and the program prints the total number of counts and the time taken for all threads to finish.
- **Scaling mode:** Measures performance across different thread counts and reports scaling factors.

## Platform

- Requires a C++23-compatible compiler (e.g., g++ 13+).
- Optimized for Linux/x86_64.
- Supports AVX-512, AVX2, and SSE instructions if available on the CPU.

## Notes

- Ensure that your CPU supports the SIMD instructions you intend to benchmark (e.g., AVX-512).
- Use the `--help` flag to see all available options:
  ```sh
  ./speed_bench --help
  ```
