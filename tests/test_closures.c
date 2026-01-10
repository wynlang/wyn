// T3.4.1: Closure Syntax and Capture Test Program
// Test lambda expressions with automatic environment capture

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "types.h"

// Simple make_type implementation for testing
Type* make_type(TypeKind kind) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = kind;
    return t;
}

// Simple add_symbol implementation for testing
void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable) {
    // Simplified implementation for testing
    if (!scope) return;
    printf("   Binding variable: %.*s (mutable: %s)\n", 
           (int)name.length, name.start, is_mutable ? "yes" : "no");
}

// Simple find_symbol implementation for testing
Symbol* find_symbol(SymbolTable* scope, Token name) {
    // Simplified implementation for testing
    static Symbol dummy_symbol = {0};
    dummy_symbol.name = name;
    dummy_symbol.type = make_type(TYPE_INT);
    dummy_symbol.is_mutable = false;
    return &dummy_symbol;
}

// Simple check_expr implementation for testing
Type* check_expr(Expr* expr, SymbolTable* scope) {
    if (!expr) return NULL;
    return make_type(TYPE_INT); // Simplified
}

// Test closure functionality
int main() {
    printf("=== T3.4.1: Closure Syntax and Capture Test ===\n");
    
    // Initialize closure system
    wyn_closures_init();
    
    // Test lambda creation
    printf("1. Testing lambda creation...\n");
    
    Token params[2] = {
        {TOKEN_IDENT, "x", 1, 0},
        {TOKEN_IDENT, "y", 1, 0}
    };
    
    Expr body = {EXPR_BINARY, .token = {TOKEN_PLUS, "+", 1, 0}};
    SymbolTable scope = {0};
    
    LambdaExpr* lambda = wyn_create_lambda(params, 2, &body, &scope);
    printf("   Lambda created: %s\n", lambda ? "Success" : "Failed");
    
    if (lambda) {
        printf("   Parameter count: %d\n", lambda->param_count);
        printf("   Captured variables: %d\n", lambda->captured_count);
    }
    
    // Test lambda validation
    printf("2. Testing lambda validation...\n");
    
    bool valid = wyn_validate_lambda(lambda, &scope);
    printf("   Lambda validation: %s\n", valid ? "Passed" : "Failed");
    
    // Test closure type creation
    printf("3. Testing closure type creation...\n");
    
    Type* closure_type = wyn_create_closure_type(lambda, &scope);
    printf("   Closure type created: %s\n", closure_type ? "Success" : "Failed");
    
    if (closure_type && closure_type->kind == TYPE_FUNCTION) {
        printf("   Parameter count: %d\n", closure_type->fn_type.param_count);
        printf("   Return type: %s\n", closure_type->fn_type.return_type ? "Set" : "None");
    }
    
    // Test Fn trait implementation
    printf("4. Testing Fn trait implementation...\n");
    
    Token fn_trait = {TOKEN_IDENT, "Fn", 2, 0};
    Token fn_mut_trait = {TOKEN_IDENT, "FnMut", 5, 0};
    Token fn_once_trait = {TOKEN_IDENT, "FnOnce", 6, 0};
    
    bool implements_fn = wyn_implements_fn_trait(closure_type, fn_trait);
    bool implements_fn_mut = wyn_implements_fn_trait(closure_type, fn_mut_trait);
    bool implements_fn_once = wyn_implements_fn_trait(closure_type, fn_once_trait);
    
    printf("   Implements Fn: %s\n", implements_fn ? "Yes" : "No");
    printf("   Implements FnMut: %s\n", implements_fn_mut ? "Yes" : "No");
    printf("   Implements FnOnce: %s\n", implements_fn_once ? "Yes" : "No");
    
    // Test closure name generation
    printf("5. Testing closure name generation...\n");
    
    char closure_name[64];
    wyn_generate_closure_name(lambda, closure_name, sizeof(closure_name));
    printf("   Generated closure name: %s\n", closure_name);
    
    // Test closure application
    printf("6. Testing closure application...\n");
    
    Expr arg1 = {EXPR_INT, .token = {TOKEN_INT, "5", 1, 0}};
    Expr arg2 = {EXPR_INT, .token = {TOKEN_INT, "3", 1, 0}};
    Expr* args[] = {&arg1, &arg2};
    
    Type* result_type = wyn_apply_closure(lambda, args, 2, &scope);
    printf("   Closure application result: %s\n", result_type ? "Success" : "Failed");
    
    // Test with wrong argument count
    Type* wrong_result = wyn_apply_closure(lambda, args, 1, &scope);
    printf("   Wrong arg count handled: %s\n", wrong_result ? "Unexpectedly succeeded" : "Correctly failed");
    
    // Test lambda with captures
    printf("7. Testing lambda with captures...\n");
    
    Token capture_params[1] = {{TOKEN_IDENT, "n", 1, 0}};
    Expr capture_body = {EXPR_BINARY, .token = {TOKEN_PLUS, "+", 1, 0}};
    
    LambdaExpr* capturing_lambda = wyn_create_lambda(capture_params, 1, &capture_body, &scope);
    printf("   Capturing lambda created: %s\n", capturing_lambda ? "Success" : "Failed");
    
    if (capturing_lambda) {
        printf("   Parameter count: %d\n", capturing_lambda->param_count);
        printf("   Captured variables: %d\n", capturing_lambda->captured_count);
    }
    
    // Print statistics
    printf("8. Closure system statistics:\n");
    wyn_print_closure_stats();
    
    // Cleanup
    if (lambda) {
        wyn_free_lambda(lambda);
    }
    if (capturing_lambda) {
        wyn_free_lambda(capturing_lambda);
    }
    if (closure_type) {
        free(closure_type);
    }
    wyn_cleanup_closures();
    
    printf("=== T3.4.1: Closure Syntax and Capture Test Complete ===\n");
    return 0;
}
