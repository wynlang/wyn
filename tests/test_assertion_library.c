#include "test.h"
#include "safe_memory.h"
#include <stdio.h>
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

// Test assert_equal function
bool test_assert_equal() {
    // Test passing case
    if (!assert_equal(42, 42, "Equal values should pass")) return false;
    
    // Test failing case (should return false but not crash)
    if (assert_equal(42, 43, "Different values should fail")) return false;
    
    return true;
}

// Test assert_not_equal function
bool test_assert_not_equal() {
    // Test passing case
    if (!assert_not_equal(42, 43, "Different values should pass")) return false;
    
    // Test failing case
    if (assert_not_equal(42, 42, "Equal values should fail")) return false;
    
    return true;
}

// Test assert_true and assert_false functions
bool test_assert_bool() {
    // Test assert_true
    if (!assert_true(true, "True condition should pass")) return false;
    if (assert_true(false, "False condition should fail")) return false;
    
    // Test assert_false
    if (!assert_false(false, "False condition should pass")) return false;
    if (assert_false(true, "True condition should fail")) return false;
    
    return true;
}

// Test assert_null and assert_not_null functions
bool test_assert_null() {
    void* null_ptr = NULL;
    void* non_null_ptr = &tests_run;
    
    // Test assert_null
    if (!assert_null(null_ptr, "NULL pointer should pass")) return false;
    if (assert_null(non_null_ptr, "Non-NULL pointer should fail")) return false;
    
    // Test assert_not_null
    if (!assert_not_null(non_null_ptr, "Non-NULL pointer should pass")) return false;
    if (assert_not_null(null_ptr, "NULL pointer should fail")) return false;
    
    return true;
}

// Test assert_float_equal function
bool test_assert_float_equal() {
    // Test passing case with exact match
    if (!assert_float_equal(3.14159, 3.14159, 0.00001, "Exact match should pass")) return false;
    
    // Test passing case within epsilon
    if (!assert_float_equal(3.14159, 3.14160, 0.0001, "Within epsilon should pass")) return false;
    
    // Test failing case outside epsilon
    if (assert_float_equal(3.14159, 3.15159, 0.001, "Outside epsilon should fail")) return false;
    
    return true;
}

// Test assert_string_equal function
bool test_assert_string_equal() {
    // Test passing case
    if (!assert_string_equal("hello", "hello", "Equal strings should pass")) return false;
    
    // Test failing case
    if (assert_string_equal("hello", "world", "Different strings should fail")) return false;
    
    // Test NULL cases
    if (!assert_string_equal(NULL, NULL, "Both NULL should pass")) return false;
    if (assert_string_equal("hello", NULL, "One NULL should fail")) return false;
    if (assert_string_equal(NULL, "hello", "One NULL should fail")) return false;
    
    return true;
}

// Test assert_greater_than and assert_less_than functions
bool test_assert_comparisons() {
    // Test assert_greater_than
    if (!assert_greater_than(10, 5, "10 > 5 should pass")) return false;
    if (assert_greater_than(5, 10, "5 > 10 should fail")) return false;
    if (assert_greater_than(5, 5, "5 > 5 should fail")) return false;
    
    // Test assert_less_than
    if (!assert_less_than(5, 10, "5 < 10 should pass")) return false;
    if (assert_less_than(10, 5, "10 < 5 should fail")) return false;
    if (assert_less_than(5, 5, "5 < 5 should fail")) return false;
    
    return true;
}

// Test custom error messages
bool test_custom_messages() {
    // Test with custom message (should fail but show custom message)
    printf("\n--- Testing custom error message (expected failure) ---\n");
    bool result = assert_equal(1, 2, "Custom error: Values should be equal for this test");
    printf("--- End of expected failure test ---\n");
    
    // Should return false but not crash
    return !result;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.6.4: Assertion Library\n");
    printf("=====================================\n\n");
    
    TEST(assert_equal);
    TEST(assert_not_equal);
    TEST(assert_bool);
    TEST(assert_null);
    TEST(assert_float_equal);
    TEST(assert_string_equal);
    TEST(assert_comparisons);
    TEST(custom_messages);
    
    printf("\n=====================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.6.4 assertion library tests PASSED!\n");
        printf("T1.6.4: Assertion Library - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.6.4 tests FAILED!\n");
        return 1;
    }
}
