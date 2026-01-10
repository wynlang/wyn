#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test macro
#define RUN_TEST(name) do { \
    printf("Running test: %s... ", #name); \
    if (name()) { \
        printf("✅ PASSED\n"); \
    } else { \
        printf("❌ FAILED\n"); \
        all_passed = false; \
    } \
} while(0)

// T1.6.1: Test Syntax Design Implementation
// Tests the 'test' keyword syntax definition and test discovery mechanism

// Mock token types for testing
typedef enum {
    TOKEN_TEST,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    const char* lexeme;
} Token;

// Test syntax structures
typedef struct {
    Token name;           // Test name (string literal)
    Token* body_tokens;   // Test body tokens
    int body_count;       // Number of tokens in body
    bool is_valid;        // Whether syntax is valid
} TestSyntax;

// Test discovery mechanism
typedef struct {
    TestSyntax* tests;
    int test_count;
    int capacity;
} TestRegistry;

static TestRegistry* g_test_registry = NULL;

// T1.6.1: Test syntax validation functions
bool wyn_is_test_keyword(Token token) {
    return token.type == TOKEN_TEST && 
           token.length == 4 && 
           memcmp(token.start, "test", 4) == 0;
}

bool wyn_validate_test_syntax(TestSyntax* test) {
    if (!test) return false;
    
    // Test must have a string name
    if (test->name.type != TOKEN_STRING) {
        return false;
    }
    
    // Test name must be non-empty
    if (test->name.length <= 2) { // At least quotes
        return false;
    }
    
    // Test must have a body
    if (!test->body_tokens || test->body_count == 0) {
        return false;
    }
    
    return true;
}

TestRegistry* wyn_create_test_registry() {
    TestRegistry* registry = malloc(sizeof(TestRegistry));
    if (!registry) return NULL;
    
    registry->tests = malloc(sizeof(TestSyntax) * 10);
    registry->test_count = 0;
    registry->capacity = 10;
    
    return registry;
}

bool wyn_register_test(TestRegistry* registry, TestSyntax* test) {
    if (!registry || !test) return false;
    
    if (!wyn_validate_test_syntax(test)) {
        return false;
    }
    
    if (registry->test_count >= registry->capacity) {
        // Expand capacity
        registry->capacity *= 2;
        registry->tests = realloc(registry->tests, sizeof(TestSyntax) * registry->capacity);
        if (!registry->tests) return false;
    }
    
    registry->tests[registry->test_count] = *test;
    registry->test_count++;
    
    return true;
}

TestSyntax* wyn_find_test_by_name(TestRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;
    
    for (int i = 0; i < registry->test_count; i++) {
        TestSyntax* test = &registry->tests[i];
        
        // Compare test name (skip quotes)
        int name_len = strlen(name);
        int test_name_len = test->name.length - 2; // Remove quotes
        
        if (name_len == test_name_len && 
            memcmp(name, test->name.start + 1, name_len) == 0) {
            return test;
        }
    }
    
    return NULL;
}

void wyn_free_test_registry(TestRegistry* registry) {
    if (!registry) return;
    
    free(registry->tests);
    free(registry);
}

// Test naming convention validation
bool wyn_validate_test_name(const char* name) {
    if (!name) return false;
    
    int len = strlen(name);
    if (len == 0 || len > 100) return false;
    
    // Test names should be descriptive
    if (len < 3) return false;
    
    // Should not start with numbers
    if (name[0] >= '0' && name[0] <= '9') return false;
    
    return true;
}

// Helper functions for creating test data
Token make_token(TokenType type, const char* text) {
    Token token = {0};
    token.type = type;
    token.start = text;
    token.length = strlen(text);
    token.lexeme = text;
    return token;
}

TestSyntax make_test_syntax(const char* name, Token* body, int body_count) {
    TestSyntax test = {0};
    test.name = make_token(TOKEN_STRING, name);
    test.body_tokens = body;
    test.body_count = body_count;
    test.is_valid = wyn_validate_test_syntax(&test);
    return test;
}

// Test functions
static bool test_keyword_recognition() {
    Token test_token = make_token(TOKEN_TEST, "test");
    Token other_token = make_token(TOKEN_IDENTIFIER, "function");
    
    if (!wyn_is_test_keyword(test_token)) {
        return false;
    }
    
    if (wyn_is_test_keyword(other_token)) {
        return false;
    }
    
    return true;
}

static bool test_syntax_validation() {
    // Valid test syntax
    Token body[] = {
        make_token(TOKEN_IDENTIFIER, "assert_equal"),
        make_token(TOKEN_LEFT_BRACE, "("),
        make_token(TOKEN_RIGHT_BRACE, ")")
    };
    
    TestSyntax valid_test = make_test_syntax("\"basic arithmetic\"", body, 3);
    
    if (!wyn_validate_test_syntax(&valid_test)) {
        return false;
    }
    
    // Invalid test syntax (no name)
    TestSyntax invalid_test = {0};
    invalid_test.name = make_token(TOKEN_IDENTIFIER, "not_a_string");
    invalid_test.body_tokens = body;
    invalid_test.body_count = 3;
    
    if (wyn_validate_test_syntax(&invalid_test)) {
        return false;
    }
    
    return true;
}

static bool test_registry_operations() {
    TestRegistry* registry = wyn_create_test_registry();
    if (!registry) return false;
    
    // Create test
    Token body[] = {
        make_token(TOKEN_IDENTIFIER, "assert_true"),
        make_token(TOKEN_LEFT_BRACE, "("),
        make_token(TOKEN_RIGHT_BRACE, ")")
    };
    
    TestSyntax test1 = make_test_syntax("\"string operations\"", body, 3);
    
    // Register test
    if (!wyn_register_test(registry, &test1)) {
        wyn_free_test_registry(registry);
        return false;
    }
    
    // Find test
    TestSyntax* found = wyn_find_test_by_name(registry, "string operations");
    if (!found) {
        wyn_free_test_registry(registry);
        return false;
    }
    
    // Check test count
    if (registry->test_count != 1) {
        wyn_free_test_registry(registry);
        return false;
    }
    
    wyn_free_test_registry(registry);
    return true;
}

static bool test_naming_conventions() {
    // Valid names
    if (!wyn_validate_test_name("basic arithmetic")) return false;
    if (!wyn_validate_test_name("string operations")) return false;
    if (!wyn_validate_test_name("edge cases")) return false;
    
    // Invalid names
    if (wyn_validate_test_name("")) return false;           // Empty
    if (wyn_validate_test_name("a")) return false;          // Too short
    if (wyn_validate_test_name("123test")) return false;    // Starts with number
    if (wyn_validate_test_name(NULL)) return false;         // Null
    
    return true;
}

static bool test_discovery_mechanism() {
    g_test_registry = wyn_create_test_registry();
    if (!g_test_registry) return false;
    
    // Register multiple tests
    Token body1[] = {make_token(TOKEN_IDENTIFIER, "test1")};
    Token body2[] = {make_token(TOKEN_IDENTIFIER, "test2")};
    Token body3[] = {make_token(TOKEN_IDENTIFIER, "test3")};
    
    TestSyntax test1 = make_test_syntax("\"test one\"", body1, 1);
    TestSyntax test2 = make_test_syntax("\"test two\"", body2, 1);
    TestSyntax test3 = make_test_syntax("\"test three\"", body3, 1);
    
    wyn_register_test(g_test_registry, &test1);
    wyn_register_test(g_test_registry, &test2);
    wyn_register_test(g_test_registry, &test3);
    
    // Check discovery
    if (g_test_registry->test_count != 3) {
        wyn_free_test_registry(g_test_registry);
        return false;
    }
    
    // Test individual discovery
    TestSyntax* found1 = wyn_find_test_by_name(g_test_registry, "test one");
    TestSyntax* found2 = wyn_find_test_by_name(g_test_registry, "test two");
    TestSyntax* found3 = wyn_find_test_by_name(g_test_registry, "test three");
    
    bool all_found = (found1 != NULL) && (found2 != NULL) && (found3 != NULL);
    
    wyn_free_test_registry(g_test_registry);
    return all_found;
}

int main() {
    printf("🔥 Testing T1.6.1: Test Syntax Design\n");
    printf("=====================================\n\n");
    
    bool all_passed = true;
    
    RUN_TEST(test_keyword_recognition);
    RUN_TEST(test_syntax_validation);
    RUN_TEST(test_registry_operations);
    RUN_TEST(test_naming_conventions);
    RUN_TEST(test_discovery_mechanism);
    
    printf("\n=====================================\n");
    if (all_passed) {
        printf("✅ All T1.6.1 test syntax design tests PASSED!\n");
        printf("T1.6.1: Test Syntax Design - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.6.1 tests FAILED!\n");
        return 1;
    }
}
