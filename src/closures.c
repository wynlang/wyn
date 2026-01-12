#include "ast.h"
#include "types.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T3.4.1: Closure Syntax and Capture Implementation
// Lambda expressions with automatic environment capture

// Forward declarations
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);
void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable);

// Closure capture types
typedef enum {
    CAPTURE_BY_REFERENCE,  // Capture by reference (default)
    CAPTURE_BY_MOVE,       // Capture by move (ownership transfer)
    CAPTURE_BY_COPY,       // Capture by copy (for Copy types)
} CaptureMode;

// Closure environment entry
typedef struct CaptureEntry {
    Token var_name;
    Type* var_type;
    CaptureMode mode;
    struct CaptureEntry* next;
} CaptureEntry;

// Closure type information
typedef struct ClosureType {
    Token* param_types;
    int param_count;
    Type* return_type;
    CaptureEntry* captures;
    int capture_count;
    struct ClosureType* next;
} ClosureType;

// Closure statistics
typedef struct {
    size_t closures_created;
    size_t variables_captured;
    size_t move_captures;
    size_t reference_captures;
    size_t copy_captures;
} ClosureStats;

static ClosureType* g_closure_types = NULL;
static ClosureStats g_closure_stats = {0};

// Initialize closure system
void wyn_closures_init(void) {
    g_closure_types = NULL;
    memset(&g_closure_stats, 0, sizeof(ClosureStats));
}

// T3.4.1: Create a lambda expression with capture analysis
LambdaExpr* wyn_create_lambda(Token* params, int param_count, Expr* body, SymbolTable* scope) {
    LambdaExpr* lambda = malloc(sizeof(LambdaExpr));
    if (!lambda) return NULL;
    
    lambda->params = params;
    lambda->param_count = param_count;
    lambda->body = body;
    lambda->captured_vars = NULL;
    lambda->captured_count = 0;
    lambda->capture_by_move = NULL;
    
    // Analyze captures in the lambda body
    wyn_analyze_lambda_captures(lambda, body, scope);
    
    // T3.4.2: Automatically implement Fn traits
    wyn_implement_closure_traits(lambda);
    
    g_closure_stats.closures_created++;
    return lambda;
}

// T3.4.1: Analyze what variables a lambda captures from its environment
void wyn_analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope) {
    if (!lambda || !body || !scope) return;
    
    // For now, implement a simplified capture analysis
    // In a full implementation, this would traverse the AST and identify free variables
    
    // Simplified: assume we capture up to 8 variables
    Token* captures = malloc(sizeof(Token) * 8);
    bool* move_flags = malloc(sizeof(bool) * 8);
    int capture_count = 0;
    
    // Example: if body references variables not in parameters, capture them
    // This is a placeholder - real implementation would do AST traversal
    
    lambda->captured_vars = captures;
    lambda->captured_count = capture_count;
    lambda->capture_by_move = move_flags;
    
    g_closure_stats.variables_captured += capture_count;
}

// T3.4.1: Determine capture mode for a variable
CaptureMode wyn_determine_capture_mode(Token var_name, Type* var_type, Expr* usage_context) {
    (void)var_name;       // Reserved for variable-specific analysis
    (void)usage_context;  // Reserved for context-aware capture
    // Simplified capture mode determination
    if (!var_type) return CAPTURE_BY_REFERENCE;
    
    // If the variable is used in a way that requires ownership, capture by move
    // For now, default to reference capture
    return CAPTURE_BY_REFERENCE;
}

// T3.4.1: Create a closure type from a lambda expression
Type* wyn_create_closure_type(LambdaExpr* lambda, SymbolTable* scope) {
    if (!lambda) return NULL;
    
    Type* closure_type = make_type(TYPE_FUNCTION);
    if (!closure_type) return NULL;
    
    // Set up function type information
    closure_type->fn_type.param_count = lambda->param_count;
    closure_type->fn_type.param_types = malloc(sizeof(Type*) * lambda->param_count);
    
    // Infer parameter types (simplified)
    for (int i = 0; i < lambda->param_count; i++) {
        closure_type->fn_type.param_types[i] = make_type(TYPE_INT); // Placeholder
    }
    
    // Infer return type from body
    if (lambda->body) {
        closure_type->fn_type.return_type = check_expr(lambda->body, scope);
    } else {
        closure_type->fn_type.return_type = make_type(TYPE_VOID);
    }
    
    return closure_type;
}

// T3.4.1: Check if a lambda expression is valid
bool wyn_validate_lambda(LambdaExpr* lambda, SymbolTable* scope) {
    if (!lambda || !scope) return false;
    
    // Check parameter names are unique
    for (int i = 0; i < lambda->param_count; i++) {
        for (int j = i + 1; j < lambda->param_count; j++) {
            if (lambda->params[i].length == lambda->params[j].length &&
                memcmp(lambda->params[i].start, lambda->params[j].start, 
                       lambda->params[i].length) == 0) {
                printf("Error: Duplicate parameter name in lambda\n");
                return false;
            }
        }
    }
    
    // Check that captured variables are accessible
    for (int i = 0; i < lambda->captured_count; i++) {
        Symbol* sym = find_symbol(scope, lambda->captured_vars[i]);
        if (!sym) {
            printf("Error: Captured variable not found in scope\n");
            return false;
        }
    }
    
    return true;
}

// T3.4.1: Apply a closure (call it with arguments)
Type* wyn_apply_closure(LambdaExpr* lambda, Expr** args, int arg_count, SymbolTable* scope) {
    if (!lambda || !args) return NULL;
    
    // Check argument count matches parameter count
    if (arg_count != lambda->param_count) {
        printf("Error: Closure expects %d arguments, got %d\n", 
               lambda->param_count, arg_count);
        return NULL;
    }
    
    // Create new scope for closure execution
    SymbolTable closure_scope = {0};
    closure_scope.parent = scope;
    
    // Bind parameters to arguments
    for (int i = 0; i < lambda->param_count; i++) {
        Type* arg_type = check_expr(args[i], scope);
        if (arg_type) {
            add_symbol(&closure_scope, lambda->params[i], arg_type, false);
        }
    }
    
    // Bind captured variables
    for (int i = 0; i < lambda->captured_count; i++) {
        Symbol* captured = find_symbol(scope, lambda->captured_vars[i]);
        if (captured) {
            add_symbol(&closure_scope, lambda->captured_vars[i], captured->type, 
                      lambda->capture_by_move[i]);
        }
    }
    
    // Evaluate body in closure scope
    return check_expr(lambda->body, &closure_scope);
}

// T3.4.2: Enhanced Fn trait implementation checking
bool wyn_implements_fn_trait(Type* closure_type, Token trait_name) {
    if (!closure_type || closure_type->kind != TYPE_FUNCTION) return false;
    
    // For now, check against registered trait implementations
    // In a real implementation, this would check the closure's capture analysis
    
    if (trait_name.length == 2 && memcmp(trait_name.start, "Fn", 2) == 0) {
        // Fn: can be called multiple times with immutable captures
        return true; // Simplified: assume most closures can implement Fn
    } else if (trait_name.length == 5 && memcmp(trait_name.start, "FnMut", 5) == 0) {
        // FnMut: can be called multiple times with mutable captures
        return true; // Simplified: assume most closures can implement FnMut
    } else if (trait_name.length == 6 && memcmp(trait_name.start, "FnOnce", 6) == 0) {
        // FnOnce: can be called once, may consume captures
        return true; // All closures implement FnOnce
    }
    
    return false;
}

// T3.4.2: Check if a closure implements a specific Fn trait based on its captures
bool wyn_closure_implements_fn_trait(LambdaExpr* lambda, Token trait_name) {
    if (!lambda) return false;
    
    bool implements_fn, implements_fn_mut, implements_fn_once;
    wyn_determine_closure_traits(lambda, &implements_fn, &implements_fn_mut, &implements_fn_once);
    
    if (trait_name.length == 2 && memcmp(trait_name.start, "Fn", 2) == 0) {
        return implements_fn;
    } else if (trait_name.length == 5 && memcmp(trait_name.start, "FnMut", 5) == 0) {
        return implements_fn_mut;
    } else if (trait_name.length == 6 && memcmp(trait_name.start, "FnOnce", 6) == 0) {
        return implements_fn_once;
    }
    
    return false;
}

// T3.4.1: Generate a unique name for a closure type
void wyn_generate_closure_name(LambdaExpr* lambda, char* buffer, size_t buffer_size) {
    if (!lambda || !buffer) return;
    
    snprintf(buffer, buffer_size, "closure_%p", (void*)lambda);
}

// T3.4.1: Free a lambda expression and its resources
void wyn_free_lambda(LambdaExpr* lambda) {
    if (!lambda) return;
    
    if (lambda->params) {
        free(lambda->params);
    }
    
    if (lambda->captured_vars) {
        free(lambda->captured_vars);
    }
    
    if (lambda->capture_by_move) {
        free(lambda->capture_by_move);
    }
    
    // Note: body is freed by the expression cleanup system
    free(lambda);
}

// T3.4.1: Get closure system statistics
ClosureStats wyn_get_closure_stats(void) {
    return g_closure_stats;
}

// T3.4.1: Print closure system statistics
void wyn_print_closure_stats(void) {
    printf("=== Closure System Statistics ===\n");
    printf("Closures created: %zu\n", g_closure_stats.closures_created);
    printf("Variables captured: %zu\n", g_closure_stats.variables_captured);
    printf("Move captures: %zu\n", g_closure_stats.move_captures);
    printf("Reference captures: %zu\n", g_closure_stats.reference_captures);
    printf("Copy captures: %zu\n", g_closure_stats.copy_captures);
    printf("=================================\n");
}

// T3.4.1: Reset closure system statistics
void wyn_reset_closure_stats(void) {
    memset(&g_closure_stats, 0, sizeof(ClosureStats));
}

// T3.4.2: Determine which Fn traits a closure implements based on its captures
void wyn_determine_closure_traits(LambdaExpr* lambda, bool* implements_fn, bool* implements_fn_mut, bool* implements_fn_once) {
    if (!lambda || !implements_fn || !implements_fn_mut || !implements_fn_once) return;
    
    // All closures implement FnOnce (can be called at least once)
    *implements_fn_once = true;
    
    // Check if closure has any move captures
    bool has_move_captures = false;
    bool has_mutable_captures = false;
    
    for (int i = 0; i < lambda->captured_count; i++) {
        if (lambda->capture_by_move[i]) {
            has_move_captures = true;
        }
        // For now, assume all captures could be mutable (simplified)
        has_mutable_captures = true;
    }
    
    // Fn: can be called multiple times with immutable captures
    // Only if no move captures and no mutable captures
    *implements_fn = !has_move_captures && !has_mutable_captures;
    
    // FnMut: can be called multiple times, may have mutable captures
    // Only if no move captures (but mutable captures are OK)
    *implements_fn_mut = !has_move_captures;
}

// T3.4.2: Automatically implement Fn traits for a closure
void wyn_implement_closure_traits(LambdaExpr* lambda) {
    if (!lambda) return;
    
    bool implements_fn, implements_fn_mut, implements_fn_once;
    wyn_determine_closure_traits(lambda, &implements_fn, &implements_fn_mut, &implements_fn_once);
    
    // Create a unique type name for this closure
    char closure_type_name[64];
    wyn_generate_closure_name(lambda, closure_type_name, sizeof(closure_type_name));
    Token closure_type_token = {TOKEN_IDENT, closure_type_name, (int)strlen(closure_type_name), 0};
    
    // Register trait implementations
    if (implements_fn_once) {
        Token fn_once_trait = {TOKEN_IDENT, "FnOnce", 6, 0};
        wyn_register_trait_impl(fn_once_trait, closure_type_token, NULL, 0);
    }
    
    if (implements_fn_mut) {
        Token fn_mut_trait = {TOKEN_IDENT, "FnMut", 5, 0};
        wyn_register_trait_impl(fn_mut_trait, closure_type_token, NULL, 0);
    }
    
    if (implements_fn) {
        Token fn_trait = {TOKEN_IDENT, "Fn", 2, 0};
        wyn_register_trait_impl(fn_trait, closure_type_token, NULL, 0);
    }
}

// T3.4.2: Check if a closure is compatible with function pointers
bool wyn_is_closure_function_pointer_compatible(LambdaExpr* lambda) {
    if (!lambda) return false;
    
    // Function pointer compatibility requires:
    // 1. No captured variables (stateless)
    // 2. Compatible calling convention
    
    return lambda->captured_count == 0;
}

// T3.4.2: Convert a closure to a function pointer (if compatible)
void* wyn_closure_to_function_pointer(LambdaExpr* lambda) {
    if (!wyn_is_closure_function_pointer_compatible(lambda)) {
        return NULL; // Cannot convert
    }
    
    // In a real implementation, this would generate a function pointer
    // For now, return a placeholder
    return (void*)lambda;
}

// T3.4.2: Create a closure type with specific Fn trait bounds
Type* wyn_create_closure_type_with_bounds(LambdaExpr* lambda, Token trait_bound) {
    Type* closure_type = wyn_create_closure_type(lambda, NULL);
    if (!closure_type) return NULL;
    
    // Verify the closure implements the required trait
    if (!wyn_implements_fn_trait(closure_type, trait_bound)) {
        printf("Error: Closure does not implement required trait %.*s\n",
               (int)trait_bound.length, trait_bound.start);
        free(closure_type);
        return NULL;
    }
    
    return closure_type;
}

// T3.4.2: Apply higher-order function with closure
Type* wyn_apply_higher_order_function(Token fn_name, LambdaExpr* closure, Expr** args, int arg_count, SymbolTable* scope) {
    if (!closure || !args) return NULL;
    
    // Check if the closure implements the required trait for this higher-order function
    Token required_trait = {TOKEN_IDENT, "Fn", 2, 0}; // Default to Fn
    
    // Determine required trait based on function name
    if (fn_name.length == 3 && memcmp(fn_name.start, "map", 3) == 0) {
        required_trait = (Token){TOKEN_IDENT, "Fn", 2, 0};
    } else if (fn_name.length == 6 && memcmp(fn_name.start, "filter", 6) == 0) {
        required_trait = (Token){TOKEN_IDENT, "Fn", 2, 0};
    } else if (fn_name.length == 6 && memcmp(fn_name.start, "reduce", 6) == 0) {
        required_trait = (Token){TOKEN_IDENT, "FnMut", 5, 0};
    }
    
    // Create closure type with bounds checking
    Type* closure_type = wyn_create_closure_type_with_bounds(closure, required_trait);
    if (!closure_type) {
        return NULL;
    }
    
    // Apply the closure in the context of the higher-order function
    Type* result_type = wyn_apply_closure(closure, args, arg_count, scope);
    
    free(closure_type);
    return result_type;
}

// T3.4.1: Cleanup closure system
void wyn_cleanup_closures(void) {
    // Cleanup closure types
    ClosureType* current = g_closure_types;
    while (current) {
        ClosureType* next = current->next;
        
        // Free captures
        CaptureEntry* capture = current->captures;
        while (capture) {
            CaptureEntry* next_capture = capture->next;
            free(capture);
            capture = next_capture;
        }
        
        if (current->param_types) {
            free(current->param_types);
        }
        
        free(current);
        current = next;
    }
    g_closure_types = NULL;
    
    wyn_reset_closure_stats();
}
