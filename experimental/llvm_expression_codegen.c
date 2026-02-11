#include "llvm_expression_codegen.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safe_memory.h"
#include "error.h"
#include "type_mapping.h"
#include "llvm_context.h"
#include "llvm_array_string_codegen.h"
#include "runtime_functions.h"
#include "types.h"

// Main expression code generation dispatcher
LLVMValueRef codegen_expression(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    
    LLVMValueRef result = NULL;
    
    switch (expr->type) {
        case EXPR_INT:
            result = codegen_int_literal(expr, ctx);
            break;
        case EXPR_FLOAT:
            result = codegen_float_literal(expr, ctx);
            break;
        case EXPR_STRING:
            result = codegen_string_literal(expr, ctx);
            break;
        case EXPR_BOOL:
            result = codegen_bool_literal(expr, ctx);
            break;
        case EXPR_BINARY:
            result = codegen_binary_expr(&expr->binary, ctx);
            break;
        case EXPR_UNARY:
            result = codegen_unary_expr(&expr->unary, ctx);
            break;
        case EXPR_IDENT:
            result = codegen_variable_ref(expr, ctx);
            break;
        case EXPR_ASSIGN:
            result = codegen_assignment(&expr->assign, ctx);
            break;
        case EXPR_CALL:
            result = codegen_function_call(&expr->call, ctx);
            break;
        case EXPR_SPAWN:
            result = codegen_spawn_expr(&expr->spawn, ctx);
            break;
        case EXPR_INDEX:
            result = codegen_array_indexing(&expr->index, ctx);
            break;
        case EXPR_ARRAY:
            result = codegen_array_literal(&expr->array, ctx);
            break;
        case EXPR_METHOD_CALL:
            result = codegen_method_call(&expr->method_call, ctx);
            break;
        case EXPR_FIELD_ACCESS:
            result = codegen_field_access(&expr->field_access, ctx);
            break;
        case EXPR_STRUCT_INIT:
            result = codegen_struct_init(&expr->struct_init, ctx);
            break;
        case EXPR_MATCH:
            result = codegen_match_expr(&expr->match, ctx);
            break;
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported expression type for code generation");
            return NULL;
    }
    
    return result;
}

// Generate code for integer literals
LLVMValueRef codegen_int_literal(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Parse integer value from token
    char* endptr;
    long long value = strtoll(expr->token.start, &endptr, 10);
    
    return LLVMConstInt(ctx->int_type, value, false);
}

// Generate code for float literals
LLVMValueRef codegen_float_literal(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Parse float value from token
    char* endptr;
    double value = strtod(expr->token.start, &endptr);
    
    return LLVMConstReal(ctx->float_type, value);
}

// Generate code for string literals
LLVMValueRef codegen_string_literal(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Extract string content (remove quotes)
    int len = expr->token.length - 2;  // Remove quotes
    char* str_content = safe_malloc(len + 1);
    if (!str_content) {
        return NULL;
    }
    
    strncpy(str_content, expr->token.start + 1, len);
    str_content[len] = '\0';
    
    // Create global string constant
    LLVMValueRef str_global = LLVMBuildGlobalStringPtr(ctx->builder, str_content, "str");
    
    safe_free(str_content);
    return str_global;
}

// Generate code for boolean literals
LLVMValueRef codegen_bool_literal(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Check if token is "true" or "false"
    bool value = (strncmp(expr->token.start, "true", 4) == 0);
    
    return LLVMConstInt(ctx->bool_type, value ? 1 : 0, false);
}

// Generate code for binary expressions
LLVMValueRef codegen_binary_expr(BinaryExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    LLVMValueRef left = codegen_expression(expr->left, ctx);
    LLVMValueRef right = codegen_expression(expr->right, ctx);
    
    if (!left || !right) {
        return NULL;
    }
    
    // Determine operation type
    switch (expr->op.type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            return codegen_arithmetic_op(left, right, expr->op.type, ctx);
        case TOKEN_EQEQ:
        case TOKEN_BANGEQ:
        case TOKEN_LT:
        case TOKEN_LTEQ:
        case TOKEN_GT:
        case TOKEN_GTEQ:
            return codegen_comparison_op(left, right, expr->op.type, ctx);
        case TOKEN_AMPAMP:
        case TOKEN_PIPEPIPE:
            return codegen_logical_op(left, right, expr->op.type, ctx);
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported binary operator");
            return NULL;
    }
}

// Generate arithmetic operations
LLVMValueRef codegen_arithmetic_op(LLVMValueRef left, LLVMValueRef right, WynTokenType op, LLVMCodegenContext* ctx) {
    if (!left || !right || !ctx) {
        return NULL;
    }
    
    LLVMTypeRef left_type = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    
    // Handle type conversion if needed
    if (is_float_type(left_type) || is_float_type(right_type)) {
        // Convert to float operation
        if (is_integer_type(left_type)) {
            left = LLVMBuildSIToFP(ctx->builder, left, ctx->float_type, "itof");
        }
        if (is_integer_type(right_type)) {
            right = LLVMBuildSIToFP(ctx->builder, right, ctx->float_type, "itof");
        }
        
        switch (op) {
            case TOKEN_PLUS:  return LLVMBuildFAdd(ctx->builder, left, right, "fadd");
            case TOKEN_MINUS: return LLVMBuildFSub(ctx->builder, left, right, "fsub");
            case TOKEN_STAR:  return LLVMBuildFMul(ctx->builder, left, right, "fmul");
            case TOKEN_SLASH: return LLVMBuildFDiv(ctx->builder, left, right, "fdiv");
            default:
                report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid float arithmetic operation");
                return NULL;
        }
    } else {
        // Integer operations
        switch (op) {
            case TOKEN_PLUS:    return LLVMBuildAdd(ctx->builder, left, right, "add");
            case TOKEN_MINUS:   return LLVMBuildSub(ctx->builder, left, right, "sub");
            case TOKEN_STAR:    return LLVMBuildMul(ctx->builder, left, right, "mul");
            case TOKEN_SLASH:   return LLVMBuildSDiv(ctx->builder, left, right, "div");
            case TOKEN_PERCENT: return LLVMBuildSRem(ctx->builder, left, right, "rem");
            default:
                report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid integer arithmetic operation");
                return NULL;
        }
    }
}

// Generate comparison operations
LLVMValueRef codegen_comparison_op(LLVMValueRef left, LLVMValueRef right, WynTokenType op, LLVMCodegenContext* ctx) {
    if (!left || !right || !ctx) {
        return NULL;
    }
    
    LLVMTypeRef left_type = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    
    if (is_float_type(left_type) || is_float_type(right_type)) {
        // Float comparison
        LLVMRealPredicate pred;
        switch (op) {
            case TOKEN_EQEQ: pred = LLVMRealOEQ; break;
            case TOKEN_BANGEQ: pred = LLVMRealONE; break;
            case TOKEN_LT: pred = LLVMRealOLT; break;
            case TOKEN_LTEQ: pred = LLVMRealOLE; break;
            case TOKEN_GT: pred = LLVMRealOGT; break;
            case TOKEN_GTEQ: pred = LLVMRealOGE; break;
            default:
                report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid float comparison operation");
                return NULL;
        }
        return LLVMBuildFCmp(ctx->builder, pred, left, right, "fcmp");
    } else {
        // Integer comparison
        LLVMIntPredicate pred;
        switch (op) {
            case TOKEN_EQEQ: pred = LLVMIntEQ; break;
            case TOKEN_BANGEQ: pred = LLVMIntNE; break;
            case TOKEN_LT: pred = LLVMIntSLT; break;
            case TOKEN_LTEQ: pred = LLVMIntSLE; break;
            case TOKEN_GT: pred = LLVMIntSGT; break;
            case TOKEN_GTEQ: pred = LLVMIntSGE; break;
            default:
                report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid integer comparison operation");
                return NULL;
        }
        return LLVMBuildICmp(ctx->builder, pred, left, right, "icmp");
    }
}

// Generate logical operations
LLVMValueRef codegen_logical_op(LLVMValueRef left, LLVMValueRef right, WynTokenType op, LLVMCodegenContext* ctx) {
    if (!left || !right || !ctx) {
        return NULL;
    }
    
    switch (op) {
        case TOKEN_AMPAMP:
            return LLVMBuildAnd(ctx->builder, left, right, "and");
        case TOKEN_PIPEPIPE:
            return LLVMBuildOr(ctx->builder, left, right, "or");
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid logical operation");
            return NULL;
    }
}

// Generate unary expressions
LLVMValueRef codegen_unary_expr(UnaryExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    LLVMValueRef operand = codegen_expression(expr->operand, ctx);
    if (!operand) {
        return NULL;
    }
    
    switch (expr->op.type) {
        case TOKEN_MINUS:
            if (is_float_type(LLVMTypeOf(operand))) {
                // For constant floats, get the value and negate it
                if (LLVMIsConstant(operand)) {
                    LLVMBool loses_info = 0;
                    double val = LLVMConstRealGetDouble(operand, &loses_info);
                    return LLVMConstReal(LLVMTypeOf(operand), -val);
                }
                return LLVMBuildFNeg(ctx->builder, operand, "fneg");
            } else {
                if (LLVMIsConstant(operand)) {
                    return LLVMConstNeg(operand);
                }
                return LLVMBuildNeg(ctx->builder, operand, "neg");
            }
        case TOKEN_BANG:
            return LLVMBuildNot(ctx->builder, operand, "not");
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported unary operator");
            return NULL;
    }
}

// Generate variable reference (placeholder - needs symbol table)
LLVMValueRef codegen_variable_ref(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Extract variable name from token
    char* var_name = safe_malloc(expr->token.length + 1);
    if (!var_name) return NULL;
    
    strncpy(var_name, expr->token.start, expr->token.length);
    var_name[expr->token.length] = '\0';
    
    // Check for boolean literals
    if (strcmp(var_name, "true") == 0) {
        safe_free(var_name);
        return LLVMConstInt(ctx->int_type, 1, false);
    }
    if (strcmp(var_name, "false") == 0) {
        safe_free(var_name);
        return LLVMConstInt(ctx->int_type, 0, false);
    }
    
    // Check for built-in 'none' constant
    if (strcmp(var_name, "none") == 0) {
        // Call wyn_none() runtime function
        LLVMValueRef function = LLVMGetNamedFunction(ctx->module, "wyn_none");
        if (!function) {
            // Declare wyn_none: WynOptional* wyn_none(void)
            LLVMTypeRef ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef func_type = LLVMFunctionType(ptr_type, NULL, 0, false);
            function = LLVMAddFunction(ctx->module, "wyn_none", func_type);
        }
        
        LLVMValueRef call = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(function),
                                            function, 
                                            NULL, 
                                            0, 
                                            "none");
        safe_free(var_name);
        return call;
    }
    
    // Check for global constants (enum variants)
    LLVMValueRef global_const = LLVMGetNamedGlobal(ctx->module, var_name);
    if (global_const && LLVMIsGlobalConstant(global_const)) {
        // Load the constant value
        LLVMValueRef loaded = LLVMBuildLoad2(ctx->builder, ctx->int_type, global_const, var_name);
        safe_free(var_name);
        return loaded;
    }
    
    // Look up variable in symbol table
    LLVMValueRef var_ptr = symbol_table_lookup(ctx->symbol_table, var_name);
    if (!var_ptr) {
        fprintf(stderr, "Error: Undefined variable '%s'\n", var_name);
        safe_free(var_name);
        return NULL;
    }
    
    // Look up the type
    LLVMTypeRef var_type = symbol_table_lookup_type(ctx->symbol_table, var_name);
    if (!var_type) {
        var_type = ctx->int_type;  // Default to int
    }
    
    // Load the value from the pointer
    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, var_type, var_ptr, var_name);
    safe_free(var_name);
    return value;
}

// Generate assignment
LLVMValueRef codegen_assignment(AssignExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return NULL;
    }
    
    // Get variable name
    char* var_name = safe_malloc(expr->name.length + 1);
    if (!var_name) return NULL;
    
    strncpy(var_name, expr->name.start, expr->name.length);
    var_name[expr->name.length] = '\0';
    
    // Look up variable in symbol table
    LLVMValueRef var_ptr = symbol_table_lookup(ctx->symbol_table, var_name);
    if (!var_ptr) {
        fprintf(stderr, "Error: Undefined variable '%s'\n", var_name);
        safe_free(var_name);
        return NULL;
    }
    
    // Generate value expression
    LLVMValueRef value = codegen_expression(expr->value, ctx);
    if (!value) {
        safe_free(var_name);
        return NULL;
    }
    
    // Store value
    LLVMBuildStore(ctx->builder, value, var_ptr);
    safe_free(var_name);
    return value;
}

// Generate function call (placeholder - needs function table)
LLVMValueRef codegen_function_call(CallExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx || !expr->callee) {
        return NULL;
    }
    
    // Get function name from callee expression (should be a variable ref)
    if (expr->callee->type != EXPR_IDENT) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Function callee must be an identifier");
        return NULL;
    }
    
    char* func_name = safe_malloc(expr->callee->token.length + 1);
    if (!func_name) return NULL;
    
    strncpy(func_name, expr->callee->token.start, expr->callee->token.length);
    func_name[expr->callee->token.length] = '\0';
    
    // Handle special Option/Result constructors
    if (strcmp(func_name, "some") == 0 || strcmp(func_name, "none") == 0 ||
        strcmp(func_name, "ok") == 0 || strcmp(func_name, "err") == 0) {
        
        // Declare runtime function if not already declared
        char runtime_name[64];
        snprintf(runtime_name, sizeof(runtime_name), "wyn_%s", func_name);
        
        LLVMValueRef function = LLVMGetNamedFunction(ctx->module, runtime_name);
        if (!function) {
            // Declare the runtime function
            LLVMTypeRef ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            
            if (strcmp(func_name, "none") == 0) {
                // WynOptional* wyn_none(void)
                LLVMTypeRef func_type = LLVMFunctionType(ptr_type, NULL, 0, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            } else {
                // WynOptional* wyn_some(void* value, size_t size)
                // WynResult* wyn_ok(void* value, size_t size)
                // WynResult* wyn_err(void* error, size_t size)
                LLVMTypeRef param_types[] = { ptr_type, LLVMInt64TypeInContext(ctx->context) };
                LLVMTypeRef func_type = LLVMFunctionType(ptr_type, param_types, 2, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            }
        }
        
        // Generate arguments
        LLVMValueRef* args = NULL;
        int arg_count = 0;
        
        if (strcmp(func_name, "none") != 0) {
            // For some/ok/err, we need to pass the value and its size
            if (expr->arg_count > 0) {
                args = safe_malloc(sizeof(LLVMValueRef) * 2);
                if (!args) {
                    safe_free(func_name);
                    return NULL;
                }
                
                // Get the value argument
                LLVMValueRef value = codegen_expression(expr->args[0], ctx);
                if (!value) {
                    safe_free(args);
                    safe_free(func_name);
                    return NULL;
                }
                
                // Allocate space for the value and store it
                LLVMTypeRef value_type = LLVMTypeOf(value);
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, value_type, "tmp");
                LLVMBuildStore(ctx->builder, value, alloca);
                
                // Cast to i8*
                args[0] = LLVMBuildBitCast(ctx->builder, alloca, 
                                          LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0), 
                                          "cast");
                
                // Get size of the value type
                LLVMValueRef size = LLVMSizeOf(value_type);
                args[1] = size;
                arg_count = 2;
            }
        }
        
        // Build call
        LLVMValueRef call = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(function),
                                            function, 
                                            args, 
                                            arg_count, 
                                            runtime_name);
        
        safe_free(args);
        safe_free(func_name);
        return call;
    }
    
    // Handle file I/O functions
    if (strcmp(func_name, "file_write") == 0 || strcmp(func_name, "file_read") == 0 ||
        strcmp(func_name, "file_append") == 0 || strcmp(func_name, "file_exists") == 0) {
        
        // Declare runtime function if not already declared
        char runtime_name[64];
        snprintf(runtime_name, sizeof(runtime_name), "wyn_%s_simple", func_name);
        
        LLVMValueRef function = LLVMGetNamedFunction(ctx->module, runtime_name);
        if (!function) {
            LLVMTypeRef ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            
            if (strcmp(func_name, "file_read") == 0 || strcmp(func_name, "file_exists") == 0) {
                // int wyn_file_read_simple(char* path) or int wyn_file_exists_simple(char* path)
                LLVMTypeRef param_types[] = { ptr_type };
                LLVMTypeRef func_type = LLVMFunctionType(ctx->int_type, param_types, 1, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            } else {
                // int wyn_file_write_simple(char* path, char* content) or int wyn_file_append_simple(char* path, char* content)
                LLVMTypeRef param_types[] = { ptr_type, ptr_type };
                LLVMTypeRef func_type = LLVMFunctionType(ctx->int_type, param_types, 2, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            }
        }
        
        // Generate arguments
        LLVMValueRef* args = safe_malloc(sizeof(LLVMValueRef) * expr->arg_count);
        if (!args) {
            safe_free(func_name);
            return NULL;
        }
        
        for (int i = 0; i < expr->arg_count; i++) {
            args[i] = codegen_expression(expr->args[i], ctx);
            if (!args[i]) {
                safe_free(args);
                safe_free(func_name);
                return NULL;
            }
        }
        
        // Build call
        LLVMValueRef call = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(function),
                                            function, 
                                            args, 
                                            expr->arg_count, 
                                            runtime_name);
        
        safe_free(args);
        safe_free(func_name);
        return call;
    }
    
    // Handle print/println/assert functions
    if (strcmp(func_name, "print") == 0 || strcmp(func_name, "println") == 0 || strcmp(func_name, "assert") == 0) {
        // Get or declare printf
        LLVMValueRef printf_fn = LLVMGetNamedFunction(ctx->module, "printf");
        if (!printf_fn) {
            LLVMTypeRef printf_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
            LLVMTypeRef printf_type = LLVMFunctionType(ctx->int_type, printf_args, 1, true);
            printf_fn = LLVMAddFunction(ctx->module, "printf", printf_type);
        }
        
        if (strcmp(func_name, "assert") == 0) {
            // assert(condition) - simple implementation
            if (expr->arg_count >= 1) {
                LLVMValueRef condition = codegen_expression(expr->args[0], ctx);
                
                // Create fail block
                LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->builder);
                LLVMValueRef current_func = LLVMGetBasicBlockParent(current_block);
                LLVMBasicBlockRef fail_block = LLVMAppendBasicBlockInContext(ctx->context, current_func, "assert_fail");
                LLVMBasicBlockRef pass_block = LLVMAppendBasicBlockInContext(ctx->context, current_func, "assert_pass");
                
                // Branch on condition
                LLVMBuildCondBr(ctx->builder, condition, pass_block, fail_block);
                
                // Fail block: print and exit
                LLVMPositionBuilderAtEnd(ctx->builder, fail_block);
                LLVMValueRef format_str = LLVMBuildGlobalStringPtr(ctx->builder, "Assertion failed\n", "assert_msg");
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_fn), printf_fn, &format_str, 1, "");
                LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->int_type, 1, false));
                
                // Continue in pass block
                LLVMPositionBuilderAtEnd(ctx->builder, pass_block);
            }
            safe_free(func_name);
            return LLVMConstInt(ctx->int_type, 0, false);
        }
        
        // print/println: print all arguments
        for (int i = 0; i < expr->arg_count; i++) {
            LLVMValueRef arg = codegen_expression(expr->args[i], ctx);
            if (!arg) continue;
            
            LLVMTypeRef arg_type = LLVMTypeOf(arg);
            LLVMValueRef format_str;
            
            if (LLVMGetTypeKind(arg_type) == LLVMIntegerTypeKind) {
                format_str = LLVMBuildGlobalStringPtr(ctx->builder, "%d", "fmt");
                LLVMValueRef args[] = { format_str, arg };
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_fn), 
                    printf_fn, args, 2, "");
            } else if (LLVMGetTypeKind(arg_type) == LLVMPointerTypeKind) {
                format_str = LLVMBuildGlobalStringPtr(ctx->builder, "%s", "fmt");
                LLVMValueRef args[] = { format_str, arg };
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_fn), 
                    printf_fn, args, 2, "");
            }
        }
        
        if (strcmp(func_name, "println") == 0) {
            LLVMValueRef newline = LLVMBuildGlobalStringPtr(ctx->builder, "\n", "nl");
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_fn), 
                printf_fn, &newline, 1, "");
        }
        
        safe_free(func_name);
        return LLVMConstInt(ctx->int_type, 0, false);
    }
    
    // Handle math functions
    if (strcmp(func_name, "min") == 0 || strcmp(func_name, "max") == 0 || strcmp(func_name, "abs") == 0) {
        char runtime_name[64];
        snprintf(runtime_name, sizeof(runtime_name), "wyn_%s", func_name);
        
        LLVMValueRef function = LLVMGetNamedFunction(ctx->module, runtime_name);
        if (!function) {
            if (strcmp(func_name, "abs") == 0) {
                LLVMTypeRef param_types[] = { ctx->int_type };
                LLVMTypeRef func_type = LLVMFunctionType(ctx->int_type, param_types, 1, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            } else {
                LLVMTypeRef param_types[] = { ctx->int_type, ctx->int_type };
                LLVMTypeRef func_type = LLVMFunctionType(ctx->int_type, param_types, 2, false);
                function = LLVMAddFunction(ctx->module, runtime_name, func_type);
            }
        }
        
        LLVMValueRef* args = safe_malloc(sizeof(LLVMValueRef) * expr->arg_count);
        if (!args) {
            safe_free(func_name);
            return NULL;
        }
        
        for (int i = 0; i < expr->arg_count; i++) {
            args[i] = codegen_expression(expr->args[i], ctx);
            if (!args[i]) {
                safe_free(args);
                safe_free(func_name);
                return NULL;
            }
        }
        
        LLVMValueRef call = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(function),
                                            function, 
                                            args, 
                                            expr->arg_count, 
                                            runtime_name);
        
        safe_free(args);
        safe_free(func_name);
        return call;
    }
    
    // Handle len() and typeof()
    if (strcmp(func_name, "len") == 0) {
        // len(array or string) - returns int
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            if (!arg) {
                safe_free(func_name);
                return NULL;
            }
            
            LLVMTypeRef arg_type = LLVMTypeOf(arg);
            
            // For strings, call strlen
            if (LLVMGetTypeKind(arg_type) == LLVMPointerTypeKind) {
                LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
                if (!strlen_fn) {
                    LLVMTypeRef strlen_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
                    LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), strlen_args, 1, false);
                    strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
                }
                safe_free(func_name);
                return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &arg, 1, "strlen");
            }
            
            // For arrays, return a placeholder (arrays don't have runtime length yet)
            safe_free(func_name);
            return LLVMConstInt(ctx->int_type, 0, false);
        }
    } else if (strcmp(func_name, "typeof") == 0) {
        // typeof(value) - returns string
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            if (!arg) {
                safe_free(func_name);
                return NULL;
            }
            
            LLVMTypeRef arg_type = LLVMTypeOf(arg);
            const char* type_name = "unknown";
            
            if (LLVMGetTypeKind(arg_type) == LLVMIntegerTypeKind) {
                type_name = "int";
            } else if (LLVMGetTypeKind(arg_type) == LLVMPointerTypeKind) {
                type_name = "string";
            }
            
            LLVMValueRef type_str = LLVMBuildGlobalStringPtr(ctx->builder, type_name, "typename");
            safe_free(func_name);
            return type_str;
        }
    }
    
    // Handle utility functions
    if (strcmp(func_name, "exit") == 0) {
        LLVMValueRef exit_fn = LLVMGetNamedFunction(ctx->module, "exit");
        if (!exit_fn) {
            LLVMTypeRef param_types[] = { ctx->int_type };
            LLVMTypeRef exit_type = LLVMFunctionType(ctx->void_type, param_types, 1, false);
            exit_fn = LLVMAddFunction(ctx->module, "exit", exit_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            if (arg) LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(exit_fn), exit_fn, &arg, 1, "");
        }
        safe_free(func_name);
        return LLVMConstInt(ctx->int_type, 0, false);
    } else if (strcmp(func_name, "panic") == 0) {
        LLVMValueRef printf_fn = LLVMGetNamedFunction(ctx->module, "printf");
        if (!printf_fn) {
            LLVMTypeRef printf_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
            LLVMTypeRef printf_type = LLVMFunctionType(ctx->int_type, printf_args, 1, true);
            printf_fn = LLVMAddFunction(ctx->module, "printf", printf_type);
        }
        LLVMValueRef exit_fn = LLVMGetNamedFunction(ctx->module, "exit");
        if (!exit_fn) {
            LLVMTypeRef param_types[] = { ctx->int_type };
            LLVMTypeRef exit_type = LLVMFunctionType(ctx->void_type, param_types, 1, false);
            exit_fn = LLVMAddFunction(ctx->module, "exit", exit_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            if (arg) {
                LLVMValueRef format = LLVMBuildGlobalStringPtr(ctx->builder, "PANIC: %s\n", "panic_fmt");
                LLVMValueRef args[] = { format, arg };
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(printf_fn), printf_fn, args, 2, "");
            }
        }
        LLVMValueRef exit_code = LLVMConstInt(ctx->int_type, 1, false);
        LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(exit_fn), exit_fn, &exit_code, 1, "");
        safe_free(func_name);
        return LLVMConstInt(ctx->int_type, 0, false);
    } else if (strcmp(func_name, "sleep") == 0) {
        LLVMValueRef usleep_fn = LLVMGetNamedFunction(ctx->module, "usleep");
        if (!usleep_fn) {
            LLVMTypeRef param_types[] = { LLVMInt32TypeInContext(ctx->context) };
            LLVMTypeRef usleep_type = LLVMFunctionType(ctx->int_type, param_types, 1, false);
            usleep_fn = LLVMAddFunction(ctx->module, "usleep", usleep_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            if (arg) {
                LLVMValueRef multiplier = LLVMConstInt(ctx->int_type, 1000, false);
                LLVMValueRef microseconds = LLVMBuildMul(ctx->builder, arg, multiplier, "us");
                LLVMValueRef trunc = LLVMBuildTrunc(ctx->builder, microseconds, LLVMInt32TypeInContext(ctx->context), "us32");
                LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(usleep_fn), usleep_fn, &trunc, 1, "");
            }
        }
        safe_free(func_name);
        return LLVMConstInt(ctx->int_type, 0, false);
    } else if (strcmp(func_name, "rand") == 0) {
        LLVMValueRef rand_fn = LLVMGetNamedFunction(ctx->module, "rand");
        if (!rand_fn) {
            LLVMTypeRef rand_type = LLVMFunctionType(ctx->int_type, NULL, 0, false);
            rand_fn = LLVMAddFunction(ctx->module, "rand", rand_type);
        }
        LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(rand_fn), rand_fn, NULL, 0, "rand");
        safe_free(func_name);
        return result;
    } else if (strcmp(func_name, "time_now") == 0) {
        LLVMValueRef time_fn = LLVMGetNamedFunction(ctx->module, "wyn_time_now");
        if (!time_fn) {
            LLVMTypeRef time_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), NULL, 0, false);
            time_fn = LLVMAddFunction(ctx->module, "wyn_time_now", time_type);
        }
        LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(time_fn), time_fn, NULL, 0, "time_now");
        safe_free(func_name);
        return result;
    } else if (strcmp(func_name, "system") == 0) {
        LLVMValueRef system_fn = LLVMGetNamedFunction(ctx->module, "system");
        if (!system_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef system_type = LLVMFunctionType(ctx->int_type, &str_type, 1, false);
            system_fn = LLVMAddFunction(ctx->module, "system", system_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef cmd = codegen_expression(expr->args[0], ctx);
            LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(system_fn), system_fn, &cmd, 1, "system");
            safe_free(func_name);
            return result;
        }
    }
    
    // Handle string functions
    if (strcmp(func_name, "str_concat") == 0) {
        LLVMValueRef strcat_fn = LLVMGetNamedFunction(ctx->module, "strcat");
        if (!strcat_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type };
            LLVMTypeRef strcat_type = LLVMFunctionType(str_type, param_types, 2, false);
            strcat_fn = LLVMAddFunction(ctx->module, "strcat", strcat_type);
        }
        LLVMValueRef malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
        if (!malloc_fn) {
            LLVMTypeRef malloc_type = LLVMFunctionType(LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0), 
                                                       (LLVMTypeRef[]){ LLVMInt64TypeInContext(ctx->context) }, 1, false);
            malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);
        }
        LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
        if (!strlen_fn) {
            LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), 
                                                       (LLVMTypeRef[]){ LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) }, 1, false);
            strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
        }
        LLVMValueRef strcpy_fn = LLVMGetNamedFunction(ctx->module, "strcpy");
        if (!strcpy_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type };
            LLVMTypeRef strcpy_type = LLVMFunctionType(str_type, param_types, 2, false);
            strcpy_fn = LLVMAddFunction(ctx->module, "strcpy", strcpy_type);
        }
        if (expr->arg_count == 2) {
            LLVMValueRef s1 = codegen_expression(expr->args[0], ctx);
            LLVMValueRef s2 = codegen_expression(expr->args[1], ctx);
            LLVMValueRef len1 = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &s1, 1, "len1");
            LLVMValueRef len2 = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &s2, 1, "len2");
            LLVMValueRef total = LLVMBuildAdd(ctx->builder, len1, len2, "total");
            LLVMValueRef size = LLVMBuildAdd(ctx->builder, total, LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 1, false), "size");
            LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(malloc_fn), malloc_fn, &size, 1, "result");
            LLVMValueRef strcpy_args[2] = { result, s1 };
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strcpy_fn), strcpy_fn, strcpy_args, 2, "");
            LLVMValueRef strcat_args[2] = { result, s2 };
            LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strcat_fn), strcat_fn, strcat_args, 2, "");
            safe_free(func_name);
            return result;
        }
    } else if (strcmp(func_name, "str_contains") == 0) {
        LLVMValueRef strstr_fn = LLVMGetNamedFunction(ctx->module, "strstr");
        if (!strstr_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type };
            LLVMTypeRef strstr_type = LLVMFunctionType(str_type, param_types, 2, false);
            strstr_fn = LLVMAddFunction(ctx->module, "strstr", strstr_type);
        }
        if (expr->arg_count == 2) {
            LLVMValueRef haystack = codegen_expression(expr->args[0], ctx);
            LLVMValueRef needle = codegen_expression(expr->args[1], ctx);
            LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strstr_fn), strstr_fn, 
                                                (LLVMValueRef[]){ haystack, needle }, 2, "strstr_result");
            LLVMValueRef null_ptr = LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0));
            LLVMValueRef is_found = LLVMBuildICmp(ctx->builder, LLVMIntNE, result, null_ptr, "is_found");
            LLVMValueRef int_result = LLVMBuildZExt(ctx->builder, is_found, ctx->int_type, "contains");
            safe_free(func_name);
            return int_result;
        }
    } else if (strcmp(func_name, "str_upper") == 0 || strcmp(func_name, "str_lower") == 0) {
        bool is_upper = strcmp(func_name, "str_upper") == 0;
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, is_upper ? "wyn_string_upper" : "wyn_string_lower");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, (LLVMTypeRef[]){ str_type }, 1, false);
            fn = LLVMAddFunction(ctx->module, is_upper ? "wyn_string_upper" : "wyn_string_lower", fn_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef arg = codegen_expression(expr->args[0], ctx);
            LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, &arg, 1, "str_case");
            safe_free(func_name);
            return result;
        }
    }
    
    // Look up function
    // Handle qualified names: module::function -> function
    char* lookup_name = func_name;
    char runtime_name_buf[128];
    char* colon_pos = strstr(func_name, "::");
    if (colon_pos) {
        lookup_name = colon_pos + 2;  // Skip "::"
        
        // Map Wyn runtime function names to C runtime names
        if (strncmp(func_name, "System::", 8) == 0) {
            snprintf(runtime_name_buf, sizeof(runtime_name_buf), "wyn_system_%s", lookup_name);
            lookup_name = runtime_name_buf;
        } else if (strncmp(func_name, "File::", 6) == 0) {
            snprintf(runtime_name_buf, sizeof(runtime_name_buf), "wyn_file_%s", lookup_name);
            lookup_name = runtime_name_buf;
        } else if (strncmp(func_name, "Time::", 6) == 0) {
            snprintf(runtime_name_buf, sizeof(runtime_name_buf), "wyn_time_%s", lookup_name);
            lookup_name = runtime_name_buf;
        } else if (strncmp(func_name, "C_Parser::", 10) == 0) {
            snprintf(runtime_name_buf, sizeof(runtime_name_buf), "wyn_c_%s", lookup_name);
            lookup_name = runtime_name_buf;
        }
    }
    
    LLVMValueRef function = LLVMGetNamedFunction(ctx->module, lookup_name);
    if (!function) {
        fprintf(stderr, "Error: Undefined function '%s'\n", func_name);
        safe_free(func_name);
        return NULL;
    }
    
    // Generate arguments
    LLVMValueRef* args = NULL;
    if (expr->arg_count > 0) {
        args = safe_malloc(sizeof(LLVMValueRef) * expr->arg_count);
        if (!args) {
            safe_free(func_name);
            return NULL;
        }
        
        for (int i = 0; i < expr->arg_count; i++) {
            args[i] = codegen_expression(expr->args[i], ctx);
            if (!args[i]) {
                safe_free(args);
                safe_free(func_name);
                return NULL;
            }
        }
    }
    
    // Build call
    LLVMValueRef call = LLVMBuildCall2(ctx->builder, 
                                        LLVMGlobalGetValueType(function),
                                        function, 
                                        args, 
                                        expr->arg_count, 
                                        func_name);
    
    safe_free(args);
    safe_free(func_name);
    return call;
}

// Generate method call (object.method(args))
LLVMValueRef codegen_method_call(MethodCallExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) return NULL;
    
    // Get method name
    char method_name[256];
    int len = expr->method.length < 255 ? expr->method.length : 255;
    memcpy(method_name, expr->method.start, len);
    method_name[len] = '\0';
    
    // Generate object expression
    LLVMValueRef object = codegen_expression(expr->object, ctx);
    if (!object) return NULL;
    
    // Handle len/length - check object type to distinguish arrays from strings
    if (strcmp(method_name, "len") == 0 || strcmp(method_name, "length") == 0) {
        // Check if object is an array type
        bool is_array = false;
        if (expr->object && expr->object->expr_type) {
            is_array = (expr->object->expr_type->kind == TYPE_ARRAY);
        }
        
        if (is_array) {
            // Array - use get_array_length
            return get_array_length(object, ctx);
        } else {
            // String - use strlen
            LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
            if (!strlen_fn) {
                LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), 
                                                           (LLVMTypeRef[]){ LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) }, 1, false);
                strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
            }
            return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &object, 1, "strlen");
        }
    } else if (strcmp(method_name, "first") == 0) {
        // array.first() - return element at index 0
        LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, false);
        LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, ctx->int_type, object, &zero, 1, "first_ptr");
        return LLVMBuildLoad2(ctx->builder, ctx->int_type, element_ptr, "first");
    } else if (strcmp(method_name, "last") == 0) {
        // array.last() - return element at index len-1
        LLVMValueRef length = get_array_length(object, ctx);
        LLVMValueRef one = LLVMConstInt(ctx->int_type, 1, false);
        LLVMValueRef last_index = LLVMBuildSub(ctx->builder, length, one, "last_index");
        LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, ctx->int_type, object, &last_index, 1, "last_ptr");
        return LLVMBuildLoad2(ctx->builder, ctx->int_type, element_ptr, "last");
    }
    
    // Handle .await() method on Future
    if (strcmp(method_name, "await") == 0) {
        LLVMValueRef future_get_fn = LLVMGetNamedFunction(ctx->module, "future_get");
        if (!future_get_fn) {
            LLVMTypeRef void_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef params[] = { void_ptr };
            LLVMTypeRef type = LLVMFunctionType(void_ptr, params, 1, false);
            future_get_fn = LLVMAddFunction(ctx->module, "future_get", type);
        }
        
        LLVMValueRef result_ptr = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(future_get_fn), 
                                                  future_get_fn, &object, 1, "result_ptr");
        
        LLVMValueRef int_ptr = LLVMBuildBitCast(ctx->builder, result_ptr, 
                                                 LLVMPointerType(ctx->int_type, 0), "int_ptr");
        LLVMValueRef result = LLVMBuildLoad2(ctx->builder, ctx->int_type, int_ptr, "result");
        
        LLVMValueRef free_fn = LLVMGetNamedFunction(ctx->module, "free");
        if (!free_fn) {
            LLVMTypeRef void_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef free_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), &void_ptr, 1, false);
            free_fn = LLVMAddFunction(ctx->module, "free", free_type);
        }
        LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(free_fn), free_fn, &result_ptr, 1, "");
        
        return result;
    }
    
    // Handle string methods
    if (strcmp(method_name, "contains") == 0) {
        LLVMValueRef strstr_fn = LLVMGetNamedFunction(ctx->module, "strstr");
        if (!strstr_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type };
            LLVMTypeRef strstr_type = LLVMFunctionType(str_type, param_types, 2, false);
            strstr_fn = LLVMAddFunction(ctx->module, "strstr", strstr_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef needle = codegen_expression(expr->args[0], ctx);
            LLVMValueRef args[2] = { object, needle };
            LLVMValueRef result = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strstr_fn), strstr_fn, 
                                                args, 2, "strstr_result");
            LLVMValueRef null_ptr = LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0));
            LLVMValueRef is_found = LLVMBuildICmp(ctx->builder, LLVMIntNE, result, null_ptr, "is_found");
            return LLVMBuildZExt(ctx->builder, is_found, ctx->int_type, "contains");
        }
    } else if (strcmp(method_name, "upper") == 0) {
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "wyn_string_upper");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, (LLVMTypeRef[]){ str_type }, 1, false);
            fn = LLVMAddFunction(ctx->module, "wyn_string_upper", fn_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, &object, 1, "upper");
    } else if (strcmp(method_name, "lower") == 0) {
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "wyn_string_lower");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, (LLVMTypeRef[]){ str_type }, 1, false);
            fn = LLVMAddFunction(ctx->module, "wyn_string_lower", fn_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, &object, 1, "lower");
    } else if (strcmp(method_name, "substring") == 0 && expr->arg_count == 2) {
        // substring(start, end) - use C runtime
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "wyn_substring");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, ctx->int_type, ctx->int_type };
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, param_types, 3, false);
            fn = LLVMAddFunction(ctx->module, "wyn_substring", fn_type);
        }
        LLVMValueRef start = codegen_expression(expr->args[0], ctx);
        LLVMValueRef end = codegen_expression(expr->args[1], ctx);
        LLVMValueRef args[] = { object, start, end };
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, args, 3, "substring");
    } else if (strcmp(method_name, "trim") == 0) {
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "wyn_trim");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, (LLVMTypeRef[]){ str_type }, 1, false);
            fn = LLVMAddFunction(ctx->module, "wyn_trim", fn_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, &object, 1, "trim");
    } else if (strcmp(method_name, "replace") == 0 && expr->arg_count == 2) {
        LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "wyn_replace");
        if (!fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type, str_type };
            LLVMTypeRef fn_type = LLVMFunctionType(str_type, param_types, 3, false);
            fn = LLVMAddFunction(ctx->module, "wyn_replace", fn_type);
        }
        LLVMValueRef old_str = codegen_expression(expr->args[0], ctx);
        LLVMValueRef new_str = codegen_expression(expr->args[1], ctx);
        LLVMValueRef args[] = { object, old_str, new_str };
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, args, 3, "replace");
    } else if (strcmp(method_name, "starts_with") == 0) {
        LLVMValueRef strncmp_fn = LLVMGetNamedFunction(ctx->module, "strncmp");
        if (!strncmp_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type, LLVMInt64TypeInContext(ctx->context) };
            LLVMTypeRef strncmp_type = LLVMFunctionType(ctx->int_type, param_types, 3, false);
            strncmp_fn = LLVMAddFunction(ctx->module, "strncmp", strncmp_type);
        }
        LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
        if (!strlen_fn) {
            LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), 
                                                       (LLVMTypeRef[]){ LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) }, 1, false);
            strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef prefix = codegen_expression(expr->args[0], ctx);
            LLVMValueRef prefix_len = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &prefix, 1, "prefix_len");
            LLVMValueRef args[3] = { object, prefix, prefix_len };
            LLVMValueRef cmp = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strncmp_fn), strncmp_fn, 
                                             args, 3, "strncmp");
            LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, false);
            LLVMValueRef is_equal = LLVMBuildICmp(ctx->builder, LLVMIntEQ, cmp, zero, "is_equal");
            return LLVMBuildZExt(ctx->builder, is_equal, ctx->int_type, "starts_with");
        }
    } else if (strcmp(method_name, "ends_with") == 0) {
        LLVMValueRef strcmp_fn = LLVMGetNamedFunction(ctx->module, "strcmp");
        if (!strcmp_fn) {
            LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMTypeRef param_types[] = { str_type, str_type };
            LLVMTypeRef strcmp_type = LLVMFunctionType(ctx->int_type, param_types, 2, false);
            strcmp_fn = LLVMAddFunction(ctx->module, "strcmp", strcmp_type);
        }
        LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
        if (!strlen_fn) {
            LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), 
                                                       (LLVMTypeRef[]){ LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) }, 1, false);
            strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
        }
        if (expr->arg_count == 1) {
            LLVMValueRef suffix = codegen_expression(expr->args[0], ctx);
            LLVMValueRef str_len = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &object, 1, "str_len");
            LLVMValueRef suffix_len = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &suffix, 1, "suffix_len");
            LLVMValueRef offset = LLVMBuildSub(ctx->builder, str_len, suffix_len, "offset");
            LLVMValueRef str_end = LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->context), object, &offset, 1, "str_end");
            LLVMValueRef args[2] = { str_end, suffix };
            LLVMValueRef cmp = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strcmp_fn), strcmp_fn, 
                                             args, 2, "strcmp");
            LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, false);
            LLVMValueRef is_equal = LLVMBuildICmp(ctx->builder, LLVMIntEQ, cmp, zero, "is_equal");
            return LLVMBuildZExt(ctx->builder, is_equal, ctx->int_type, "ends_with");
        }
    } else if (strcmp(method_name, "abs") == 0) {
        // For int.abs() or float.abs()
        if (is_float_type(LLVMTypeOf(object))) {
            // Float abs: select(x < 0.0, -x, x)
            LLVMValueRef zero = LLVMConstReal(ctx->float_type, 0.0);
            LLVMValueRef is_neg = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, object, zero, "is_neg");
            LLVMValueRef neg_val = LLVMBuildFNeg(ctx->builder, object, "neg");
            return LLVMBuildSelect(ctx->builder, is_neg, neg_val, object, "abs");
        } else {
            // Int abs: select(x < 0, -x, x)
            LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, false);
            LLVMValueRef is_neg = LLVMBuildICmp(ctx->builder, LLVMIntSLT, object, zero, "is_neg");
            LLVMValueRef neg_val = LLVMBuildNeg(ctx->builder, object, "neg");
            return LLVMBuildSelect(ctx->builder, is_neg, neg_val, object, "abs");
        }
    } else if (strcmp(method_name, "floor") == 0 && is_float_type(LLVMTypeOf(object))) {
        // Float floor using LLVM intrinsic llvm.floor.f64
        LLVMTypeRef fn_type = LLVMFunctionType(ctx->float_type, (LLVMTypeRef[]){ctx->float_type}, 1, false);
        LLVMValueRef floor_fn = LLVMGetNamedFunction(ctx->module, "llvm.floor.f64");
        if (!floor_fn) {
            floor_fn = LLVMAddFunction(ctx->module, "llvm.floor.f64", fn_type);
        }
        LLVMValueRef args[] = { object };
        return LLVMBuildCall2(ctx->builder, fn_type, floor_fn, args, 1, "floor");
    } else if (strcmp(method_name, "ceil") == 0 && is_float_type(LLVMTypeOf(object))) {
        // Float ceil using LLVM intrinsic llvm.ceil.f64
        LLVMTypeRef fn_type = LLVMFunctionType(ctx->float_type, (LLVMTypeRef[]){ctx->float_type}, 1, false);
        LLVMValueRef ceil_fn = LLVMGetNamedFunction(ctx->module, "llvm.ceil.f64");
        if (!ceil_fn) {
            ceil_fn = LLVMAddFunction(ctx->module, "llvm.ceil.f64", fn_type);
        }
        LLVMValueRef args[] = { object };
        return LLVMBuildCall2(ctx->builder, fn_type, ceil_fn, args, 1, "ceil");
    } else if (strcmp(method_name, "round") == 0 && is_float_type(LLVMTypeOf(object))) {
        // Float round using LLVM intrinsic llvm.round.f64
        LLVMTypeRef fn_type = LLVMFunctionType(ctx->float_type, (LLVMTypeRef[]){ctx->float_type}, 1, false);
        LLVMValueRef round_fn = LLVMGetNamedFunction(ctx->module, "llvm.round.f64");
        if (!round_fn) {
            round_fn = LLVMAddFunction(ctx->module, "llvm.round.f64", fn_type);
        }
        LLVMValueRef args[] = { object };
        return LLVMBuildCall2(ctx->builder, fn_type, round_fn, args, 1, "round");
    } else if (strcmp(method_name, "min") == 0) {
        // For int.min(other)
        if (expr->arg_count == 1) {
            LLVMValueRef other = codegen_expression(expr->args[0], ctx);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, object, other, "cmp");
            return LLVMBuildSelect(ctx->builder, cmp, object, other, "min");
        }
    } else if (strcmp(method_name, "max") == 0) {
        // For int.max(other)
        if (expr->arg_count == 1) {
            LLVMValueRef other = codegen_expression(expr->args[0], ctx);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, object, other, "cmp");
            return LLVMBuildSelect(ctx->builder, cmp, object, other, "max");
        }
    } else if (strcmp(method_name, "to_string") == 0) {
        // For int.to_string() or bool.to_string()
        LLVMValueRef sprintf_fn = LLVMGetNamedFunction(ctx->module, "sprintf");
        if (!sprintf_fn) {
            LLVMTypeRef sprintf_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0), 
                                           LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
            LLVMTypeRef sprintf_type = LLVMFunctionType(ctx->int_type, sprintf_args, 2, true);
            sprintf_fn = LLVMAddFunction(ctx->module, "sprintf", sprintf_type);
        }
        LLVMValueRef malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
        if (!malloc_fn) {
            LLVMTypeRef malloc_type = LLVMFunctionType(LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0), 
                                                       (LLVMTypeRef[]){ LLVMInt64TypeInContext(ctx->context) }, 1, false);
            malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);
        }
        LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 32, false);
        LLVMValueRef buffer = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(malloc_fn), malloc_fn, &size, 1, "buffer");
        LLVMValueRef format = LLVMBuildGlobalStringPtr(ctx->builder, "%d", "fmt");
        LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(sprintf_fn), sprintf_fn, 
                      (LLVMValueRef[]){ buffer, format, object }, 3, "");
        return buffer;
    } else if (strcmp(method_name, "floor") == 0) {
        // float.floor() - use C floor function
        LLVMValueRef floor_fn = LLVMGetNamedFunction(ctx->module, "floor");
        if (!floor_fn) {
            LLVMTypeRef double_type = LLVMDoubleTypeInContext(ctx->context);
            LLVMTypeRef floor_type = LLVMFunctionType(double_type, (LLVMTypeRef[]){ double_type }, 1, false);
            floor_fn = LLVMAddFunction(ctx->module, "floor", floor_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(floor_fn), floor_fn, &object, 1, "floor");
    } else if (strcmp(method_name, "ceil") == 0) {
        // float.ceil() - use C ceil function
        LLVMValueRef ceil_fn = LLVMGetNamedFunction(ctx->module, "ceil");
        if (!ceil_fn) {
            LLVMTypeRef double_type = LLVMDoubleTypeInContext(ctx->context);
            LLVMTypeRef ceil_type = LLVMFunctionType(double_type, (LLVMTypeRef[]){ double_type }, 1, false);
            ceil_fn = LLVMAddFunction(ctx->module, "ceil", ceil_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(ceil_fn), ceil_fn, &object, 1, "ceil");
    } else if (strcmp(method_name, "round") == 0) {
        // float.round() - use C round function
        LLVMValueRef round_fn = LLVMGetNamedFunction(ctx->module, "round");
        if (!round_fn) {
            LLVMTypeRef double_type = LLVMDoubleTypeInContext(ctx->context);
            LLVMTypeRef round_type = LLVMFunctionType(double_type, (LLVMTypeRef[]){ double_type }, 1, false);
            round_fn = LLVMAddFunction(ctx->module, "round", round_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(round_fn), round_fn, &object, 1, "round");
    }
    
    return NULL;
}

// Generate array access (implemented in llvm_array_string_codegen.c)
LLVMValueRef codegen_array_access(IndexExpr* expr, LLVMCodegenContext* ctx) {
    return codegen_array_indexing(expr, ctx);
}

// Generate struct field access
LLVMValueRef codegen_field_access(FieldAccessExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) return NULL;
    
    // Get the struct value
    LLVMValueRef struct_val = codegen_expression(expr->object, ctx);
    if (!struct_val) return NULL;
    
    // Get struct type from the expression
    Type* struct_type = expr->object->expr_type;
    if (!struct_type || struct_type->kind != TYPE_STRUCT) {
        report_error(ERR_TYPE_MISMATCH, NULL, 0, 0, "Field access on non-struct type");
        return NULL;
    }
    
    // Get the LLVM type to find the struct name
    LLVMTypeRef llvm_type = LLVMTypeOf(struct_val);
    const char* type_name_cstr = LLVMGetStructName(llvm_type);
    
    if (!type_name_cstr) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Cannot determine struct type name");
        return NULL;
    }
    
    // Look up the struct definition to get field information
    if (!ctx->program) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "No program context");
        return NULL;
    }
    
    StructStmt* struct_def = NULL;
    for (int i = 0; i < ctx->program->count; i++) {
        Stmt* stmt = ctx->program->stmts[i];
        if (stmt->type == STMT_STRUCT) {
            Token name = stmt->struct_decl.name;
            // Compare with LLVM type name
            if (strlen(type_name_cstr) == (size_t)name.length &&
                memcmp(type_name_cstr, name.start, name.length) == 0) {
                struct_def = &stmt->struct_decl;
                break;
            }
        }
    }
    
    if (!struct_def) {
        report_error(ERR_TYPE_MISMATCH, NULL, 0, 0, "Struct definition not found");
        return NULL;
    }
    
    // Find field index in the struct definition
    int field_index = -1;
    for (int i = 0; i < struct_def->field_count; i++) {
        Token field_name = struct_def->fields[i];
        if (field_name.length == expr->field.length &&
            memcmp(field_name.start, expr->field.start, field_name.length) == 0) {
            field_index = i;
            break;
        }
    }
    
    if (field_index == -1) {
        report_error(ERR_TYPE_MISMATCH, NULL, 0, 0, "Field not found in struct");
        return NULL;
    }
    
    // Extract field value using extractvalue
    return LLVMBuildExtractValue(ctx->builder, struct_val, field_index, "field_val");
}

// Generate struct initialization
LLVMValueRef codegen_struct_init(StructInitExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx || !ctx->program) return NULL;
    
    // Find struct definition in program
    StructStmt* struct_def = NULL;
    for (int i = 0; i < ctx->program->count; i++) {
        Stmt* stmt = ctx->program->stmts[i];
        if (stmt->type == STMT_STRUCT) {
            Token name = stmt->struct_decl.name;
            if (name.length == expr->type_name.length &&
                memcmp(name.start, expr->type_name.start, name.length) == 0) {
                struct_def = &stmt->struct_decl;
                break;
            }
        }
    }
    
    if (!struct_def) {
        report_error(ERR_TYPE_MISMATCH, NULL, 0, 0, "Struct definition not found");
        return NULL;
    }
    
    // Build LLVM struct type directly from field types
    extern LLVMTypeRef get_llvm_type_from_wyn_type(Expr* type_expr, LLVMCodegenContext* ctx);
    
    LLVMTypeRef* field_types = malloc(sizeof(LLVMTypeRef) * struct_def->field_count);
    if (!field_types) {
        report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to allocate field types");
        return NULL;
    }
    
    for (int i = 0; i < struct_def->field_count; i++) {
        field_types[i] = get_llvm_type_from_wyn_type(struct_def->field_types[i], ctx);
        if (!field_types[i]) {
            free(field_types);
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to get LLVM type for struct field");
            return NULL;
        }
    }
    
    // Create LLVM struct type
    char struct_name[256];
    snprintf(struct_name, sizeof(struct_name), "%.*s", expr->type_name.length, expr->type_name.start);
    LLVMTypeRef llvm_struct_type = LLVMStructCreateNamed(ctx->context, struct_name);
    LLVMStructSetBody(llvm_struct_type, field_types, struct_def->field_count, false);
    
    free(field_types);
    
    if (!llvm_struct_type) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to create LLVM struct type");
        return NULL;
    }
    
    // Create undef struct value
    LLVMValueRef struct_val = LLVMGetUndef(llvm_struct_type);
    
    // Insert each field value
    for (int i = 0; i < expr->field_count; i++) {
        LLVMValueRef field_val = codegen_expression(expr->field_values[i], ctx);
        if (!field_val) {
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to codegen struct field value");
            return NULL;
        }
        
        // Find the field index in the struct type
        Token field_name = expr->field_names[i];
        int field_index = -1;
        for (int j = 0; j < struct_def->field_count; j++) {
            Token def_field_name = struct_def->fields[j];
            if (def_field_name.length == field_name.length &&
                memcmp(def_field_name.start, field_name.start, field_name.length) == 0) {
                field_index = j;
                break;
            }
        }
        
        if (field_index == -1) {
            report_error(ERR_TYPE_MISMATCH, NULL, 0, 0, "Field not found in struct type");
            return NULL;
        }
        
        struct_val = LLVMBuildInsertValue(ctx->builder, struct_val, field_val, field_index, "struct_val");
    }
    
    return struct_val;
}

// Generate match expression
LLVMValueRef codegen_match_expr(MatchExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx || expr->arm_count == 0) return NULL;
    
    // Evaluate the match value
    LLVMValueRef match_val = codegen_expression(expr->value, ctx);
    if (!match_val) return NULL;
    
    // Create blocks for each arm and the merge block
    LLVMBasicBlockRef* arm_blocks = malloc(sizeof(LLVMBasicBlockRef) * expr->arm_count);
    LLVMBasicBlockRef* next_blocks = malloc(sizeof(LLVMBasicBlockRef) * expr->arm_count);
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(ctx->current_function, "match.end");
    
    // Create all blocks
    for (int i = 0; i < expr->arm_count; i++) {
        arm_blocks[i] = LLVMAppendBasicBlock(ctx->current_function, "match.arm");
        next_blocks[i] = (i < expr->arm_count - 1) ? 
            LLVMAppendBasicBlock(ctx->current_function, "match.next") : merge_block;
    }
    
    // Allocate result variable
    LLVMTypeRef result_type = ctx->int_type;  // Default to int
    LLVMValueRef result_ptr = LLVMBuildAlloca(ctx->builder, result_type, "match.result");
    
    // Generate code for each arm
    for (int i = 0; i < expr->arm_count; i++) {
        Pattern* pat = expr->arms[i].pattern;
        
        // Check pattern
        LLVMValueRef cond = NULL;
        if (pat->type == PATTERN_WILDCARD) {
            // Wildcard always matches - unconditional branch
            LLVMBuildBr(ctx->builder, arm_blocks[i]);
        } else if (pat->type == PATTERN_OR) {
            // Or pattern: match any of the sub-patterns
            LLVMValueRef or_cond = NULL;
            for (int j = 0; j < pat->or_pat.pattern_count; j++) {
                Pattern* sub_pat = pat->or_pat.patterns[j];
                if (sub_pat->type == PATTERN_LITERAL) {
                    Token lit_token = sub_pat->literal.value;
                    char* lit_str = malloc(lit_token.length + 1);
                    memcpy(lit_str, lit_token.start, lit_token.length);
                    lit_str[lit_token.length] = '\0';
                    int lit_int = atoi(lit_str);
                    free(lit_str);
                    
                    LLVMValueRef lit_val = LLVMConstInt(ctx->int_type, lit_int, false);
                    LLVMValueRef sub_cond = LLVMBuildICmp(ctx->builder, LLVMIntEQ, match_val, lit_val, "or.cmp");
                    
                    if (or_cond) {
                        or_cond = LLVMBuildOr(ctx->builder, or_cond, sub_cond, "or.result");
                    } else {
                        or_cond = sub_cond;
                    }
                }
            }
            if (or_cond) {
                LLVMBuildCondBr(ctx->builder, or_cond, arm_blocks[i], next_blocks[i]);
            }
        } else if (pat->type == PATTERN_GUARD) {
            // Guard pattern: check base pattern then guard condition
            Pattern* base_pat = pat->guard.pattern;
            
            if (base_pat->type == PATTERN_IDENT) {
                // Create variable binding
                Token name_token = base_pat->ident.name;
                char* var_name = malloc(name_token.length + 1);
                memcpy(var_name, name_token.start, name_token.length);
                var_name[name_token.length] = '\0';
                
                // Store match value in variable
                LLVMValueRef var_ptr = LLVMBuildAlloca(ctx->builder, ctx->int_type, var_name);
                LLVMBuildStore(ctx->builder, match_val, var_ptr);
                
                // Add to symbol table
                symbol_table_insert_typed(ctx->symbol_table, var_name, var_ptr, ctx->int_type);
                
                // Evaluate guard condition
                LLVMValueRef guard_cond = codegen_expression(pat->guard.guard, ctx);
                LLVMBuildCondBr(ctx->builder, guard_cond, arm_blocks[i], next_blocks[i]);
                
                free(var_name);
            } else if (base_pat->type == PATTERN_STRUCT) {
                // Struct pattern with guard - bind fields then check guard
                for (int j = 0; j < base_pat->struct_pat.field_count; j++) {
                    Token field_name = base_pat->struct_pat.field_names[j];
                    char* name = malloc(field_name.length + 1);
                    memcpy(name, field_name.start, field_name.length);
                    name[field_name.length] = '\0';
                    
                    // Extract field value
                    LLVMValueRef field_val = LLVMBuildExtractValue(ctx->builder, match_val, j, name);
                    
                    // Allocate and store
                    LLVMValueRef var_ptr = LLVMBuildAlloca(ctx->builder, ctx->int_type, name);
                    LLVMBuildStore(ctx->builder, field_val, var_ptr);
                    symbol_table_insert_typed(ctx->symbol_table, name, var_ptr, ctx->int_type);
                    
                    free(name);
                }
                
                // Evaluate guard condition
                LLVMValueRef guard_cond = codegen_expression(pat->guard.guard, ctx);
                LLVMBuildCondBr(ctx->builder, guard_cond, arm_blocks[i], next_blocks[i]);
            } else {
                // Unsupported base pattern for guards
                LLVMBuildBr(ctx->builder, next_blocks[i]);
            }
        } else if (pat->type == PATTERN_RANGE) {
            // Range pattern: check if value is in range
            LLVMValueRef start_val = codegen_expression(pat->range.start, ctx);
            LLVMValueRef end_val = codegen_expression(pat->range.end, ctx);
            
            // Check: match_val >= start && match_val < end
            LLVMValueRef ge_cond = LLVMBuildICmp(ctx->builder, LLVMIntSGE, match_val, start_val, "range.ge");
            LLVMValueRef lt_cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, match_val, end_val, "range.lt");
            LLVMValueRef range_cond = LLVMBuildAnd(ctx->builder, ge_cond, lt_cond, "range.check");
            
            LLVMBuildCondBr(ctx->builder, range_cond, arm_blocks[i], next_blocks[i]);
        } else if (pat->type == PATTERN_LITERAL) {
            // Compare with literal value - parse the token
            Token lit_token = pat->literal.value;
            // Convert token to int (simple case)
            char* lit_str = malloc(lit_token.length + 1);
            memcpy(lit_str, lit_token.start, lit_token.length);
            lit_str[lit_token.length] = '\0';
            int lit_int = atoi(lit_str);
            free(lit_str);
            
            LLVMValueRef lit_val = LLVMConstInt(ctx->int_type, lit_int, false);
            cond = LLVMBuildICmp(ctx->builder, LLVMIntEQ, match_val, lit_val, "match.cmp");
            LLVMBuildCondBr(ctx->builder, cond, arm_blocks[i], next_blocks[i]);
        } else if (pat->type == PATTERN_IDENT) {
            // Check if it's an enum variant (global constant) or variable binding
            Token name_token = pat->ident.name;
            char* var_name = malloc(name_token.length + 1);
            memcpy(var_name, name_token.start, name_token.length);
            var_name[name_token.length] = '\0';
            
            // Try to find as global constant
            LLVMValueRef global_const = LLVMGetNamedGlobal(ctx->module, var_name);
            if (global_const && LLVMIsGlobalConstant(global_const)) {
                // It's an enum variant - load and compare
                LLVMValueRef enum_val = LLVMBuildLoad2(ctx->builder, ctx->int_type, global_const, var_name);
                cond = LLVMBuildICmp(ctx->builder, LLVMIntEQ, match_val, enum_val, "match.cmp");
                LLVMBuildCondBr(ctx->builder, cond, arm_blocks[i], next_blocks[i]);
            } else {
                // Variable binding - always matches
                LLVMBuildBr(ctx->builder, arm_blocks[i]);
            }
            
            free(var_name);
        } else if (pat->type == PATTERN_STRUCT) {
            // Struct pattern - always matches (type checking done earlier)
            LLVMBuildBr(ctx->builder, arm_blocks[i]);
        } else {
            // Unknown pattern - skip
            LLVMBuildBr(ctx->builder, next_blocks[i]);
        }
        
        // Generate arm body
        LLVMPositionBuilderAtEnd(ctx->builder, arm_blocks[i]);
        
        // Bind pattern variables for struct destructuring
        Pattern* arm_pat = expr->arms[i].pattern;
        if (arm_pat->type == PATTERN_STRUCT) {
            // Extract struct fields and bind to variables
            for (int j = 0; j < arm_pat->struct_pat.field_count; j++) {
                Token field_name = arm_pat->struct_pat.field_names[j];
                char* name = malloc(field_name.length + 1);
                memcpy(name, field_name.start, field_name.length);
                name[field_name.length] = '\0';
                
                // Extract field value
                LLVMValueRef field_val = LLVMBuildExtractValue(ctx->builder, match_val, j, name);
                
                // Allocate and store
                LLVMValueRef var_ptr = LLVMBuildAlloca(ctx->builder, ctx->int_type, name);
                LLVMBuildStore(ctx->builder, field_val, var_ptr);
                symbol_table_insert_typed(ctx->symbol_table, name, var_ptr, ctx->int_type);
                
                free(name);
            }
        }
        
        LLVMValueRef arm_result = codegen_expression(expr->arms[i].result, ctx);
        if (arm_result) {
            LLVMBuildStore(ctx->builder, arm_result, result_ptr);
        }
        LLVMBuildBr(ctx->builder, merge_block);
        
        // Position for next comparison
        if (i < expr->arm_count - 1) {
            LLVMPositionBuilderAtEnd(ctx->builder, next_blocks[i]);
        }
    }
    
    // Merge block
    LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
    LLVMValueRef result = LLVMBuildLoad2(ctx->builder, result_type, result_ptr, "match.value");
    
    free(arm_blocks);
    free(next_blocks);
    
    return result;
}

// Type conversion utilities
LLVMValueRef convert_to_type(LLVMValueRef value, LLVMTypeRef target_type, LLVMCodegenContext* ctx) {
    if (!value || !target_type || !ctx) {
        return NULL;
    }
    
    LLVMTypeRef source_type = LLVMTypeOf(value);
    
    // If types are the same, no conversion needed
    if (source_type == target_type) {
        return value;
    }
    
    // Handle common conversions
    if (is_integer_type(source_type) && is_float_type(target_type)) {
        return LLVMBuildSIToFP(ctx->builder, value, target_type, "itof");
    } else if (is_float_type(source_type) && is_integer_type(target_type)) {
        return LLVMBuildFPToSI(ctx->builder, value, target_type, "ftoi");
    }
    
    return value;  // No conversion available
}

// Type checking utilities
bool is_numeric_type(LLVMTypeRef type) {
    return is_integer_type(type) || is_float_type(type);
}

bool is_integer_type(LLVMTypeRef type) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind != LLVMIntegerTypeKind) {
        return false;
    }
    
    // Check if it's a boolean (1-bit integer) - not considered numeric
    unsigned width = LLVMGetIntTypeWidth(type);
    return width > 1;  // Only integers wider than 1 bit are considered numeric
}

bool is_float_type(LLVMTypeRef type) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    return kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind;
}

// Generate code for spawn expression (returns Future*)
LLVMValueRef codegen_spawn_expr(SpawnExpr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx || !expr->call) return NULL;
    
    Expr* call_expr = expr->call;
    if (call_expr->type != EXPR_CALL || call_expr->call.callee->type != EXPR_IDENT) return NULL;
    
    // Extract function name
    const char* func_name_start = call_expr->call.callee->token.start;
    int func_name_len = call_expr->call.callee->token.length;
    char func_name[256];
    snprintf(func_name, sizeof(func_name), "%.*s", func_name_len, func_name_start);
    
    LLVMValueRef target_fn = LLVMGetNamedFunction(ctx->module, func_name);
    if (!target_fn) return NULL;
    
    // Check if function returns a value
    LLVMTypeRef return_type = LLVMGetReturnType(LLVMGlobalGetValueType(target_fn));
    bool has_return = LLVMGetTypeKind(return_type) != LLVMVoidTypeKind;
    
    if (!has_return) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Cannot capture Future from void function");
        return NULL;
    }
    
    // Get/declare wyn_spawn_async
    LLVMValueRef spawn_fn = LLVMGetNamedFunction(ctx->module, "wyn_spawn_async");
    if (!spawn_fn) {
        LLVMTypeRef void_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
        LLVMTypeRef params[] = { void_ptr, void_ptr };
        LLVMTypeRef type = LLVMFunctionType(void_ptr, params, 2, false);
        spawn_fn = LLVMAddFunction(ctx->module, "wyn_spawn_async", type);
    }
    
    // Create wrapper
    char wrapper_name[256];
    snprintf(wrapper_name, sizeof(wrapper_name), "__spawn_%s_%d", func_name, ctx->spawn_counter++);
    
    LLVMTypeRef void_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef wrapper_type = LLVMFunctionType(void_ptr, &void_ptr, 1, false);
    LLVMValueRef wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx->context);
    LLVMPositionBuilderAtEnd(builder, entry);
    
    // Codegen arguments
    int arg_count = call_expr->call.arg_count;
    LLVMValueRef* call_args = NULL;
    if (arg_count > 0) {
        call_args = malloc(sizeof(LLVMValueRef) * arg_count);
        LLVMBuilderRef saved_builder = ctx->builder;
        ctx->builder = builder;
        
        for (int i = 0; i < arg_count; i++) {
            call_args[i] = codegen_expression(call_expr->call.args[i], ctx);
        }
        
        ctx->builder = saved_builder;
    }
    
    LLVMValueRef result = LLVMBuildCall2(builder, LLVMGlobalGetValueType(target_fn), target_fn, call_args, arg_count, "");
    
    if (call_args) free(call_args);
    
    // Allocate space for result and return pointer
    LLVMTypeRef result_type = LLVMTypeOf(result);
    LLVMValueRef result_alloc = LLVMBuildMalloc(builder, result_type, "result_alloc");
    LLVMBuildStore(builder, result, result_alloc);
    LLVMValueRef result_ptr = LLVMBuildBitCast(builder, result_alloc, void_ptr, "");
    LLVMBuildRet(builder, result_ptr);
    
    LLVMDisposeBuilder(builder);
    
    // Call spawn function and return Future*
    LLVMValueRef args[] = {
        LLVMBuildBitCast(ctx->builder, wrapper_fn, void_ptr, ""),
        LLVMConstNull(void_ptr)
    };
    
    return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(spawn_fn), spawn_fn, args, 2, "future");
}

#endif // WITH_LLVM
