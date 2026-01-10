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

// Test basic string interpolation parsing
bool test_basic_interpolation_parsing() {
    const char* input = "Hello {name}!";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    if (!interp) return false;
    if (interp->part_count != 3) {
        free_string_interpolation(interp);
        return false;
    }
    
    // Should have: "Hello ", {expr}, "!"
    StringInterpPart* part = interp->parts;
    if (!part || part->type != INTERP_TEXT) {
        free_string_interpolation(interp);
        return false;
    }
    
    part = part->next;
    if (!part || part->type != INTERP_EXPR) {
        free_string_interpolation(interp);
        return false;
    }
    
    part = part->next;
    if (!part || part->type != INTERP_TEXT) {
        free_string_interpolation(interp);
        return false;
    }
    
    free_string_interpolation(interp);
    return true;
}

// Test nested braces in expressions
bool test_nested_braces() {
    const char* input = "Result: {func({x})}";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    if (!interp) return false;
    
    // Should parse successfully despite nested braces
    bool success = interp->part_count >= 2;
    
    free_string_interpolation(interp);
    return success;
}

// Test malformed interpolation (missing closing brace)
bool test_malformed_interpolation() {
    const char* input = "Hello {name";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    // Should return NULL for malformed input
    return interp == NULL;
}

// Test string with no interpolation
bool test_no_interpolation() {
    const char* input = "Hello World";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    if (!interp) return false;
    if (interp->part_count != 1) {
        free_string_interpolation(interp);
        return false;
    }
    
    // Should have one text part
    StringInterpPart* part = interp->parts;
    bool success = (part && part->type == INTERP_TEXT);
    
    free_string_interpolation(interp);
    return success;
}

// Test multiple interpolations
bool test_multiple_interpolations() {
    const char* input = "{greeting} {name}, you have {count} messages";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    if (!interp) return false;
    
    // Should have multiple parts
    bool success = interp->part_count >= 5; // 3 expressions + 2 text parts
    
    free_string_interpolation(interp);
    return success;
}

// Test empty string
bool test_empty_string() {
    const char* input = "";
    StringInterpolation* interp = parse_string_interpolation(input, 0);
    
    // Should return NULL for empty input
    return interp == NULL;
}

// Test string interpolation evaluation
bool test_interpolation_evaluation() {
    const char* input = "Hello {name}!";
    StringInterpolation* interp = parse_string_interpolation(input, strlen(input));
    
    if (!interp) return false;
    
    WynString* result = evaluate_string_interpolation(interp);
    if (!result) {
        free_string_interpolation(interp);
        return false;
    }
    
    // Should produce "Hello {expr}!" (simplified evaluation)
    bool success = (strcmp(result->data, "Hello {expr}!") == 0);
    
    wyn_string_free(result);
    free_string_interpolation(interp);
    return success;
}

// Test edge cases and error handling
bool test_edge_cases() {
    // Test NULL input
    StringInterpolation* null_result = parse_string_interpolation(NULL, 0);
    if (null_result != NULL) return false;
    
    // Test empty braces
    const char* empty_braces = "Hello {}!";
    StringInterpolation* empty_interp = parse_string_interpolation(empty_braces, strlen(empty_braces));
    if (!empty_interp) return false;
    
    bool success = empty_interp->part_count >= 2;
    free_string_interpolation(empty_interp);
    
    return success;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.3.3: String Interpolation Parser\n");
    printf("===============================================\n\n");
    
    TEST(basic_interpolation_parsing);
    TEST(nested_braces);
    TEST(malformed_interpolation);
    TEST(no_interpolation);
    TEST(multiple_interpolations);
    TEST(empty_string);
    TEST(interpolation_evaluation);
    TEST(edge_cases);
    
    printf("\n===============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.3.3 string interpolation parser tests PASSED!\n");
        printf("T1.3.3: String Interpolation Parser - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.3.3 tests FAILED!\n");
        return 1;
    }
}
