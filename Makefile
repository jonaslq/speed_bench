# Enkel Makefile för C++-projekt
CXX = g++
COMMON_FLAGS = -O3 -Wall -Wextra -std=c++23 -fopenmp
BASE_FLAGS = $(COMMON_FLAGS) -march=native

SRCS = main.cpp loops.cpp
TARGET = speed_bench

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(BASE_FLAGS) -o $(TARGET) $(SRCS)


.PHONY: clean
clean:
	rm -f $(TARGET)