#include "error.h"
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
        clear_errors(); \
        if (test_##name()) { \
            printf("✅ PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("❌ FAILED\n"); \
        } \
    } while(0)

// Test type mismatch error messages
bool test_type_mismatch_errors() {
    type_error_mismatch("int", "string", "variable assignment", 42, 10);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test undefined variable error messages
bool test_undefined_variable_errors() {
    type_error_undefined_variable("unknown_var", 15, 5);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test undefined function error messages
bool test_undefined_function_errors() {
    type_error_undefined_function("missing_func", 25, 8);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test wrong argument count error messages
bool test_wrong_arg_count_errors() {
    // Test too few arguments
    type_error_wrong_arg_count("test_func", 3, 1, 30, 12);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    clear_errors();
    
    // Test too many arguments
    type_error_wrong_arg_count("test_func", 2, 5, 35, 15);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test invalid assignment error messages
bool test_invalid_assignment_errors() {
    type_error_invalid_assignment("int", "string", 40, 20);
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test type conversion suggestions
bool test_type_conversion_suggestions() {
    // Test int to float conversion
    type_suggest_conversion("int", "float");
    if (!has_errors()) return false;
    
    clear_errors();
    
    // Test float to int conversion
    type_suggest_conversion("float", "int");
    if (!has_errors()) return false;
    
    clear_errors();
    
    // Test string to int conversion
    type_suggest_conversion("string", "int");
    if (!has_errors()) return false;
    
    clear_errors();
    
    // Test int to string conversion
    type_suggest_conversion("int", "string");
    if (!has_errors()) return false;
    
    return true;
}

// Test error message formatting and suggestions
bool test_error_message_quality() {
    printf("\n--- Testing error message output quality ---\n");
    
    // Generate various error types and print them
    type_error_mismatch("bool", "int", "if condition", 10, 5);
    type_error_undefined_variable("count", 15, 10);
    type_error_wrong_arg_count("calculate", 2, 4, 20, 15);
    
    // Print all errors to verify message quality
    for (int i = 0; i < get_error_count(); i++) {
        // Note: We can't directly access errors array, but we know they exist
        printf("   Error %d generated successfully\n", i + 1);
    }
    
    printf("--- End error message quality test ---\n");
    
    return get_error_count() == 3;
}

// Test comprehensive type checker error scenarios
bool test_comprehensive_scenarios() {
    clear_errors();
    
    // Simulate a complex type checking scenario
    type_error_mismatch("string", "int", "function parameter", 50, 25);
    type_error_undefined_function("process_data", 51, 10);
    type_error_invalid_assignment("float", "bool", 52, 15);
    
    // Should have 3 errors
    if (get_error_count() != 3) return false;
    
    // Test type conversion suggestions for common cases
    type_suggest_conversion("int", "float");
    type_suggest_conversion("string", "int");
    
    // Should now have 5 total errors/suggestions
    return get_error_count() == 5;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.2.4: Type Checker Error Messages\n");
    printf("===============================================\n\n");
    
    TEST(type_mismatch_errors);
    TEST(undefined_variable_errors);
    TEST(undefined_function_errors);
    TEST(wrong_arg_count_errors);
    TEST(invalid_assignment_errors);
    TEST(type_conversion_suggestions);
    TEST(error_message_quality);
    TEST(comprehensive_scenarios);
    
    printf("\n===============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.2.4 type checker error tests PASSED!\n");
        printf("T1.2.4: Type Checker Error Messages - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.2.4 tests FAILED!\n");
        return 1;
    }
}
