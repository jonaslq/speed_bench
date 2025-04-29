# Enkel Makefile för C++-projekt
CXX = g++
CXXFLAGS = -O3 -march=native -std=c++20
CXXFLAGS_AVX = -O3 -march=native -mavx2 -std=c++20
TARGET = speed_bench
SRC = main.cpp

all: $(TARGET) $(TARGET)_avx

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

$(TARGET)_avx: $(SRC)
	$(CXX) $(CXXFLAGS_AVX) -o $(TARGET)_avx $(SRC)

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET)_avx