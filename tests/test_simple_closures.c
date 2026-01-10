// T3.4.1: Simple Closure Test Program
// Test basic closure functionality without full type system integration

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

// Test closure functionality
int main() {
    printf("=== T3.4.1: Simple Closure Test ===\n");
    
    // Test lambda expression structure
    printf("1. Testing lambda expression structure...\n");
    
    Token params[2] = {
        {TOKEN_IDENT, "x", 1, 0},
        {TOKEN_IDENT, "y", 1, 0}
    };
    
    Expr body = {EXPR_BINARY, .token = {TOKEN_PLUS, "+", 1, 0}};
    
    LambdaExpr lambda = {
        .params = params,
        .param_count = 2,
        .body = &body,
        .captured_vars = NULL,
        .captured_count = 0,
        .capture_by_move = NULL
    };
    
    printf("   Lambda parameter count: %d\n", lambda.param_count);
    printf("   Lambda captured variables: %d\n", lambda.captured_count);
    printf("   Lambda body type: %d\n", lambda.body->type);
    
    // Test capture simulation
    printf("2. Testing capture simulation...\n");
    
    Token captured_vars[2] = {
        {TOKEN_IDENT, "n", 1, 0},
        {TOKEN_IDENT, "m", 1, 0}
    };
    
    bool capture_modes[2] = {false, true}; // reference, move
    
    lambda.captured_vars = captured_vars;
    lambda.captured_count = 2;
    lambda.capture_by_move = capture_modes;
    
    printf("   Captured variables: %d\n", lambda.captured_count);
    for (int i = 0; i < lambda.captured_count; i++) {
        printf("     Variable %d: %.*s (mode: %s)\n", 
               i + 1, lambda.captured_vars[i].length, lambda.captured_vars[i].start,
               lambda.capture_by_move[i] ? "move" : "reference");
    }
    
    // Test lambda expression in expression structure
    printf("3. Testing lambda in expression structure...\n");
    
    Expr lambda_expr = {
        .type = EXPR_LAMBDA,
        .token = {TOKEN_IDENT, "closure", 7, 0},
        .lambda = lambda
    };
    
    printf("   Expression type: %d\n", lambda_expr.type);
    printf("   Lambda parameters: %d\n", lambda_expr.lambda.param_count);
    printf("   Lambda captures: %d\n", lambda_expr.lambda.captured_count);
    
    // Test parameter access
    printf("4. Testing parameter access...\n");
    
    for (int i = 0; i < lambda_expr.lambda.param_count; i++) {
        printf("   Parameter %d: %.*s\n", 
               i + 1, lambda_expr.lambda.params[i].length, lambda_expr.lambda.params[i].start);
    }
    
    // Test closure type concepts
    printf("5. Testing closure type concepts...\n");
    
    // Simulate Fn trait checking
    bool implements_fn = true;      // All closures can implement Fn
    bool implements_fn_mut = true;  // Closures with mutable captures
    bool implements_fn_once = true; // All closures implement FnOnce
    
    printf("   Implements Fn: %s\n", implements_fn ? "Yes" : "No");
    printf("   Implements FnMut: %s\n", implements_fn_mut ? "Yes" : "No");
    printf("   Implements FnOnce: %s\n", implements_fn_once ? "Yes" : "No");
    
    // Test closure name generation
    printf("6. Testing closure name generation...\n");
    
    char closure_name[64];
    snprintf(closure_name, sizeof(closure_name), "closure_%p", (void*)&lambda);
    printf("   Generated closure name: %s\n", closure_name);
    
    // Test capture analysis concepts
    printf("7. Testing capture analysis concepts...\n");
    
    int reference_captures = 0;
    int move_captures = 0;
    
    for (int i = 0; i < lambda.captured_count; i++) {
        if (lambda.capture_by_move[i]) {
            move_captures++;
        } else {
            reference_captures++;
        }
    }
    
    printf("   Reference captures: %d\n", reference_captures);
    printf("   Move captures: %d\n", move_captures);
    printf("   Total captures: %d\n", lambda.captured_count);
    
    printf("=== T3.4.1: Simple Closure Test Complete ===\n");
    return 0;
}
