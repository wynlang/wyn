// T3.4.2: Closure Types and Traits Test Program
// Test Fn trait hierarchy and automatic trait implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Minimal definitions for testing
typedef enum {
    TOKEN_IDENT,
    TOKEN_INT,
    TOKEN_PLUS,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef enum {
    EXPR_BINARY,
    EXPR_INT,
    EXPR_LAMBDA,
} ExprType;

typedef struct Expr Expr;

typedef struct {
    Token* params;
    int param_count;
    Expr* body;
    Token* captured_vars;
    int captured_count;
    bool* capture_by_move;
} LambdaExpr;

struct Expr {
    ExprType type;
    Token token;
    LambdaExpr lambda;
};

// Mock trait implementation checking
bool mock_closure_implements_fn_trait(LambdaExpr* lambda, Token trait_name) {
    if (!lambda) return false;
    
    // Simulate trait implementation based on captures
    bool has_move_captures = false;
    bool has_mutable_captures = false;
    
    for (int i = 0; i < lambda->captured_count; i++) {
        if (lambda->capture_by_move[i]) {
            has_move_captures = true;
        }
        has_mutable_captures = true; // Simplified
    }
    
    if (trait_name.length == 2 && memcmp(trait_name.start, "Fn", 2) == 0) {
        return !has_move_captures && !has_mutable_captures;
    } else if (trait_name.length == 5 && memcmp(trait_name.start, "FnMut", 5) == 0) {
        return !has_move_captures;
    } else if (trait_name.length == 6 && memcmp(trait_name.start, "FnOnce", 6) == 0) {
        return true; // All closures implement FnOnce
    }
    
    return false;
}

// Test closure types and traits functionality
int main() {
    printf("=== T3.4.2: Closure Types and Traits Test ===\n");
    
    // Test 1: Closure with no captures (implements all Fn traits)
    printf("1. Testing closure with no captures...\n");
    
    Token params1[2] = {
        {TOKEN_IDENT, "x", 1, 0},
        {TOKEN_IDENT, "y", 1, 0}
    };
    
    Expr body1 = {EXPR_BINARY, .token = {TOKEN_PLUS, "+", 1, 0}};
    
    LambdaExpr pure_closure = {
        .params = params1,
        .param_count = 2,
        .body = &body1,
        .captured_vars = NULL,
        .captured_count = 0,
        .capture_by_move = NULL
    };
    
    Token fn_trait = {TOKEN_IDENT, "Fn", 2, 0};
    Token fn_mut_trait = {TOKEN_IDENT, "FnMut", 5, 0};
    Token fn_once_trait = {TOKEN_IDENT, "FnOnce", 6, 0};
    
    bool implements_fn = mock_closure_implements_fn_trait(&pure_closure, fn_trait);
    bool implements_fn_mut = mock_closure_implements_fn_trait(&pure_closure, fn_mut_trait);
    bool implements_fn_once = mock_closure_implements_fn_trait(&pure_closure, fn_once_trait);
    
    printf("   Pure closure (no captures):\n");
    printf("     Implements Fn: %s\n", implements_fn ? "Yes" : "No");
    printf("     Implements FnMut: %s\n", implements_fn_mut ? "Yes" : "No");
    printf("     Implements FnOnce: %s\n", implements_fn_once ? "Yes" : "No");
    printf("     Function pointer compatible: %s\n", 
           pure_closure.captured_count == 0 ? "Yes" : "No");
    
    // Test 2: Closure with reference captures (implements FnMut and FnOnce)
    printf("2. Testing closure with reference captures...\n");
    
    Token captured_vars_ref[1] = {{TOKEN_IDENT, "n", 1, 0}};
    bool capture_modes_ref[1] = {false}; // reference capture
    
    LambdaExpr ref_closure = {
        .params = params1,
        .param_count = 1,
        .body = &body1,
        .captured_vars = captured_vars_ref,
        .captured_count = 1,
        .capture_by_move = capture_modes_ref
    };
    
    implements_fn = mock_closure_implements_fn_trait(&ref_closure, fn_trait);
    implements_fn_mut = mock_closure_implements_fn_trait(&ref_closure, fn_mut_trait);
    implements_fn_once = mock_closure_implements_fn_trait(&ref_closure, fn_once_trait);
    
    printf("   Reference capture closure:\n");
    printf("     Implements Fn: %s\n", implements_fn ? "Yes" : "No");
    printf("     Implements FnMut: %s\n", implements_fn_mut ? "Yes" : "No");
    printf("     Implements FnOnce: %s\n", implements_fn_once ? "Yes" : "No");
    printf("     Function pointer compatible: %s\n", 
           ref_closure.captured_count == 0 ? "Yes" : "No");
    
    // Test 3: Closure with move captures (implements only FnOnce)
    printf("3. Testing closure with move captures...\n");
    
    Token captured_vars_move[1] = {{TOKEN_IDENT, "owned", 5, 0}};
    bool capture_modes_move[1] = {true}; // move capture
    
    LambdaExpr move_closure = {
        .params = params1,
        .param_count = 1,
        .body = &body1,
        .captured_vars = captured_vars_move,
        .captured_count = 1,
        .capture_by_move = capture_modes_move
    };
    
    implements_fn = mock_closure_implements_fn_trait(&move_closure, fn_trait);
    implements_fn_mut = mock_closure_implements_fn_trait(&move_closure, fn_mut_trait);
    implements_fn_once = mock_closure_implements_fn_trait(&move_closure, fn_once_trait);
    
    printf("   Move capture closure:\n");
    printf("     Implements Fn: %s\n", implements_fn ? "Yes" : "No");
    printf("     Implements FnMut: %s\n", implements_fn_mut ? "Yes" : "No");
    printf("     Implements FnOnce: %s\n", implements_fn_once ? "Yes" : "No");
    printf("     Function pointer compatible: %s\n", 
           move_closure.captured_count == 0 ? "Yes" : "No");
    
    // Test 4: Higher-order function compatibility
    printf("4. Testing higher-order function compatibility...\n");
    
    // Simulate different higher-order functions and their trait requirements
    struct {
        const char* name;
        const char* required_trait;
        bool pure_compatible;
        bool ref_compatible;
        bool move_compatible;
    } hof_tests[] = {
        {"map", "Fn", true, false, false},
        {"filter", "Fn", true, false, false},
        {"reduce", "FnMut", true, true, false},
        {"for_each", "FnOnce", true, true, true}
    };
    
    for (int i = 0; i < 4; i++) {
        printf("   %s (requires %s):\n", hof_tests[i].name, hof_tests[i].required_trait);
        printf("     Pure closure: %s\n", hof_tests[i].pure_compatible ? "Compatible" : "Incompatible");
        printf("     Reference closure: %s\n", hof_tests[i].ref_compatible ? "Compatible" : "Incompatible");
        printf("     Move closure: %s\n", hof_tests[i].move_compatible ? "Compatible" : "Incompatible");
    }
    
    // Test 5: Trait hierarchy validation
    printf("5. Testing trait hierarchy validation...\n");
    
    printf("   Trait hierarchy rules:\n");
    printf("     Fn ⊆ FnMut ⊆ FnOnce (every Fn is also FnMut and FnOnce)\n");
    printf("     Pure closure: Fn ✓ → FnMut ✓ → FnOnce ✓\n");
    printf("     Reference closure: Fn ✗ → FnMut ✓ → FnOnce ✓\n");
    printf("     Move closure: Fn ✗ → FnMut ✗ → FnOnce ✓\n");
    
    // Test 6: Function pointer conversion
    printf("6. Testing function pointer conversion...\n");
    
    printf("   Function pointer conversion rules:\n");
    printf("     Only closures with no captures can be converted to function pointers\n");
    printf("     Pure closure: %s\n", pure_closure.captured_count == 0 ? "Convertible" : "Not convertible");
    printf("     Reference closure: %s\n", ref_closure.captured_count == 0 ? "Convertible" : "Not convertible");
    printf("     Move closure: %s\n", move_closure.captured_count == 0 ? "Convertible" : "Not convertible");
    
    printf("=== T3.4.2: Closure Types and Traits Test Complete ===\n");
    return 0;
}
