#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "../src/arc_runtime.h"
#include "../src/memory_pool.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.3.5: Memory Pool Optimization
// Comprehensive test suite for memory pool implementation

// Test 1: Basic pool initialization and allocation
void test_pool_basic_operations() {
    printf("Testing basic pool operations...\n");
    
    wyn_pool_init();
    wyn_pool_reset_stats();
    
    // Test small allocations (should use pools)
    void* ptr1 = wyn_pool_alloc(32);
    void* ptr2 = wyn_pool_alloc(64);
    void* ptr3 = wyn_pool_alloc(128);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);
    
    // Test that pointers are different
    assert(ptr1 != ptr2);
    assert(ptr2 != ptr3);
    assert(ptr1 != ptr3);
    
    // Check statistics
    WynPoolStats stats = wyn_pool_get_stats();
    assert(stats.total_allocations >= 3);
    assert(stats.total_blocks >= 3);
    
    // Free allocations
    wyn_pool_free(ptr1, 32);
    wyn_pool_free(ptr2, 64);
    wyn_pool_free(ptr3, 128);
    
    // Check deallocation statistics
    stats = wyn_pool_get_stats();
    assert(stats.total_deallocations >= 3);
    
    printf("✅ Basic pool operations test passed\n");
}

// Test 2: Size class allocation
void test_size_class_allocation() {
    printf("Testing size class allocation...\n");
    
    wyn_pool_reset_stats();
    
    // Test various sizes that should map to different size classes
    struct {
        size_t size;
        size_t expected_class_size;
    } test_cases[] = {
        {1, 8},      // Should use 8-byte class
        {8, 8},      // Exact fit
        {9, 16},     // Should use 16-byte class
        {25, 32},    // Should use 32-byte class
        {100, 128},  // Should use 128-byte class
        {1000, 1024}, // Should use 1024-byte class
    };
    
    void* ptrs[6];
    for (int i = 0; i < 6; i++) {
        ptrs[i] = wyn_pool_alloc(test_cases[i].size);
        assert(ptrs[i] != NULL);
        
        // Write test pattern to verify allocation size
        memset(ptrs[i], 0xAA, test_cases[i].size);
    }
    
    // Verify test patterns
    for (int i = 0; i < 6; i++) {
        uint8_t* data = (uint8_t*)ptrs[i];
        for (size_t j = 0; j < test_cases[i].size; j++) {
            assert(data[j] == 0xAA);
        }
    }
    
    // Clean up
    for (int i = 0; i < 6; i++) {
        wyn_pool_free(ptrs[i], test_cases[i].size);
    }
    
    printf("✅ Size class allocation test passed\n");
}

// Test 3: Large allocation handling
void test_large_allocation() {
    printf("Testing large allocation handling...\n");
    
    wyn_pool_reset_stats();
    
    // Allocate something larger than max pooled size
    size_t large_size = 4096; // Larger than MAX_POOLED_SIZE (2048)
    void* large_ptr = wyn_pool_alloc(large_size);
    assert(large_ptr != NULL);
    
    // Write test pattern
    memset(large_ptr, 0xBB, large_size);
    
    // Verify pattern
    uint8_t* data = (uint8_t*)large_ptr;
    for (size_t i = 0; i < large_size; i++) {
        assert(data[i] == 0xBB);
    }
    
    // Check that this was handled as large allocation
    WynPoolStats stats = wyn_pool_get_stats();
    assert(stats.large_allocations >= 1);
    
    // Free large allocation
    wyn_pool_free(large_ptr, large_size);
    
    stats = wyn_pool_get_stats();
    assert(stats.large_deallocations >= 1);
    
    printf("✅ Large allocation test passed\n");
}

// Test 4: Pool reuse and fragmentation
void test_pool_reuse() {
    printf("Testing pool reuse and fragmentation...\n");
    
    wyn_pool_reset_stats();
    
    const int num_allocs = 10;
    void* ptrs[num_allocs];
    
    // Allocate many blocks of same size
    for (int i = 0; i < num_allocs; i++) {
        ptrs[i] = wyn_pool_alloc(64);
        assert(ptrs[i] != NULL);
    }
    
    WynPoolStats stats1 = wyn_pool_get_stats();
    size_t blocks_after_alloc = stats1.total_blocks;
    
    // Free half of them
    for (int i = 0; i < num_allocs / 2; i++) {
        wyn_pool_free(ptrs[i], 64);
    }
    
    WynPoolStats stats2 = wyn_pool_get_stats();
    assert(stats2.free_blocks >= num_allocs / 2);
    
    // Allocate again - should reuse freed blocks
    for (int i = 0; i < num_allocs / 2; i++) {
        ptrs[i] = wyn_pool_alloc(64);
        assert(ptrs[i] != NULL);
    }
    
    WynPoolStats stats3 = wyn_pool_get_stats();
    // Should not have allocated many new blocks
    assert(stats3.total_blocks <= blocks_after_alloc + 2); // Allow some variance
    
    // Clean up remaining
    for (int i = 0; i < num_allocs; i++) {
        wyn_pool_free(ptrs[i], 64);
    }
    
    printf("✅ Pool reuse test passed\n");
}

// Test 5: ARC pooled allocation
void test_arc_pooled_allocation() {
    printf("Testing ARC pooled allocation...\n");
    
    wyn_pool_reset_stats();
    
    // Create ARC objects using pooled allocation
    WynObject* obj1 = wyn_arc_alloc_pooled(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc_pooled(64, WYN_TYPE_ARRAY, NULL);
    WynObject* obj3 = wyn_arc_alloc_pooled(128, WYN_TYPE_STRUCT, NULL);
    
    assert(obj1 && obj2 && obj3);
    
    // Verify ARC object properties
    assert(wyn_arc_is_valid(obj1));
    assert(wyn_arc_is_valid(obj2));
    assert(wyn_arc_is_valid(obj3));
    
    assert(wyn_arc_get_ref_count(obj1) == 1);
    assert(wyn_arc_get_ref_count(obj2) == 1);
    assert(wyn_arc_get_ref_count(obj3) == 1);
    
    assert(wyn_arc_get_type_id(obj1) == WYN_TYPE_STRING);
    assert(wyn_arc_get_type_id(obj2) == WYN_TYPE_ARRAY);
    assert(wyn_arc_get_type_id(obj3) == WYN_TYPE_STRUCT);
    
    // Test data access
    char* data1 = (char*)wyn_arc_get_data(obj1);
    strcpy(data1, "Hello Pool");
    assert(strcmp(data1, "Hello Pool") == 0);
    
    // Clean up using pooled deallocation
    wyn_arc_deallocate_pooled(obj1);
    wyn_arc_deallocate_pooled(obj2);
    wyn_arc_deallocate_pooled(obj3);
    
    // Check pool statistics
    WynPoolStats stats = wyn_pool_get_stats();
    assert(stats.total_allocations >= 3);
    assert(stats.total_deallocations >= 3);
    
    printf("✅ ARC pooled allocation test passed\n");
}

// Test 6: Batch operations
void test_batch_operations() {
    printf("Testing batch operations...\n");
    
    wyn_pool_reset_stats();
    
    const size_t batch_size = 5;
    void* ptrs[batch_size];
    size_t sizes[batch_size] = {16, 32, 64, 128, 256};
    
    // Batch allocation
    wyn_pool_alloc_batch(ptrs, sizes, batch_size);
    
    // Verify all allocations succeeded
    for (size_t i = 0; i < batch_size; i++) {
        assert(ptrs[i] != NULL);
        
        // Write test pattern
        memset(ptrs[i], (int)(0xCC + i), sizes[i]);
    }
    
    // Verify patterns
    for (size_t i = 0; i < batch_size; i++) {
        uint8_t* data = (uint8_t*)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(data[j] == (uint8_t)(0xCC + i));
        }
    }
    
    // Batch deallocation
    wyn_pool_free_batch(ptrs, sizes, batch_size);
    
    // Verify pointers were cleared
    for (size_t i = 0; i < batch_size; i++) {
        assert(ptrs[i] == NULL);
    }
    
    printf("✅ Batch operations test passed\n");
}

// Test 7: Statistics and monitoring
void test_statistics_monitoring() {
    printf("Testing statistics and monitoring...\n");
    
    wyn_pool_reset_stats();
    
    // Perform various allocations
    void* small_ptr = wyn_pool_alloc(32);
    void* medium_ptr = wyn_pool_alloc(256);
    void* large_ptr = wyn_pool_alloc(4096); // Large allocation
    
    assert(small_ptr && medium_ptr && large_ptr);
    
    // Check statistics
    WynPoolStats stats = wyn_pool_get_stats();
    assert(stats.total_allocations >= 2); // Small and medium
    assert(stats.large_allocations >= 1); // Large
    assert(stats.total_memory_used > 0);
    assert(stats.peak_memory_used >= stats.total_memory_used);
    
    // Print detailed statistics
    wyn_pool_print_stats();
    wyn_pool_print_detailed_stats();
    
    // Clean up
    wyn_pool_free(small_ptr, 32);
    wyn_pool_free(medium_ptr, 256);
    wyn_pool_free(large_ptr, 4096);
    
    stats = wyn_pool_get_stats();
    assert(stats.total_deallocations >= 2);
    assert(stats.large_deallocations >= 1);
    
    printf("✅ Statistics monitoring test passed\n");
}

// Test 8: Thread safety
typedef struct {
    int thread_id;
    int allocations_per_thread;
    void** ptrs;
    size_t* sizes;
} PoolThreadTestData;

void* pool_thread_test_function(void* arg) {
    PoolThreadTestData* data = (PoolThreadTestData*)arg;
    
    // Perform allocations
    for (int i = 0; i < data->allocations_per_thread; i++) {
        size_t size = 32 + (i % 8) * 16; // Vary sizes
        data->ptrs[i] = wyn_pool_alloc(size);
        data->sizes[i] = size;
        assert(data->ptrs[i] != NULL);
        
        // Write test pattern
        memset(data->ptrs[i], data->thread_id, size);
    }
    
    // Verify patterns
    for (int i = 0; i < data->allocations_per_thread; i++) {
        uint8_t* ptr_data = (uint8_t*)data->ptrs[i];
        for (size_t j = 0; j < data->sizes[i]; j++) {
            assert(ptr_data[j] == (uint8_t)data->thread_id);
        }
    }
    
    // Free allocations
    for (int i = 0; i < data->allocations_per_thread; i++) {
        wyn_pool_free(data->ptrs[i], data->sizes[i]);
    }
    
    return NULL;
}

void test_thread_safety() {
    printf("Testing thread safety...\n");
    
    wyn_pool_reset_stats();
    
    const int num_threads = 4;
    const int allocs_per_thread = 50;
    pthread_t threads[num_threads];
    PoolThreadTestData thread_data[num_threads];
    
    // Allocate storage for each thread
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i + 1; // Non-zero for pattern
        thread_data[i].allocations_per_thread = allocs_per_thread;
        thread_data[i].ptrs = malloc(allocs_per_thread * sizeof(void*));
        thread_data[i].sizes = malloc(allocs_per_thread * sizeof(size_t));
        assert(thread_data[i].ptrs && thread_data[i].sizes);
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int result = pthread_create(&threads[i], NULL, pool_thread_test_function, &thread_data[i]);
        assert(result == 0);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Check final statistics
    WynPoolStats stats = wyn_pool_get_stats();
    assert(stats.total_allocations >= num_threads * allocs_per_thread);
    assert(stats.total_deallocations >= num_threads * allocs_per_thread);
    
    // Clean up thread data
    for (int i = 0; i < num_threads; i++) {
        free(thread_data[i].ptrs);
        free(thread_data[i].sizes);
    }
    
    printf("✅ Thread safety test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.3.5 Test Suite ===\n");
    printf("Testing Memory Pool Optimization\n\n");
    
    // Run all tests
    test_pool_basic_operations();
    test_size_class_allocation();
    test_large_allocation();
    test_pool_reuse();
    test_arc_pooled_allocation();
    test_batch_operations();
    test_statistics_monitoring();
    test_thread_safety();
    
    // Print final statistics
    printf("\n=== Final Memory Pool Statistics ===\n");
    wyn_pool_print_stats();
    
    // Cleanup
    wyn_pool_cleanup();
    
    printf("\n✅ All T2.3.5 tests passed successfully!\n");
    printf("Memory Pool Optimization implementation complete.\n");
    
    return 0;
}
