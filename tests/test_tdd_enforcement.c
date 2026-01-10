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

// Test register_function functionality
bool test_register_function() {
    // Register some test functions
    register_function("test_function_1", 10);
    register_function("test_function_2", 20);
    
    CoverageReport* report = generate_coverage_report();
    if (!report) return false;
    if (report->function_count < 2) return false;
    if (report->tested_functions != 0) return false;
    if (report->coverage_percentage != 0.0) return false;
    
    free_coverage_report(report);
    return true;
}

// Test mark_function_tested functionality
bool test_mark_function_tested() {
    // Mark one function as tested
    mark_function_tested("test_function_1");
    
    CoverageReport* report = generate_coverage_report();
    if (!report) return false;
    if (report->tested_functions != 1) return false;
    if (report->coverage_percentage <= 0.0) return false;
    
    free_coverage_report(report);
    return true;
}

// Test coverage report generation
bool test_coverage_report() {
    // Register another function and mark it tested
    register_function("test_function_3", 30);
    mark_function_tested("test_function_3");
    
    CoverageReport* report = generate_coverage_report();
    if (!report) return false;
    if (report->function_count < 3) return false;
    if (report->tested_functions < 2) return false;
    
    // Check that functions array is populated
    if (!report->functions) return false;
    
    free_coverage_report(report);
    return true;
}

// Test coverage requirements checking
bool test_coverage_requirements() {
    CoverageReport* report = generate_coverage_report();
    if (!report) return false;
    
    // Test with low requirement (should pass)
    bool result1 = check_coverage_requirements(report, 50.0);
    
    // Test with high requirement (should fail)
    bool result2 = check_coverage_requirements(report, 95.0);
    
    free_coverage_report(report);
    
    // Should pass low requirement and fail high requirement
    return result1 && !result2;
}

// Test print_coverage_warnings function
bool test_print_coverage_warnings() {
    CoverageReport* report = generate_coverage_report();
    if (!report) return false;
    
    printf("\n--- Testing coverage warnings output ---\n");
    print_coverage_warnings(report);
    printf("--- End of coverage warnings test ---\n");
    
    free_coverage_report(report);
    
    // Should not crash
    return true;
}

// Test edge cases
bool test_edge_cases() {
    // Test with NULL inputs
    register_function(NULL, 0);
    mark_function_tested(NULL);
    
    // Test empty coverage report by creating a fresh one
    CoverageReport* empty_report = safe_malloc(sizeof(CoverageReport));
    if (!empty_report) return false;
    
    empty_report->function_count = 0;
    empty_report->tested_functions = 0;
    empty_report->coverage_percentage = 100.0; // No functions = 100% coverage
    empty_report->functions = NULL;
    
    print_coverage_warnings(empty_report);
    bool result = check_coverage_requirements(empty_report, 80.0);
    
    safe_free(empty_report);
    
    // Empty report should have 100% coverage
    return result;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.6.5: TDD Enforcement\n");
    printf("===================================\n\n");
    
    TEST(register_function);
    TEST(mark_function_tested);
    TEST(coverage_report);
    TEST(coverage_requirements);
    TEST(print_coverage_warnings);
    TEST(edge_cases);
    
    printf("\n===================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.6.5 TDD enforcement tests PASSED!\n");
        printf("T1.6.5: TDD Enforcement - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.6.5 tests FAILED!\n");
        return 1;
    }
}
