#ifndef LLVM_STATEMENT_CODEGEN_H
#define LLVM_STATEMENT_CODEGEN_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include "ast.h"
#include "llvm_context.h"

// Statement code generation functions
void codegen_statement(Stmt* stmt, LLVMCodegenContext* ctx);

// Control flow statements
void codegen_if_statement(IfStmt* stmt, LLVMCodegenContext* ctx);
void codegen_while_statement(WhileStmt* stmt, LLVMCodegenContext* ctx);
void codegen_for_statement(ForStmt* stmt, LLVMCodegenContext* ctx);

// Variable declarations with scoping
void codegen_var_declaration(VarStmt* stmt, LLVMCodegenContext* ctx);
LLVMValueRef create_local_variable(const char* name, LLVMTypeRef type, LLVMCodegenContext* ctx);

// Return statements
void codegen_return_statement(ReturnStmt* stmt, LLVMCodegenContext* ctx);

// Block statements
void codegen_block_statement(BlockStmt* stmt, LLVMCodegenContext* ctx);

// Expression statements
void codegen_expression_statement(Expr* expr, LLVMCodegenContext* ctx);

// Scoping utilities
void enter_scope(LLVMCodegenContext* ctx);
void exit_scope(LLVMCodegenContext* ctx);

#else
// Fallback when LLVM is not available
static inline void codegen_statement(void* stmt, void* ctx) { (void)stmt; (void)ctx; }
#endif

#endif // LLVM_STATEMENT_CODEGEN_H
