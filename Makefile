# Enkel Makefile för C++-projekt
CXX = g++
COMMON_FLAGS = -O3 -Wall -Wextra -std=c++23 -fopenmp
BASE_FLAGS = $(COMMON_FLAGS) -march=native
SSE_FLAGS = $(COMMON_FLAGS) -march=native -msse4.2
AVX2_FLAGS = $(COMMON_FLAGS) -march=native -mavx2
AVX512_FLAGS = $(COMMON_FLAGS) -march=native -mavx512f -mavx512dq -mavx512bw -mavx512vl

SRCS = main.cpp loops.cpp
TARGET = speed_bench

all: $(TARGET) $(TARGET)_sse $(TARGET)_avx2 $(TARGET)_avx512

$(TARGET): $(SRCS)
	$(CXX) $(BASE_FLAGS) -o $(TARGET) $(SRCS)

$(TARGET)_sse: $(SRCS)
	$(CXX) $(SSE_FLAGS) -o $@ $(SRCS)

$(TARGET)_avx2: $(SRCS)
	$(CXX) $(AVX2_FLAGS) -o $@ $(SRCS)

$(TARGET)_avx512: $(SRCS)
	$(CXX) $(AVX512_FLAGS) -o $@ $(SRCS)

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET)_sse $(TARGET)_avx2 $(TARGET)_avx512