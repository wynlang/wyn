#include "security.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

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

// Test safe memory allocation
bool test_safe_malloc() {
    init_memory_tracking();
    
    // Test normal allocation
    void* ptr1 = safe_malloc(100);
    if (!ptr1) return false;
    
    // Test zero allocation (should fail)
    void* ptr2 = safe_malloc(0);
    if (ptr2) return false;
    
    // Test excessive allocation (should fail)
    void* ptr3 = safe_malloc(MAX_SAFE_ALLOCATION_SIZE + 1);
    if (ptr3) return false;
    
    safe_free(ptr1);
    
    // Should have no leaks
    bool no_leaks = !has_memory_leaks();
    cleanup_memory_tracking();
    
    return no_leaks;
}

// Test safe calloc with overflow checking
bool test_safe_calloc() {
    init_memory_tracking();
    
    // Test normal allocation
    void* ptr1 = safe_calloc(10, 10);
    if (!ptr1) return false;
    
    // Test overflow (should fail)
    void* ptr2 = safe_calloc(SIZE_MAX, 2);
    if (ptr2) return false;
    
    safe_free(ptr1);
    
    bool no_leaks = !has_memory_leaks();
    cleanup_memory_tracking();
    
    return no_leaks;
}

// Test safe string operations
bool test_safe_string_ops() {
    char dest[100];
    
    // Test safe_strcpy
    if (safe_strcpy(dest, sizeof(dest), "Hello") != 0) return false;
    if (strcmp(dest, "Hello") != 0) return false;
    
    // Test buffer overflow protection
    if (safe_strcpy(dest, 5, "Hello World") == 0) return false; // Should fail
    
    // Test safe_strcat
    if (safe_strcpy(dest, sizeof(dest), "Hello") != 0) return false;
    if (safe_strcat(dest, sizeof(dest), " World") != 0) return false;
    if (strcmp(dest, "Hello World") != 0) return false;
    
    // Test strcat overflow protection
    if (safe_strcat(dest, 12, " and more text") == 0) return false; // Should fail
    
    return true;
}

// Test integer overflow checking
bool test_overflow_checking() {
    size_t result;
    
    // Test normal multiplication
    if (!check_mul_overflow_size_t(10, 20, &result)) return false;
    if (result != 200) return false;
    
    // Test overflow detection
    if (check_mul_overflow_size_t(SIZE_MAX, 2, &result)) return false; // Should detect overflow
    
    // Test addition overflow
    if (!check_add_overflow_size_t(100, 200, &result)) return false;
    if (result != 300) return false;
    
    if (check_add_overflow_size_t(SIZE_MAX, 1, &result)) return false; // Should detect overflow
    
    return true;
}

// Test memory leak detection
bool test_memory_leak_detection() {
    init_memory_tracking();
    
    // Allocate without freeing (intentional leak)
    void* ptr = safe_malloc(100);
    if (!ptr) return false;
    
    // Should detect leak
    bool has_leaks = has_memory_leaks();
    
    // Clean up
    safe_free(ptr);
    bool no_leaks_after_free = !has_memory_leaks();
    
    cleanup_memory_tracking();
    
    return has_leaks && no_leaks_after_free;
}

// Test safe strdup
bool test_safe_strdup() {
    init_memory_tracking();
    
    // Test normal strdup
    char* copy = safe_strdup("Hello World");
    if (!copy) return false;
    if (strcmp(copy, "Hello World") != 0) return false;
    
    // Test NULL string (should fail)
    char* null_copy = safe_strdup(NULL);
    if (null_copy) return false;
    
    safe_free(copy);
    
    bool no_leaks = !has_memory_leaks();
    cleanup_memory_tracking();
    
    return no_leaks;
}

// Test buffer validation
bool test_buffer_validation() {
    char buffer[100];
    
    // Valid buffer
    if (!validate_buffer(buffer, sizeof(buffer))) return false;
    
    // NULL buffer (should fail)
    if (validate_buffer(NULL, 100)) return false;
    
    // Zero size (should fail)
    if (validate_buffer(buffer, 0)) return false;
    
    // Excessive size (should fail)
    if (validate_buffer(buffer, MAX_SAFE_ALLOCATION_SIZE + 1)) return false;
    
    return true;
}

// Test string validation
bool test_string_validation() {
    char test_str[] = "Hello World";
    
    // Valid string
    if (!validate_string(test_str, 100)) return false;
    
    // NULL string (should fail)
    if (validate_string(NULL, 100)) return false;
    
    // String length check - this should pass since string is shorter than max
    if (!validate_string(test_str, strlen(test_str) + 10)) return false;
    
    return true;
}

// Test safe snprintf
bool test_safe_snprintf() {
    char buffer[100];
    
    // Normal formatting
    int result = safe_snprintf(buffer, sizeof(buffer), "Number: %d", 42);
    if (result < 0) return false;
    if (strcmp(buffer, "Number: 42") != 0) return false;
    
    // Test buffer overflow protection
    result = safe_snprintf(buffer, 5, "This is a very long string");
    if (result >= 0) return false; // Should fail due to truncation
    
    return true;
}

// Main test runner
int main() {
    printf("🔒 Running Wyn Security Library Tests\n");
    printf("=====================================\n\n");
    
    TEST(safe_malloc);
    TEST(safe_calloc);
    TEST(safe_string_ops);
    TEST(overflow_checking);
    TEST(memory_leak_detection);
    TEST(safe_strdup);
    TEST(buffer_validation);
    TEST(string_validation);
    TEST(safe_snprintf);
    
    printf("\n=====================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All security tests PASSED!\n");
        return 0;
    } else {
        printf("❌ Some security tests FAILED!\n");
        return 1;
    }
}
