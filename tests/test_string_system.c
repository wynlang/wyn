#include "wyn_string.h"
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

// Test string creation
bool test_string_creation() {
    WynString* str = wyn_string_new("Hello World");
    if (!str) return false;
    if (str->length != 11) return false;
    if (strcmp(str->data, "Hello World") != 0) return false;
    if (!str->is_heap_allocated) return false;
    
    wyn_string_free(str);
    return true;
}

// Test empty string creation
bool test_empty_string() {
    WynString* str = wyn_string_new("");
    if (!str) return false;
    if (str->length != 0) return false;
    if (str->data[0] != '\0') return false;
    
    wyn_string_free(str);
    return true;
}

// Test NULL string creation
bool test_null_string() {
    WynString* str = wyn_string_new(NULL);
    if (!str) return false;
    if (str->length != 0) return false;
    if (str->data[0] != '\0') return false;
    
    wyn_string_free(str);
    return true;
}

// Test string with capacity
bool test_string_with_capacity() {
    WynString* str = wyn_string_with_capacity(100);
    if (!str) return false;
    if (str->capacity != 100) return false;
    if (str->length != 0) return false;
    if (str->data[0] != '\0') return false;
    
    wyn_string_free(str);
    return true;
}

// Test UTF-8 validation
bool test_utf8_validation() {
    // Valid ASCII
    if (!wyn_string_is_valid_utf8("Hello", 5)) return false;
    
    // Valid UTF-8 (2-byte)
    if (!wyn_string_is_valid_utf8("café", 5)) return false;
    
    // Invalid UTF-8
    char invalid[] = {0xFF, 0xFE, 0x00};
    if (wyn_string_is_valid_utf8(invalid, 2)) return false;
    
    return true;
}

// Test UTF-8 length calculation
bool test_utf8_length() {
    WynString* str = wyn_string_new("café"); // 4 bytes, 4 characters
    if (!str) return false;
    
    size_t utf8_len = wyn_string_utf8_length(str);
    if (utf8_len != 4) {
        wyn_string_free(str);
        return false;
    }
    
    wyn_string_free(str);
    return true;
}

// Test hash function
bool test_hash_function() {
    WynString* str1 = wyn_string_new("test");
    WynString* str2 = wyn_string_new("test");
    
    if (!str1 || !str2) return false;
    
    uint32_t hash1 = wyn_string_hash(str1);
    uint32_t hash2 = wyn_string_hash(str2);
    
    // Same strings should have same hash
    if (hash1 != hash2) {
        wyn_string_free(str1);
        wyn_string_free(str2);
        return false;
    }
    
    // Hash should be cached
    if (!str1->hash_valid || !str2->hash_valid) {
        wyn_string_free(str1);
        wyn_string_free(str2);
        return false;
    }
    
    wyn_string_free(str1);
    wyn_string_free(str2);
    return true;
}

// Test hash invalidation
bool test_hash_invalidation() {
    WynString* str = wyn_string_new("test");
    if (!str) return false;
    
    // Calculate hash
    wyn_string_hash(str);
    if (!str->hash_valid) {
        wyn_string_free(str);
        return false;
    }
    
    // Invalidate hash
    wyn_string_invalidate_hash(str);
    if (str->hash_valid) {
        wyn_string_free(str);
        return false;
    }
    
    wyn_string_free(str);
    return true;
}

// Main test runner
int main() {
    printf("🔤 Testing T1.3.1: String Data Structure Design\n");
    printf("===============================================\n\n");
    
    TEST(string_creation);
    TEST(empty_string);
    TEST(null_string);
    TEST(string_with_capacity);
    TEST(utf8_validation);
    TEST(utf8_length);
    TEST(hash_function);
    TEST(hash_invalidation);
    
    printf("\n===============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.3.1 string data structure tests PASSED!\n");
        printf("T1.3.1: String Data Structure Design - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.3.1 tests FAILED!\n");
        return 1;
    }
}
