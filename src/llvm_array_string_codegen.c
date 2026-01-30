#include "llvm_array_string_codegen.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safe_memory.h"
#include "error.h"
#include "runtime_functions.h"
#include "llvm_expression_codegen.h"

// Generate code for array literals
LLVMValueRef codegen_array_literal(ArrayExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Determine element type (assume int for now, could be enhanced with type inference)
    LLVMTypeRef element_type = ctx->int_type;
    
    // Create array type
    LLVMTypeRef array_type = LLVMArrayType(element_type, expr->count);
    
    // Allocate array on stack
    LLVMValueRef array_alloca = LLVMBuildAlloca(ctx->builder, array_type, "array_literal");
    
    // Initialize array elements
    for (int i = 0; i < expr->count; i++) {
        if (expr->elements[i]) {
            // Generate code for element
            LLVMValueRef element_value = codegen_expression(expr->elements[i], ctx);
            if (element_value) {
                // Get pointer to array element
                LLVMValueRef indices[] = {
                    LLVMConstInt(ctx->int_type, 0, 0),  // Array base
                    LLVMConstInt(ctx->int_type, i, 0)   // Element index
                };
                LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, array_type, array_alloca, 
                                                        indices, 2, "element_ptr");
                
                // Store element value
                LLVMBuildStore(ctx->builder, element_value, element_ptr);
            }
        }
    }
    
    return array_alloca;
}

// Generate code for array indexing with bounds checking
LLVMValueRef codegen_array_indexing(IndexExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Generate code for array expression (returns the alloca pointer for variables)
    LLVMValueRef array_var = codegen_expression(expr->array, ctx);
    if (!array_var) {
        return NULL;
    }
    
    // For identifiers, we already have the loaded pointer value
    // Don't load again - array_var is already the array pointer
    LLVMValueRef array_ptr = array_var;
    
    // Generate code for index expression
    LLVMValueRef index = codegen_expression(expr->index, ctx);
    if (!index) {
        return NULL;
    }
    
    // For LLVM 21 with opaque pointers
    LLVMTypeRef int_type = ctx->int_type;
    LLVMTypeRef array_type = LLVMArrayType(int_type, 0);  // [0 x i32] for flexible arrays
    
    // Create GEP to access array element
    LLVMValueRef indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0),  // Array base
        index                               // Element index
    };
    LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, array_type, 
                                            array_ptr, indices, 2, "array_element_ptr");
    
    // Load the element value
    return LLVMBuildLoad2(ctx->builder, int_type, element_ptr, "array_element");
}

// Generate bounds checking code
LLVMValueRef codegen_array_bounds_check(LLVMValueRef array, LLVMValueRef index, LLVMCodegenContext* ctx) {
    if (!array || !index || !ctx) {
        return NULL;
    }
    
    // Get current function and create basic blocks
    LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef bounds_ok_block = LLVMAppendBasicBlock(function, "bounds_ok");
    LLVMBasicBlockRef bounds_fail_block = LLVMAppendBasicBlock(function, "bounds_fail");
    
    // Get array length (for now, assume it's stored or can be computed)
    LLVMValueRef array_length = get_array_length(array, ctx);
    
    // Check if index >= 0 and index < length
    LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, 0);
    LLVMValueRef index_ge_zero = LLVMBuildICmp(ctx->builder, LLVMIntSGE, index, zero, "index_ge_zero");
    LLVMValueRef index_lt_length = LLVMBuildICmp(ctx->builder, LLVMIntSLT, index, array_length, "index_lt_length");
    LLVMValueRef bounds_ok = LLVMBuildAnd(ctx->builder, index_ge_zero, index_lt_length, "bounds_ok");
    
    // Branch based on bounds check
    LLVMBuildCondBr(ctx->builder, bounds_ok, bounds_ok_block, bounds_fail_block);
    
    // Generate bounds failure block
    LLVMPositionBuilderAtEnd(ctx->builder, bounds_fail_block);
    codegen_bounds_check_failure(ctx);
    LLVMBuildUnreachable(ctx->builder);
    
    // Continue with bounds ok block
    LLVMPositionBuilderAtEnd(ctx->builder, bounds_ok_block);
    
    return bounds_ok;
}

// Generate bounds check failure code
void codegen_bounds_check_failure(LLVMCodegenContext* ctx) {
    if (!ctx) {
        return;
    }
    
    // Call runtime error function for bounds check failure
    if (ctx->runtime_functions.printf_fn) {
        LLVMValueRef error_msg = LLVMBuildGlobalStringPtr(ctx->builder, 
            "Runtime Error: Array index out of bounds\\n", "bounds_error_msg");
        LLVMValueRef args[] = { error_msg };
        LLVMBuildCall2(ctx->builder, LLVMGetElementType(LLVMTypeOf(ctx->runtime_functions.printf_fn)),
                      ctx->runtime_functions.printf_fn, args, 1, "");
    }
    
    // Exit with error code
    LLVMTypeRef exit_fn_type = LLVMFunctionType(ctx->void_type, &ctx->int_type, 1, 0);
    LLVMValueRef exit_fn = LLVMGetNamedFunction(ctx->module, "exit");
    if (!exit_fn) {
        exit_fn = LLVMAddFunction(ctx->module, "exit", exit_fn_type);
    }
    
    LLVMValueRef exit_code = LLVMConstInt(ctx->int_type, 1, 0);
    LLVMValueRef args[] = { exit_code };
    LLVMBuildCall2(ctx->builder, exit_fn_type, exit_fn, args, 1, "");
}

// Get array length (simplified implementation)
LLVMValueRef get_array_length(LLVMValueRef array, LLVMCodegenContext* ctx) {
    if (!array || !ctx) {
        return NULL;
    }
    
    // For static arrays, get the length from the type
    LLVMTypeRef array_type = LLVMTypeOf(array);
    if (LLVMGetTypeKind(array_type) == LLVMPointerTypeKind) {
        array_type = LLVMGetElementType(array_type);
    }
    
    if (LLVMGetTypeKind(array_type) == LLVMArrayTypeKind) {
        unsigned length = LLVMGetArrayLength(array_type);
        return LLVMConstInt(ctx->int_type, length, 0);
    }
    
    // For dynamic arrays, this would need to be stored with the array
    // For now, return a default length
    return LLVMConstInt(ctx->int_type, 10, 0);
}

// Get pointer to array data
LLVMValueRef get_array_data_ptr(LLVMValueRef array, LLVMCodegenContext* ctx) {
    if (!array || !ctx) {
        return NULL;
    }
    
    // For static arrays, return the array pointer directly
    return array;
}

// Generate string concatenation
LLVMValueRef codegen_string_concatenation(LLVMValueRef left, LLVMValueRef right, LLVMCodegenContext* ctx) {
    if (!left || !right || !ctx) {
        return NULL;
    }
    
    // For now, create a placeholder implementation
    // In a full implementation, this would:
    // 1. Get lengths of both strings
    // 2. Allocate new string with combined length
    // 3. Copy both strings to new buffer
    // 4. Return new string
    
    report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "String concatenation not yet fully implemented");
    return NULL;
}

// Generate string length operation
LLVMValueRef codegen_string_length(LLVMValueRef string_val, LLVMCodegenContext* ctx) {
    if (!string_val || !ctx) {
        return NULL;
    }
    
    // Call strlen function
    LLVMTypeRef strlen_type = LLVMFunctionType(ctx->int_type, &ctx->string_type, 1, 0);
    LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
    if (!strlen_fn) {
        strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
    }
    
    LLVMValueRef args[] = { string_val };
    return LLVMBuildCall2(ctx->builder, strlen_type, strlen_fn, args, 1, "string_length");
}

// Generate dynamic memory allocation
LLVMValueRef codegen_dynamic_allocation(LLVMTypeRef element_type, LLVMValueRef count, LLVMCodegenContext* ctx) {
    if (!element_type || !count || !ctx) {
        return NULL;
    }
    
    // Calculate total size needed
    LLVMValueRef element_size = LLVMSizeOf(element_type);
    LLVMValueRef total_size = LLVMBuildMul(ctx->builder, element_size, count, "total_size");
    
    // Call malloc
    if (ctx->runtime_functions.malloc_fn) {
        LLVMValueRef args[] = { total_size };
        LLVMValueRef ptr = LLVMBuildCall2(ctx->builder, 
            LLVMGetElementType(LLVMTypeOf(ctx->runtime_functions.malloc_fn)),
            ctx->runtime_functions.malloc_fn, args, 1, "dynamic_array");
        
        // Cast to appropriate pointer type
        LLVMTypeRef ptr_type = LLVMPointerType(element_type, 0);
        return LLVMBuildBitCast(ctx->builder, ptr, ptr_type, "typed_array_ptr");
    }
    
    return NULL;
}

// String operations dispatcher
LLVMValueRef codegen_string_operations(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Handle different string operations based on expression type
    switch (expr->type) {
        case EXPR_STRING:
            return codegen_string_literal(expr, ctx);
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported string operation");
            return NULL;
    }
}

#endif // WITH_LLVM
