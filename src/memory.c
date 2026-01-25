#include "memory.h"
#include "security.h"
#include <stdlib.h>

// Forward declaration for pattern cleanup
void wyn_free_pattern(Pattern* pattern);

// Free expression recursively
void free_expr(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_BINARY:
            free_binary_expr(&expr->binary);
            break;
        case EXPR_CALL:
            free_call_expr(&expr->call);
            break;
        case EXPR_METHOD_CALL:
            free_method_call_expr(&expr->method_call);
            break;
        case EXPR_ARRAY:
            free_array_expr(&expr->array);
            break;
        case EXPR_INDEX:
            free_expr(expr->index.array);
            free_expr(expr->index.index);
            break;
        case EXPR_ASSIGN:
            free_expr(expr->assign.value);
            break;
        case EXPR_STRUCT_INIT:
            free_struct_init_expr(&expr->struct_init);
            break;
        case EXPR_FIELD_ACCESS:
            free_expr(expr->field_access.object);
            break;
        case EXPR_UNARY:
            free_expr(expr->unary.operand);
            break;
        case EXPR_AWAIT:
            free_expr(expr->await.expr);
            break;
        case EXPR_MATCH:
            free_match_expr(&expr->match);
            break;
        case EXPR_BLOCK:
            for (int i = 0; i < expr->block.stmt_count; i++) {
                free_stmt(expr->block.stmts[i]);
            }
            free(expr->block.stmts);
            if (expr->block.result) {
                free_expr(expr->block.result);
            }
            break;
        case EXPR_TERNARY:
            free_expr(expr->ternary.condition);
            free_expr(expr->ternary.then_expr);
            free_expr(expr->ternary.else_expr);
            break;
        case EXPR_SOME:
        case EXPR_OK:
        case EXPR_ERR:
            free_expr(expr->option.value);
            break;
        case EXPR_PIPELINE:
            for (int i = 0; i < expr->pipeline.stage_count; i++) {
                free_expr(expr->pipeline.stages[i]);
            }
            safe_free(expr->pipeline.stages);
            break;
        case EXPR_IF_EXPR:
            free_expr(expr->if_expr.condition);
            free_expr(expr->if_expr.then_expr);
            free_expr(expr->if_expr.else_expr);
            break;
        case EXPR_STRING_INTERP:
            free_string_interp_expr(&expr->string_interp);
            break;
        case EXPR_RANGE:
            free_expr(expr->range.start);
            free_expr(expr->range.end);
            break;
        case EXPR_LAMBDA:
            free_lambda_expr(&expr->lambda);
            break;
        case EXPR_MAP:
            free_map_expr(&expr->map);
            break;
        case EXPR_TUPLE:
            free_tuple_expr(&expr->tuple);
            break;
        case EXPR_TUPLE_INDEX:
            free_expr(expr->tuple_index.tuple);
            break;
        case EXPR_INDEX_ASSIGN:
            free_expr(expr->index_assign.object);
            free_expr(expr->index_assign.index);
            free_expr(expr->index_assign.value);
            break;
        case EXPR_FIELD_ASSIGN:
            free_expr(expr->field_assign.object);
            free_expr(expr->field_assign.value);
            break;
        case EXPR_OPTIONAL_TYPE:  // T2.5.1: Optional Type Implementation
            free_expr(expr->optional_type.inner_type);
            break;
        case EXPR_UNION_TYPE:     // T2.5.2: Union Type Support
            for (int i = 0; i < expr->union_type.type_count; i++) {
                free_expr(expr->union_type.types[i]);
            }
            safe_free(expr->union_type.types);
            break;
        case EXPR_HASHMAP_LITERAL:  // v1.3.0: HashMap literal
        case EXPR_HASHSET_LITERAL:  // v1.3.0: HashSet literal
            free_array_expr(&expr->array);
            break;
        case EXPR_RESULT_TYPE:
            // No additional cleanup needed
            break;
        case EXPR_TRY:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STRING:
        case EXPR_CHAR:
        case EXPR_IDENT:
        case EXPR_BOOL:
        case EXPR_NONE:
        case EXPR_DESTRUCTURE:
        case EXPR_SPREAD:
        case EXPR_PATTERN:
            // No additional cleanup needed for these types
            break;
    }
    
    safe_free(expr);
}

// Free statement recursively
void free_stmt(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_EXPR:
            free_expr(stmt->expr);
            break;
        case STMT_VAR:
            free_var_stmt(&stmt->var);
            break;
        case STMT_CONST:
            free_var_stmt(&stmt->const_stmt);  // Reuse VarStmt structure
            break;
        case STMT_RETURN:
            free_expr(stmt->ret.value);
            break;
        case STMT_BLOCK:
            free_block_stmt(&stmt->block);
            break;
        case STMT_UNSAFE:
            free_block_stmt(&stmt->block);
            break;
        case STMT_FN:
            free_fn_stmt(&stmt->fn);
            break;
        case STMT_EXTERN:
            if (stmt->extern_fn.params) free(stmt->extern_fn.params);
            if (stmt->extern_fn.param_types) {
                for (int i = 0; i < stmt->extern_fn.param_count; i++) {
                    if (stmt->extern_fn.param_types[i]) free_expr(stmt->extern_fn.param_types[i]);
                }
                free(stmt->extern_fn.param_types);
            }
            if (stmt->extern_fn.return_type) free_expr(stmt->extern_fn.return_type);
            break;
        case STMT_MACRO:
            if (stmt->macro.params) free(stmt->macro.params);
            break;
        case STMT_STRUCT:
            free_struct_stmt(&stmt->struct_decl);
            break;
        case STMT_IMPL:
            free_impl_stmt(&stmt->impl);
            break;
        case STMT_TRAIT:
            // T3.2.1: Free trait statement
            if (stmt->trait_decl.methods) {
                for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                    free_fn_stmt(stmt->trait_decl.methods[i]);
                    free(stmt->trait_decl.methods[i]);
                }
                free(stmt->trait_decl.methods);
            }
            if (stmt->trait_decl.method_has_default) {
                free(stmt->trait_decl.method_has_default);
            }
            if (stmt->trait_decl.type_params) {
                free(stmt->trait_decl.type_params);
            }
            break;
        case STMT_IF:
            free_if_stmt(&stmt->if_stmt);
            break;
        case STMT_WHILE:
            free_while_stmt(&stmt->while_stmt);
            break;
        case STMT_FOR:
            free_for_stmt(&stmt->for_stmt);
            break;
        case STMT_ENUM:
            free_enum_stmt(&stmt->enum_decl);
            break;
        case STMT_TYPE_ALIAS:
            // No additional cleanup needed
            break;
        case STMT_IMPORT:
            free_import_stmt(&stmt->import);
            break;
        case STMT_EXPORT:
            free_stmt(stmt->export.stmt);
            break;
        case STMT_TRY:
            free_try_stmt(&stmt->try_stmt);
            break;
        case STMT_THROW:
            free_expr(stmt->throw_stmt.value);
            break;
        case STMT_CATCH:
            free_stmt(stmt->catch_stmt.body);
            break;
        case STMT_SPAWN:
            free_expr(stmt->expr);
            break;
        case STMT_TEST:  // T1.6.2: Testing Framework Agent addition
            free_stmt(stmt->test_stmt.body);
            break;
        case STMT_MATCH:  // T3.3.1: Enhanced match with destructuring patterns
            free_expr(stmt->match_stmt.value);
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                // Free the destructuring pattern
                if (stmt->match_stmt.cases[i].pattern) {
                    wyn_free_pattern(stmt->match_stmt.cases[i].pattern);
                }
                if (stmt->match_stmt.cases[i].guard) {
                    free_expr(stmt->match_stmt.cases[i].guard);
                }
                if (stmt->match_stmt.cases[i].body) {
                    free_stmt(stmt->match_stmt.cases[i].body);
                }
            }
            safe_free(stmt->match_stmt.cases);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_ASYNC_FN:
        case STMT_MODULE:
            // No additional cleanup needed
            break;
    }
    
    safe_free(stmt);
}

// Free program
void free_program(Program* prog) {
    if (!prog) return;
    
    for (int i = 0; i < prog->count; i++) {
        free_stmt(prog->stmts[i]);
    }
    safe_free(prog->stmts);
    safe_free(prog);
}

// Expression cleanup implementations
void free_binary_expr(BinaryExpr* expr) {
    free_expr(expr->left);
    free_expr(expr->right);
}

void free_call_expr(CallExpr* expr) {
    free_expr(expr->callee);
    for (int i = 0; i < expr->arg_count; i++) {
        free_expr(expr->args[i]);
    }
    safe_free(expr->args);
}

void free_method_call_expr(MethodCallExpr* expr) {
    free_expr(expr->object);
    for (int i = 0; i < expr->arg_count; i++) {
        free_expr(expr->args[i]);
    }
    safe_free(expr->args);
}

void free_array_expr(ArrayExpr* expr) {
    for (int i = 0; i < expr->count; i++) {
        free_expr(expr->elements[i]);
    }
    safe_free(expr->elements);
}

void free_struct_init_expr(StructInitExpr* expr) {
    for (int i = 0; i < expr->field_count; i++) {
        free_expr(expr->field_values[i]);
    }
    safe_free(expr->field_names);
    safe_free(expr->field_values);
}

void free_match_expr(MatchExpr* expr) {
    free_expr(expr->value);
    for (int i = 0; i < expr->arm_count; i++) {
        free_expr(expr->arms[i].result);
    }
    safe_free(expr->arms);
}

void free_string_interp_expr(StringInterpExpr* expr) {
    for (int i = 0; i < expr->count; i++) {
        safe_free(expr->parts[i]);
        free_expr(expr->expressions[i]);
    }
    safe_free(expr->parts);
    safe_free(expr->expressions);
}

void free_map_expr(MapExpr* expr) {
    for (int i = 0; i < expr->count; i++) {
        free_expr(expr->keys[i]);
        free_expr(expr->values[i]);
    }
    safe_free(expr->keys);
    safe_free(expr->values);
}

void free_tuple_expr(TupleExpr* expr) {
    for (int i = 0; i < expr->count; i++) {
        free_expr(expr->elements[i]);
    }
    safe_free(expr->elements);
}

// T3.4.1: Free lambda expression
void free_lambda_expr(LambdaExpr* expr) {
    if (expr->params) {
        safe_free(expr->params);
    }
    free_expr(expr->body);
    if (expr->captured_vars) {
        safe_free(expr->captured_vars);
    }
    if (expr->capture_by_move) {
        safe_free(expr->capture_by_move);
    }
}

// Statement cleanup implementations
void free_var_stmt(VarStmt* stmt) {
    free_expr(stmt->type);
    free_expr(stmt->init);
    // T3.3.2: Free pattern if used
    if (stmt->uses_pattern && stmt->pattern) {
        wyn_free_pattern(stmt->pattern);
    }
}

void free_fn_stmt(FnStmt* stmt) {
    safe_free(stmt->params);
    for (int i = 0; i < stmt->param_count; i++) {
        free_expr(stmt->param_types[i]);
        // T1.5.2: Free default parameter expressions
        if (stmt->param_defaults && stmt->param_defaults[i]) {
            free_expr(stmt->param_defaults[i]);
        }
    }
    safe_free(stmt->param_types);
    safe_free(stmt->param_mutable);
    safe_free(stmt->param_defaults);  // T1.5.2: Free defaults array
    safe_free(stmt->type_params);
    free_expr(stmt->return_type);
    free_stmt(stmt->body);
}

void free_struct_stmt(StructStmt* stmt) {
    safe_free(stmt->fields);
    for (int i = 0; i < stmt->field_count; i++) {
        free_expr(stmt->field_types[i]);
    }
    safe_free(stmt->field_types);
    safe_free(stmt->field_arc_managed); // T2.5.3: ARC integration cleanup
}

void free_impl_stmt(ImplStmt* stmt) {
    for (int i = 0; i < stmt->method_count; i++) {
        free_fn_stmt(stmt->methods[i]);
        safe_free(stmt->methods[i]);
    }
    safe_free(stmt->methods);
}

void free_block_stmt(BlockStmt* stmt) {
    for (int i = 0; i < stmt->count; i++) {
        free_stmt(stmt->stmts[i]);
    }
    safe_free(stmt->stmts);
}

void free_if_stmt(IfStmt* stmt) {
    free_expr(stmt->condition);
    free_stmt(stmt->then_branch);
    free_stmt(stmt->else_branch);
}

void free_while_stmt(WhileStmt* stmt) {
    free_expr(stmt->condition);
    free_stmt(stmt->body);
}

void free_for_stmt(ForStmt* stmt) {
    free_stmt(stmt->init);
    free_expr(stmt->condition);
    free_expr(stmt->increment);
    free_stmt(stmt->body);
    free_expr(stmt->array_expr);
}

void free_enum_stmt(EnumStmt* stmt) {
    safe_free(stmt->variants);
}

void free_import_stmt(ImportStmt* stmt) {
    safe_free(stmt->items);
}

void free_try_stmt(TryStmt* stmt) {
    free_stmt(stmt->try_block);
    free_stmt(stmt->try_block);
    for (int i = 0; i < stmt->catch_count; i++) {
        free_stmt(stmt->catch_blocks[i]);
    }
    if (stmt->catch_blocks) free(stmt->catch_blocks);
    if (stmt->exception_types) free(stmt->exception_types);
    if (stmt->exception_vars) free(stmt->exception_vars);
    if (stmt->finally_block) free_stmt(stmt->finally_block);
}

// T1.1.5: RAII Pattern Implementation

// Global cleanup stack for scoped resource management
static AutoCleanup* cleanup_stack[256];
static int cleanup_count = 0;

void auto_free_cleanup(void* ptr) {
    void** p = (void**)ptr;
    if (p && *p) {
        safe_free(*p);
        *p = NULL;
    }
}

void auto_expr_cleanup(Expr** expr) {
    if (expr && *expr) {
        free_expr(*expr);
        *expr = NULL;
    }
}

void auto_stmt_cleanup(Stmt** stmt) {
    if (stmt && *stmt) {
        free_stmt(*stmt);
        *stmt = NULL;
    }
}

void auto_program_cleanup(Program** prog) {
    if (prog && *prog) {
        free_program(*prog);
        *prog = NULL;
    }
}

AutoCleanup* create_auto_cleanup(void* resource, void (*cleanup_fn)(void*)) {
    if (!resource || !cleanup_fn) return NULL;
    
    AutoCleanup* cleanup = safe_malloc(sizeof(AutoCleanup));
    if (!cleanup) return NULL;
    
    cleanup->resource = resource;
    cleanup->cleanup_fn = cleanup_fn;
    
    return cleanup;
}

void register_cleanup(AutoCleanup* cleanup) {
    if (!cleanup || cleanup_count >= 256) return;
    
    cleanup_stack[cleanup_count++] = cleanup;
}

void cleanup_scope(void) {
    // Clean up in reverse order (LIFO)
    for (int i = cleanup_count - 1; i >= 0; i--) {
        AutoCleanup* cleanup = cleanup_stack[i];
        if (cleanup) {
            if (cleanup->cleanup_fn && cleanup->resource) {
                cleanup->cleanup_fn(cleanup->resource);
            }
            safe_free(cleanup);
            cleanup_stack[i] = NULL;
        }
    }
    cleanup_count = 0;
}
