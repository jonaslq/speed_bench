#include "loops.hpp"

// Standard optimization loop
void optimized_loop_work() {
#ifdef ASM_OPTIMIZED_LOOP
    volatile uint32_t count = 0xFFFFFFFF;
    asm volatile (
        "1: sub $1, %[cnt]\n\t"
        "jnz 1b\n\t"
        : [cnt] "+r" (count)
        :
        : "cc"
    );
#else
    volatile uint32_t count = 0xFFFFFFFF;
    while (count != 0) {
        --count;
    }
#endif
}

// SSE version - processes 4 counters in parallel
void sse_loop_work() {
    alignas(16) volatile uint32_t count[4] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };
    
    asm volatile (
        "movdqa (%[ptr]), %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"      // Zero register för jämförelse
        "1:\n\t"
        "psubd %[one], %%xmm0\n\t"     // Subtrahera 1 från varje element
        "movdqa %%xmm0, (%[ptr])\n\t"  // Spara tillbaka för volatile
        "pcmpeqd %%xmm1, %%xmm0\n\t"   // Jämför med noll
        "pmovmskb %%xmm0, %%eax\n\t"   // Få jämförelsemask
        "cmpl $0xffff, %%eax\n\t"      // Kolla om alla är noll
        "jne 1b\n\t"                    // Fortsätt om inte alla är noll
        : 
        : [ptr] "r" (count),
          [one] "m" (*count)  // Använd minnesoperand för att förhindra optimering
        : "xmm0", "xmm1", "eax", "memory", "cc"
    );
}

// AVX2 version - processes 8 counters in parallel
void avx2_loop_work() {
    alignas(32) volatile uint32_t count[8] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };
    
    asm volatile (
        "vmovdqa (%[ptr]), %%ymm0\n\t"
        "vpxor %%ymm1, %%ymm1, %%ymm1\n\t"  // Zero register för jämförelse
        "1:\n\t"
        "vpsubd %[one], %%ymm0, %%ymm0\n\t" // Subtrahera 1 från varje element
        "vmovdqa %%ymm0, (%[ptr])\n\t"      // Spara tillbaka för volatile
        "vpcmpeqd %%ymm1, %%ymm0, %%ymm2\n\t" // Jämför med noll
        "vpmovmskb %%ymm2, %%eax\n\t"       // Få jämförelsemask
        "cmpl $0xffffffff, %%eax\n\t"       // Kolla om alla är noll
        "jne 1b\n\t"                         // Fortsätt om inte alla är noll
        "vzeroupper\n\t"
        :
        : [ptr] "r" (count),
          [one] "m" (*count)  // Använd minnesoperand för att förhindra optimering
        : "ymm0", "ymm1", "ymm2", "eax", "memory", "cc"
    );
}

// AVX-512 version - processes 16 counters in parallel
void avx512_loop_work() {
    alignas(64) volatile uint32_t count[16] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };
    
    asm volatile (
        "vmovdqa64 (%[ptr]), %%zmm0\n\t"
        "vpxord %%zmm1, %%zmm1, %%zmm1\n\t"  // Zero register för jämförelse
        "1:\n\t"
        "vpsubd %[one], %%zmm0, %%zmm0\n\t"  // Subtrahera 1 från varje element
        "vmovdqa64 %%zmm0, (%[ptr])\n\t"     // Spara tillbaka för volatile
        "vpcmpeqd %%zmm1, %%zmm0, %%k1\n\t"  // Jämför med noll -> mask
        "kmovw %%k1, %%eax\n\t"              // Flytta mask till GPR
        "cmpl $0xffff, %%eax\n\t"            // Kolla om alla är noll
        "jne 1b\n\t"                         // Fortsätt om inte alla är noll
        :
        : [ptr] "r" (count),
          [one] "m" (*count)  // Använd minnesoperand för att förhindra optimering
        : "zmm0", "zmm1", "k1", "eax", "memory", "cc"
    );
}