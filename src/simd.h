// SIMD Optimization Module for Wyn Language
// T8.2.1: SIMD instruction utilization

#ifndef WYN_SIMD_H
#define WYN_SIMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __x86_64__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

// SIMD capability detection
typedef struct {
    bool sse2_available;
    bool sse4_available;
    bool avx_available;
    bool avx2_available;
    bool neon_available;
} simd_capabilities_t;

// SIMD operation types
typedef enum {
    SIMD_ADD,
    SIMD_SUB,
    SIMD_MUL,
    SIMD_DIV,
    SIMD_MIN,
    SIMD_MAX,
    SIMD_CMP_EQ,
    SIMD_CMP_LT,
    SIMD_CMP_GT
} simd_op_t;

// Initialize SIMD capabilities
simd_capabilities_t simd_detect_capabilities(void);

// Vector operations for different data types
void simd_add_i32_array(const int32_t* a, const int32_t* b, int32_t* result, size_t count);
void simd_add_f32_array(const float* a, const float* b, float* result, size_t count);
void simd_add_f64_array(const double* a, const double* b, double* result, size_t count);

void simd_mul_i32_array(const int32_t* a, const int32_t* b, int32_t* result, size_t count);
void simd_mul_f32_array(const float* a, const float* b, float* result, size_t count);

// String operations with SIMD
size_t simd_strlen(const char* str);
int simd_strcmp(const char* a, const char* b);
char* simd_strchr(const char* str, int c);

// Memory operations
void simd_memcpy(void* dest, const void* src, size_t n);
void simd_memset(void* ptr, int value, size_t n);
int simd_memcmp(const void* a, const void* b, size_t n);

// Array reduction operations
int32_t simd_sum_i32_array(const int32_t* array, size_t count);
float simd_sum_f32_array(const float* array, size_t count);
int32_t simd_min_i32_array(const int32_t* array, size_t count);
int32_t simd_max_i32_array(const int32_t* array, size_t count);

// LLVM integration functions
void simd_generate_vector_add(void* llvm_builder, void* lhs, void* rhs, void* result_type);
void simd_generate_vector_mul(void* llvm_builder, void* lhs, void* rhs, void* result_type);
void simd_optimize_loop_vectorization(void* llvm_function);

#endif // WYN_SIMD_H
