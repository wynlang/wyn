#include "test.h"
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

// Test is_test_keyword function
bool test_is_test_keyword() {
    Token test_token = {TOKEN_IDENT, "test", 4, 0};
    Token other_token = {TOKEN_IDENT, "function", 8, 0};
    
    if (!is_test_keyword(test_token)) return false;
    if (is_test_keyword(other_token)) return false;
    
    return true;
}

// Test parse_test_block function
bool test_parse_test_block() {
    Stmt* test_stmt = parse_test_block();
    if (!test_stmt) return false;
    if (test_stmt->type != STMT_TEST) return false;
    if (test_stmt->test_stmt.is_async != false) return false;
    
    // Clean up
    if (test_stmt->test_stmt.body) {
        safe_free(test_stmt->test_stmt.body);
    }
    safe_free(test_stmt);
    
    return true;
}

// Test assertion parsing
bool test_parse_assertion() {
    TestAssertion* assertion = parse_assertion();
    if (!assertion) return false;
    if (assertion->type != ASSERT_TRUE) return false;
    
    safe_free(assertion);
    return true;
}

// Test registration system
bool test_registration_system() {
    // Create a test
    TestStmt test;
    test.name.type = TOKEN_IDENT;
    test.name.start = "example_test";
    test.name.length = 12;
    test.body = NULL;
    test.is_async = false;
    
    int initial_count = get_test_count();
    register_test(&test);
    
    if (get_test_count() != initial_count + 1) return false;
    
    TestStmt** all_tests = get_all_tests();
    if (!all_tests) return false;
    
    return true;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.6.2: Test Parser Integration\n");
    printf("==========================================\n\n");
    
    TEST(is_test_keyword);
    TEST(parse_test_block);
    TEST(parse_assertion);
    TEST(registration_system);
    
    printf("\n==========================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.6.2 test parser integration tests PASSED!\n");
        printf("T1.6.2: Test Parser Integration - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.6.2 tests FAILED!\n");
        return 1;
    }
}
