// SIMD Optimization Implementation for Wyn Language
// T8.2.1: SIMD instruction utilization

#include "simd.h"
#include <string.h>
#include <stdio.h>

// Global SIMD capabilities
static simd_capabilities_t g_simd_caps = {0};
static bool g_simd_initialized = false;

// Detect available SIMD capabilities
simd_capabilities_t simd_detect_capabilities(void) {
    if (g_simd_initialized) {
        return g_simd_caps;
    }
    
    simd_capabilities_t caps = {0};
    
#ifdef __x86_64__
    // Check CPUID for x86_64 SIMD support
    uint32_t eax, ebx, ecx, edx;
    
    // Check for SSE2
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    caps.sse2_available = (edx & (1 << 26)) != 0;
    
    // Check for SSE4.1
    caps.sse4_available = (ecx & (1 << 19)) != 0;
    
    // Check for AVX
    caps.avx_available = (ecx & (1 << 28)) != 0;
    
    // Check for AVX2
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    caps.avx2_available = (ebx & (1 << 5)) != 0;
#endif

#ifdef __ARM_NEON__
    caps.neon_available = true;
#endif
    
    g_simd_caps = caps;
    g_simd_initialized = true;
    
    return caps;
}

// SIMD integer addition
void simd_add_i32_array(const int32_t* a, const int32_t* b, int32_t* result, size_t count) {
    size_t i = 0;
    
#ifdef __x86_64__
    if (g_simd_caps.avx2_available && count >= 8) {
        for (; i + 8 <= count; i += 8) {
            __m256i va = _mm256_loadu_si256((__m256i*)(a + i));
            __m256i vb = _mm256_loadu_si256((__m256i*)(b + i));
            __m256i vr = _mm256_add_epi32(va, vb);
            _mm256_storeu_si256((__m256i*)(result + i), vr);
        }
    } else if (g_simd_caps.sse2_available && count >= 4) {
        for (; i + 4 <= count; i += 4) {
            __m128i va = _mm_loadu_si128((__m128i*)(a + i));
            __m128i vb = _mm_loadu_si128((__m128i*)(b + i));
            __m128i vr = _mm_add_epi32(va, vb);
            _mm_storeu_si128((__m128i*)(result + i), vr);
        }
    }
#endif

#ifdef __ARM_NEON__
    if (g_simd_caps.neon_available && count >= 4) {
        for (; i + 4 <= count; i += 4) {
            int32x4_t va = vld1q_s32(a + i);
            int32x4_t vb = vld1q_s32(b + i);
            int32x4_t vr = vaddq_s32(va, vb);
            vst1q_s32(result + i, vr);
        }
    }
#endif
    
    // Scalar fallback
    for (; i < count; i++) {
        result[i] = a[i] + b[i];
    }
}

// SIMD float addition
void simd_add_f32_array(const float* a, const float* b, float* result, size_t count) {
    size_t i = 0;
    
#ifdef __x86_64__
    if (g_simd_caps.avx_available && count >= 8) {
        for (; i + 8 <= count; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(result + i, vr);
        }
    } else if (g_simd_caps.sse2_available && count >= 4) {
        for (; i + 4 <= count; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 vr = _mm_add_ps(va, vb);
            _mm_storeu_ps(result + i, vr);
        }
    }
#endif

#ifdef __ARM_NEON__
    if (g_simd_caps.neon_available && count >= 4) {
        for (; i + 4 <= count; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            float32x4_t vr = vaddq_f32(va, vb);
            vst1q_f32(result + i, vr);
        }
    }
#endif
    
    // Scalar fallback
    for (; i < count; i++) {
        result[i] = a[i] + b[i];
    }
}

// SIMD multiplication
void simd_mul_f32_array(const float* a, const float* b, float* result, size_t count) {
    size_t i = 0;
    
#ifdef __x86_64__
    if (g_simd_caps.avx_available && count >= 8) {
        for (; i + 8 <= count; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 vr = _mm256_mul_ps(va, vb);
            _mm256_storeu_ps(result + i, vr);
        }
    } else if (g_simd_caps.sse2_available && count >= 4) {
        for (; i + 4 <= count; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 vr = _mm_mul_ps(va, vb);
            _mm_storeu_ps(result + i, vr);
        }
    }
#endif
    
    // Scalar fallback
    for (; i < count; i++) {
        result[i] = a[i] * b[i];
    }
}

// SIMD string length
size_t simd_strlen(const char* str) {
#ifdef __x86_64__
    if (g_simd_caps.sse2_available) {
        const char* p = str;
        __m128i zero = _mm_setzero_si128();
        
        // Align to 16-byte boundary
        while (((uintptr_t)p & 15) && *p) p++;
        
        if (*p) {
            while (1) {
                __m128i chunk = _mm_load_si128((__m128i*)p);
                __m128i cmp = _mm_cmpeq_epi8(chunk, zero);
                int mask = _mm_movemask_epi8(cmp);
                
                if (mask) {
                    return (p - str) + __builtin_ctz(mask);
                }
                p += 16;
            }
        }
        
        return p - str;
    }
#endif
    
    return strlen(str);
}

// SIMD memory copy
void simd_memcpy(void* dest, const void* src, size_t n) {
#ifdef __x86_64__
    if (g_simd_caps.avx2_available && n >= 32) {
        const char* s = (const char*)src;
        char* d = (char*)dest;
        size_t i = 0;
        
        for (; i + 32 <= n; i += 32) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)(s + i));
            _mm256_storeu_si256((__m256i*)(d + i), chunk);
        }
        
        // Handle remaining bytes
        memcpy(d + i, s + i, n - i);
        return;
    }
#endif
    
    memcpy(dest, src, n);
}

// SIMD array sum
int32_t simd_sum_i32_array(const int32_t* array, size_t count) {
    int32_t sum = 0;
    size_t i = 0;
    
#ifdef __x86_64__
    if (g_simd_caps.avx2_available && count >= 8) {
        __m256i vsum = _mm256_setzero_si256();
        
        for (; i + 8 <= count; i += 8) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)(array + i));
            vsum = _mm256_add_epi32(vsum, chunk);
        }
        
        // Horizontal sum
        __m128i lo = _mm256_extracti128_si256(vsum, 0);
        __m128i hi = _mm256_extracti128_si256(vsum, 1);
        __m128i sum128 = _mm_add_epi32(lo, hi);
        
        sum128 = _mm_hadd_epi32(sum128, sum128);
        sum128 = _mm_hadd_epi32(sum128, sum128);
        sum = _mm_extract_epi32(sum128, 0);
    }
#endif
    
    // Scalar remainder
    for (; i < count; i++) {
        sum += array[i];
    }
    
    return sum;
}

// Initialize SIMD system
void simd_init(void) {
    simd_detect_capabilities();
    
    printf("SIMD Capabilities:\n");
    printf("  SSE2: %s\n", g_simd_caps.sse2_available ? "Yes" : "No");
    printf("  SSE4: %s\n", g_simd_caps.sse4_available ? "Yes" : "No");
    printf("  AVX:  %s\n", g_simd_caps.avx_available ? "Yes" : "No");
    printf("  AVX2: %s\n", g_simd_caps.avx2_available ? "Yes" : "No");
    printf("  NEON: %s\n", g_simd_caps.neon_available ? "Yes" : "No");
}
