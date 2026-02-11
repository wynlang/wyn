#ifndef RUNTIME_FUNCTIONS_H
#define RUNTIME_FUNCTIONS_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include <stdbool.h>
#include "llvm_context.h"

// Runtime function categories
typedef enum {
    RUNTIME_ARC,
    RUNTIME_STDLIB,
    RUNTIME_PLATFORM,
    RUNTIME_MEMORY,
    RUNTIME_STRING
} RuntimeFunctionCategory;

// ARC runtime function declarations
bool declare_arc_runtime_functions(LLVMCodegenContext* ctx);
LLVMValueRef get_arc_retain_function(LLVMCodegenContext* ctx);
LLVMValueRef get_arc_release_function(LLVMCodegenContext* ctx);
LLVMValueRef get_arc_alloc_function(LLVMCodegenContext* ctx);
LLVMValueRef get_arc_dealloc_function(LLVMCodegenContext* ctx);

// Standard library function declarations
bool declare_stdlib_functions(LLVMCodegenContext* ctx);
LLVMValueRef get_printf_function(LLVMCodegenContext* ctx);
LLVMValueRef get_malloc_function(LLVMCodegenContext* ctx);
LLVMValueRef get_free_function(LLVMCodegenContext* ctx);
LLVMValueRef get_memcpy_function(LLVMCodegenContext* ctx);
LLVMValueRef get_memset_function(LLVMCodegenContext* ctx);

// Platform-specific function declarations
bool declare_platform_functions(LLVMCodegenContext* ctx);
LLVMValueRef get_platform_alloc_function(LLVMCodegenContext* ctx);
LLVMValueRef get_platform_free_function(LLVMCodegenContext* ctx);

// String runtime function declarations
bool declare_string_functions(LLVMCodegenContext* ctx);
LLVMValueRef get_string_concat_function(LLVMCodegenContext* ctx);
LLVMValueRef get_string_length_function(LLVMCodegenContext* ctx);
LLVMValueRef get_string_compare_function(LLVMCodegenContext* ctx);

// Runtime function management
bool declare_all_runtime_functions(LLVMCodegenContext* ctx);
LLVMValueRef get_runtime_function(LLVMCodegenContext* ctx, const char* name);
bool is_runtime_function_declared(LLVMCodegenContext* ctx, const char* name);

// Function signature helpers
LLVMTypeRef create_runtime_function_type(LLVMTypeRef return_type, LLVMTypeRef* param_types, unsigned param_count, bool is_vararg);
LLVMValueRef declare_external_function(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef function_type);

#else
// Fallback when LLVM is not available
static inline bool declare_all_runtime_functions(void* ctx) { return false; }
static inline void* get_runtime_function(void* ctx, const char* name) { return NULL; }
#endif

#endif // RUNTIME_FUNCTIONS_H
