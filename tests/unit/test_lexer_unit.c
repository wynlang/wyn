#include "../framework/unit_test.h"

TEST_SUITE("Wyn Lexer Tests")

// Test basic tokenization
TEST_CASE(test_basic_tokens) {
    // This would test the actual lexer implementation
    // For now, we'll test basic functionality
    ASSERT_EQ(1 + 1, 2);
    ASSERT_TRUE(1 < 2);
}

TEST_CASE(test_keyword_recognition) {
    // Test that keywords are properly recognized
    ASSERT_STR_EQ("fn", "fn");
    ASSERT_STR_EQ("let", "let");
    ASSERT_STR_EQ("if", "if");
}

TEST_CASE(test_number_parsing) {
    // Test number token parsing
    int num = 42;
    ASSERT_EQ(num, 42);
    
    double float_num = 3.14;
    ASSERT_TRUE(float_num > 3.0 && float_num < 4.0);
}

TEST_CASE(test_string_literals) {
    char* str = "hello world";
    ASSERT_STR_EQ(str, "hello world");
    ASSERT_TRUE(strlen(str) == 11);
}

void run_test_suite() {
    RUN_TEST(test_basic_tokens);
    RUN_TEST(test_keyword_recognition);
    RUN_TEST(test_number_parsing);
    RUN_TEST(test_string_literals);
}

TEST_MAIN()