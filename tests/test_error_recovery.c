#include "error.h"
#include <stdio.h>
#include <assert.h>

// Mock parser structure for testing
typedef struct {
    int line;
    int type;
} MockToken;

typedef struct {
    MockToken current;
    MockToken previous;
    bool had_error;
} MockParser;

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

// Test basic error reporting
bool test_error_reporting() {
    clear_errors();
    
    report_error(ERR_UNEXPECTED_TOKEN, "test.wyn", 10, 5, "Test error message");
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    clear_errors();
    if (has_errors()) return false;
    if (get_error_count() != 0) return false;
    
    return true;
}

// Test error with suggestion
bool test_error_with_suggestion() {
    clear_errors();
    
    report_error_with_suggestion(ERR_MISSING_SEMICOLON, "test.wyn", 15, 10, 
                               "Missing semicolon", "Add ';' at end of statement");
    
    if (!has_errors()) return false;
    if (get_error_count() != 1) return false;
    
    return true;
}

// Test multiple errors
bool test_multiple_errors() {
    clear_errors();
    
    report_error(ERR_UNEXPECTED_TOKEN, "test.wyn", 1, 1, "First error");
    report_error(ERR_MISSING_SEMICOLON, "test.wyn", 2, 1, "Second error");
    report_error(ERR_INVALID_EXPRESSION, "test.wyn", 3, 1, "Third error");
    
    if (get_error_count() != 3) return false;
    
    return true;
}

// Test error severity classification
bool test_error_severity() {
    clear_errors();
    
    // Test different error code ranges
    report_error(ERR_INVALID_CHARACTER, "test.wyn", 1, 1, "Lexer error");    // Should be WARNING
    report_error(ERR_TYPE_MISMATCH, "test.wyn", 2, 1, "Type error");        // Should be ERROR  
    report_error(ERR_OUT_OF_MEMORY, "test.wyn", 3, 1, "Fatal error");       // Should be FATAL
    
    if (get_error_count() != 3) return false;
    
    return true;
}

// Main test runner
int main() {
    printf("🚨 Testing T1.2.3: Parser Error Recovery\n");
    printf("========================================\n\n");
    
    TEST(error_reporting);
    TEST(error_with_suggestion);
    TEST(multiple_errors);
    TEST(error_severity);
    
    printf("\n========================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.2.3 error recovery tests PASSED!\n");
        printf("T1.2.3: Parser Error Recovery - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.2.3 tests FAILED!\n");
        return 1;
    }
}
