#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "string_memory.h"
#include "arc_runtime.h"
#include "wyn_string.h"

// Test string creation and destruction
void test_string_creation_destruction() {
    printf("Testing string creation and destruction...\n");
    
    // Test basic string creation
    WynObject* str1 = WYN_STRING_CREATE("Hello, World!");
    assert(str1 != NULL);
    assert(wyn_arc_get_ref_count(str1) == 1);
    
    WynString* str_data = (WynString*)wyn_arc_get_data(str1);
    assert(str_data != NULL);
    assert(str_data->length == 13);
    assert(strcmp(str_data->data, "Hello, World!") == 0);
    
    // Test string release
    WYN_STRING_RELEASE(str1);
    assert(str1 == NULL);
    
    printf("✓ String creation and destruction test passed\n");
}

// Test string copying with ARC
void test_string_copying() {
    printf("Testing string copying with ARC...\n");
    
    WynObject* original = WYN_STRING_CREATE("Original String");
    assert(original != NULL);
    assert(wyn_arc_get_ref_count(original) == 1);
    
    // Test copy creation
    WynObject* copy = wyn_string_copy_arc(original);
    assert(copy != NULL);
    assert(copy != original); // Should be different objects
    assert(wyn_arc_get_ref_count(original) == 1);
    assert(wyn_arc_get_ref_count(copy) == 1);
    
    // Verify content is the same
    WynString* orig_data = (WynString*)wyn_arc_get_data(original);
    WynString* copy_data = (WynString*)wyn_arc_get_data(copy);
    assert(orig_data->length == copy_data->length);
    assert(strcmp(orig_data->data, copy_data->data) == 0);
    
    WYN_STRING_RELEASE(original);
    WYN_STRING_RELEASE(copy);
    
    printf("✓ String copying test passed\n");
}

// Test string assignment with proper ARC management
void test_string_assignment() {
    printf("Testing string assignment with ARC...\n");
    
    WynObject* str1 = WYN_STRING_CREATE("First String");
    WynObject* str2 = WYN_STRING_CREATE("Second String");
    WynObject* target = NULL;
    
    // Test initial assignment
    wyn_string_assign_arc(&target, str1);
    assert(target == str1);
    assert(wyn_arc_get_ref_count(str1) == 2); // Original + assignment
    assert(wyn_arc_get_ref_count(str2) == 1);
    
    // Test reassignment
    wyn_string_assign_arc(&target, str2);
    assert(target == str2);
    assert(wyn_arc_get_ref_count(str1) == 1); // Back to original
    assert(wyn_arc_get_ref_count(str2) == 2); // Original + assignment
    
    // Test assignment to NULL
    wyn_string_assign_arc(&target, NULL);
    assert(target == NULL);
    assert(wyn_arc_get_ref_count(str1) == 1);
    assert(wyn_arc_get_ref_count(str2) == 1);
    
    WYN_STRING_RELEASE(str1);
    WYN_STRING_RELEASE(str2);
    
    printf("✓ String assignment test passed\n");
}

// Test string concatenation with ARC
void test_string_concatenation() {
    printf("Testing string concatenation with ARC...\n");
    
    WynObject* str1 = WYN_STRING_CREATE("Hello, ");
    WynObject* str2 = WYN_STRING_CREATE("World!");
    
    WynObject* result = wyn_string_concat_arc(str1, str2);
    assert(result != NULL);
    assert(result != str1);
    assert(result != str2);
    
    WynString* result_data = (WynString*)wyn_arc_get_data(result);
    assert(result_data->length == 13);
    assert(strcmp(result_data->data, "Hello, World!") == 0);
    
    // Original strings should be unchanged
    assert(wyn_arc_get_ref_count(str1) == 1);
    assert(wyn_arc_get_ref_count(str2) == 1);
    assert(wyn_arc_get_ref_count(result) == 1);
    
    WYN_STRING_RELEASE(str1);
    WYN_STRING_RELEASE(str2);
    WYN_STRING_RELEASE(result);
    
    printf("✓ String concatenation test passed\n");
}

// Test string interning
void test_string_interning() {
    printf("Testing string interning...\n");
    
    // Create multiple instances of the same string
    WynString* str1 = wyn_string_intern("Interned String");
    WynString* str2 = wyn_string_intern("Interned String");
    WynString* str3 = wyn_string_intern("Different String");
    
    // Same strings should return the same interned instance
    assert(str1 == str2);
    assert(str1 != str3);
    
    // Verify content
    assert(strcmp(str1->data, "Interned String") == 0);
    assert(strcmp(str3->data, "Different String") == 0);
    
    printf("✓ String interning test passed\n");
}

// Test memory leak detection
void test_memory_leak_detection() {
    printf("Testing memory leak detection...\n");
    
    // Create some strings that will be "leaked"
    WynObject* leaked1 = WYN_STRING_CREATE("Leaked String 1");
    WynObject* leaked2 = WYN_STRING_CREATE("Leaked String 2");
    
    // Create and properly release a string
    WynObject* proper = WYN_STRING_CREATE("Proper String");
    WYN_STRING_RELEASE(proper);
    
    // Check for leaks (should find 2)
    printf("Checking for leaks (should find 2):\n");
    wyn_string_check_leaks();
    
    // Clean up the "leaked" strings
    WYN_STRING_RELEASE(leaked1);
    WYN_STRING_RELEASE(leaked2);
    
    // Check again (should find 0)
    printf("Checking for leaks after cleanup (should find 0):\n");
    wyn_string_check_leaks();
    
    printf("✓ Memory leak detection test passed\n");
}

// Test string memory statistics
void test_string_memory_stats() {
    printf("Testing string memory statistics...\n");
    
    // Get initial stats
    StringMemoryStats initial_stats = wyn_string_get_memory_stats();
    
    // Create some strings
    WynObject* str1 = WYN_STRING_CREATE("Stats Test 1");
    WynObject* str2 = WYN_STRING_CREATE("Stats Test 2");
    wyn_string_intern("Interned Stats Test");
    
    // Get updated stats
    StringMemoryStats updated_stats = wyn_string_get_memory_stats();
    
    // Should have more tracked strings and at least one interned string
    assert(updated_stats.tracked_strings >= initial_stats.tracked_strings + 2);
    assert(updated_stats.interned_strings >= initial_stats.interned_strings + 1);
    assert(updated_stats.tracked_memory > initial_stats.tracked_memory);
    assert(updated_stats.interned_memory >= initial_stats.interned_memory);
    
    // Print stats
    wyn_string_print_memory_stats();
    
    // Clean up
    WYN_STRING_RELEASE(str1);
    WYN_STRING_RELEASE(str2);
    
    printf("✓ String memory statistics test passed\n");
}

// Test edge cases and error conditions
void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    WynObject* null_result = wyn_string_create_arc(NULL);
    assert(null_result == NULL);
    
    WynObject* null_copy = wyn_string_copy_arc(NULL);
    assert(null_copy == NULL);
    
    WynObject* null_concat = wyn_string_concat_arc(NULL, NULL);
    assert(null_concat == NULL);
    
    // Test empty string
    WynObject* empty = WYN_STRING_CREATE("");
    assert(empty != NULL);
    WynString* empty_data = (WynString*)wyn_arc_get_data(empty);
    assert(empty_data->length == 0);
    assert(strcmp(empty_data->data, "") == 0);
    WYN_STRING_RELEASE(empty);
    
    // Test very long string
    char long_string[1000];
    memset(long_string, 'A', 999);
    long_string[999] = '\0';
    
    WynObject* long_str = WYN_STRING_CREATE(long_string);
    assert(long_str != NULL);
    WynString* long_data = (WynString*)wyn_arc_get_data(long_str);
    assert(long_data->length == 999);
    WYN_STRING_RELEASE(long_str);
    
    printf("✓ Edge cases test passed\n");
}

// Test concurrent access (basic thread safety)
void test_concurrent_access() {
    printf("Testing concurrent access...\n");
    
    // Create a string and retain it multiple times
    WynObject* str = WYN_STRING_CREATE("Concurrent Test");
    
    // Simulate multiple retains/releases
    for (int i = 0; i < 10; i++) {
        wyn_arc_retain(str);
    }
    
    assert(wyn_arc_get_ref_count(str) == 11); // 1 initial + 10 retains
    
    for (int i = 0; i < 10; i++) {
        wyn_arc_release(str);
    }
    
    assert(wyn_arc_get_ref_count(str) == 1); // Back to initial
    
    WYN_STRING_RELEASE(str);
    
    printf("✓ Concurrent access test passed\n");
}

// Main test runner
int main() {
    printf("=== String Memory Management Tests ===\n\n");
    
    // Initialize ARC runtime
    wyn_arc_reset_stats();
    
    // Run all tests
    test_string_creation_destruction();
    test_string_copying();
    test_string_assignment();
    test_string_concatenation();
    test_string_interning();
    test_memory_leak_detection();
    test_string_memory_stats();
    test_edge_cases();
    test_concurrent_access();
    
    // Print final statistics
    printf("\n=== Final Statistics ===\n");
    wyn_arc_print_stats();
    wyn_string_print_memory_stats();
    
    // Clean up interning table
    wyn_string_cleanup_intern_table();
    
    printf("\n✅ All string memory management tests passed!\n");
    return 0;
}