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
    
    // Look up variable in symbol table
    LLVMValueRef var_ptr = symbol_table_lookup(ctx->symbol_table, var_name);
    if (!var_ptr) {
        fprintf(stderr, "Error: Undefined variable '%s'\n", var_name);
        safe_free(var_name);
        return NULL;
    }
    
    // Load the value from the pointer
    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, LLVMInt32Type(), var_ptr, var_name);
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
