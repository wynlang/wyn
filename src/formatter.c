#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

// Basic formatter - formats Wyn code
void format_stmt(Stmt* stmt, int indent);
void format_expr(Expr* expr);

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("    ");
    }
}

void format_expr(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_BOOL:
        case EXPR_IDENT:
            printf("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_STRING:
            printf("\"%.*s\"", expr->token.length - 2, expr->token.start + 1);
            break;
        case EXPR_BINARY:
            format_expr(expr->binary.left);
            printf(" %.*s ", expr->binary.op.length, expr->binary.op.start);
            format_expr(expr->binary.right);
            break;
        case EXPR_UNARY:
            printf("%.*s", expr->unary.op.length, expr->unary.op.start);
            format_expr(expr->unary.operand);
            break;
        default:
            printf("...");
            break;
    }
}

void format_stmt(Stmt* stmt, int indent) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_VAR:
            print_indent(indent);
            printf("let %.*s = ", stmt->var.name.length, stmt->var.name.start);
            format_expr(stmt->var.init);
            printf(";\n");
            break;
        case STMT_RETURN:
            print_indent(indent);
            printf("return ");
            format_expr(stmt->return_stmt.value);
            printf(";\n");
            break;
        case STMT_FN:
            print_indent(indent);
            printf("fn %.*s() -> int {\n", stmt->fn.name.length, stmt->fn.name.start);
            if (stmt->fn.body) {
                for (int i = 0; i < stmt->fn.body->block.count; i++) {
                    format_stmt(stmt->fn.body->block.stmts[i], indent + 1);
                }
            }
            print_indent(indent);
            printf("}\n");
            break;
        default:
            break;
    }
}

int wyn_format_file(const char* filename) {
    // Basic implementation - just pretty print
    printf("// Formatted: %s\n", filename);
    return 0;
}
