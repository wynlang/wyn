#include "test.h"
#include "simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int test_simd_capability_detection() {
    printf("Testing SIMD capability detection...\n");
    
    simd_capabilities_t caps = simd_detect_capabilities();
    
    printf("  Detected capabilities:\n");
    printf("    SSE2: %s\n", caps.sse2_available ? "Yes" : "No");
    printf("    SSE4: %s\n", caps.sse4_available ? "Yes" : "No");
    printf("    AVX:  %s\n", caps.avx_available ? "Yes" : "No");
    printf("    AVX2: %s\n", caps.avx2_available ? "Yes" : "No");
    printf("    NEON: %s\n", caps.neon_available ? "Yes" : "No");
    
    printf("  PASS: SIMD capability detection working\n");
    return 1;
}

static int test_simd_integer_operations() {
    printf("Testing SIMD integer operations...\n");
    
    const size_t count = 1024;
    int32_t* a = malloc(count * sizeof(int32_t));
    int32_t* b = malloc(count * sizeof(int32_t));
    int32_t* result = malloc(count * sizeof(int32_t));
    int32_t* expected = malloc(count * sizeof(int32_t));
    
    // Initialize test data
    for (size_t i = 0; i < count; i++) {
        a[i] = (int32_t)i;
        b[i] = (int32_t)(i * 2);
        expected[i] = a[i] + b[i];
    }
    
    // Test SIMD addition
    simd_add_i32_array(a, b, result, count);
    
    // Verify results
    for (size_t i = 0; i < count; i++) {
        if (result[i] != expected[i]) {
            printf("  FAIL: Mismatch at index %zu: got %d, expected %d\n", 
                   i, result[i], expected[i]);
            free(a); free(b); free(result); free(expected);
            return 0;
        }
    }
    
    free(a); free(b); free(result); free(expected);
    printf("  PASS: SIMD integer operations correct\n");
    return 1;
}

static int test_simd_float_operations() {
    printf("Testing SIMD float operations...\n");
    
    const size_t count = 1024;
    float* a = malloc(count * sizeof(float));
    float* b = malloc(count * sizeof(float));
    float* result = malloc(count * sizeof(float));
    
    // Initialize test data
    for (size_t i = 0; i < count; i++) {
        a[i] = (float)i * 0.5f;
        b[i] = (float)i * 1.5f;
    }
    
    // Test SIMD addition
    simd_add_f32_array(a, b, result, count);
    
    // Verify results (with floating point tolerance)
    for (size_t i = 0; i < count; i++) {
        float expected = a[i] + b[i];
        float diff = result[i] - expected;
        if (diff < 0) diff = -diff;
        
        if (diff > 1e-6f) {
            printf("  FAIL: Float mismatch at index %zu: got %f, expected %f\n", 
                   i, result[i], expected);
            free(a); free(b); free(result);
            return 0;
        }
    }
    
    // Test SIMD multiplication
    simd_mul_f32_array(a, b, result, count);
    
    for (size_t i = 0; i < count; i++) {
        float expected = a[i] * b[i];
        float diff = result[i] - expected;
        if (diff < 0) diff = -diff;
        
        if (diff > 1e-6f) {
            printf("  FAIL: Multiplication mismatch at index %zu\n", i);
            free(a); free(b); free(result);
            return 0;
        }
    }
    
    free(a); free(b); free(result);
    printf("  PASS: SIMD float operations correct\n");
    return 1;
}

static int test_simd_string_operations() {
    printf("Testing SIMD string operations...\n");
    
    const char* test_strings[] = {
        "hello",
        "world",
        "this is a longer string for testing",
        "",
        "a",
        NULL
    };
    
    for (int i = 0; test_strings[i] != NULL; i++) {
        size_t simd_len = simd_strlen(test_strings[i]);
        size_t std_len = strlen(test_strings[i]);
        
        if (simd_len != std_len) {
            printf("  FAIL: String length mismatch for '%s': got %zu, expected %zu\n",
                   test_strings[i], simd_len, std_len);
            return 0;
        }
    }
    
    printf("  PASS: SIMD string operations correct\n");
    return 1;
}

static int test_simd_memory_operations() {
    printf("Testing SIMD memory operations...\n");
    
    const size_t size = 4096;
    char* src = malloc(size);
    char* dest1 = malloc(size);
    char* dest2 = malloc(size);
    
    // Initialize source data
    for (size_t i = 0; i < size; i++) {
        src[i] = (char)(i & 0xFF);
    }
    
    // Test SIMD memcpy vs standard memcpy
    simd_memcpy(dest1, src, size);
    memcpy(dest2, src, size);
    
    if (memcmp(dest1, dest2, size) != 0) {
        printf("  FAIL: SIMD memcpy differs from standard memcpy\n");
        free(src); free(dest1); free(dest2);
        return 0;
    }
    
    free(src); free(dest1); free(dest2);
    printf("  PASS: SIMD memory operations correct\n");
    return 1;
}

static int test_simd_reduction_operations() {
    printf("Testing SIMD reduction operations...\n");
    
    const size_t count = 1000;
    int32_t* array = malloc(count * sizeof(int32_t));
    
    // Initialize test data
    int32_t expected_sum = 0;
    for (size_t i = 0; i < count; i++) {
        array[i] = (int32_t)i;
        expected_sum += array[i];
    }
    
    // Test SIMD sum
    int32_t simd_sum = simd_sum_i32_array(array, count);
    
    if (simd_sum != expected_sum) {
        printf("  FAIL: SIMD sum mismatch: got %d, expected %d\n", 
               simd_sum, expected_sum);
        free(array);
        return 0;
    }
    
    free(array);
    printf("  PASS: SIMD reduction operations correct\n");
    return 1;
}

static int test_simd_performance() {
    printf("Testing SIMD performance...\n");
    
    const size_t count = 1000000;
    float* a = malloc(count * sizeof(float));
    float* b = malloc(count * sizeof(float));
    float* result = malloc(count * sizeof(float));
    
    // Initialize data
    for (size_t i = 0; i < count; i++) {
        a[i] = (float)i;
        b[i] = (float)(i + 1);
    }
    
    // Time SIMD operation
    clock_t start = clock();
    simd_add_f32_array(a, b, result, count);
    clock_t end = clock();
    
    double simd_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // Time scalar operation
    start = clock();
    for (size_t i = 0; i < count; i++) {
        result[i] = a[i] + b[i];
    }
    end = clock();
    
    double scalar_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("  SIMD time: %.6f seconds\n", simd_time);
    printf("  Scalar time: %.6f seconds\n", scalar_time);
    
    if (simd_time > 0 && scalar_time > 0) {
        double speedup = scalar_time / simd_time;
        printf("  Speedup: %.2fx\n", speedup);
    }
    
    free(a); free(b); free(result);
    printf("  PASS: SIMD performance test completed\n");
    return 1;
}

int main() {
    printf("=== T8.2.1: SIMD Instruction Utilization Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_simd_capability_detection()) passed_tests++;
    total_tests++; if (test_simd_integer_operations()) passed_tests++;
    total_tests++; if (test_simd_float_operations()) passed_tests++;
    total_tests++; if (test_simd_string_operations()) passed_tests++;
    total_tests++; if (test_simd_memory_operations()) passed_tests++;
    total_tests++; if (test_simd_reduction_operations()) passed_tests++;
    total_tests++; if (test_simd_performance()) passed_tests++;
    
    // Print summary
    printf("\n=== SIMD Optimization Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All SIMD optimization tests passed!\n");
        printf("⚡ SIMD acceleration ready for production\n");
        return 0;
    } else {
        printf("❌ Some SIMD optimization tests failed\n");
        return 1;
    }
}
