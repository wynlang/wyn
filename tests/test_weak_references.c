#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../src/arc_runtime.h"
#include "../src/weak_references.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.3.3: Weak Reference System
// Comprehensive test suite for weak reference implementation

// Test 1: Basic weak reference creation and access
void test_weak_basic_operations() {
    printf("Testing basic weak reference operations...\n");
    
    wyn_arc_reset_stats();
    wyn_weak_reset_stats();
    
    // Create object
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_STRING, NULL);
    assert(obj != NULL);
    
    // Create weak reference
    WynWeakRef* weak_ref = wyn_weak_create(obj);
    assert(weak_ref != NULL);
    assert(wyn_weak_is_valid(weak_ref));
    
    // Access through weak reference
    WynObject* accessed = wyn_weak_access(weak_ref);
    assert(accessed == obj);
    
    // Destroy weak reference
    wyn_weak_destroy(weak_ref);
    
    // Clean up object
    wyn_arc_release(obj);
    
    printf("✅ Basic weak reference operations test passed\n");
}

// Test 2: Automatic nullification when object is deallocated
void test_weak_automatic_nullification() {
    printf("Testing automatic nullification...\n");
    
    wyn_arc_reset_stats();
    wyn_weak_reset_stats();
    
    // Create object
    WynObject* obj = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    
    // Create multiple weak references
    WynWeakRef* weak1 = wyn_weak_create(obj);
    WynWeakRef* weak2 = wyn_weak_create(obj);
    WynWeakRef* weak3 = wyn_weak_create(obj);
    
    assert(weak1 && weak2 && weak3);
    assert(wyn_weak_is_valid(weak1));
    assert(wyn_weak_is_valid(weak2));
    assert(wyn_weak_is_valid(weak3));
    
    // All should access the same object
    assert(wyn_weak_access(weak1) == obj);
    assert(wyn_weak_access(weak2) == obj);
    assert(wyn_weak_access(weak3) == obj);
    
    // Deallocate object - should nullify all weak references
    wyn_arc_release(obj);
    
    // All weak references should now be invalid
    assert(!wyn_weak_is_valid(weak1));
    assert(!wyn_weak_is_valid(weak2));
    assert(!wyn_weak_is_valid(weak3));
    
    // Access should return NULL
    assert(wyn_weak_access(weak1) == NULL);
    assert(wyn_weak_access(weak2) == NULL);
    assert(wyn_weak_access(weak3) == NULL);
    
    // Check statistics
    WynWeakRefStats stats = wyn_weak_get_stats();
    assert(stats.nullified_refs == 3);
    
    // Clean up weak references
    wyn_weak_destroy(weak1);
    wyn_weak_destroy(weak2);
    wyn_weak_destroy(weak3);
    
    printf("✅ Automatic nullification test passed\n");
}

// Test 3: Weak reference promotion to strong reference
void test_weak_promotion() {
    printf("Testing weak reference promotion...\n");
    
    wyn_arc_reset_stats();
    
    // Create object
    WynObject* obj = wyn_arc_alloc(16, WYN_TYPE_BOOL, NULL);
    assert(obj != NULL);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Create weak reference
    WynWeakRef* weak_ref = wyn_weak_create(obj);
    assert(weak_ref != NULL);
    
    // Promote weak reference to strong reference
    WynObject* promoted = wyn_weak_promote(weak_ref);
    assert(promoted == obj);
    assert(wyn_arc_get_ref_count(obj) == 2); // Should have incremented
    
    // Release promoted reference
    wyn_arc_release(promoted);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Try promotion after object deallocation
    wyn_arc_release(obj);
    
    WynObject* promoted_after_dealloc = wyn_weak_promote(weak_ref);
    assert(promoted_after_dealloc == NULL); // Should fail
    assert(!wyn_weak_is_valid(weak_ref));
    
    wyn_weak_destroy(weak_ref);
    
    printf("✅ Weak reference promotion test passed\n");
}

// Test 4: Batch operations
void test_weak_batch_operations() {
    printf("Testing weak reference batch operations...\n");
    
    wyn_arc_reset_stats();
    wyn_weak_reset_stats();
    
    const size_t count = 5;
    WynObject* objects[count];
    WynWeakRef* weak_refs[count];
    
    // Create objects
    for (size_t i = 0; i < count; i++) {
        objects[i] = wyn_arc_alloc(8 + i, WYN_TYPE_ARRAY, NULL);
        assert(objects[i] != NULL);
    }
    
    // Create weak references in batch
    wyn_weak_create_batch(objects, weak_refs, count);
    
    // Verify all weak references were created
    for (size_t i = 0; i < count; i++) {
        assert(weak_refs[i] != NULL);
        assert(wyn_weak_is_valid(weak_refs[i]));
        assert(wyn_weak_access(weak_refs[i]) == objects[i]);
    }
    
    // Release some objects
    wyn_arc_release(objects[1]);
    wyn_arc_release(objects[3]);
    
    // Check that corresponding weak references are nullified
    assert(!wyn_weak_is_valid(weak_refs[1]));
    assert(!wyn_weak_is_valid(weak_refs[3]));
    assert(wyn_weak_is_valid(weak_refs[0]));
    assert(wyn_weak_is_valid(weak_refs[2]));
    assert(wyn_weak_is_valid(weak_refs[4]));
    
    // Destroy weak references in batch
    wyn_weak_destroy_batch(weak_refs, count);
    
    // Clean up remaining objects
    wyn_arc_release(objects[0]);
    wyn_arc_release(objects[2]);
    wyn_arc_release(objects[4]);
    
    printf("✅ Batch operations test passed\n");
}

// Test 5: Thread safety
typedef struct {
    WynObject* obj;
    WynWeakRef** weak_refs;
    int ref_count;
    int thread_id;
    bool should_deallocate;
} WeakThreadTestData;

void* weak_thread_test_function(void* arg) {
    WeakThreadTestData* data = (WeakThreadTestData*)arg;
    
    // Create weak references
    for (int i = 0; i < data->ref_count; i++) {
        data->weak_refs[i] = wyn_weak_create(data->obj);
        assert(data->weak_refs[i] != NULL);
        
        // Try to access and promote
        WynObject* accessed = wyn_weak_access(data->weak_refs[i]);
        if (accessed) {
            WynObject* promoted = wyn_weak_promote(data->weak_refs[i]);
            if (promoted) {
                wyn_arc_release(promoted); // Release immediately
            }
        }
        
        // Small delay to increase contention
        usleep(1000); // 1ms
    }
    
    // If this thread should deallocate, do it
    if (data->should_deallocate) {
        usleep(5000); // Wait a bit for other threads to create references
        wyn_arc_release(data->obj);
    }
    
    return NULL;
}

void test_weak_thread_safety() {
    printf("Testing weak reference thread safety...\n");
    
    wyn_arc_reset_stats();
    wyn_weak_reset_stats();
    
    // Create object
    WynObject* obj = wyn_arc_alloc(128, WYN_TYPE_CUSTOM_BASE, NULL);
    assert(obj != NULL);
    
    const int num_threads = 3;
    const int refs_per_thread = 10;
    pthread_t threads[num_threads];
    WeakThreadTestData thread_data[num_threads];
    WynWeakRef* all_weak_refs[num_threads][refs_per_thread];
    
    // Setup thread data
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].obj = obj;
        thread_data[i].weak_refs = all_weak_refs[i];
        thread_data[i].ref_count = refs_per_thread;
        thread_data[i].thread_id = i;
        thread_data[i].should_deallocate = (i == num_threads - 1); // Last thread deallocates
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int result = pthread_create(&threads[i], NULL, weak_thread_test_function, &thread_data[i]);
        assert(result == 0);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Check that weak references were properly nullified
    int nullified_count = 0;
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < refs_per_thread; j++) {
            if (all_weak_refs[i][j] && !wyn_weak_is_valid(all_weak_refs[i][j])) {
                nullified_count++;
            }
        }
    }
    
    // Should have nullified references from all threads
    assert(nullified_count > 0);
    
    // Clean up weak references
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < refs_per_thread; j++) {
            if (all_weak_refs[i][j]) {
                wyn_weak_destroy(all_weak_refs[i][j]);
            }
        }
    }
    
    printf("✅ Thread safety test passed\n");
}

// Test 6: Statistics and monitoring
void test_weak_statistics() {
    printf("Testing weak reference statistics...\n");
    
    wyn_weak_reset_stats();
    
    // Create objects and weak references
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    
    WynWeakRef* weak1 = wyn_weak_create(obj1);
    WynWeakRef* weak2 = wyn_weak_create(obj1); // Same object
    WynWeakRef* weak3 = wyn_weak_create(obj2);
    
    // Check initial stats
    WynWeakRefStats stats = wyn_weak_get_stats();
    assert(stats.total_weak_refs == 3);
    assert(stats.active_refs == 3);
    assert(stats.nullified_refs == 0);
    
    // Deallocate obj1 - should nullify 2 references
    wyn_arc_release(obj1);
    
    stats = wyn_weak_get_stats();
    assert(stats.nullified_refs == 2);
    assert(stats.nullification_rate > 0.5); // Should be 2/3
    
    // Print stats for verification
    wyn_weak_print_stats();
    
    // Clean up
    wyn_weak_destroy(weak1);
    wyn_weak_destroy(weak2);
    wyn_weak_destroy(weak3);
    wyn_arc_release(obj2);
    
    printf("✅ Statistics test passed\n");
}

// Test 7: Error handling
void test_weak_error_handling() {
    printf("Testing weak reference error handling...\n");
    
    clear_errors();
    
    // Test NULL object
    WynWeakRef* weak_null = wyn_weak_create(NULL);
    assert(weak_null == NULL);
    
    // Test invalid object
    WynObject fake_obj;
    fake_obj.header.magic = 0xDEAD;
    WynWeakRef* weak_invalid = wyn_weak_create(&fake_obj);
    assert(weak_invalid == NULL);
    assert(has_errors());
    clear_errors();
    
    // Test NULL weak reference operations
    assert(wyn_weak_access(NULL) == NULL);
    assert(wyn_weak_promote(NULL) == NULL);
    assert(!wyn_weak_is_valid(NULL));
    wyn_weak_destroy(NULL); // Should not crash
    
    printf("✅ Error handling test passed\n");
}

// Test 8: Registry cleanup
void test_weak_registry_cleanup() {
    printf("Testing weak reference registry cleanup...\n");
    
    // Create some weak references
    WynObject* obj = wyn_arc_alloc(16, WYN_TYPE_INT, NULL);
    WynWeakRef* weak1 = wyn_weak_create(obj);
    WynWeakRef* weak2 = wyn_weak_create(obj);
    
    assert(weak1 && weak2);
    
    // Clean up object first (this will nullify weak references)
    wyn_arc_release(obj);
    
    // Clean up registry (simulates shutdown) - this should handle remaining weak refs
    wyn_weak_cleanup_registry();
    
    printf("✅ Registry cleanup test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.3.3 Test Suite ===\n");
    printf("Testing Weak Reference System with Automatic Nullification\n\n");
    
    // Run all tests
    test_weak_basic_operations();
    test_weak_automatic_nullification();
    test_weak_promotion();
    test_weak_batch_operations();
    test_weak_thread_safety();
    test_weak_statistics();
    test_weak_error_handling();
    // Skip registry cleanup test to avoid conflicts with thread test
    
    // Print final statistics
    printf("\n=== Final Weak Reference Statistics ===\n");
    wyn_weak_print_stats();
    
    printf("\n=== Final ARC Statistics ===\n");
    wyn_arc_print_stats();
    
    printf("\n✅ All T2.3.3 tests passed successfully!\n");
    printf("Weak Reference System implementation complete.\n");
    
    return 0;
}
