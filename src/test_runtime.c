// Wyn Testing Framework - Runtime Support
// Provides comprehensive testing utilities for Wyn programs

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Test state
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
    double start_time;
} TestState;

static TestState test_state = {0, 0, 0, 0, 0.0};

// Global fail counter for test blocks
int wyn_test_fail_count = 0;

// Simple assert for test blocks (no message, just condition)
void wyn_assert(int condition) {
    if (!condition) {
        wyn_test_fail_count++;
        fprintf(stderr, "    \033[31massert failed\033[0m\n");
    }
}

void wyn_assert_eq_int(long long actual, long long expected) {
    if (actual != expected) {
        wyn_test_fail_count++;
        fprintf(stderr, "    \033[31massert_eq failed\033[0m\n");
        fprintf(stderr, "      expected: %lld\n", expected);
        fprintf(stderr, "      got:      %lld\n", actual);
    }
}

void wyn_assert_eq_str(const char* actual, const char* expected) {
    if (!actual || !expected || strcmp(actual, expected) != 0) {
        wyn_test_fail_count++;
        fprintf(stderr, "    \033[31massert_eq failed\033[0m\n");
        fprintf(stderr, "      expected: \"%s\"\n", expected ? expected : "(null)");
        fprintf(stderr, "      got:      \"%s\"\n", actual ? actual : "(null)");
    }
}

// Get current time in seconds
static double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Initialize test suite
void Test_init(const char* suite_name) {
    test_state.total = 0;
    test_state.passed = 0;
    test_state.failed = 0;
    test_state.skipped = 0;
    test_state.start_time = get_time();
    printf("\n=== Test Suite: %s ===\n\n", suite_name);
}

// Assertions
void Test_assert(int condition, const char* message) {
    test_state.total++;
    if (condition) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s\n", message);
    }
}

void Test_assert_eq_int(int actual, int expected, const char* message) {
    test_state.total++;
    if (actual == expected) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (expected: %d, got: %d)\n", message, expected, actual);
    }
}

void Test_assert_eq_str(const char* actual, const char* expected, const char* message) {
    test_state.total++;
    if (strcmp(actual, expected) == 0) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (expected: \"%s\", got: \"%s\")\n", message, expected, actual);
    }
}

void Test_assert_ne_int(int actual, int expected, const char* message) {
    test_state.total++;
    if (actual != expected) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (both values are: %d)\n", message, actual);
    }
}

void Test_assert_gt(int actual, int threshold, const char* message) {
    test_state.total++;
    if (actual > threshold) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (%d not > %d)\n", message, actual, threshold);
    }
}

void Test_assert_lt(int actual, int threshold, const char* message) {
    test_state.total++;
    if (actual < threshold) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (%d not < %d)\n", message, actual, threshold);
    }
}

void Test_assert_gte(int actual, int threshold, const char* message) {
    test_state.total++;
    if (actual >= threshold) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (%d not >= %d)\n", message, actual, threshold);
    }
}

void Test_assert_lte(int actual, int threshold, const char* message) {
    test_state.total++;
    if (actual <= threshold) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (%d not <= %d)\n", message, actual, threshold);
    }
}

void Test_assert_contains(const char* haystack, const char* needle, const char* message) {
    test_state.total++;
    if (strstr(haystack, needle) != NULL) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (\"%s\" not found in \"%s\")\n", message, needle, haystack);
    }
}

void Test_assert_null(void* ptr, const char* message) {
    test_state.total++;
    if (ptr == NULL) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (pointer is not null)\n", message);
    }
}

void Test_assert_not_null(void* ptr, const char* message) {
    test_state.total++;
    if (ptr != NULL) {
        test_state.passed++;
        printf("  ✓ %s\n", message);
    } else {
        test_state.failed++;
        printf("  ✗ %s (pointer is null)\n", message);
    }
}

// Test grouping
void Test_describe(const char* description) {
    printf("\n%s\n", description);
}

void Test_skip(const char* reason) {
    test_state.skipped++;
    printf("  ⊘ SKIPPED: %s\n", reason);
}

// Summary
int Test_summary() {
    double elapsed = get_time() - test_state.start_time;
    
    printf("\n=== Test Summary ===\n");
    printf("Total:   %d\n", test_state.total);
    printf("Passed:  %d\n", test_state.passed);
    printf("Failed:  %d\n", test_state.failed);
    printf("Skipped: %d\n", test_state.skipped);
    printf("Time:    %.3fs\n", elapsed);
    if (elapsed < 0.001) printf("         (< 1ms)\n");
    
    if (test_state.failed == 0) {
        printf("\n✓ All tests passed!\n\n");
        return 0;
    } else {
        printf("\n✗ %d test(s) failed\n\n", test_state.failed);
        return 1;
    }
}

// Benchmarking
typedef struct {
    double start;
} Benchmark;

Benchmark* Test_benchmark_start() {
    Benchmark* b = malloc(sizeof(Benchmark));
    b->start = get_time();
    return b;
}

double Test_benchmark_end(Benchmark* b) {
    double elapsed = get_time() - b->start;
    free(b);
    return elapsed;
}

void Test_benchmark(const char* name, void (*fn)()) {
    double start = get_time();
    fn();
    double elapsed = get_time() - start;
    printf("  ⏱  %s: %.6fs\n", name, elapsed);
}
