#ifndef WYN_UNIT_TEST_H
#define WYN_UNIT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test framework macros
#define TEST_SUITE(name) \
    static int test_count = 0; \
    static int test_passed = 0; \
    static const char* suite_name = name; \
    void run_test_suite();

#define TEST_CASE(name) \
    void test_##name(); \
    static void run_##name() { \
        printf("Running %s...", #name); \
        fflush(stdout); \
        test_count++; \
        test_##name(); \
        test_passed++; \
        printf(" PASS\n"); \
    } \
    void test_##name()

#define RUN_TEST(name) run_##name()

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected: %ld, Actual: %ld\n", (long)(expected), (long)(actual)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected: \"%s\", Actual: \"%s\"\n", (expected), (actual)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected true, got false: %s\n", #condition); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected false, got true: %s\n", #condition); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected NULL, got %p\n", (ptr)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "\nAssertion failed: %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "Expected non-NULL pointer\n"); \
            exit(1); \
        } \
    } while(0)

#define TEST_MAIN() \
    int main() { \
        printf("Running test suite: %s\n", suite_name); \
        run_test_suite(); \
        printf("\nTest suite completed: %d/%d tests passed\n", test_passed, test_count); \
        return test_passed == test_count ? 0 : 1; \
    }

// Memory leak detection helpers
typedef struct MemoryBlock {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct MemoryBlock* next;
} MemoryBlock;

extern MemoryBlock* memory_blocks;

void* test_malloc(size_t size, const char* file, int line);
void test_free(void* ptr, const char* file, int line);
void check_memory_leaks();

#define MALLOC(size) test_malloc(size, __FILE__, __LINE__)
#define FREE(ptr) test_free(ptr, __FILE__, __LINE__)

#endif // WYN_UNIT_TEST_H