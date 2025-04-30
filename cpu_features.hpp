#pragma once
#include <array>
#include <bitset>
#include <string>

class CPUFeatures {
public:
    static bool hasSSE() {
        return checkCPUFeature(7, 0);  // SSE bit
    }
    
    static bool hasAVX2() {
        return checkCPUFeature(28, 7); // AVX2 bit
    }
    
    static bool hasAVX512F() {
        return checkCPUFeature(16, 7); // AVX-512 Foundation
    }

private:
    static bool checkCPUFeature(int bit, int level) {
        std::array<int, 4> cpui;
        __cpuid(level, cpui[0], cpui[1], cpui[2], cpui[3]);
        return (cpui[2] & (1 << bit)) != 0;
    }
    
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