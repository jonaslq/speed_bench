# Simple Makefile for C++ project
CXX = g++
COMMON_FLAGS = -O3 -Wall -Wextra -std=c++23 -fopenmp
BASE_FLAGS = $(COMMON_FLAGS) -march=native
X86_64_FLAGS = $(COMMON_FLAGS) -march=x86-64 -mavx -mavx2 -mavx512f

SRCS = main.cpp loops.cpp
TARGET = speed_bench
X86_64_TARGET = $(TARGET)_x86_64

all: native x86_64

native: $(SRCS)
	$(CXX) $(BASE_FLAGS) -o $(TARGET) $(SRCS)

x86_64: $(SRCS)
	$(CXX) $(X86_64_FLAGS) -o $(X86_64_TARGET) $(SRCS)

.PHONY: clean all native x86_64 help
clean:
	rm -f $(TARGET) $(X86_64_TARGET)

help:
	@echo "Available targets:"
	@echo "  all        - Build both native and x86_64 versions"
	@echo "  native     - Build the native version of the benchmark"
	@echo "  x86_64     - Build the x86_64 version of the benchmark"
	@echo "  clean      - Remove all built binaries"