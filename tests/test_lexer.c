#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "common.h"

void init_lexer(const char* source);
Token next_token();

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf("✓ test_%s\n", #name); \
} while(0)

TEST(int) {
    init_lexer("42");
    Token t = next_token();
    assert(t.type == TOKEN_INT);
    assert(t.length == 2);
}

TEST(float) {
    init_lexer("3.14");
    Token t = next_token();
    assert(t.type == TOKEN_FLOAT);
    assert(t.length == 4);
}

TEST(ident) {
    init_lexer("x");
    Token t = next_token();
    assert(t.type == TOKEN_IDENT);
    assert(t.length == 1);
}

TEST(string) {
    init_lexer("\"hello\"");
    Token t = next_token();
    assert(t.type == TOKEN_STRING);
}

TEST(keywords) {
    const char* keywords[] = {"fn", "struct", "enum", "var", "if", "else", "elseif", "return"};
    TokenType types[] = {TOKEN_FN, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_VAR, TOKEN_IF, TOKEN_ELSE, TOKEN_ELSEIF, TOKEN_RETURN};
    
    for (int i = 0; i < 8; i++) {
        init_lexer(keywords[i]);
        Token t = next_token();
        assert(t.type == types[i]);
    }
}

TEST(operators) {
    init_lexer("+ - * / == != < > <= >=");
    assert(next_token().type == TOKEN_PLUS);
    assert(next_token().type == TOKEN_MINUS);
    assert(next_token().type == TOKEN_STAR);
    assert(next_token().type == TOKEN_SLASH);
    assert(next_token().type == TOKEN_EQEQ);
    assert(next_token().type == TOKEN_BANGEQ);
    assert(next_token().type == TOKEN_LT);
    assert(next_token().type == TOKEN_GT);
    assert(next_token().type == TOKEN_LTEQ);
    assert(next_token().type == TOKEN_GTEQ);
}

TEST(delimiters) {
    init_lexer("( ) { } [ ] , : ; .");
    assert(next_token().type == TOKEN_LPAREN);
    assert(next_token().type == TOKEN_RPAREN);
    assert(next_token().type == TOKEN_LBRACE);
    assert(next_token().type == TOKEN_RBRACE);
    assert(next_token().type == TOKEN_LBRACKET);
    assert(next_token().type == TOKEN_RBRACKET);
    assert(next_token().type == TOKEN_COMMA);
    assert(next_token().type == TOKEN_COLON);
    assert(next_token().type == TOKEN_SEMI);
    assert(next_token().type == TOKEN_DOT);
}

TEST(arrow) {
    init_lexer("->");
    Token t = next_token();
    assert(t.type == TOKEN_ARROW);
}

TEST(function_signature) {
    init_lexer("fn add(a: int, b: int) -> int");
    assert(next_token().type == TOKEN_FN);
    assert(next_token().type == TOKEN_IDENT);
    assert(next_token().type == TOKEN_LPAREN);
    assert(next_token().type == TOKEN_IDENT);
    assert(next_token().type == TOKEN_COLON);
    assert(next_token().type == TOKEN_IDENT);
    assert(next_token().type == TOKEN_COMMA);
    assert(next_token().type == TOKEN_IDENT);
    assert(next_token().type == TOKEN_COLON);
    assert(next_token().type == TOKEN_IDENT);
    assert(next_token().type == TOKEN_RPAREN);
    assert(next_token().type == TOKEN_ARROW);
    assert(next_token().type == TOKEN_IDENT);
}

TEST(whitespace_and_comments) {
    init_lexer("  42  // comment\n  43  ");
    Token t1 = next_token();
    Token t2 = next_token();
    assert(t1.type == TOKEN_INT);
    assert(t2.type == TOKEN_INT);
    assert(t1.line == 1);
    assert(t2.line == 2);
}

int main() {
    printf("=== Lexer Tests ===\n");
    RUN_TEST(int);
    RUN_TEST(float);
    RUN_TEST(ident);
    RUN_TEST(string);
    RUN_TEST(keywords);
    RUN_TEST(operators);
    RUN_TEST(delimiters);
    RUN_TEST(arrow);
    RUN_TEST(function_signature);
    RUN_TEST(whitespace_and_comments);
    
    printf("\n%d/%d tests passed!\n", tests_passed, tests_run);
    return tests_run != tests_passed;
}
