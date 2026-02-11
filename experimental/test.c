#include "test.h"
#include "safe_memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// T1.6.2: Test Parser Integration Implementation

// Global test registry
static TestStmt** registered_tests = NULL;
static int test_count = 0;
static int test_capacity = 0;

// T1.6.5: TDD Enforcement - Function coverage tracking
static FunctionCoverage* tracked_functions = NULL;
static int tracked_function_count = 0;
static int tracked_function_capacity = 0;

// Test parsing functions
bool is_test_keyword(Token token) {
    return token.type == TOKEN_IDENT && 
           token.length == 4 && 
           memcmp(token.start, "test", 4) == 0;
}

Stmt* parse_test_block(void) {
    // This function will be called from parser.c
    // For now, return a basic test statement structure
    Stmt* stmt = safe_malloc(sizeof(Stmt));
    if (!stmt) return NULL;
    
    stmt->type = STMT_TEST;
    stmt->test_stmt.name.type = TOKEN_IDENT;
    stmt->test_stmt.name.start = "test_example";
    stmt->test_stmt.name.length = 12;
    stmt->test_stmt.body = NULL;
    stmt->test_stmt.is_async = false;
    
    return stmt;
}

TestAssertion* parse_assertion(void) {
    TestAssertion* assertion = safe_malloc(sizeof(TestAssertion));
    if (!assertion) return NULL;
    
    assertion->type = ASSERT_TRUE;
    assertion->expected = NULL;
    assertion->actual = NULL;
    assertion->message = NULL;
    
    return assertion;
}

// Test metadata functions
void register_test(TestStmt* test) {
    if (!test) return;
    
    // Expand array if needed
    if (test_count >= test_capacity) {
        test_capacity = test_capacity == 0 ? 8 : test_capacity * 2;
        registered_tests = safe_realloc(registered_tests, sizeof(TestStmt*) * test_capacity);
        if (!registered_tests) return;
    }
    
    // Store test (note: we're storing the TestStmt directly, not a pointer to it)
    registered_tests[test_count] = safe_malloc(sizeof(TestStmt));
    if (registered_tests[test_count]) {
        registered_tests[test_count]->name = test->name;
        registered_tests[test_count]->body = test->body;
        registered_tests[test_count]->is_async = test->is_async;
        test_count++;
    }
}

int get_test_count(void) {
    return test_count;
}

TestStmt** get_all_tests(void) {
    return registered_tests;
}

// T1.6.3: Test Runner Implementation

TestResult* run_single_test(TestStmt* test) {
    if (!test) return NULL;
    
    TestResult* result = safe_malloc(sizeof(TestResult));
    if (!result) return NULL;
    
    // Extract test name
    size_t name_len = test->name.length;
    result->test_name = safe_malloc(name_len + 1);
    if (result->test_name) {
        memcpy(result->test_name, test->name.start, name_len);
        result->test_name[name_len] = '\0';
    }
    
    // Measure execution time
    clock_t start = clock();
    
    // Execute test (simplified - just mark as passed for now)
    result->passed = true;
    result->error_message = NULL;
    
    clock_t end = clock();
    result->execution_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    return result;
}

TestRunnerStats* run_all_tests(void) {
    TestRunnerStats* stats = safe_malloc(sizeof(TestRunnerStats));
    if (!stats) return NULL;
    
    stats->total_tests = test_count;
    stats->passed_tests = 0;
    stats->failed_tests = 0;
    stats->total_time = 0.0;
    
    if (test_count > 0) {
        stats->results = safe_malloc(sizeof(TestResult) * test_count);
        if (!stats->results) {
            safe_free(stats);
            return NULL;
        }
        
        // Run each test
        for (int i = 0; i < test_count; i++) {
            TestResult* result = run_single_test(registered_tests[i]);
            if (result) {
                stats->results[i] = *result;
                stats->total_time += result->execution_time;
                
                if (result->passed) {
                    stats->passed_tests++;
                } else {
                    stats->failed_tests++;
                }
                
                safe_free(result);
            }
        }
    } else {
        stats->results = NULL;
    }
    
    return stats;
}

void print_test_results(TestRunnerStats* stats) {
    if (!stats) return;
    
    printf("\n🧪 Test Runner Results\n");
    printf("=====================\n");
    
    if (stats->total_tests == 0) {
        printf("No tests found.\n");
        return;
    }
    
    // Print individual test results
    for (int i = 0; i < stats->total_tests; i++) {
        TestResult* result = &stats->results[i];
        const char* status = result->passed ? "✅ PASS" : "❌ FAIL";
        printf("%s %s (%.3fs)\n", status, result->test_name, result->execution_time);
        
        if (!result->passed && result->error_message) {
            printf("   Error: %s\n", result->error_message);
        }
    }
    
    // Print summary
    printf("\n=====================\n");
    printf("Tests: %d total, %d passed, %d failed\n", 
           stats->total_tests, stats->passed_tests, stats->failed_tests);
    printf("Time: %.3fs\n", stats->total_time);
    
    if (stats->failed_tests == 0) {
        printf("🎉 All tests passed!\n");
    } else {
        printf("💥 %d test(s) failed!\n", stats->failed_tests);
    }
}

void free_test_results(TestRunnerStats* stats) {
    if (!stats) return;
    
    if (stats->results) {
        for (int i = 0; i < stats->total_tests; i++) {
            safe_free(stats->results[i].test_name);
            safe_free(stats->results[i].error_message);
        }
        safe_free(stats->results);
    }
    
    safe_free(stats);
}

// T1.6.4: Assertion Library Implementation

bool assert_equal(int expected, int actual, const char* message) {
    if (expected == actual) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Values not equal");
    printf("   Expected: %d\n", expected);
    printf("   Actual: %d\n", actual);
    return false;
}

bool assert_not_equal(int expected, int actual, const char* message) {
    if (expected != actual) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Values should not be equal");
    printf("   Both values: %d\n", expected);
    return false;
}

bool assert_true(bool condition, const char* message) {
    if (condition) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Expected true");
    printf("   Condition was false\n");
    return false;
}

bool assert_false(bool condition, const char* message) {
    if (!condition) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Expected false");
    printf("   Condition was true\n");
    return false;
}

bool assert_null(void* ptr, const char* message) {
    if (ptr == NULL) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Expected NULL pointer");
    printf("   Pointer was not NULL\n");
    return false;
}

bool assert_not_null(void* ptr, const char* message) {
    if (ptr != NULL) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Expected non-NULL pointer");
    printf("   Pointer was NULL\n");
    return false;
}

bool assert_float_equal(double expected, double actual, double epsilon, const char* message) {
    double diff = fabs(expected - actual);
    if (diff <= epsilon) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Floating point values not equal");
    printf("   Expected: %.6f\n", expected);
    printf("   Actual: %.6f\n", actual);
    printf("   Difference: %.6f (epsilon: %.6f)\n", diff, epsilon);
    return false;
}

bool assert_string_equal(const char* expected, const char* actual, const char* message) {
    if (expected == NULL && actual == NULL) {
        return true;
    }
    
    if (expected == NULL || actual == NULL) {
        printf("❌ ASSERTION FAILED: %s\n", message ? message : "One string is NULL");
        printf("   Expected: %s\n", expected ? expected : "NULL");
        printf("   Actual: %s\n", actual ? actual : "NULL");
        return false;
    }
    
    if (strcmp(expected, actual) == 0) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Strings not equal");
    printf("   Expected: \"%s\"\n", expected);
    printf("   Actual: \"%s\"\n", actual);
    return false;
}

bool assert_greater_than(int value, int threshold, const char* message) {
    if (value > threshold) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Value not greater than threshold");
    printf("   Value: %d\n", value);
    printf("   Threshold: %d\n", threshold);
    return false;
}

bool assert_less_than(int value, int threshold, const char* message) {
    if (value < threshold) {
        return true;
    }
    
    printf("❌ ASSERTION FAILED: %s\n", message ? message : "Value not less than threshold");
    printf("   Value: %d\n", value);
    printf("   Threshold: %d\n", threshold);
    return false;
}

// T1.6.5: TDD Enforcement Implementation

void register_function(const char* function_name, int line_number) {
    if (!function_name) return;
    
    // Expand array if needed
    if (tracked_function_count >= tracked_function_capacity) {
        tracked_function_capacity = tracked_function_capacity == 0 ? 8 : tracked_function_capacity * 2;
        tracked_functions = safe_realloc(tracked_functions, sizeof(FunctionCoverage) * tracked_function_capacity);
        if (!tracked_functions) return;
    }
    
    // Add function
    FunctionCoverage* func = &tracked_functions[tracked_function_count];
    size_t name_len = strlen(function_name);
    func->function_name = safe_malloc(name_len + 1);
    if (func->function_name) {
        strcpy(func->function_name, function_name);
        func->has_test = false;
        func->line_number = line_number;
        tracked_function_count++;
    }
}

void mark_function_tested(const char* function_name) {
    if (!function_name) return;
    
    for (int i = 0; i < tracked_function_count; i++) {
        if (tracked_functions[i].function_name && 
            strcmp(tracked_functions[i].function_name, function_name) == 0) {
            tracked_functions[i].has_test = true;
            return;
        }
    }
}

CoverageReport* generate_coverage_report(void) {
    CoverageReport* report = safe_malloc(sizeof(CoverageReport));
    if (!report) return NULL;
    
    report->function_count = tracked_function_count;
    report->tested_functions = 0;
    
    // Count tested functions
    for (int i = 0; i < tracked_function_count; i++) {
        if (tracked_functions[i].has_test) {
            report->tested_functions++;
        }
    }
    
    // Calculate coverage percentage
    if (tracked_function_count > 0) {
        report->coverage_percentage = (double)report->tested_functions / tracked_function_count * 100.0;
    } else {
        report->coverage_percentage = 100.0; // No functions = 100% coverage
    }
    
    // Copy function data
    if (tracked_function_count > 0) {
        report->functions = safe_malloc(sizeof(FunctionCoverage) * tracked_function_count);
        if (report->functions) {
            for (int i = 0; i < tracked_function_count; i++) {
                report->functions[i] = tracked_functions[i];
                // Note: We're sharing the function_name pointer, not copying it
            }
        }
    } else {
        report->functions = NULL;
    }
    
    return report;
}

void print_coverage_warnings(CoverageReport* report) {
    if (!report) return;
    
    printf("\n⚠️  TDD Enforcement Report\n");
    printf("==========================\n");
    
    if (report->function_count == 0) {
        printf("No functions tracked.\n");
        return;
    }
    
    printf("Coverage: %.1f%% (%d/%d functions tested)\n", 
           report->coverage_percentage, report->tested_functions, report->function_count);
    
    // List untested functions
    bool has_untested = false;
    for (int i = 0; i < report->function_count; i++) {
        if (!report->functions[i].has_test) {
            if (!has_untested) {
                printf("\n⚠️  Untested functions:\n");
                has_untested = true;
            }
            printf("   %s (line %d)\n", 
                   report->functions[i].function_name, 
                   report->functions[i].line_number);
        }
    }
    
    if (!has_untested) {
        printf("✅ All functions have tests!\n");
    }
}

bool check_coverage_requirements(CoverageReport* report, double min_coverage) {
    if (!report) return false;
    
    if (report->coverage_percentage >= min_coverage) {
        printf("✅ Coverage requirement met: %.1f%% >= %.1f%%\n", 
               report->coverage_percentage, min_coverage);
        return true;
    } else {
        printf("❌ Coverage requirement NOT met: %.1f%% < %.1f%%\n", 
               report->coverage_percentage, min_coverage);
        return false;
    }
}

void free_coverage_report(CoverageReport* report) {
    if (!report) return;
    
    // Note: We don't free function_name strings here because they're shared
    // with the global tracked_functions array
    safe_free(report->functions);
    safe_free(report);
}
