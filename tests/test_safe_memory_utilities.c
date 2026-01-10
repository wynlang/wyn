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

// Test safe_strncat
bool test_safe_strncat() {
    char dest[20] = "Hello";
    
    // Normal concatenation
    if (!safe_strncat(dest, " World", sizeof(dest))) return false;
    if (strcmp(dest, "Hello World") != 0) return false;
    
    // Test overflow protection
    char small[10] = "Hello";
    if (safe_strncat(small, " Very Long String", sizeof(small)) != NULL) return false;
    
    return true;
}

// Test safe_array_access
bool test_safe_array_access() {
    int array[10];
    
    // Valid access
    if (!safe_array_access(array, 5, 10, sizeof(int))) return false;
    
    // Invalid access (out of bounds)
    if (safe_array_access(array, 15, 10, sizeof(int))) return false;
    
    // NULL array
    if (safe_array_access(NULL, 0, 10, sizeof(int))) return false;
    
    return true;
}

// Test safe_buffer_copy
bool test_safe_buffer_copy() {
    char src[] = "Hello";
    char dest[20];
    
    // Normal copy
    if (!safe_buffer_copy(dest, sizeof(dest), src, strlen(src) + 1)) return false;
    if (strcmp(dest, "Hello") != 0) return false;
    
    // Test overflow protection
    char small_dest[3];
    if (safe_buffer_copy(small_dest, sizeof(small_dest), src, strlen(src) + 1)) return false;
    
    return true;
}

// Test safe_string_concat
bool test_safe_string_concat() {
    char* result = safe_string_concat("Hello", " World");
    if (!result) return false;
    if (strcmp(result, "Hello World") != 0) {
        safe_free(result);
        return false;
    }
    safe_free(result);
    
    // Test NULL handling
    if (safe_string_concat(NULL, "test") != NULL) return false;
    if (safe_string_concat("test", NULL) != NULL) return false;
    
    return true;
}

// Test safe_realloc_with_old_size
bool test_safe_realloc_with_old_size() {
    char* ptr = safe_malloc(10);
    strcpy(ptr, "Hello");
    
    // Expand buffer
    ptr = safe_realloc_with_old_size(ptr, 10, 20);
    if (!ptr) return false;
    if (strcmp(ptr, "Hello") != 0) {
        safe_free(ptr);
        return false;
    }
    
    // Shrink buffer
    ptr = safe_realloc_with_old_size(ptr, 20, 6);
    if (!ptr) return false;
    if (strcmp(ptr, "Hello") != 0) {
        safe_free(ptr);
        return false;
    }
    
    safe_free(ptr);
    return true;
}

// Test validation functions
bool test_validation_functions() {
    char test_str[] = "Hello";
    
    // Test validate_memory_range
    if (!validate_memory_range(test_str, sizeof(test_str))) return false;
    if (validate_memory_range(NULL, 10)) return false;
    if (validate_memory_range(test_str, 0)) return false;
    
    // Test validate_string_bounds
    if (!validate_string_bounds(test_str, 10)) return false;
    if (!validate_string_bounds(test_str, 5)) return false;
    if (validate_string_bounds(test_str, 3)) return false;
    if (validate_string_bounds(NULL, 10)) return false;
    
    return true;
}

// Main test runner
int main() {
    printf("🧠 Testing T1.1.4: Safe Memory Utilities\n");
    printf("========================================\n\n");
    
    TEST(safe_strncat);
    TEST(safe_array_access);
    TEST(safe_buffer_copy);
    TEST(safe_string_concat);
    TEST(safe_realloc_with_old_size);
    TEST(validation_functions);
    
    printf("\n========================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.1.4 safe memory utility tests PASSED!\n");
        printf("T1.1.4: Implement Safe Memory Utilities - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.1.4 tests FAILED!\n");
        return 1;
    }
}
