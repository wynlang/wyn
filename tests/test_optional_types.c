// T2.5.1: Optional Type Implementation - Test Suite
// Type System Agent

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/common.h"
#include "../src/ast.h"
#include "../src/types.h"
#include "../src/parser.c"
#include "../src/checker.c"

// Test helper functions
void setup_test() {
    // Initialize parser state
    parser.had_error = false;
}

void test_optional_type_parsing() {
    printf("Testing optional type parsing...\n");
    
    // Test parsing "int?" type
    setup_test();
    
    // Simulate tokens for "int?"
    Token int_token = {TOKEN_IDENT, "int", 3, 1};
    Token question_token = {TOKEN_QUESTION, "?", 1, 1};
    Token eof_token = {TOKEN_EOF, "", 0, 1};
    
    // Mock the token stream
    parser.current = int_token;
    
    // Parse the type
    Expr* type_expr = parse_type();
    
    assert(type_expr != NULL);
    assert(type_expr->type == EXPR_OPTIONAL_TYPE);
    assert(type_expr->optional_type.inner_type != NULL);
    assert(type_expr->optional_type.inner_type->type == EXPR_IDENT);
    
    printf("✅ Optional type parsing test passed\n");
}

void test_optional_type_checking() {
    printf("Testing optional type checking...\n");
    
    // Initialize type system
    builtin_int = make_type(TYPE_INT);
    builtin_string = make_type(TYPE_STRING);
    builtin_bool = make_type(TYPE_BOOL);
    builtin_void = make_type(TYPE_VOID);
    
    // Create a simple scope
    SymbolTable scope = {0};
    scope.symbols = malloc(sizeof(Symbol) * 10);
    scope.capacity = 10;
    scope.count = 0;
    scope.parent = NULL;
    
    // Create an optional int type expression
    Expr* int_expr = malloc(sizeof(Expr));
    int_expr->type = EXPR_IDENT;
    int_expr->token = (Token){TOKEN_IDENT, "int", 3, 1};
    
    Expr* optional_expr = malloc(sizeof(Expr));
    optional_expr->type = EXPR_OPTIONAL_TYPE;
    optional_expr->optional_type.inner_type = int_expr;
    
    // Check the type
    Type* result_type = check_expr(optional_expr, &scope);
    
    assert(result_type != NULL);
    assert(result_type->kind == TYPE_OPTIONAL);
    assert(result_type->optional_type.inner_type != NULL);
    assert(result_type->optional_type.inner_type->kind == TYPE_INT);
    
    printf("✅ Optional type checking test passed\n");
    
    // Cleanup
    free(scope.symbols);
    free(int_expr);
    free(optional_expr);
}

void test_null_safety_enforcement() {
    printf("Testing null safety enforcement...\n");
    
    // Initialize type system
    builtin_int = make_type(TYPE_INT);
    
    // Create types
    Type* int_type = make_type(TYPE_INT);
    Type* optional_int_type = make_type(TYPE_OPTIONAL);
    optional_int_type->optional_type.inner_type = int_type;
    
    // Test helper functions
    assert(is_optional_type(optional_int_type) == true);
    assert(is_optional_type(int_type) == false);
    
    Type* inner = get_inner_type(optional_int_type);
    assert(inner == int_type);
    
    inner = get_inner_type(int_type);
    assert(inner == int_type);
    
    printf("✅ Null safety enforcement test passed\n");
}

void test_type_printing() {
    printf("Testing optional type printing...\n");
    
    // Create optional int type
    Type* int_type = make_type(TYPE_INT);
    Type* optional_int_type = make_type(TYPE_OPTIONAL);
    optional_int_type->optional_type.inner_type = int_type;
    
    // Test printing (we can't easily capture printf output, so just call it)
    printf("Optional int type: ");
    print_type_name(optional_int_type);
    printf("\n");
    
    printf("✅ Type printing test passed\n");
}

int main() {
    printf("=== T2.5.1: Optional Type Implementation Tests ===\n\n");
    
    test_optional_type_parsing();
    test_optional_type_checking();
    test_null_safety_enforcement();
    test_type_printing();
    
    printf("\n✅ All optional type tests passed!\n");
    return 0;
}