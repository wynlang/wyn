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
    
    // Determine element type (assume int for now)
    LLVMTypeRef element_type = ctx->int_type;
    
    // Allocate array with length prefix on heap
    // Layout: [length: i64][element0][element1]...
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    int element_count = expr->count;
    
    // Calculate total size: 8 bytes (length) + element_count * element_size
    LLVMValueRef element_size = LLVMSizeOf(element_type);
    LLVMValueRef count_val = LLVMConstInt(i64_type, element_count, false);
    LLVMValueRef data_size = LLVMBuildMul(ctx->builder, count_val, element_size, "data_size");
    LLVMValueRef length_size = LLVMConstInt(i64_type, 8, false);
    LLVMValueRef total_size = LLVMBuildAdd(ctx->builder, length_size, data_size, "total_size");
    
    // Allocate memory
    LLVMValueRef malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!malloc_fn) {
        LLVMTypeRef malloc_type = LLVMFunctionType(
            LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
            (LLVMTypeRef[]){ i64_type }, 1, false);
        malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);
    }
    
    LLVMValueRef array_ptr = LLVMBuildCall2(ctx->builder, 
        LLVMGlobalGetValueType(malloc_fn), malloc_fn, &total_size, 1, "array_alloc");
    
    // Store length at offset 0
    LLVMValueRef length_ptr = LLVMBuildBitCast(ctx->builder, array_ptr,
        LLVMPointerType(i64_type, 0), "length_ptr");
    LLVMBuildStore(ctx->builder, count_val, length_ptr);
    
    // Get pointer to data (offset 8)
    LLVMValueRef data_ptr = LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->context),
        array_ptr, (LLVMValueRef[]){ length_size }, 1, "data_ptr");
    LLVMValueRef typed_data_ptr = LLVMBuildBitCast(ctx->builder, data_ptr,
        LLVMPointerType(element_type, 0), "typed_data");
    
    // Initialize array elements
    for (int i = 0; i < expr->count; i++) {
        if (expr->elements[i]) {
            // Generate code for element
            LLVMValueRef element_value = codegen_expression(expr->elements[i], ctx);
            if (element_value) {
                // Get pointer to array element
                LLVMValueRef indices[] = { LLVMConstInt(ctx->int_type, i, false) };
                LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, element_type,
                    typed_data_ptr, indices, 1, "element_ptr");
                LLVMBuildStore(ctx->builder, element_value, element_ptr);
            }
        }
    }
    
    return typed_data_ptr;
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

// Get array length (reads from runtime metadata)
LLVMValueRef get_array_length(LLVMValueRef array, LLVMCodegenContext* ctx) {
    if (!array || !ctx) {
        return NULL;
    }
    
    // Array pointer points to data, length is at offset -8
    // Layout: [length: i64][data...]
    //                      ^ array points here
    
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    
    // Cast array to i8*
    LLVMValueRef i8_ptr = LLVMBuildBitCast(ctx->builder, array,
        LLVMPointerType(i8_type, 0), "i8_ptr");
    
    // Go back 8 bytes to get length pointer
    LLVMValueRef minus_8 = LLVMConstInt(i64_type, -8, true);
    LLVMValueRef length_i8_ptr = LLVMBuildGEP2(ctx->builder, i8_type,
        i8_ptr, (LLVMValueRef[]){ minus_8 }, 1, "length_i8_ptr");
    
    // Cast to i64*
    LLVMValueRef length_ptr = LLVMBuildBitCast(ctx->builder, length_i8_ptr,
        LLVMPointerType(i64_type, 0), "length_ptr");
    
    // Load length
    LLVMValueRef length = LLVMBuildLoad2(ctx->builder, i64_type, length_ptr, "array_length");
    
    // Truncate to int (i32)
    return LLVMBuildTrunc(ctx->builder, length, ctx->int_type, "length_int");
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
