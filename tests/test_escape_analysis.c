#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/arc_runtime.h"
#include "../src/escape_analysis.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.4.1: Escape Analysis Implementation
// Comprehensive test suite for escape analysis implementation

// Test 1: Basic escape analysis initialization
void test_escape_analysis_init() {
    printf("Testing escape analysis initialization...\n");
    
    wyn_escape_analysis_init();
    
    // Check that we can get stats without crashing
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.total_allocation_sites == 0);
    assert(stats.stack_allocatable_sites == 0);
    assert(stats.eliminated_retain_release_pairs == 0);
    
    printf("✅ Escape analysis initialization test passed\n");
}

// Test 2: Allocation site registration
void test_allocation_site_registration() {
    printf("Testing allocation site registration...\n");
    
    wyn_escape_reset();
    
    // Register some allocation sites
    AllocationSite* site1 = wyn_escape_register_allocation((void*)0x1000, 64, WYN_TYPE_STRING);
    AllocationSite* site2 = wyn_escape_register_allocation((void*)0x2000, 128, WYN_TYPE_ARRAY);
    AllocationSite* site3 = wyn_escape_register_allocation((void*)0x3000, 32, WYN_TYPE_INT);
    
    assert(site1 != NULL);
    assert(site2 != NULL);
    assert(site3 != NULL);
    
    // Check statistics
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.total_allocation_sites == 3);
    
    printf("✅ Allocation site registration test passed\n");
}

// Test 3: Reference counting for escape analysis
void test_reference_counting() {
    printf("Testing reference counting for escape analysis...\n");
    
    wyn_escape_reset();
    
    // Register allocation site
    AllocationSite* site = wyn_escape_register_allocation((void*)0x4000, 48, WYN_TYPE_STRUCT);
    assert(site != NULL);
    
    // Add references
    wyn_escape_add_reference(site);
    wyn_escape_add_reference(site);
    wyn_escape_add_reference(site);
    
    // The reference count should be tracked internally
    // We can't directly access it, but it affects escape analysis
    
    printf("✅ Reference counting test passed\n");
}

// Test 4: Escape analysis execution
void test_escape_analysis_execution() {
    printf("Testing escape analysis execution...\n");
    
    wyn_escape_reset();
    
    // Register allocation sites with different characteristics
    AllocationSite* small_site = wyn_escape_register_allocation((void*)0x5000, 32, WYN_TYPE_INT);
    AllocationSite* large_site = wyn_escape_register_allocation((void*)0x6000, 1024, WYN_TYPE_ARRAY);
    
    assert(small_site && large_site);
    
    // Add single reference to small site (likely stack allocatable)
    wyn_escape_add_reference(small_site);
    
    // Add many references to large site (likely heap allocated)
    for (int i = 0; i < 5; i++) {
        wyn_escape_add_reference(large_site);
    }
    
    // Run escape analysis
    wyn_escape_analyze_all();
    
    // Check results
    bool small_can_stack = wyn_escape_can_stack_allocate(small_site);
    bool large_can_stack = wyn_escape_can_stack_allocate(large_site);
    
    // Small object with single reference should be stack allocatable
    assert(small_can_stack == true);
    
    // Large object with many references should not be stack allocatable
    assert(large_can_stack == false);
    
    printf("✅ Escape analysis execution test passed\n");
}

// Test 5: Retain/release optimization
void test_retain_release_optimization() {
    printf("Testing retain/release optimization...\n");
    
    wyn_escape_reset();
    
    // Register allocation site for small object
    AllocationSite* site = wyn_escape_register_allocation((void*)0x7000, 16, WYN_TYPE_BOOL);
    assert(site != NULL);
    
    // Add single reference
    wyn_escape_add_reference(site);
    
    // Run escape analysis
    wyn_escape_analyze_all();
    
    // Check if retain/release can be optimized away
    bool needs_retain_release = wyn_escape_needs_retain_release(site);
    
    // Small object with single reference should not need retain/release
    assert(needs_retain_release == false);
    
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.eliminated_retain_release_pairs >= 1);
    
    printf("✅ Retain/release optimization test passed\n");
}

// Test 6: Stack allocation simulation
void test_stack_allocation() {
    printf("Testing stack allocation simulation...\n");
    
    // Test stack allocation functions
    void* ptr1 = wyn_stack_alloc(64);
    void* ptr2 = wyn_stack_alloc(128);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr1 != ptr2);
    
    // Write test patterns
    memset(ptr1, 0xAA, 64);
    memset(ptr2, 0xBB, 128);
    
    // Verify patterns
    uint8_t* data1 = (uint8_t*)ptr1;
    uint8_t* data2 = (uint8_t*)ptr2;
    
    for (int i = 0; i < 64; i++) {
        assert(data1[i] == 0xAA);
    }
    
    for (int i = 0; i < 128; i++) {
        assert(data2[i] == 0xBB);
    }
    
    // Clean up
    wyn_stack_free(ptr1);
    wyn_stack_free(ptr2);
    
    printf("✅ Stack allocation test passed\n");
}

// Test 7: Optimized ARC operations
void test_optimized_arc_operations() {
    printf("Testing optimized ARC operations...\n");
    
    wyn_escape_reset();
    
    // Create objects using optimized allocation
    WynObject* obj1 = wyn_arc_alloc_optimized(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc_optimized(64, WYN_TYPE_ARRAY, NULL);
    
    assert(obj1 && obj2);
    
    // Verify objects are valid
    assert(wyn_arc_is_valid(obj1));
    assert(wyn_arc_is_valid(obj2));
    
    // Test optimized retain/release (without specific sites for now)
    WynObject* retained = wyn_arc_retain_escape_optimized(obj1, NULL);
    assert(retained == obj1);
    
    wyn_arc_release_escape_optimized(retained, NULL);
    
    // Clean up
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    // Check that allocation sites were registered
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.total_allocation_sites >= 2);
    
    printf("✅ Optimized ARC operations test passed\n");
}

// Test 8: Statistics and reporting
void test_statistics_reporting() {
    printf("Testing statistics and reporting...\n");
    
    wyn_escape_reset();
    
    // Create various allocation scenarios
    AllocationSite* sites[5];
    for (int i = 0; i < 5; i++) {
        sites[i] = wyn_escape_register_allocation((void*)(0x8000 + i * 0x1000), 
                                                 32 + i * 16, WYN_TYPE_INT);
        assert(sites[i] != NULL);
        
        // Add different numbers of references
        for (int j = 0; j <= i; j++) {
            wyn_escape_add_reference(sites[i]);
        }
    }
    
    // Run escape analysis
    wyn_escape_analyze_all();
    
    // Get and verify statistics
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.total_allocation_sites == 5);
    assert(stats.stack_allocatable_sites > 0);
    assert(stats.stack_allocation_ratio >= 0.0 && stats.stack_allocation_ratio <= 1.0);
    assert(stats.optimization_ratio >= 0.0 && stats.optimization_ratio <= 1.0);
    
    // Print statistics for verification
    wyn_escape_print_stats();
    
    printf("✅ Statistics reporting test passed\n");
}

// Test 9: LLVM optimization pass integration
void test_llvm_optimization_integration() {
    printf("Testing LLVM optimization pass integration...\n");
    
    wyn_escape_reset();
    
    // Register some allocation sites
    AllocationSite* site1 = wyn_escape_register_allocation((void*)0x9000, 24, WYN_TYPE_FLOAT);
    AllocationSite* site2 = wyn_escape_register_allocation((void*)0xA000, 48, WYN_TYPE_STRING);
    
    assert(site1 && site2);
    
    wyn_escape_add_reference(site1);
    wyn_escape_add_reference(site2);
    wyn_escape_add_reference(site2);
    
    // Run LLVM optimization pass (which internally runs escape analysis)
    wyn_escape_llvm_optimization_pass();
    
    // Verify analysis was performed
    WynEscapeStats stats = wyn_escape_get_stats();
    assert(stats.total_allocation_sites == 2);
    
    printf("✅ LLVM optimization integration test passed\n");
}

// Test 10: Enable/disable functionality
void test_enable_disable() {
    printf("Testing enable/disable functionality...\n");
    
    // Disable escape analysis
    wyn_escape_set_enabled(false);
    
    // Try to register allocation site (should fail or be ignored)
    AllocationSite* site = wyn_escape_register_allocation((void*)0xB000, 32, WYN_TYPE_INT);
    // site may be NULL when disabled
    
    // Re-enable
    wyn_escape_set_enabled(true);
    
    // Now registration should work
    site = wyn_escape_register_allocation((void*)0xC000, 32, WYN_TYPE_INT);
    assert(site != NULL);
    
    printf("✅ Enable/disable functionality test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.4.1 Test Suite ===\n");
    printf("Testing Escape Analysis Implementation\n\n");
    
    // Run all tests
    test_escape_analysis_init();
    test_allocation_site_registration();
    test_reference_counting();
    test_escape_analysis_execution();
    test_retain_release_optimization();
    test_stack_allocation();
    test_optimized_arc_operations();
    test_statistics_reporting();
    test_llvm_optimization_integration();
    test_enable_disable();
    
    // Print final statistics
    printf("\n=== Final Escape Analysis Statistics ===\n");
    wyn_escape_print_stats();
    
    // Cleanup
    wyn_escape_cleanup();
    
    printf("\n✅ All T2.4.1 tests passed successfully!\n");
    printf("Escape Analysis Implementation complete.\n");
    
    return 0;
}
