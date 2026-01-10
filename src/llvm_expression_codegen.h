#ifndef LLVM_EXPRESSION_CODEGEN_H
#define LLVM_EXPRESSION_CODEGEN_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include "ast.h"
#include "llvm_context.h"

// Expression code generation functions
LLVMValueRef codegen_expression(Expr* expr, LLVMCodegenContext* ctx);

// Basic literal expressions
LLVMValueRef codegen_int_literal(Expr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_float_literal(Expr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_string_literal(Expr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_bool_literal(Expr* expr, LLVMCodegenContext* ctx);

// Binary expressions
LLVMValueRef codegen_binary_expr(BinaryExpr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_arithmetic_op(LLVMValueRef left, LLVMValueRef right, TokenType op, LLVMCodegenContext* ctx);
LLVMValueRef codegen_comparison_op(LLVMValueRef left, LLVMValueRef right, TokenType op, LLVMCodegenContext* ctx);
LLVMValueRef codegen_logical_op(LLVMValueRef left, LLVMValueRef right, TokenType op, LLVMCodegenContext* ctx);

// Unary expressions
LLVMValueRef codegen_unary_expr(UnaryExpr* expr, LLVMCodegenContext* ctx);

// Variable operations
LLVMValueRef codegen_variable_ref(Expr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_assignment(AssignExpr* expr, LLVMCodegenContext* ctx);

// Function calls
LLVMValueRef codegen_function_call(CallExpr* expr, LLVMCodegenContext* ctx);

// Array operations
LLVMValueRef codegen_array_access(IndexExpr* expr, LLVMCodegenContext* ctx);

// Type conversion utilities
LLVMValueRef convert_to_type(LLVMValueRef value, LLVMTypeRef target_type, LLVMCodegenContext* ctx);
bool is_numeric_type(LLVMTypeRef type);
bool is_integer_type(LLVMTypeRef type);
bool is_float_type(LLVMTypeRef type);

#else
// Fallback when LLVM is not available
static inline void* codegen_expression(void* expr, void* ctx) { return NULL; }
#endif

#endif // LLVM_EXPRESSION_CODEGEN_H
