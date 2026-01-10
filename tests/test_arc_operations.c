#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "../src/arc_runtime.h"
#include "../src/arc_operations.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.3.2: Reference Counting Operations
// Comprehensive test suite for enhanced ARC reference counting operations

// Test 1: Optimized retain/release operations
void test_optimized_operations() {
    printf("Testing optimized retain/release operations...\n");
    
    wyn_arc_reset_stats();
    wyn_arc_reset_performance_stats();
    
    // Allocate test object
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_STRING, NULL);
    assert(obj != NULL);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Test optimized retain
    WynObject* obj2 = wyn_arc_retain_optimized(obj);
    assert(obj2 == obj);
    assert(wyn_arc_get_ref_count(obj) == 2);
    
    // Test optimized release
    wyn_arc_release_optimized(obj2);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Final release
    wyn_arc_release_optimized(obj);
    
    // Check performance stats
    WynARCPerformanceStats perf = wyn_arc_get_performance_stats();
    assert(perf.total_retains == 1);
    assert(perf.total_releases == 2);
    
    printf("✅ Optimized operations test passed\n");
}

// Test 2: Fast path optimization
void test_fast_path_optimization() {
    printf("Testing fast path optimization...\n");
    
    wyn_arc_reset_performance_stats();
    
    // Allocate object
    WynObject* obj = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    
    // Perform many retain/release operations (should hit fast path)
    for (int i = 0; i < 100; i++) {
        WynObject* retained = wyn_arc_retain_optimized(obj);
        assert(retained == obj);
        wyn_arc_release_optimized(retained);
    }
    
    // Check that fast path was used
    WynARCPerformanceStats perf = wyn_arc_get_performance_stats();
    assert(perf.total_retains == 100);
    assert(perf.total_releases == 100);
    
    // Fast path should be used for most operations
    assert(perf.fast_path_ratio_retain > 0.5); // At least 50% fast path
    assert(perf.fast_path_ratio_release > 0.5);
    
    wyn_arc_release(obj); // Final cleanup
    
    printf("✅ Fast path optimization test passed\n");
}

// Test 3: Batch operations
void test_batch_operations() {
    printf("Testing batch operations...\n");
    
    wyn_arc_reset_stats();
    
    // Create array of objects
    const size_t count = 10;
    WynObject* objects[count];
    
    // Allocate objects
    for (size_t i = 0; i < count; i++) {
        objects[i] = wyn_arc_alloc(16 + i, WYN_TYPE_ARRAY, NULL);
        assert(objects[i] != NULL);
    }
    
    // Test batch retain
    wyn_arc_retain_batch(objects, count);
    
    // Verify all objects have ref count 2
    for (size_t i = 0; i < count; i++) {
        assert(wyn_arc_get_ref_count(objects[i]) == 2);
    }
    
    // Test batch release
    wyn_arc_release_batch(objects, count);
    
    // Verify all objects have ref count 1
    for (size_t i = 0; i < count; i++) {
        assert(wyn_arc_get_ref_count(objects[i]) == 1);
    }
    
    // Final cleanup
    wyn_arc_release_batch(objects, count);
    
    // Check statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.current_objects == 0);
    assert(stats.total_deallocations == count);
    
    printf("✅ Batch operations test passed\n");
}

// Test 4: Conditional operations
void test_conditional_operations() {
    printf("Testing conditional operations...\n");
    
    // Test retain_if_not_null
    assert(wyn_arc_retain_if_not_null(NULL) == NULL);
    
    WynObject* obj = wyn_arc_alloc(8, WYN_TYPE_BOOL, NULL);
    assert(obj != NULL);
    
    WynObject* retained = wyn_arc_retain_if_not_null(obj);
    assert(retained == obj);
    assert(wyn_arc_get_ref_count(obj) == 2);
    
    // Test release_if_not_null
    wyn_arc_release_if_not_null(NULL); // Should not crash
    wyn_arc_release_if_not_null(retained);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    wyn_arc_release(obj);
    
    printf("✅ Conditional operations test passed\n");
}

// Test 5: Move semantics
void test_move_semantics() {
    printf("Testing move semantics...\n");
    
    wyn_arc_reset_stats();
    
    // Allocate object
    WynObject* obj = wyn_arc_alloc(16, WYN_TYPE_STRUCT, NULL);
    assert(obj != NULL);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Test move operation
    WynObject* obj_ptr = obj;
    WynObject* moved = wyn_arc_move(&obj_ptr);
    
    assert(moved == obj);
    assert(obj_ptr == NULL); // Source pointer should be cleared
    assert(wyn_arc_get_ref_count(moved) == 1); // No retain/release
    
    // Test move with NULL
    WynObject* null_ptr = NULL;
    WynObject* moved_null = wyn_arc_move(&null_ptr);
    assert(moved_null == NULL);
    
    wyn_arc_release(moved);
    
    printf("✅ Move semantics test passed\n");
}

// Test 6: Overflow detection
void test_overflow_detection() {
    printf("Testing overflow detection...\n");
    
    wyn_arc_reset_performance_stats();
    
    WynObject* obj = wyn_arc_alloc(8, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    
    // Manually set reference count near overflow to test detection
    // This is a bit hacky but necessary for testing
    atomic_store(&obj->header.ref_count, ARC_COUNT_MASK - 50);
    
    // This should trigger overflow warning but not panic
    wyn_arc_retain_optimized(obj);
    
    // Check that overflow check was performed
    WynARCPerformanceStats perf = wyn_arc_get_performance_stats();
    assert(perf.overflow_checks > 0);
    
    // Reset and cleanup
    atomic_store(&obj->header.ref_count, 1);
    wyn_arc_release(obj);
    
    printf("✅ Overflow detection test passed\n");
}

// Test 7: Performance monitoring
void test_performance_monitoring() {
    printf("Testing performance monitoring...\n");
    
    wyn_arc_reset_performance_stats();
    
    WynObject* obj = wyn_arc_alloc(32, WYN_TYPE_FUNCTION, NULL);
    assert(obj != NULL);
    
    // Perform various operations
    for (int i = 0; i < 50; i++) {
        wyn_arc_retain_optimized(obj);
    }
    
    for (int i = 0; i < 50; i++) {
        wyn_arc_release_optimized(obj);
    }
    
    // Check performance statistics
    WynARCPerformanceStats perf = wyn_arc_get_performance_stats();
    assert(perf.total_retains == 50);
    assert(perf.total_releases == 50);
    assert(perf.fast_path_ratio_retain >= 0.0 && perf.fast_path_ratio_retain <= 1.0);
    assert(perf.fast_path_ratio_release >= 0.0 && perf.fast_path_ratio_release <= 1.0);
    
    // Print stats for verification
    wyn_arc_print_performance_stats();
    
    wyn_arc_release(obj);
    
    printf("✅ Performance monitoring test passed\n");
}

// Test 8: Thread safety stress test
typedef struct {
    WynObject* obj;
    int iterations;
    int thread_id;
} ThreadTestData;

void* thread_test_function(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;
    
    for (int i = 0; i < data->iterations; i++) {
        // Retain and immediately release
        WynObject* retained = wyn_arc_retain_optimized(data->obj);
        assert(retained == data->obj);
        wyn_arc_release_optimized(retained);
        
        // Small delay to increase contention
        for (volatile int j = 0; j < 10; j++) {}
    }
    
    return NULL;
}

void test_thread_safety_stress() {
    printf("Testing thread safety stress test...\n");
    
    wyn_arc_reset_performance_stats();
    
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_CUSTOM_BASE, NULL);
    assert(obj != NULL);
    
    const int num_threads = 4;
    const int iterations_per_thread = 1000;
    pthread_t threads[num_threads];
    ThreadTestData thread_data[num_threads];
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].obj = obj;
        thread_data[i].iterations = iterations_per_thread;
        thread_data[i].thread_id = i;
        
        int result = pthread_create(&threads[i], NULL, thread_test_function, &thread_data[i]);
        assert(result == 0);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Object should still have reference count of 1
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Check performance stats
    WynARCPerformanceStats perf = wyn_arc_get_performance_stats();
    assert(perf.total_retains == num_threads * iterations_per_thread);
    assert(perf.total_releases == num_threads * iterations_per_thread);
    
    wyn_arc_release(obj);
    
    printf("✅ Thread safety stress test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.3.2 Test Suite ===\n");
    printf("Testing Reference Counting Operations with Performance Optimizations\n\n");
    
    // Run all tests
    test_optimized_operations();
    test_fast_path_optimization();
    test_batch_operations();
    test_conditional_operations();
    test_move_semantics();
    test_overflow_detection();
    test_performance_monitoring();
    test_thread_safety_stress();
    
    // Print final statistics
    printf("\n=== Final Performance Statistics ===\n");
    wyn_arc_print_performance_stats();
    
    printf("\n=== Final ARC Statistics ===\n");
    wyn_arc_print_stats();
    
    printf("\n✅ All T2.3.2 tests passed successfully!\n");
    printf("ARC Reference Counting Operations implementation complete.\n");
    
    return 0;
}
