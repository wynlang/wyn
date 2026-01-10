#include "wyn_string.h"
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

// Test string concatenation
bool test_string_concat() {
    WynString* a = wyn_string_new("Hello");
    WynString* b = wyn_string_new(" World");
    
    if (!a || !b) {
        wyn_string_free(a);
        wyn_string_free(b);
        return false;
    }
    
    WynString* result = wyn_string_concat(a, b);
    if (!result) {
        wyn_string_free(a);
        wyn_string_free(b);
        return false;
    }
    
    bool success = (result->length == 11) && 
                   (strcmp(result->data, "Hello World") == 0);
    
    wyn_string_free(a);
    wyn_string_free(b);
    wyn_string_free(result);
    
    return success;
}

// Test string comparison
bool test_string_compare() {
    WynString* a = wyn_string_new("apple");
    WynString* b = wyn_string_new("banana");
    WynString* c = wyn_string_new("apple");
    
    if (!a || !b || !c) {
        wyn_string_free(a);
        wyn_string_free(b);
        wyn_string_free(c);
        return false;
    }
    
    bool success = (wyn_string_compare(a, b) < 0) &&  // apple < banana
                   (wyn_string_compare(b, a) > 0) &&  // banana > apple
                   (wyn_string_compare(a, c) == 0);   // apple == apple
    
    wyn_string_free(a);
    wyn_string_free(b);
    wyn_string_free(c);
    
    return success;
}

// Test string copying
bool test_string_copy() {
    WynString* original = wyn_string_new("Test String");
    if (!original) return false;
    
    WynString* copy = wyn_string_copy(original);
    if (!copy) {
        wyn_string_free(original);
        return false;
    }
    
    bool success = (copy->length == original->length) &&
                   (strcmp(copy->data, original->data) == 0) &&
                   (copy->data != original->data); // Different memory
    
    wyn_string_free(original);
    wyn_string_free(copy);
    
    return success;
}

// Test substring extraction
bool test_string_substring() {
    WynString* str = wyn_string_new("Hello World");
    if (!str) return false;
    
    WynString* sub = wyn_string_substring(str, 6, 11); // "World"
    if (!sub) {
        wyn_string_free(str);
        return false;
    }
    
    bool success = (sub->length == 5) && 
                   (strcmp(sub->data, "World") == 0);
    
    wyn_string_free(str);
    wyn_string_free(sub);
    
    return success;
}

// Test string find operation
bool test_string_find() {
    WynString* haystack = wyn_string_new("Hello World Hello");
    WynString* needle = wyn_string_new("World");
    
    if (!haystack || !needle) {
        wyn_string_free(haystack);
        wyn_string_free(needle);
        return false;
    }
    
    size_t pos = wyn_string_find(haystack, needle);
    bool success = (pos == 6); // "World" starts at position 6
    
    wyn_string_free(haystack);
    wyn_string_free(needle);
    
    return success;
}

// Test string replace operation
bool test_string_replace() {
    WynString* str = wyn_string_new("Hello World");
    WynString* old = wyn_string_new("World");
    WynString* new_str = wyn_string_new("Universe");
    
    if (!str || !old || !new_str) {
        wyn_string_free(str);
        wyn_string_free(old);
        wyn_string_free(new_str);
        return false;
    }
    
    WynString* result = wyn_string_replace(str, old, new_str);
    if (!result) {
        wyn_string_free(str);
        wyn_string_free(old);
        wyn_string_free(new_str);
        return false;
    }
    
    bool success = (strcmp(result->data, "Hello Universe") == 0);
    
    wyn_string_free(str);
    wyn_string_free(old);
    wyn_string_free(new_str);
    wyn_string_free(result);
    
    return success;
}

// Test edge cases
bool test_edge_cases() {
    // Test with NULL inputs
    WynString* null_result = wyn_string_concat(NULL, NULL);
    if (null_result != NULL) return false;
    
    // Test empty strings
    WynString* empty = wyn_string_new("");
    WynString* hello = wyn_string_new("Hello");
    
    if (!empty || !hello) {
        wyn_string_free(empty);
        wyn_string_free(hello);
        return false;
    }
    
    WynString* concat_result = wyn_string_concat(empty, hello);
    bool success = concat_result && (strcmp(concat_result->data, "Hello") == 0);
    
    wyn_string_free(empty);
    wyn_string_free(hello);
    wyn_string_free(concat_result);
    
    return success;
}

// Test UTF-8 length calculation
bool test_utf8_length() {
    WynString* ascii = wyn_string_new("Hello");
    WynString* utf8 = wyn_string_new("Héllo"); // Contains é (2 bytes in UTF-8)
    
    if (!ascii || !utf8) {
        wyn_string_free(ascii);
        wyn_string_free(utf8);
        return false;
    }
    
    size_t ascii_len = wyn_string_utf8_length(ascii);
    size_t utf8_len = wyn_string_utf8_length(utf8);
    
    bool success = (ascii_len == 5) && (utf8_len == 5); // Both should be 5 characters
    
    wyn_string_free(ascii);
    wyn_string_free(utf8);
    
    return success;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.3.2: Basic String Operations\n");
    printf("==========================================\n\n");
    
    TEST(string_concat);
    TEST(string_compare);
    TEST(string_copy);
    TEST(string_substring);
    TEST(string_find);
    TEST(string_replace);
    TEST(edge_cases);
    TEST(utf8_length);
    
    printf("\n==========================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.3.2 basic string operation tests PASSED!\n");
        printf("T1.3.2: Basic String Operations - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.3.2 tests FAILED!\n");
        return 1;
    }
}
