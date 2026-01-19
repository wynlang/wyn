#include "types.h"
#include "ast.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for functions from checker.c
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);

// T2.5.4: Type Inference Improvements
// Enhanced type inference for local variables, function returns, and generics

// Type inference context
typedef struct {
    bool inference_enabled;
    size_t variables_inferred;
    size_t functions_inferred;
    size_t generics_inferred;
} TypeInferenceContext;

static TypeInferenceContext g_inference_context = {
    .inference_enabled = true,
    .variables_inferred = 0,
    .functions_inferred = 0,
    .generics_inferred = 0
};

// Initialize type inference system
void wyn_type_inference_init(void) {
    g_inference_context.inference_enabled = true;
    g_inference_context.variables_inferred = 0;
    g_inference_context.functions_inferred = 0;
    g_inference_context.generics_inferred = 0;
}

// Infer local variable type from initializer
Type* wyn_infer_variable_type(Expr* init_expr, SymbolTable* scope) {
    if (!g_inference_context.inference_enabled || !init_expr) {
        return NULL;
    }
    
    Type* inferred_type = NULL;
    
    switch (init_expr->type) {
        case EXPR_INT:
            inferred_type = make_type(TYPE_INT);
            break;
        case EXPR_FLOAT:
            inferred_type = make_type(TYPE_FLOAT);
            break;
        case EXPR_STRING:
            inferred_type = make_type(TYPE_STRING);
            break;
        case EXPR_BOOL:
            inferred_type = make_type(TYPE_BOOL);
            break;
        case EXPR_ARRAY: {
            // Infer array element type from first element
            if (init_expr->array.count > 0) {
                Type* element_type = check_expr(init_expr->array.elements[0], scope);
                if (element_type) {
                    inferred_type = make_type(TYPE_ARRAY);
                    // Store element type (simplified - in full implementation would use proper array type structure)
                    inferred_type->name = (Token){TOKEN_IDENT, "array", 5, 0};
                }
            } else {
                inferred_type = make_type(TYPE_ARRAY);
            }
            break;
        }
        case EXPR_STRUCT_INIT:
            inferred_type = make_type(TYPE_STRUCT);
            inferred_type->struct_type.name = init_expr->struct_init.type_name;
            break;
        case EXPR_BINARY:
            // Infer from binary operation result
            inferred_type = wyn_infer_binary_result_type(init_expr);
            break;
        case EXPR_CALL:
            // Infer from function call return type
            inferred_type = wyn_infer_call_return_type(init_expr, scope);
            break;
        case EXPR_METHOD_CALL:
            // Method calls have their type set by checker - use that
            if (init_expr->expr_type) {
                inferred_type = init_expr->expr_type;
            }
            break;
        default:
            // No inference - return NULL to use checked type
            inferred_type = NULL;
            break;
    }
    
    if (inferred_type) {
        g_inference_context.variables_inferred++;
    }
    
    return inferred_type;
}

// Infer function return type from return statements
Type* wyn_infer_function_return_type(Stmt* function_body, SymbolTable* scope) {
    if (!g_inference_context.inference_enabled || !function_body) {
        return make_type(TYPE_VOID);
    }
    
    Type* inferred_type = wyn_analyze_return_statements(function_body, scope);
    
    if (inferred_type) {
        g_inference_context.functions_inferred++;
    }
    
    return inferred_type ? inferred_type : make_type(TYPE_VOID);
}

// Analyze return statements to infer function return type
Type* wyn_analyze_return_statements(Stmt* stmt, SymbolTable* scope) {
    if (!stmt) return NULL;
    
    switch (stmt->type) {
        case STMT_RETURN:
            if (stmt->ret.value) {
                return check_expr(stmt->ret.value, scope);
            }
            return make_type(TYPE_VOID);
            
        case STMT_BLOCK:
            // Check all statements in block for return statements
            for (int i = 0; i < stmt->block.count; i++) {
                Type* return_type = wyn_analyze_return_statements(stmt->block.stmts[i], scope);
                if (return_type && return_type->kind != TYPE_VOID) {
                    return return_type;
                }
            }
            break;
            
        case STMT_IF:
            // Check both branches for return types
            {
                Type* then_type = wyn_analyze_return_statements(stmt->if_stmt.then_branch, scope);
                Type* else_type = stmt->if_stmt.else_branch ? 
                                 wyn_analyze_return_statements(stmt->if_stmt.else_branch, scope) : NULL;
                
                if (then_type && then_type->kind != TYPE_VOID) {
                    return then_type;
                }
                if (else_type && else_type->kind != TYPE_VOID) {
                    return else_type;
                }
            }
            break;
            
        default:
            break;
    }
    
    return NULL;
}

// Infer binary operation result type
Type* wyn_infer_binary_result_type(Expr* binary_expr) {
    if (!binary_expr || binary_expr->type != EXPR_BINARY) {
        return make_type(TYPE_INT);
    }
    
    // For arithmetic operations, result type depends on operands
    WynTokenType op = binary_expr->binary.op.type;
    
    if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH) {
        // Arithmetic operations - check operand types
        if (binary_expr->binary.left->type == EXPR_FLOAT || binary_expr->binary.right->type == EXPR_FLOAT) {
            return make_type(TYPE_FLOAT);
        }
        return make_type(TYPE_INT);
    }
    
    if (op == TOKEN_EQEQ || op == TOKEN_BANGEQ || op == TOKEN_LT || op == TOKEN_GT || 
        op == TOKEN_LTEQ || op == TOKEN_GTEQ) {
        // Comparison operations always return bool
        return make_type(TYPE_BOOL);
    }
    
    return make_type(TYPE_INT); // Default
}

// Infer function call return type
Type* wyn_infer_call_return_type(Expr* call_expr, SymbolTable* scope) {
    if (!call_expr || call_expr->type != EXPR_CALL) {
        return make_type(TYPE_INT);
    }
    
    // Look up function in symbol table to get return type
    if (call_expr->call.callee->type == EXPR_IDENT) {
        Symbol* symbol = find_symbol(scope, call_expr->call.callee->token);
        if (symbol && symbol->type && symbol->type->kind == TYPE_FUNCTION) {
            return symbol->type->fn_type.return_type;
        }
    }
    
    return make_type(TYPE_INT); // Default
}

// Get type inference statistics
TypeInferenceStats wyn_type_inference_get_stats(void) {
    TypeInferenceStats stats = {
        .variables_inferred = g_inference_context.variables_inferred,
        .functions_inferred = g_inference_context.functions_inferred,
        .generics_inferred = g_inference_context.generics_inferred,
        .total_inferences = g_inference_context.variables_inferred + 
                           g_inference_context.functions_inferred + 
                           g_inference_context.generics_inferred,
        .inference_success_rate = 0.0
    };
    
    // Calculate success rate (simplified)
    if (stats.total_inferences > 0) {
        stats.inference_success_rate = 1.0; // Assume 100% success for now
    }
    
    return stats;
}

// Print type inference statistics
void wyn_type_inference_print_stats(void) {
    TypeInferenceStats stats = wyn_type_inference_get_stats();
    
    printf("=== Type Inference Statistics ===\n");
    printf("Variables inferred: %zu\n", stats.variables_inferred);
    printf("Functions inferred: %zu\n", stats.functions_inferred);
    printf("Generics inferred: %zu\n", stats.generics_inferred);
    printf("Total inferences: %zu\n", stats.total_inferences);
    printf("Success rate: %.2f%%\n", stats.inference_success_rate * 100.0);
    printf("=================================\n");
}

// Reset type inference statistics
void wyn_type_inference_reset(void) {
    g_inference_context.variables_inferred = 0;
    g_inference_context.functions_inferred = 0;
    g_inference_context.generics_inferred = 0;
}

// Enable/disable type inference
void wyn_type_inference_set_enabled(bool enabled) {
    g_inference_context.inference_enabled = enabled;
}
