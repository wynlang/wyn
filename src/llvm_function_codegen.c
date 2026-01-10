#include "llvm_function_codegen.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safe_memory.h"
#include "error.h"
#include "type_mapping.h"
#include "llvm_statement_codegen.h"

// Generate LLVM function definition
LLVMValueRef codegen_function_definition(FnStmt* fn_stmt, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !ctx) {
        return NULL;
    }
    
    // First create/get the function declaration
    LLVMValueRef function = codegen_function_declaration(fn_stmt, ctx);
    if (!function) {
        return NULL;
    }
    
    // Create entry basic block
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(function, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry_block);
    
    // Generate function prologue
    codegen_function_prologue(fn_stmt, function, ctx);
    
    // Set current function context
    LLVMValueRef prev_function = ctx->current_function;
    ctx->current_function = function;
    
    // Generate function body
    if (fn_stmt->body) {
        codegen_statement(fn_stmt->body, ctx);
    }
    
    // Generate function epilogue (if no explicit return)
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        codegen_function_epilogue(fn_stmt, function, ctx);
    }
    
    // Restore previous function context
    ctx->current_function = prev_function;
    
    return function;
}

// Generate LLVM function declaration (forward declaration support)
LLVMValueRef codegen_function_declaration(FnStmt* fn_stmt, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !ctx) {
        return NULL;
    }
    
    // Get function name
    char* func_name = get_function_name_from_token(&fn_stmt->name);
    if (!func_name) {
        return NULL;
    }
    
    // Check if function already exists
    LLVMValueRef existing_func = LLVMGetNamedFunction(ctx->module, func_name);
    if (existing_func) {
        safe_free(func_name);
        return existing_func;
    }
    
    // Get parameter types
    LLVMTypeRef* param_types = codegen_function_parameter_types(fn_stmt, ctx);
    
    // Get return type
    LLVMTypeRef return_type = ctx->void_type; // Default to void
    if (fn_stmt->return_type) {
        return_type = get_llvm_type_from_wyn_type(fn_stmt->return_type, ctx);
        if (!return_type) {
            return_type = ctx->void_type;
        }
    }
    
    // Create function type
    LLVMTypeRef func_type = LLVMFunctionType(return_type, param_types, fn_stmt->param_count, 0);
    
    // Create function
    LLVMValueRef function = LLVMAddFunction(ctx->module, func_name, func_type);
    
    // Set parameter names
    for (int i = 0; i < fn_stmt->param_count; i++) {
        LLVMValueRef param = LLVMGetParam(function, i);
        char* param_name = safe_malloc(fn_stmt->params[i].length + 1);
        if (param_name) {
            strncpy(param_name, fn_stmt->params[i].start, fn_stmt->params[i].length);
            param_name[fn_stmt->params[i].length] = '\0';
            LLVMSetValueName(param, param_name);
            safe_free(param_name);
        }
    }
    
    // Clean up
    if (param_types) {
        safe_free(param_types);
    }
    safe_free(func_name);
    
    return function;
}

// Generate parameter types array
LLVMTypeRef* codegen_function_parameter_types(FnStmt* fn_stmt, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !ctx || fn_stmt->param_count == 0) {
        return NULL;
    }
    
    LLVMTypeRef* param_types = safe_malloc(sizeof(LLVMTypeRef) * fn_stmt->param_count);
    if (!param_types) {
        return NULL;
    }
    
    for (int i = 0; i < fn_stmt->param_count; i++) {
        if (fn_stmt->param_types && fn_stmt->param_types[i]) {
            param_types[i] = get_llvm_type_from_wyn_type(fn_stmt->param_types[i], ctx);
        } else {
            // Default to int type if no type specified
            param_types[i] = ctx->int_type;
        }
        
        if (!param_types[i]) {
            param_types[i] = ctx->int_type; // Fallback
        }
    }
    
    return param_types;
}

// Generate parameter handling (allocate local variables for parameters)
void codegen_function_parameters(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !function || !ctx) {
        return;
    }
    
    // Create local variables for each parameter
    for (int i = 0; i < fn_stmt->param_count; i++) {
        LLVMValueRef param = LLVMGetParam(function, i);
        LLVMTypeRef param_type = LLVMTypeOf(param);
        
        // Get parameter name
        char* param_name = safe_malloc(fn_stmt->params[i].length + 1);
        if (!param_name) {
            continue;
        }
        strncpy(param_name, fn_stmt->params[i].start, fn_stmt->params[i].length);
        param_name[fn_stmt->params[i].length] = '\0';
        
        // Create alloca for parameter
        LLVMValueRef param_alloca = create_local_variable(param_name, param_type, ctx);
        if (param_alloca) {
            // Store parameter value into local variable
            LLVMBuildStore(ctx->builder, param, param_alloca);
        }
        
        safe_free(param_name);
    }
}

// Generate function prologue
void codegen_function_prologue(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !function || !ctx) {
        return;
    }
    
    // Handle parameters (create local variables)
    codegen_function_parameters(fn_stmt, function, ctx);
    
    // TODO: Add stack frame setup, debugging info, etc.
}

// Generate function epilogue
void codegen_function_epilogue(FnStmt* fn_stmt, LLVMValueRef function, LLVMCodegenContext* ctx) {
    if (!fn_stmt || !function || !ctx) {
        return;
    }
    
    // Get function return type
    LLVMTypeRef func_type = LLVMGetElementType(LLVMTypeOf(function));
    LLVMTypeRef return_type = LLVMGetReturnType(func_type);
    
    // Generate appropriate return
    if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
        LLVMBuildRetVoid(ctx->builder);
    } else {
        // Return default value for the type
        LLVMValueRef default_value = NULL;
        
        switch (LLVMGetTypeKind(return_type)) {
            case LLVMIntegerTypeKind:
                default_value = LLVMConstInt(return_type, 0, 0);
                break;
            case LLVMFloatTypeKind:
            case LLVMDoubleTypeKind:
                default_value = LLVMConstReal(return_type, 0.0);
                break;
            case LLVMPointerTypeKind:
                default_value = LLVMConstNull(return_type);
                break;
            default:
                // For other types, try to create a null/zero value
                default_value = LLVMConstNull(return_type);
                break;
        }
        
        if (default_value) {
            LLVMBuildRet(ctx->builder, default_value);
        } else {
            LLVMBuildRetVoid(ctx->builder);
        }
    }
}

// Convert Wyn type expression to LLVM type
LLVMTypeRef get_llvm_type_from_wyn_type(Expr* type_expr, LLVMCodegenContext* ctx) {
    if (!type_expr || !ctx) {
        return ctx->int_type; // Default fallback
    }
    
    // For now, implement basic type mapping
    // This would need to be expanded based on the actual type system
    
    // If it's an identifier expression, check the name
    if (type_expr->type == EXPR_IDENT) {
        Token* name_token = &type_expr->token;
        
        if (name_token->length == 3 && strncmp(name_token->start, "int", 3) == 0) {
            return ctx->int_type;
        } else if (name_token->length == 5 && strncmp(name_token->start, "float", 5) == 0) {
            return ctx->float_type;
        } else if (name_token->length == 4 && strncmp(name_token->start, "bool", 4) == 0) {
            return ctx->bool_type;
        } else if (name_token->length == 6 && strncmp(name_token->start, "string", 6) == 0) {
            return ctx->string_type;
        }
    }
    
    // Default to int type
    return ctx->int_type;
}

// Extract function name from token
char* get_function_name_from_token(Token* name_token) {
    if (!name_token || !name_token->start || name_token->length == 0) {
        return NULL;
    }
    
    char* name = safe_malloc(name_token->length + 1);
    if (!name) {
        return NULL;
    }
    
    strncpy(name, name_token->start, name_token->length);
    name[name_token->length] = '\0';
    
    return name;
}

#endif // WITH_LLVM
