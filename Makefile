# Enkel Makefile för C++-projekt
CXX = g++
CXXFLAGS = -O3 -march=native -std=c++20
TARGET = speed_bench
SRC = main.cpp

all: $(TARGET) speed_bench_x86_64

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

speed_bench_x86_64: $(SRC)
	$(CXX) -O3 -march=x86-64 -std=c++20 -o speed_bench_x86_64 $(SRC)

clean:
	rm -f $(TARGET) speed_bench_x86_64