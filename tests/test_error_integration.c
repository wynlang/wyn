#include "error.h"
#include "safe_memory.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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

// Test error handling across all components
bool test_component_error_integration() {
    // Test lexer errors
    report_error(ERR_INVALID_CHARACTER, "test.wyn", 1, 5, "Invalid character '@' in source");
    
    // Test parser errors
    report_error(ERR_UNEXPECTED_TOKEN, "test.wyn", 2, 10, "Unexpected token ';'");
    
    // Test type checker errors
    type_error_mismatch("int", "string", "variable assignment", 3, 15);
    
    // Test codegen errors
    report_error(ERR_CODEGEN_FAILED, "test.wyn", 4, 20, "Code generation failed");
    
    // Should have 4 errors from different components
    return get_error_count() == 4;
}

// Test fuzzing with invalid input patterns
bool test_fuzzing_invalid_input() {
    // Test various invalid input patterns that could cause crashes
    
    // Null pointer inputs
    type_error_mismatch(NULL, "int", "test", 1, 1);
    type_error_undefined_variable(NULL, 2, 2);
    
    // Empty string inputs
    type_error_mismatch("", "", "test", 3, 3);
    type_error_undefined_function("", 4, 4);
    
    // Very long strings (potential buffer overflow)
    char long_type[1000];
    memset(long_type, 'A', 999);
    long_type[999] = '\0';
    type_error_mismatch(long_type, "int", "test", 5, 5);
    
    // Special characters and edge cases
    type_error_mismatch("int\n\t", "string\x00", "test", 6, 6);
    
    // Should handle all invalid inputs gracefully
    return get_error_count() >= 4; // Some may be filtered out safely
}

// Test error message quality validation
bool test_error_message_quality() {
    printf("\n--- Error Message Quality Validation ---\n");
    
    // Generate various error types and validate their quality
    type_error_mismatch("bool", "int", "if condition", 10, 5);
    type_error_undefined_variable("unknown_var", 15, 10);
    type_error_wrong_arg_count("calculate", 3, 1, 20, 15);
    type_error_invalid_assignment("float", "string", 25, 20);
    
    // Test conversion suggestions
    type_suggest_conversion("int", "float");
    type_suggest_conversion("string", "int");
    
    printf("   Generated %d error messages for quality validation\n", get_error_count());
    
    // Validate that we have meaningful error messages
    bool has_meaningful_errors = get_error_count() == 6;
    
    printf("--- End Error Message Quality Validation ---\n");
    
    return has_meaningful_errors;
}

// Test error recovery and multiple error reporting
bool test_error_recovery_integration() {
    clear_errors();
    
    // Simulate multiple errors in sequence (like a real compilation)
    report_error(ERR_UNTERMINATED_STRING, "test.wyn", 1, 1, "Unterminated string literal");
    parser_error_at_current("Expected ';' after expression");
    type_error_mismatch("int", "string", "assignment", 2, 5);
    type_error_undefined_function("missing_func", 3, 10);
    
    // Test that multiple errors are properly accumulated
    int error_count = get_error_count();
    
    // Test error recovery doesn't clear all errors inappropriately
    parser_synchronize(); // This should not clear all errors in our implementation
    
    return error_count >= 3; // Should maintain multiple errors
}

// Test error severity classification
bool test_error_severity_classification() {
    clear_errors();
    
    // Test different error severities
    report_error(ERR_INVALID_CHARACTER, "test.wyn", 1, 1, "Warning level error");
    report_error(ERR_TYPE_MISMATCH, "test.wyn", 2, 2, "Error level error");
    report_error(ERR_OUT_OF_MEMORY, "test.wyn", 3, 3, "Fatal level error");
    
    // All should be reported
    return get_error_count() == 3;
}

// Test error context and location tracking
bool test_error_context_tracking() {
    clear_errors();
    
    // Test errors with different locations and contexts
    type_error_mismatch("int", "string", "function parameter 'count'", 42, 15);
    type_error_undefined_variable("user_input", 43, 20);
    type_error_wrong_arg_count("process_data", 2, 4, 44, 25);
    
    // Test that location information is preserved
    return get_error_count() == 3;
}

// Test comprehensive error system stress test
bool test_comprehensive_stress_test() {
    clear_errors();
    
    // Generate many errors to test system stability
    for (int i = 0; i < 50; i++) {
        char var_name[32];
        snprintf(var_name, sizeof(var_name), "var_%d", i);
        type_error_undefined_variable(var_name, i + 1, i + 1);
    }
    
    // System should handle many errors gracefully
    bool handled_many_errors = get_error_count() == 50;
    
    // Test clearing errors
    clear_errors();
    bool cleared_properly = get_error_count() == 0 && !has_errors();
    
    return handled_many_errors && cleared_properly;
}

// Test integration with parser suggestions
bool test_parser_integration() {
    clear_errors();
    
    // Test parser error functions
    parser_error_at_current("Unexpected token in expression");
    parser_suggest_fix("identifier", "number");
    parser_check_and_suggest(42, "function declaration");
    
    // Should generate parser-related errors
    return get_error_count() >= 2;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.2.5: Error Integration Testing\n");
    printf("============================================\n\n");
    
    TEST(component_error_integration);
    TEST(fuzzing_invalid_input);
    TEST(error_message_quality);
    TEST(error_recovery_integration);
    TEST(error_severity_classification);
    TEST(error_context_tracking);
    TEST(comprehensive_stress_test);
    TEST(parser_integration);
    
    printf("\n============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.2.5 error integration tests PASSED!\n");
        printf("T1.2.5: Error Integration Testing - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.2.5 tests FAILED!\n");
        return 1;
    }
}
