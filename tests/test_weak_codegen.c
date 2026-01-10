#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/arc_runtime.h"
#include "../src/weak_codegen.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.4.3: Weak Reference Code Generation
// Comprehensive test suite for weak reference compiler integration

// Test 1: Basic weak reference code generation initialization
void test_weak_codegen_init() {
    printf("Testing weak reference code generation initialization...\n");
    
    wyn_weak_codegen_init();
    
    // Check that we can get stats without crashing
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 0);
    assert(stats.null_checks_generated == 0);
    assert(stats.strong_promotions_generated == 0);
    assert(stats.weak_assignments_generated == 0);
    
    printf("✅ Weak reference code generation initialization test passed\n");
}

// Test 2: Strong to weak reference conversion code generation
void test_strong_to_weak_conversion() {
    printf("Testing strong to weak reference conversion code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test object
    WynObject* obj = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    assert(obj != NULL);
    
    // Generate code for weak reference creation
    void* weak_ref = wyn_weak_codegen_create_weak(obj, (void*)0x1000);
    assert(weak_ref != NULL);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 1);
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref, (void*)0x1000);
    wyn_arc_release(obj);
    
    printf("✅ Strong to weak reference conversion test passed\n");
}

// Test 3: Weak to strong reference promotion code generation
void test_weak_to_strong_promotion() {
    printf("Testing weak to strong reference promotion code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test object
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    assert(obj != NULL);
    
    // Create weak reference
    void* weak_ref = wyn_weak_codegen_create_weak(obj, (void*)0x2000);
    assert(weak_ref != NULL);
    
    // Generate code for strong reference promotion
    void* strong_ref = wyn_weak_codegen_promote_to_strong(weak_ref, (void*)0x2100);
    assert(strong_ref != NULL);
    assert(strong_ref == obj);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 1);
    assert(stats.strong_promotions_generated == 1);
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref, (void*)0x2000);
    wyn_arc_release(strong_ref); // Release promoted reference
    wyn_arc_release(obj);
    
    printf("✅ Weak to strong reference promotion test passed\n");
}

// Test 4: Null checking code generation
void test_null_checking_codegen() {
    printf("Testing null checking code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test object
    WynObject* obj = wyn_arc_alloc(48, WYN_TYPE_STRUCT, NULL);
    assert(obj != NULL);
    
    // Create weak reference
    void* weak_ref = wyn_weak_codegen_create_weak(obj, (void*)0x3000);
    assert(weak_ref != NULL);
    
    // Generate null check code (should be valid)
    bool is_valid = wyn_weak_codegen_null_check(weak_ref, (void*)0x3100);
    assert(is_valid == true);
    
    // Release the object to invalidate weak reference
    wyn_arc_release(obj);
    
    // Generate null check code (should be invalid now)
    is_valid = wyn_weak_codegen_null_check(weak_ref, (void*)0x3200);
    assert(is_valid == false);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.null_checks_generated == 2);
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref, (void*)0x3000);
    
    printf("✅ Null checking code generation test passed\n");
}

// Test 5: Weak reference assignment code generation
void test_weak_assignment_codegen() {
    printf("Testing weak reference assignment code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test objects
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_FLOAT, NULL);
    assert(obj1 && obj2);
    
    // Create source weak references
    void* weak_ref1 = wyn_weak_codegen_create_weak(obj1, (void*)0x4000);
    void* weak_ref2 = wyn_weak_codegen_create_weak(obj2, (void*)0x4100);
    assert(weak_ref1 && weak_ref2);
    
    // Generate assignment code
    void* target_weak_ref = NULL;
    wyn_weak_codegen_assign_weak(&target_weak_ref, weak_ref1, (void*)0x4200);
    assert(target_weak_ref != NULL);
    
    // Reassign to different weak reference
    wyn_weak_codegen_assign_weak(&target_weak_ref, weak_ref2, (void*)0x4300);
    assert(target_weak_ref != NULL);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 2); // Original creations
    assert(stats.weak_assignments_generated == 2); // Two assignments
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref1, (void*)0x4000);
    wyn_weak_codegen_cleanup_weak(weak_ref2, (void*)0x4100);
    wyn_weak_codegen_cleanup_weak(target_weak_ref, (void*)0x4200);
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("✅ Weak reference assignment code generation test passed\n");
}

// Test 6: Automatic conversion code generation
void test_automatic_conversion_codegen() {
    printf("Testing automatic conversion code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test object
    WynObject* obj = wyn_arc_alloc(40, WYN_TYPE_BOOL, NULL);
    assert(obj != NULL);
    
    // Generate automatic strong-to-weak conversion
    void* weak_ref = wyn_weak_codegen_auto_convert_to_weak(obj, (void*)0x5000);
    assert(weak_ref != NULL);
    
    // Generate automatic weak-to-strong conversion
    void* strong_ref = wyn_weak_codegen_auto_convert_to_strong(weak_ref, (void*)0x5100);
    assert(strong_ref != NULL);
    assert(strong_ref == obj);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 1);
    assert(stats.strong_promotions_generated == 1);
    assert(stats.null_checks_generated == 1); // Auto conversion includes null check
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref, (void*)0x5000);
    wyn_arc_release(strong_ref); // Release promoted reference
    wyn_arc_release(obj);
    
    printf("✅ Automatic conversion code generation test passed\n");
}

// Test 7: Safe access code generation
void test_safe_access_codegen() {
    printf("Testing safe access code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test object
    WynObject* obj = wyn_arc_alloc(56, WYN_TYPE_FUNCTION, NULL);
    assert(obj != NULL);
    
    // Create weak reference
    void* weak_ref = wyn_weak_codegen_create_weak(obj, (void*)0x6000);
    assert(weak_ref != NULL);
    
    // Generate safe access code (should succeed)
    void* accessed_obj = wyn_weak_codegen_safe_access(weak_ref, (void*)0x6100);
    assert(accessed_obj == obj);
    
    // Release object to invalidate weak reference
    wyn_arc_release(obj);
    
    // Generate safe access code (should return NULL)
    accessed_obj = wyn_weak_codegen_safe_access(weak_ref, (void*)0x6200);
    assert(accessed_obj == NULL);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.null_checks_generated >= 1); // At least one null check
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref, (void*)0x6000);
    
    printf("✅ Safe access code generation test passed\n");
}

// Test 8: Weak reference comparison code generation
void test_weak_comparison_codegen() {
    printf("Testing weak reference comparison code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test objects
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    assert(obj1 && obj2);
    
    // Create weak references
    void* weak_ref1a = wyn_weak_codegen_create_weak(obj1, (void*)0x7000);
    void* weak_ref1b = wyn_weak_codegen_create_weak(obj1, (void*)0x7100);
    void* weak_ref2 = wyn_weak_codegen_create_weak(obj2, (void*)0x7200);
    assert(weak_ref1a && weak_ref1b && weak_ref2);
    
    // Generate comparison code
    bool same_object = wyn_weak_codegen_compare_weak(weak_ref1a, weak_ref1b, (void*)0x7300);
    assert(same_object == true); // Both point to obj1
    
    bool different_objects = wyn_weak_codegen_compare_weak(weak_ref1a, weak_ref2, (void*)0x7400);
    assert(different_objects == false); // Point to different objects
    
    // Test null comparisons
    bool null_comparison = wyn_weak_codegen_compare_weak(NULL, NULL, (void*)0x7500);
    assert(null_comparison == true); // Both null
    
    bool null_vs_valid = wyn_weak_codegen_compare_weak(weak_ref1a, NULL, (void*)0x7600);
    assert(null_vs_valid == false); // One null, one valid
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.null_checks_generated >= 8); // Multiple comparisons with null checks
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref1a, (void*)0x7000);
    wyn_weak_codegen_cleanup_weak(weak_ref1b, (void*)0x7100);
    wyn_weak_codegen_cleanup_weak(weak_ref2, (void*)0x7200);
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("✅ Weak reference comparison code generation test passed\n");
}

// Test 9: Weak reference array handling code generation
void test_weak_array_codegen() {
    printf("Testing weak reference array handling code generation...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test objects
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_FLOAT, NULL);
    assert(obj1 && obj2);
    
    // Create weak references
    void* weak_ref1 = wyn_weak_codegen_create_weak(obj1, (void*)0x8000);
    void* weak_ref2 = wyn_weak_codegen_create_weak(obj2, (void*)0x8100);
    assert(weak_ref1 && weak_ref2);
    
    // Create weak reference array
    void* weak_array[4] = {NULL, NULL, NULL, NULL};
    
    // Generate array assignment code
    wyn_weak_codegen_array_set_weak(weak_array, 0, weak_ref1, (void*)0x8200);
    wyn_weak_codegen_array_set_weak(weak_array, 1, weak_ref2, (void*)0x8300);
    wyn_weak_codegen_array_set_weak(weak_array, 2, NULL, (void*)0x8400); // Null assignment
    
    assert(weak_array[0] != NULL);
    assert(weak_array[1] != NULL);
    assert(weak_array[2] == NULL);
    assert(weak_array[3] == NULL);
    
    // Reassign array element
    wyn_weak_codegen_array_set_weak(weak_array, 0, weak_ref2, (void*)0x8500);
    
    // Check statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_assignments_generated == 4); // Four array assignments
    
    // Clean up array
    for (int i = 0; i < 4; i++) {
        if (weak_array[i]) {
            wyn_weak_codegen_cleanup_weak(weak_array[i], (void*)(0x8600 + i));
        }
    }
    
    // Clean up original weak references
    wyn_weak_codegen_cleanup_weak(weak_ref1, (void*)0x8000);
    wyn_weak_codegen_cleanup_weak(weak_ref2, (void*)0x8100);
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("✅ Weak reference array handling code generation test passed\n");
}

// Test 10: Statistics and reporting
void test_weak_codegen_statistics() {
    printf("Testing weak reference code generation statistics...\n");
    
    wyn_weak_codegen_reset();
    
    // Create test objects and perform various operations
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    assert(obj1 && obj2);
    
    // Perform various code generation operations
    void* weak_ref1 = wyn_weak_codegen_create_weak(obj1, (void*)0x9000);
    void* weak_ref2 = wyn_weak_codegen_create_weak(obj2, (void*)0x9100);
    
    void* strong_ref1 = wyn_weak_codegen_promote_to_strong(weak_ref1, (void*)0x9200);
    void* strong_ref2 = wyn_weak_codegen_promote_to_strong(weak_ref2, (void*)0x9300);
    
    wyn_weak_codegen_null_check(weak_ref1, (void*)0x9400);
    wyn_weak_codegen_null_check(weak_ref2, (void*)0x9500);
    
    void* target_weak = NULL;
    wyn_weak_codegen_assign_weak(&target_weak, weak_ref1, (void*)0x9600);
    
    // Get and verify statistics
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    assert(stats.weak_conversions_generated == 2);
    assert(stats.strong_promotions_generated == 2);
    assert(stats.null_checks_generated >= 2);
    assert(stats.weak_assignments_generated == 1);
    
    // Check calculated ratios
    assert(stats.null_safety_ratio > 0.0);
    assert(stats.conversion_efficiency > 0.0);
    
    // Print statistics for verification
    wyn_weak_codegen_print_stats();
    
    // Clean up
    wyn_weak_codegen_cleanup_weak(weak_ref1, (void*)0x9000);
    wyn_weak_codegen_cleanup_weak(weak_ref2, (void*)0x9100);
    wyn_weak_codegen_cleanup_weak(target_weak, (void*)0x9600);
    wyn_arc_release(strong_ref1);
    wyn_arc_release(strong_ref2);
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("✅ Weak reference code generation statistics test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.4.3 Test Suite ===\n");
    printf("Testing Weak Reference Code Generation\n\n");
    
    // Run all tests
    test_weak_codegen_init();
    test_strong_to_weak_conversion();
    test_weak_to_strong_promotion();
    test_null_checking_codegen();
    test_weak_assignment_codegen();
    test_automatic_conversion_codegen();
    test_safe_access_codegen();
    test_weak_comparison_codegen();
    test_weak_array_codegen();
    test_weak_codegen_statistics();
    
    // Print final statistics
    printf("\n=== Final Weak Reference Code Generation Statistics ===\n");
    wyn_weak_codegen_print_stats();
    
    // Cleanup
    wyn_weak_codegen_cleanup();
    
    printf("\n✅ All T2.4.3 tests passed successfully!\n");
    printf("Weak Reference Code Generation implementation complete.\n");
    
    return 0;
}
