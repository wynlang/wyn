#ifndef LLVM_FUNCTION_CODEGEN_H
#define LLVM_FUNCTION_CODEGEN_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include "ast.h"
#include "llvm_context.h"

// Function definition code generation
LLVMValueRef codegen_function_definition(FnStmt* fn_stmt, LLVMCodegenContext* ctx);

// Function declaration (forward declaration support)
LLVMValueRef codegen_function_declaration(FnStmt* fn_stmt, LLVMCodegenContext* ctx);

// Parameter handling
LLVMTypeRef* codegen_function_parameter_types(FnStmt* fn_stmt, LLVMCodegenContext* ctx);
void codegen_function_parameters(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx);

// Function prologue and epilogue
void codegen_function_prologue(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx);
void codegen_function_epilogue(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx);

// Helper functions
LLVMTypeRef get_llvm_type_from_wyn_type(Expr* type_expr, LLVMCodegenContext* ctx);
char* get_function_name_from_token(Token* name_token);

#else
// Fallback when LLVM is not available
typedef struct FnStmt FnStmt;
typedef struct LLVMCodegenContext LLVMCodegenContext;
typedef void* LLVMValueRef;
typedef void* LLVMTypeRef;

static inline LLVMValueRef codegen_function_definition(FnStmt* fn_stmt, LLVMCodegenContext* ctx) { return NULL; }
static inline LLVMValueRef codegen_function_declaration(FnStmt* fn_stmt, LLVMCodegenContext* ctx) { return NULL; }
#endif

#endif // LLVM_FUNCTION_CODEGEN_H
