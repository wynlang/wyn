#include "wyn_string.h"
#include "safe_memory.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s... ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("✅ PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("❌ FAILED\n"); \
        } \
    } while(0)

// Test string length method
bool test_string_method_length() {
    WynString* str = wyn_string_new("Hello World");
    if (!str) return false;
    
    size_t length = wyn_string_method_length(str);
    bool success = (length == 11);
    
    wyn_string_free(str);
    return success;
}

// Test string upper method
bool test_string_method_upper() {
    WynString* str = wyn_string_new("Hello World");
    if (!str) return false;
    
    WynString* upper = wyn_string_method_upper(str);
    if (!upper) {
        wyn_string_free(str);
        return false;
    }
    
    bool success = (strcmp(upper->data, "HELLO WORLD") == 0);
    
    wyn_string_free(str);
    wyn_string_free(upper);
    return success;
}

// Test string lower method
bool test_string_method_lower() {
    WynString* str = wyn_string_new("Hello World");
    if (!str) return false;
    
    WynString* lower = wyn_string_method_lower(str);
    if (!lower) {
        wyn_string_free(str);
        return false;
    }
    
    bool success = (strcmp(lower->data, "hello world") == 0);
    
    wyn_string_free(str);
    wyn_string_free(lower);
    return success;
}

// Test string trim method
bool test_string_method_trim() {
    WynString* str = wyn_string_new("  Hello World  ");
    if (!str) return false;
    
    WynString* trimmed = wyn_string_method_trim(str);
    if (!trimmed) {
        wyn_string_free(str);
        return false;
    }
    
    bool success = (strcmp(trimmed->data, "Hello World") == 0);
    
    wyn_string_free(str);
    wyn_string_free(trimmed);
    return success;
}

// Test string contains method
bool test_string_method_contains() {
    WynString* str = wyn_string_new("Hello World");
    WynString* substr1 = wyn_string_new("World");
    WynString* substr2 = wyn_string_new("xyz");
    
    if (!str || !substr1 || !substr2) {
        wyn_string_free(str);
        wyn_string_free(substr1);
        wyn_string_free(substr2);
        return false;
    }
    
    bool contains_world = wyn_string_method_contains(str, substr1);
    bool contains_xyz = wyn_string_method_contains(str, substr2);
    
    bool success = contains_world && !contains_xyz;
    
    wyn_string_free(str);
    wyn_string_free(substr1);
    wyn_string_free(substr2);
    return success;
}

// Test edge cases
bool test_method_edge_cases() {
    // Test with NULL inputs
    size_t null_length = wyn_string_method_length(NULL);
    if (null_length != 0) return false;
    
    WynString* null_upper = wyn_string_method_upper(NULL);
    if (null_upper != NULL) return false;
    
    // Test with empty string
    WynString* empty = wyn_string_new("");
    if (!empty) return false;
    
    size_t empty_length = wyn_string_method_length(empty);
    WynString* empty_trimmed = wyn_string_method_trim(empty);
    
    bool success = (empty_length == 0) && 
                   (empty_trimmed && empty_trimmed->length == 0);
    
    wyn_string_free(empty);
    wyn_string_free(empty_trimmed);
    return success;
}

// Test trim with various whitespace
bool test_trim_whitespace() {
    WynString* str1 = wyn_string_new("\t\n  Hello  \r\n\t");
    if (!str1) return false;
    
    WynString* trimmed1 = wyn_string_method_trim(str1);
    if (!trimmed1) {
        wyn_string_free(str1);
        return false;
    }
    
    bool success = (strcmp(trimmed1->data, "Hello") == 0);
    
    wyn_string_free(str1);
    wyn_string_free(trimmed1);
    return success;
}

// Test case conversion with mixed case
bool test_case_conversion() {
    WynString* mixed = wyn_string_new("HeLLo WoRLd");
    if (!mixed) return false;
    
    WynString* upper = wyn_string_method_upper(mixed);
    WynString* lower = wyn_string_method_lower(mixed);
    
    if (!upper || !lower) {
        wyn_string_free(mixed);
        wyn_string_free(upper);
        wyn_string_free(lower);
        return false;
    }
    
    bool success = (strcmp(upper->data, "HELLO WORLD") == 0) &&
                   (strcmp(lower->data, "hello world") == 0);
    
    wyn_string_free(mixed);
    wyn_string_free(upper);
    wyn_string_free(lower);
    return success;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.3.4: String Method Integration\n");
    printf("============================================\n\n");
    
    TEST(string_method_length);
    TEST(string_method_upper);
    TEST(string_method_lower);
    TEST(string_method_trim);
    TEST(string_method_contains);
    TEST(method_edge_cases);
    TEST(trim_whitespace);
    TEST(case_conversion);
    
    printf("\n============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.3.4 string method integration tests PASSED!\n");
        printf("T1.3.4: String Method Integration - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.3.4 tests FAILED!\n");
        return 1;
    }
}
