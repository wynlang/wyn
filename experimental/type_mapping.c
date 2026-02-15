#include "type_mapping.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include "safe_memory.h"
#include "error.h"

// Map Wyn types to LLVM types
LLVMTypeRef map_wyn_type_to_llvm(Type* wyn_type, LLVMCodegenContext* ctx) {
    if (!wyn_type || !ctx) {
        return NULL;
    }
    
    switch (wyn_type->kind) {
        case TYPE_INT:
            return ctx->int_type;
        case TYPE_FLOAT:
            return ctx->float_type;
        case TYPE_BOOL:
            return ctx->bool_type;
        case TYPE_STRING:
            return ctx->string_type;
        case TYPE_VOID:
            return ctx->void_type;
        case TYPE_ARRAY:
            // For now, use pointer to element type
            return LLVMPointerType(ctx->int_type, 0);
        case TYPE_STRUCT:
            return create_struct_type(&wyn_type->struct_type, ctx);
        case TYPE_FUNCTION:
            return create_function_type(&wyn_type->fn_type, ctx);
        case TYPE_OPTIONAL:
            return create_optional_type(&wyn_type->optional_type, ctx);
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported type for LLVM mapping");
            return NULL;
    }
}

// Get basic LLVM types
LLVMTypeRef get_basic_llvm_type(TypeKind kind, LLVMCodegenContext* ctx) {
    if (!ctx) return NULL;
    
    switch (kind) {
        case TYPE_INT:    return ctx->int_type;
        case TYPE_FLOAT:  return ctx->float_type;
        case TYPE_BOOL:   return ctx->bool_type;
        case TYPE_STRING: return ctx->string_type;
        case TYPE_VOID:   return ctx->void_type;
        default:          return NULL;
    }
}

// Create struct type with proper layout
LLVMTypeRef create_struct_type(StructType* struct_type, LLVMCodegenContext* ctx) {
    if (!struct_type || !ctx) {
        return NULL;
    }
    
    LLVMTypeRef* field_types = safe_malloc(sizeof(LLVMTypeRef) * struct_type->field_count);
    if (!field_types) {
        report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to allocate struct field types");
        return NULL;
    }
    
    // Map each field type
    for (int i = 0; i < struct_type->field_count; i++) {
        field_types[i] = map_wyn_type_to_llvm(struct_type->field_types[i], ctx);
        if (!field_types[i]) {
            safe_free(field_types);
            return NULL;
        }
    }
    
    // Create LLVM struct type
    LLVMTypeRef struct_llvm_type = LLVMStructTypeInContext(
        ctx->context, field_types, struct_type->field_count, false
    );
    
    safe_free(field_types);
    return struct_llvm_type;
}

// Validate struct layout compatibility
bool validate_struct_layout(StructType* struct_type, LLVMTypeRef llvm_type) {
    if (!struct_type || !llvm_type) {
        return false;
    }
    
    // Check if it's a struct type
    if (LLVMGetTypeKind(llvm_type) != LLVMStructTypeKind) {
        return false;
    }
    
    // Check field count matches
    unsigned llvm_field_count = LLVMCountStructElementTypes(llvm_type);
    return (int)llvm_field_count == struct_type->field_count;
}

// Create function type
LLVMTypeRef create_function_type(FunctionType* fn_type, LLVMCodegenContext* ctx) {
    if (!fn_type || !ctx) {
        return NULL;
    }
    
    // Map parameter types
    LLVMTypeRef* param_types = NULL;
    if (fn_type->param_count > 0) {
        param_types = safe_malloc(sizeof(LLVMTypeRef) * fn_type->param_count);
        if (!param_types) {
            report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to allocate function parameter types");
            return NULL;
        }
        
        for (int i = 0; i < fn_type->param_count; i++) {
            param_types[i] = map_wyn_type_to_llvm(fn_type->param_types[i], ctx);
            if (!param_types[i]) {
                safe_free(param_types);
                return NULL;
            }
        }
    }
    
    // Map return type
    LLVMTypeRef return_type = map_wyn_type_to_llvm(fn_type->return_type, ctx);
    if (!return_type) {
        safe_free(param_types);
        return NULL;
    }
    
    // Create function type
    LLVMTypeRef function_type = LLVMFunctionType(
        return_type, param_types, fn_type->param_count, false
    );
    
    safe_free(param_types);
    return function_type;
}

// Declare function with proper signature
LLVMValueRef declare_function(const char* name, FunctionType* fn_type, LLVMCodegenContext* ctx) {
    if (!name || !fn_type || !ctx) {
        return NULL;
    }
    
    LLVMTypeRef function_type = create_function_type(fn_type, ctx);
    if (!function_type) {
        return NULL;
    }
    
    return LLVMAddFunction(ctx->module, name, function_type);
}

// Create array type
LLVMTypeRef create_array_type(Type* element_type, int length, LLVMCodegenContext* ctx) {
    if (!element_type || !ctx || length < 0) {
        return NULL;
    }
    
    LLVMTypeRef element_llvm_type = map_wyn_type_to_llvm(element_type, ctx);
    if (!element_llvm_type) {
        return NULL;
    }
    
    return LLVMArrayType(element_llvm_type, length);
}

// Create optional type (for T2.5.1 integration)
LLVMTypeRef create_optional_type(OptionalType* opt_type, LLVMCodegenContext* ctx) {
    if (!opt_type || !ctx) {
        return NULL;
    }
    
    // Optional type is represented as a struct with a boolean flag and the value
    LLVMTypeRef inner_type = map_wyn_type_to_llvm(opt_type->inner_type, ctx);
    if (!inner_type) {
        return NULL;
    }
    
    LLVMTypeRef field_types[2] = {
        ctx->bool_type,  // has_value flag
        inner_type       // the actual value
    };
    
    return LLVMStructTypeInContext(ctx->context, field_types, 2, false);
}

// Get type size in bytes
size_t get_type_size(Type* wyn_type, LLVMCodegenContext* ctx) {
    if (!wyn_type || !ctx || !ctx->target_machine) {
        return 0;
    }
    
    LLVMTypeRef llvm_type = map_wyn_type_to_llvm(wyn_type, ctx);
    if (!llvm_type) {
        return 0;
    }
    
    LLVMTargetDataRef target_data = LLVMCreateTargetDataLayout(ctx->target_machine);
    size_t size = LLVMSizeOfTypeInBits(target_data, llvm_type) / 8;
    LLVMDisposeTargetData(target_data);
    
    return size;
}

// Get type alignment in bytes
size_t get_type_alignment(Type* wyn_type, LLVMCodegenContext* ctx) {
    if (!wyn_type || !ctx || !ctx->target_machine) {
        return 1;
    }
    
    LLVMTypeRef llvm_type = map_wyn_type_to_llvm(wyn_type, ctx);
    if (!llvm_type) {
        return 1;
    }
    
    LLVMTargetDataRef target_data = LLVMCreateTargetDataLayout(ctx->target_machine);
    size_t alignment = LLVMABIAlignmentOfType(target_data, llvm_type);
    LLVMDisposeTargetData(target_data);
    
    return alignment;
}

// Check type compatibility
bool are_types_compatible(Type* type1, Type* type2) {
    if (!type1 || !type2) {
        return false;
    }
    
    if (type1->kind != type2->kind) {
        return false;
    }
    
    // For basic types, kind matching is sufficient
    switch (type1->kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_BOOL:
        case TYPE_STRING:
        case TYPE_VOID:
            return true;
        case TYPE_ARRAY:
        case TYPE_STRUCT:
        case TYPE_FUNCTION:
        case TYPE_OPTIONAL:
            // More complex compatibility checking would go here
            return true;
        default:
            return false;
    }
}

// Check if types can be converted
bool can_convert_types(Type* from, Type* to) {
    if (!from || !to) {
        return false;
    }
    
    // Same types are always convertible
    if (are_types_compatible(from, to)) {
        return true;
    }
    
    // Allow numeric conversions
    if ((from->kind == TYPE_INT && to->kind == TYPE_FLOAT) ||
        (from->kind == TYPE_FLOAT && to->kind == TYPE_INT)) {
        return true;
    }
    
    return false;
}

// Initialize type mapping system
bool init_type_mapping(LLVMCodegenContext* ctx) {
    if (!ctx) {
        return false;
    }
    
    // Initialize basic types in context (these should already be set up)
    if (!ctx->int_type) {
        ctx->int_type = LLVMInt64TypeInContext(ctx->context);
    }
    if (!ctx->float_type) {
        ctx->float_type = LLVMDoubleTypeInContext(ctx->context);
    }
    if (!ctx->bool_type) {
        ctx->bool_type = LLVMInt1TypeInContext(ctx->context);
    }
    if (!ctx->string_type) {
        ctx->string_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    }
    if (!ctx->void_type) {
        ctx->void_type = LLVMVoidTypeInContext(ctx->context);
    }
    
    return true;
}

// Cleanup type mapping system
void cleanup_type_mapping(LLVMCodegenContext* ctx) {
    // Type cleanup is handled by LLVM context disposal
    (void)ctx;
}

#endif // WITH_LLVM
