#include "llvm_statement_codegen.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safe_memory.h"
#include "error.h"
#include "type_mapping.h"
#include "llvm_expression_codegen.h"
#include "llvm_function_codegen.h"

// Main statement code generation dispatcher
void codegen_statement(Stmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    switch (stmt->type) {
        case STMT_EXPR:
            codegen_expression_statement(stmt->expr, ctx);
            break;
        case STMT_VAR:
            codegen_var_declaration(&stmt->var, ctx);
            break;
        case STMT_IF:
            codegen_if_statement(&stmt->if_stmt, ctx);
            break;
        case STMT_WHILE:
            codegen_while_statement(&stmt->while_stmt, ctx);
            break;
        case STMT_FOR:
            codegen_for_statement(&stmt->for_stmt, ctx);
            break;
        case STMT_RETURN:
            codegen_return_statement(&stmt->ret, ctx);
            break;
        case STMT_BLOCK:
            codegen_block_statement(&stmt->block, ctx);
            break;
        case STMT_FN:
            codegen_function_definition(&stmt->fn, ctx);
            break;
        default:
            report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Unsupported statement type for code generation");
            break;
    }
}

// Generate code for if statements
void codegen_if_statement(IfStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    // Generate condition
    LLVMValueRef condition = codegen_expression(stmt->condition, ctx);
    if (!condition) {
        return;
    }
    
    // Get current function
    LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    
    // Create basic blocks
    LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(function, "if.then");
    LLVMBasicBlockRef else_block = stmt->else_branch ? LLVMAppendBasicBlock(function, "if.else") : NULL;
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(function, "if.end");
    
    // Branch based on condition
    if (else_block) {
        LLVMBuildCondBr(ctx->builder, condition, then_block, else_block);
    } else {
        LLVMBuildCondBr(ctx->builder, condition, then_block, merge_block);
    }
    
    // Generate then block
    LLVMPositionBuilderAtEnd(ctx->builder, then_block);
    codegen_statement(stmt->then_branch, ctx);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, merge_block);
    }
    
    // Generate else block if present
    if (else_block) {
        LLVMPositionBuilderAtEnd(ctx->builder, else_block);
        codegen_statement(stmt->else_branch, ctx);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
            LLVMBuildBr(ctx->builder, merge_block);
        }
    }
    
    // Continue with merge block
    LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
}

// Generate code for while statements
void codegen_while_statement(WhileStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    // Get current function
    LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    
    // Create basic blocks
    LLVMBasicBlockRef loop_header = LLVMAppendBasicBlock(function, "while.header");
    LLVMBasicBlockRef loop_body = LLVMAppendBasicBlock(function, "while.body");
    LLVMBasicBlockRef loop_end = LLVMAppendBasicBlock(function, "while.end");
    
    // Jump to loop header
    LLVMBuildBr(ctx->builder, loop_header);
    
    // Generate loop header (condition check)
    LLVMPositionBuilderAtEnd(ctx->builder, loop_header);
    LLVMValueRef condition = codegen_expression(stmt->condition, ctx);
    if (!condition) {
        return;
    }
    LLVMBuildCondBr(ctx->builder, condition, loop_body, loop_end);
    
    // Generate loop body
    LLVMPositionBuilderAtEnd(ctx->builder, loop_body);
    codegen_statement(stmt->body, ctx);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, loop_header);
    }
    
    // Continue with loop end
    LLVMPositionBuilderAtEnd(ctx->builder, loop_end);
}

// Generate code for for statements (placeholder)
void codegen_for_statement(ForStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    // For now, create a simple placeholder
    // In a full implementation, this would handle for loop initialization, condition, and increment
    report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "For statements not yet implemented");
}

// Generate code for variable declarations
void codegen_var_declaration(VarStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    // Get variable name from token
    char* var_name = safe_malloc(stmt->name.length + 1);
    if (!var_name) {
        return;
    }
    strncpy(var_name, stmt->name.start, stmt->name.length);
    var_name[stmt->name.length] = '\0';
    
    // Determine variable type (for now, use int as default)
    LLVMTypeRef var_type = ctx->int_type;
    
    // Create local variable (alloca)
    LLVMValueRef var_alloca = create_local_variable(var_name, var_type, ctx);
    
    // Generate initialization if present
    if (stmt->init) {
        LLVMValueRef init_value = codegen_expression(stmt->init, ctx);
        if (init_value) {
            LLVMBuildStore(ctx->builder, init_value, var_alloca);
        }
    }
    
    safe_free(var_name);
}

// Create a local variable (alloca instruction)
LLVMValueRef create_local_variable(const char* name, LLVMTypeRef type, LLVMCodegenContext* ctx) {
    if (!name || !type || !ctx) {
        return NULL;
    }
    
    // Create alloca at the beginning of the function
    LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(function);
    
    // Save current position
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->builder);
    
    // Position at the beginning of entry block
    LLVMValueRef first_instr = LLVMGetFirstInstruction(entry_block);
    if (first_instr) {
        LLVMPositionBuilderBefore(ctx->builder, first_instr);
    } else {
        LLVMPositionBuilderAtEnd(ctx->builder, entry_block);
    }
    
    // Create alloca
    LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, type, name);
    
    // Restore position
    LLVMPositionBuilderAtEnd(ctx->builder, current_block);
    
    return alloca;
}

// Generate code for return statements
void codegen_return_statement(ReturnStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    if (stmt->value) {
        // Return with value
        LLVMValueRef return_value = codegen_expression(stmt->value, ctx);
        if (return_value) {
            LLVMBuildRet(ctx->builder, return_value);
        }
    } else {
        // Return void
        LLVMBuildRetVoid(ctx->builder);
    }
}

// Generate code for block statements
void codegen_block_statement(BlockStmt* stmt, LLVMCodegenContext* ctx) {
    if (!stmt || !ctx) {
        return;
    }
    
    // Enter new scope
    enter_scope(ctx);
    
    // Generate code for each statement in the block
    for (int i = 0; i < stmt->count; i++) {
        if (stmt->stmts[i]) {
            codegen_statement(stmt->stmts[i], ctx);
        }
    }
    
    // Exit scope
    exit_scope(ctx);
}

// Generate code for expression statements
void codegen_expression_statement(Expr* expr, LLVMCodegenContext* ctx) {
    if (!expr || !ctx) {
        return;
    }
    
    // Generate the expression (result is discarded)
    codegen_expression(expr, ctx);
}

// Enter a new scope (placeholder for symbol table)
void enter_scope(LLVMCodegenContext* ctx) {
    if (!ctx) {
        return;
    }
    
    // For now, this is a placeholder
    // In a full implementation, this would push a new scope onto a symbol table stack
}

// Exit the current scope (placeholder for symbol table)
void exit_scope(LLVMCodegenContext* ctx) {
    if (!ctx) {
        return;
    }
    
    // For now, this is a placeholder
    // In a full implementation, this would pop the current scope from a symbol table stack
}

#endif // WITH_LLVM
