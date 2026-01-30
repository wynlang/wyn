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
        case EXPR_INDEX:
            result = codegen_array_indexing(&expr->index, ctx);
            break;
        case EXPR_ARRAY:
            result = codegen_array_literal(&expr->array, ctx);
            break;
        case EXPR_METHOD_CALL:
            result = codegen_method_call(&expr->method_call, ctx);
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
                return LLVMBuildFNeg(ctx->builder, operand, "fneg");
            } else {
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
    LLVMValueRef function = LLVMGetNamedFunction(ctx->module, func_name);
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
    } else if (strcmp(method_name, "len") == 0 || strcmp(method_name, "length") == 0) {
        LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
        if (!strlen_fn) {
            LLVMTypeRef strlen_type = LLVMFunctionType(LLVMInt64TypeInContext(ctx->context), 
                                                       (LLVMTypeRef[]){ LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) }, 1, false);
            strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);
        }
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, &object, 1, "strlen");
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
        // For int.abs()
        LLVMValueRef zero = LLVMConstInt(ctx->int_type, 0, false);
        LLVMValueRef is_neg = LLVMBuildICmp(ctx->builder, LLVMIntSLT, object, zero, "is_neg");
        LLVMValueRef neg_val = LLVMBuildNeg(ctx->builder, object, "neg");
        return LLVMBuildSelect(ctx->builder, is_neg, neg_val, object, "abs");
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
    }
    
    return NULL;
}

// Generate array access (implemented in llvm_array_string_codegen.c)
LLVMValueRef codegen_array_access(IndexExpr* expr, LLVMCodegenContext* ctx) {
    return codegen_array_indexing(expr, ctx);
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

#endif // WITH_LLVM
