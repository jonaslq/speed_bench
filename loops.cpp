#include "loops.hpp"
#include "cpu_features.hpp"
#include <stdexcept>

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

// SSE version - processes 4 counters in parallel using intrinsics
void sse_loop_work() {
    CPUFeatures features;
    if (!features.hasSSE()) {
        throw std::runtime_error("CPU does not support SSE instructions");
    }

    alignas(16) volatile uint32_t count[4] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };

    __m128i vec = _mm_load_si128((__m128i const*)const_cast<uint32_t*>(count));
    const __m128i one = _mm_set1_epi32(1);
    const __m128i zero = _mm_setzero_si128();

    do {
        vec = _mm_sub_epi32(vec, one);
        _mm_store_si128((__m128i*)const_cast<uint32_t*>(count), vec);
        std::atomic_thread_fence(std::memory_order_release);

        // Kontrollera om alla element i vektorn är noll
        __m128i cmp = _mm_cmpeq_epi32(vec, zero);
        int mask = _mm_movemask_epi8(cmp);

        if (mask == 0xFFFF) {
            break; // Alla 4 element är noll
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        vec = _mm_load_si128((__m128i const*)const_cast<uint32_t*>(count));
    } while (true);
}

// AVX2 version - processes 8 counters in parallel using intrinsics
void avx2_loop_work() {
    CPUFeatures features;
    if (!features.hasAVX2()) {
        throw std::runtime_error("CPU does not support AVX2 instructions");
    }

    alignas(32) volatile uint32_t count[8] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };
    
    __m256i vec = _mm256_load_si256((__m256i const*)const_cast<uint32_t*>(count));
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i zero = _mm256_setzero_si256();
    
    do {
        vec = _mm256_sub_epi32(vec, one);
        _mm256_store_si256((__m256i*)const_cast<uint32_t*>(count), vec);
        std::atomic_thread_fence(std::memory_order_release);
        
        __m256i cmp = _mm256_cmpeq_epi32(vec, zero);
        int mask = _mm256_movemask_epi8(cmp);
        
        if (mask == -1) break;
        
        vec = _mm256_load_si256((__m256i const*)const_cast<uint32_t*>(count));
        std::atomic_thread_fence(std::memory_order_acquire);
    } while (true);
    
    _mm256_zeroupper();
}

// AVX-512 version - processes 16 counters in parallel using intrinsics
void avx512_loop_work() {
    CPUFeatures features;
    if (!features.hasAVX512F()) {
        throw std::runtime_error("CPU does not support AVX-512 instructions");
    }

    alignas(64) volatile uint32_t count[16] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    };

    // Kontrollera om AVX-512 är aktiverat vid kompilering
#ifdef __AVX512F__
    __m512i vec = _mm512_load_si512(const_cast<uint32_t*>(count));
    const __m512i one = _mm512_set1_epi32(1);
    const __m512i zero = _mm512_setzero_si512();

    do {
        vec = _mm512_sub_epi32(vec, one);
        _mm512_store_si512(const_cast<uint32_t*>(count), vec);
        std::atomic_thread_fence(std::memory_order_release);

        __mmask16 mask = _mm512_cmpeq_epi32_mask(vec, zero);

        if (mask == 0xFFFF) break;  // Alla 16 bitar måste vara satta

        std::atomic_thread_fence(std::memory_order_acquire);
        vec = _mm512_load_si512(const_cast<uint32_t*>(count));
    } while (true);
#else
    throw std::runtime_error("AVX-512 instructions are not enabled in the compiler");
#endif
}