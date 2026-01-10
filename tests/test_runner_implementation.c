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

// Test run_single_test function
bool test_run_single_test() {
    // Create a test
    TestStmt test;
    test.name.type = TOKEN_IDENT;
    test.name.start = "example_test";
    test.name.length = 12;
    test.body = NULL;
    test.is_async = false;
    
    TestResult* result = run_single_test(&test);
    if (!result) return false;
    if (!result->test_name) return false;
    if (strcmp(result->test_name, "example_test") != 0) return false;
    if (!result->passed) return false;
    if (result->execution_time < 0) return false;
    
    safe_free(result->test_name);
    safe_free(result);
    return true;
}

// Test run_all_tests with no tests
bool test_run_all_tests_empty() {
    TestRunnerStats* stats = run_all_tests();
    if (!stats) return false;
    if (stats->total_tests != 0) return false;
    if (stats->passed_tests != 0) return false;
    if (stats->failed_tests != 0) return false;
    if (stats->results != NULL) return false;
    
    free_test_results(stats);
    return true;
}

// Test run_all_tests with registered tests
bool test_run_all_tests_with_tests() {
    // Register a test
    TestStmt test;
    test.name.type = TOKEN_IDENT;
    test.name.start = "registered_test";
    test.name.length = 15;
    test.body = NULL;
    test.is_async = false;
    
    register_test(&test);
    
    TestRunnerStats* stats = run_all_tests();
    if (!stats) return false;
    if (stats->total_tests != 1) return false;
    if (stats->passed_tests != 1) return false;
    if (stats->failed_tests != 0) return false;
    if (!stats->results) return false;
    
    free_test_results(stats);
    return true;
}

// Test print_test_results function
bool test_print_test_results() {
    TestRunnerStats stats;
    stats.total_tests = 1;
    stats.passed_tests = 1;
    stats.failed_tests = 0;
    stats.total_time = 0.001;
    
    TestResult result;
    result.test_name = "print_test";
    result.passed = true;
    result.error_message = NULL;
    result.execution_time = 0.001;
    
    stats.results = &result;
    
    // This should not crash
    print_test_results(&stats);
    
    return true;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.6.3: Test Runner Implementation\n");
    printf("==============================================\n\n");
    
    TEST(run_single_test);
    TEST(run_all_tests_empty);
    TEST(run_all_tests_with_tests);
    TEST(print_test_results);
    
    printf("\n==============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.6.3 test runner tests PASSED!\n");
        printf("T1.6.3: Test Runner Implementation - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.6.3 tests FAILED!\n");
        return 1;
    }
}
