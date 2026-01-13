#ifndef WYN_OPTIMIZE_H
#define WYN_OPTIMIZE_H

#include <stdbool.h>
#include "ast.h"

// Optimization levels
typedef enum {
    OPT_NONE = 0,
    OPT_O1 = 1,
    OPT_O2 = 2
} OptLevel;

// Global optimization settings
extern OptLevel opt_level;

// Dead code elimination
bool is_dead_code(Stmt* stmt);
void eliminate_dead_code(Program* prog);

// Constant folding
Expr* fold_constants(Expr* expr);

// Function inlining
bool should_inline_function(Stmt* func_stmt);
void inline_small_functions(Program* prog);

// Optimization initialization
void init_optimizer(OptLevel level);

#endif