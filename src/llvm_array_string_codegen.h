#ifndef LLVM_ARRAY_STRING_CODEGEN_H
#define LLVM_ARRAY_STRING_CODEGEN_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include "ast.h"
#include "llvm_context.h"

// Array operations
LLVMValueRef codegen_array_literal(ArrayExpr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_array_indexing(IndexExpr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_array_bounds_check(LLVMValueRef array, LLVMValueRef index, LLVMCodegenContext* ctx);

// String operations
LLVMValueRef codegen_string_operations(Expr* expr, LLVMCodegenContext* ctx);
LLVMValueRef codegen_string_concatenation(LLVMValueRef left, LLVMValueRef right, LLVMCodegenContext* ctx);
LLVMValueRef codegen_string_length(LLVMValueRef string_val, LLVMCodegenContext* ctx);

// Memory allocation for dynamic data
LLVMValueRef codegen_dynamic_allocation(LLVMTypeRef element_type, LLVMValueRef count, LLVMCodegenContext* ctx);
void codegen_bounds_check_failure(LLVMCodegenContext* ctx);

// Helper functions
LLVMValueRef get_array_length(LLVMValueRef array, LLVMCodegenContext* ctx);
LLVMValueRef get_array_data_ptr(LLVMValueRef array, LLVMCodegenContext* ctx);

#else
// Fallback when LLVM is not available
static inline void* codegen_array_literal(void* expr, void* ctx) { return NULL; }
static inline void* codegen_array_indexing(void* expr, void* ctx) { return NULL; }
#endif

#endif // LLVM_ARRAY_STRING_CODEGEN_H
