#ifndef MEMORY_H
#define MEMORY_H

#include "ast.h"

// Memory cleanup functions for AST nodes
void free_expr(Expr* expr);
void free_stmt(Stmt* stmt);
void free_program(Program* prog);

// Memory cleanup for specific expression types
void free_binary_expr(BinaryExpr* expr);
void free_call_expr(CallExpr* expr);
void free_method_call_expr(MethodCallExpr* expr);
void free_array_expr(ArrayExpr* expr);
void free_struct_init_expr(StructInitExpr* expr);
void free_match_expr(MatchExpr* expr);
void free_string_interp_expr(StringInterpExpr* expr);
void free_lambda_expr(LambdaExpr* expr);
void free_map_expr(MapExpr* expr);
void free_tuple_expr(TupleExpr* expr);

// Memory cleanup for specific statement types
void free_var_stmt(VarStmt* stmt);
void free_fn_stmt(FnStmt* stmt);
void free_struct_stmt(StructStmt* stmt);
void free_impl_stmt(ImplStmt* stmt);
void free_block_stmt(BlockStmt* stmt);
void free_if_stmt(IfStmt* stmt);
void free_while_stmt(WhileStmt* stmt);
void free_for_stmt(ForStmt* stmt);
void free_enum_stmt(EnumStmt* stmt);
void free_import_stmt(ImportStmt* stmt);
void free_try_stmt(TryStmt* stmt);

// T1.1.5: RAII Pattern Implementation
typedef struct {
    void* resource;
    void (*cleanup_fn)(void*);
} AutoCleanup;

// RAII-style automatic cleanup macros
#define AUTO_CLEANUP(type, var, cleanup_fn) \
    type var __attribute__((cleanup(cleanup_fn)))

#define AUTO_FREE(var) \
    void* var __attribute__((cleanup(auto_free_cleanup)))

// Cleanup functions for RAII pattern
void auto_free_cleanup(void* ptr);
void auto_expr_cleanup(Expr** expr);
void auto_stmt_cleanup(Stmt** stmt);
void auto_program_cleanup(Program** prog);

// Scoped resource management
AutoCleanup* create_auto_cleanup(void* resource, void (*cleanup_fn)(void*));
void register_cleanup(AutoCleanup* cleanup);
void cleanup_scope(void);

#endif // MEMORY_H
