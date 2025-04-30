#pragma once
#include <array>
#include <bitset>
#include <string>

class CPUFeatures {
public:
    static bool hasSSE() {
        std::array<int, 4> cpui;
        __cpuid(1, cpui[0], cpui[1], cpui[2], cpui[3]);
        return (cpui[3] & (1 << 25)) != 0;  // Check EDX bit 25 for SSE
    }
    
    static bool hasAVX2() {
        std::array<int, 4> cpui;
        __cpuid(7, cpui[0], cpui[1], cpui[2], cpui[3]);
        return (cpui[1] & (1 << 5)) != 0;   // Check EBX bit 5 for AVX2
    }
    
    static bool hasAVX512F() {
        std::array<int, 4> cpui;
        __cpuid(7, cpui[0], cpui[1], cpui[2], cpui[3]);
        return (cpui[1] & (1 << 16)) != 0;  // Check EBX bit 16 for AVX-512F
    }

private:
    static void __cpuid(int level, int& a, int& b, int& c, int& d) {
        #if defined(__x86_64__) || defined(_M_X64)
            asm volatile("cpuid"
                : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                : "a"(level), "c"(0)
                : );
        #else
            a = b = c = d = 0;
        #endif
    }
};