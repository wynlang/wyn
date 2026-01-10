#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "ast.h"

void init_lexer(const char* source);
void init_parser();
Expr* expression();

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf("✓ test_%s\n", #name); \
} while(0)

TEST(arithmetic_ops) {
    init_lexer("1 + 2 - 3 * 4 / 2 % 3");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_BINARY);
}

TEST(comparison_ops) {
    init_lexer("x < y");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_BINARY);
    assert(expr->binary.op.type == TOKEN_LT);
}

TEST(logical_ops) {
    init_lexer("true and false or not true");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_BINARY);
}

TEST(bitwise_ops) {
    init_lexer("x & y | z ^ w");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_BINARY);
}

TEST(compound_assign) {
    init_lexer("x += 5");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_ASSIGN);
}

TEST(unary_ops) {
    init_lexer("-x");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_UNARY);
}

TEST(array_literal) {
    init_lexer("[1, 2, 3]");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_ARRAY);
    assert(expr->array.count == 3);
}

TEST(array_index) {
    init_lexer("arr[0]");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_INDEX);
}

TEST(field_access) {
    init_lexer("obj.field");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_FIELD_ACCESS);
}

TEST(method_call) {
    init_lexer("obj.method(1, 2)");
    init_parser();
    Expr* expr = expression();
    assert(expr != NULL);
    assert(expr->type == EXPR_METHOD_CALL);
    assert(expr->method_call.arg_count == 2);
}

int main() {
    printf("=== Operator Tests ===\n");
    RUN_TEST(arithmetic_ops);
    RUN_TEST(comparison_ops);
    RUN_TEST(logical_ops);
    RUN_TEST(bitwise_ops);
    RUN_TEST(compound_assign);
    RUN_TEST(unary_ops);
    RUN_TEST(array_literal);
    RUN_TEST(array_index);
    RUN_TEST(field_access);
    RUN_TEST(method_call);
    
    printf("\n%d/%d tests passed!\n", tests_passed, tests_run);
    return tests_run != tests_passed;
}
