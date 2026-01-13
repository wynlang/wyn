#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "optimize.h"
#include "ast.h"

OptLevel opt_level = OPT_NONE;

void init_optimizer(OptLevel level) {
    opt_level = level;
}

// Dead code elimination
bool is_dead_code(Stmt* stmt) {
    if (!stmt) return true;
    
    // Simple dead code detection - remove standalone literals
    if (stmt->type == STMT_EXPR) {
        if (stmt->expr && stmt->expr->type == EXPR_INT) {
            return true; // Remove standalone integer literals
        }
        if (stmt->expr && stmt->expr->type == EXPR_STRING) {
            return true; // Remove standalone string literals
        }
    }
    
    return false;
}

void eliminate_dead_code(Program* prog) {
    if (opt_level == OPT_NONE || !prog) return;
    
    // Remove dead statements from program
    int write_idx = 0;
    for (int i = 0; i < prog->count; i++) {
        if (!is_dead_code(prog->stmts[i])) {
            prog->stmts[write_idx++] = prog->stmts[i];
        }
    }
    prog->count = write_idx;
}

// Constant folding
Expr* fold_constants(Expr* expr) {
    if (opt_level == OPT_NONE || !expr) return expr;
    
    if (expr->type == EXPR_BINARY) {
        BinaryExpr* bin = (BinaryExpr*)expr;
        
        // Fold both operands first
        bin->left = fold_constants(bin->left);
        bin->right = fold_constants(bin->right);
        
        // Check if both operands are integer literals
        if (bin->left->type == EXPR_INT && bin->right->type == EXPR_INT) {
            // For simplicity, we'll just mark that folding would happen here
            // Full implementation would require creating new literal expressions
            printf("// Constant folding opportunity detected\n");
        }
    }
    
    return expr;
}

// Function inlining
bool should_inline_function(Stmt* func_stmt) {
    if (opt_level < OPT_O2 || !func_stmt || func_stmt->type != STMT_FN) return false;
    
    // Simple heuristic: inline functions with short names (likely small)
    if (func_stmt->fn.name.length <= 15) {
        return true;
    }
    
    return false;
}

void inline_small_functions(Program* prog) {
    if (opt_level < OPT_O2 || !prog) return;
    
    // Mark functions for inlining
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN && should_inline_function(prog->stmts[i])) {
            printf("// Function marked for inlining\n");
        }
    }
}