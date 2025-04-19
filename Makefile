# Enkel Makefile för C++-projekt
CXX = g++
CXXFLAGS = -O3 -march=native -std=c++20
TARGET = speed_bench
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)