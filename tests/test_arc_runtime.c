#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../src/arc_runtime.h"
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.3.1: Object Header Design
// Comprehensive test suite for ARC runtime object header implementation

// Custom destructor for testing
static int destructor_call_count = 0;

void test_destructor(void* data) {
    (void)data; // Suppress unused parameter warning
    destructor_call_count++;
    // Custom cleanup logic would go here
}

// Test 1: Basic object allocation and validation
void test_arc_basic_allocation() {
    printf("Testing basic ARC allocation...\n");
    
    // Reset stats
    wyn_arc_reset_stats();
    
    // Allocate object
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_STRING, NULL);
    assert(obj != NULL);
    
    // Validate object header
    assert(wyn_arc_is_valid(obj));
    assert(wyn_arc_get_ref_count(obj) == 1);
    assert(wyn_arc_get_type_id(obj) == WYN_TYPE_STRING);
    assert(wyn_arc_get_size(obj) == 64);
    assert(wyn_arc_get_data(obj) != NULL);
    
    // Check statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.total_allocations == 1);
    assert(stats.current_objects == 1);
    
    // Release object
    wyn_arc_release(obj);
    
    // Check final statistics
    stats = wyn_arc_get_stats();
    assert(stats.total_deallocations == 1);
    assert(stats.current_objects == 0);
    
    printf("✅ Basic allocation test passed\n");
}

// Test 2: Reference counting operations
void test_arc_reference_counting() {
    printf("Testing ARC reference counting...\n");
    
    wyn_arc_reset_stats();
    
    // Allocate object
    WynObject* obj = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Retain object multiple times
    WynObject* obj2 = wyn_arc_retain(obj);
    assert(obj2 == obj);
    assert(wyn_arc_get_ref_count(obj) == 2);
    
    WynObject* obj3 = wyn_arc_retain(obj);
    assert(obj3 == obj);
    assert(wyn_arc_get_ref_count(obj) == 3);
    
    // Release objects
    wyn_arc_release(obj3);
    assert(wyn_arc_get_ref_count(obj) == 2);
    
    wyn_arc_release(obj2);
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Final release should deallocate
    wyn_arc_release(obj);
    
    // Check statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.current_objects == 0);
    assert(stats.total_deallocations == 1);
    
    printf("✅ Reference counting test passed\n");
}

// Test 3: Custom destructor functionality
void test_arc_custom_destructor() {
    printf("Testing ARC custom destructor...\n");
    
    destructor_call_count = 0;
    wyn_arc_reset_stats();
    
    // Allocate object with custom destructor
    WynObject* obj = wyn_arc_alloc(16, WYN_TYPE_CUSTOM_BASE, test_destructor);
    assert(obj != NULL);
    
    // Release object - should call destructor
    wyn_arc_release(obj);
    
    // Verify destructor was called
    assert(destructor_call_count == 1);
    
    printf("✅ Custom destructor test passed\n");
}

// Test 4: Weak reference operations
void test_arc_weak_references() {
    printf("Testing ARC weak references...\n");
    
    wyn_arc_reset_stats();
    
    // Allocate object
    WynObject* obj = wyn_arc_alloc(8, WYN_TYPE_BOOL, NULL);
    assert(obj != NULL);
    
    // Create weak reference
    WynObject* weak_obj = wyn_arc_weak_retain(obj);
    assert(weak_obj == obj);
    
    // Reference count should still be 1 (weak references don't count)
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    // Release weak reference
    wyn_arc_weak_release(weak_obj);
    
    // Release strong reference
    wyn_arc_release(obj);
    
    // Check statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.current_objects == 0);
    
    printf("✅ Weak reference test passed\n");
}

// Test 5: Error handling and validation
void test_arc_error_handling() {
    printf("Testing ARC error handling...\n");
    
    clear_errors();
    
    // Test NULL object handling
    assert(wyn_arc_retain(NULL) == NULL);
    wyn_arc_release(NULL);
    wyn_arc_weak_retain(NULL);
    wyn_arc_weak_release(NULL);
    
    // Test zero-size allocation
    WynObject* obj = wyn_arc_alloc(0, WYN_TYPE_INT, NULL);
    assert(obj == NULL);
    assert(has_errors());
    clear_errors();
    
    // Test invalid object validation
    assert(!wyn_arc_is_valid(NULL));
    
    // Create a fake object with wrong magic
    WynObject fake_obj;
    fake_obj.header.magic = 0xDEAD;
    assert(!wyn_arc_is_valid(&fake_obj));
    
    printf("✅ Error handling test passed\n");
}

// Test 6: Memory statistics tracking
void test_arc_statistics() {
    printf("Testing ARC statistics tracking...\n");
    
    wyn_arc_reset_stats();
    
    // Allocate multiple objects
    WynObject* obj1 = wyn_arc_alloc(100, WYN_TYPE_ARRAY, NULL);
    WynObject* obj2 = wyn_arc_alloc(200, WYN_TYPE_STRUCT, NULL);
    WynObject* obj3 = wyn_arc_alloc(50, WYN_TYPE_FUNCTION, NULL);
    
    assert(obj1 && obj2 && obj3);
    
    // Check statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.total_allocations == 3);
    assert(stats.current_objects == 3);
    assert(stats.peak_objects == 3);
    
    // Release one object
    wyn_arc_release(obj2);
    
    stats = wyn_arc_get_stats();
    assert(stats.total_deallocations == 1);
    assert(stats.current_objects == 2);
    assert(stats.peak_objects == 3);  // Peak should remain
    
    // Release remaining objects
    wyn_arc_release(obj1);
    wyn_arc_release(obj3);
    
    stats = wyn_arc_get_stats();
    assert(stats.total_deallocations == 3);
    assert(stats.current_objects == 0);
    
    printf("✅ Statistics tracking test passed\n");
}

// Test 7: Data access and manipulation
void test_arc_data_access() {
    printf("Testing ARC data access...\n");
    
    // Allocate object for string data
    WynObject* obj = wyn_arc_alloc(256, WYN_TYPE_STRING, NULL);
    assert(obj != NULL);
    
    // Get data pointer and write test data
    char* data = (char*)wyn_arc_get_data(obj);
    assert(data != NULL);
    
    strcpy(data, "Hello, ARC Runtime!");
    assert(strcmp(data, "Hello, ARC Runtime!") == 0);
    
    // Verify data persists
    char* data2 = (char*)wyn_arc_get_data(obj);
    assert(strcmp(data2, "Hello, ARC Runtime!") == 0);
    
    wyn_arc_release(obj);
    
    printf("✅ Data access test passed\n");
}

// Test 8: Thread safety (basic test)
void test_arc_thread_safety() {
    printf("Testing ARC thread safety (basic)...\n");
    
    // This is a basic test - full thread safety would require pthread testing
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    
    // Simulate concurrent retain/release operations
    for (int i = 0; i < 100; i++) {
        wyn_arc_retain(obj);
        wyn_arc_release(obj);
    }
    
    // Object should still have reference count of 1
    assert(wyn_arc_get_ref_count(obj) == 1);
    
    wyn_arc_release(obj);
    
    printf("✅ Basic thread safety test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.3.1 Test Suite ===\n");
    
    // Run all tests
    test_arc_basic_allocation();
    test_arc_reference_counting();
    test_arc_custom_destructor();
    test_arc_weak_references();
    test_arc_error_handling();
    test_arc_statistics();
    test_arc_data_access();
    test_arc_thread_safety();
    
    // Print final statistics
    printf("\n=== Final ARC Statistics ===\n");
    wyn_arc_print_stats();
    
    printf("\n✅ All T2.3.1 tests passed successfully!\n");
    printf("ARC Runtime Object Header Design implementation complete.\n");
    
    return 0;
}