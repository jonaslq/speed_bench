# C++ Speed Benchmark

This project is a simple C++ benchmark tool that measures how fast your CPU can execute a highly optimized counting loop, both single-threaded and multi-threaded. It uses inline assembly for maximum performance on x86_64 systems.

## Features

- Runs a tight counting loop in each thread to stress the CPU.
- Automatically detects logical and physical CPU cores.
- Reports total iterations, loops, and iterations per second.
- Supports both multi-threaded benchmarking and a 'singlepass' mode for latency measurement.

## Usage

1. Build the project:
   ```sh
   make
   ```
2. Run the benchmark (multi-threaded throughput test):
   ```sh
   ./speed_bench
   ```
   Run the singlepass (latency) test, where each physical core counts from 4294967295 to 0 once:
   ```sh
   ./speed_bench singlepass
   ```

## Output

- **Default mode:** Prints the number of loops and iterations completed by each thread in 10 seconds, as well as the total iterations per second for your system.
- **Singlepass mode:** Each physical core/thread counts from 4294967295 to 0 once, and the program prints the total number of counts and the time taken for all threads to finish.

## Platform

- Requires a C++20-compatible compiler (e.g. g++ 10+)
- Optimized for Linux/x86_64
