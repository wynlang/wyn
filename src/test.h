#ifndef TEST_H
#define TEST_H

#include "common.h"
#include "ast.h"
#include <stdbool.h>

// T1.6.2: Test Parser Integration

// Test metadata structure
typedef struct {
    Token name;
    Stmt* body;
    bool is_async;
} TestStmt;

// Test assertion types
typedef enum {
    ASSERT_EQUAL,
    ASSERT_NOT_EQUAL,
    ASSERT_TRUE,
    ASSERT_FALSE,
    ASSERT_NULL,
    ASSERT_NOT_NULL,
    ASSERT_FLOAT_EQUAL,
    ASSERT_STRING_EQUAL,
    ASSERT_GREATER_THAN,
    ASSERT_LESS_THAN
} AssertType;

// Test assertion structure
typedef struct {
    AssertType type;
    Expr* expected;
    Expr* actual;
    char* message;
} TestAssertion;

// T1.6.3: Test Runner Implementation

// Test result structure
typedef struct {
    char* test_name;
    bool passed;
    char* error_message;
    double execution_time;
} TestResult;

// Test runner statistics
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    double total_time;
    TestResult* results;
} TestRunnerStats;

// Test parsing functions
Stmt* parse_test_block(void);
bool is_test_keyword(Token token);
TestAssertion* parse_assertion(void);

// Test metadata functions
void register_test(TestStmt* test);
int get_test_count(void);
TestStmt** get_all_tests(void);

// T1.6.3: Test runner functions
TestRunnerStats* run_all_tests(void);
TestResult* run_single_test(TestStmt* test);
void print_test_results(TestRunnerStats* stats);
void free_test_results(TestRunnerStats* stats);

// T1.6.4: Assertion Library functions
bool assert_equal(int expected, int actual, const char* message);
bool assert_not_equal(int expected, int actual, const char* message);
bool assert_true(bool condition, const char* message);
bool assert_false(bool condition, const char* message);
bool assert_null(void* ptr, const char* message);
bool assert_not_null(void* ptr, const char* message);
bool assert_float_equal(double expected, double actual, double epsilon, const char* message);
bool assert_string_equal(const char* expected, const char* actual, const char* message);
bool assert_greater_than(int value, int threshold, const char* message);
bool assert_less_than(int value, int threshold, const char* message);

// T1.6.5: TDD Enforcement structures
typedef struct {
    char* function_name;
    bool has_test;
    int line_number;
} FunctionCoverage;

typedef struct {
    FunctionCoverage* functions;
    int function_count;
    int tested_functions;
    double coverage_percentage;
} CoverageReport;

// T1.6.5: TDD Enforcement functions
void register_function(const char* function_name, int line_number);
void mark_function_tested(const char* function_name);
CoverageReport* generate_coverage_report(void);
void print_coverage_warnings(CoverageReport* report);
bool check_coverage_requirements(CoverageReport* report, double min_coverage);
void free_coverage_report(CoverageReport* report);

#endif
