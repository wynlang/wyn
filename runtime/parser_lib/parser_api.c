// Wyn C Parser API
// Exposes the C parser to Wyn programs via C_Parser:: module

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../src/common.h"
#include "../../src/ast.h"

// External functions from lexer.c and parser.c
extern void init_lexer(const char* source);
extern void init_parser(void);
extern Program* parse_program(void);
extern bool parser_had_error(void);

// Global state
static Program* g_program = NULL;
static int g_error_count = 0;
static char* g_error_messages[10] = {0};
static char* g_ast_buffer = NULL;
static size_t g_ast_buffer_size = 0;
static size_t g_ast_buffer_pos = 0;

int wyn_c_init_lexer(const char* source) {
    if (!source) return 0;
    g_error_count = 0;
    for (int i = 0; i < 10; i++) {
        if (g_error_messages[i]) { free(g_error_messages[i]); g_error_messages[i] = NULL; }
    }
    init_lexer(source);
    return 1;
}

void wyn_c_init_parser(void) {
    g_program = NULL;
    init_parser();
}

int64_t wyn_c_parse_program(void) {
    g_program = parse_program();
    if (!g_program) return 0;
    if (parser_had_error()) {
        g_error_count = 1;
        if (!g_error_messages[0]) g_error_messages[0] = strdup("Parse error");
    }
    return (int64_t)(intptr_t)g_program;
}

int wyn_c_get_error_count(void) { return g_error_count; }

char* wyn_c_get_error_message(int idx) {
    if (idx < 0 || idx >= g_error_count || idx >= 10) return "";
    return g_error_messages[idx] ? g_error_messages[idx] : "";
}

// Buffer helpers
static void buf_init(void) {
    if (g_ast_buffer) free(g_ast_buffer);
    g_ast_buffer_size = 4096;
    g_ast_buffer = malloc(g_ast_buffer_size);
    g_ast_buffer_pos = 0;
    if (g_ast_buffer) g_ast_buffer[0] = '\0';
}

static void buf_grow(size_t need) {
    while (g_ast_buffer_pos + need + 1 > g_ast_buffer_size) {
        g_ast_buffer_size *= 2;
        g_ast_buffer = realloc(g_ast_buffer, g_ast_buffer_size);
    }
}

static void buf_str(const char* s) {
    if (!g_ast_buffer || !s) return;
    size_t len = strlen(s);
    buf_grow(len);
    if (g_ast_buffer) { strcpy(g_ast_buffer + g_ast_buffer_pos, s); g_ast_buffer_pos += len; }
}

static void buf_tok(const char* s, int len) {
    if (!s || len <= 0 || !g_ast_buffer) return;
    buf_grow((size_t)len);
    memcpy(g_ast_buffer + g_ast_buffer_pos, s, len);
    g_ast_buffer_pos += len;
    g_ast_buffer[g_ast_buffer_pos] = '\0';
}

static void ser_expr(Expr* e);
static void ser_stmt(Stmt* s);

static void ser_expr(Expr* e) {
    if (!e) { buf_str("null"); return; }
    buf_str("(");
    switch (e->type) {
        case EXPR_INT: buf_str("Int "); buf_tok(e->token.start, e->token.length); break;
        case EXPR_FLOAT: buf_str("Float "); buf_tok(e->token.start, e->token.length); break;
        case EXPR_STRING: buf_str("String "); buf_tok(e->token.start, e->token.length); break;
        case EXPR_IDENT: buf_str("Ident "); buf_tok(e->token.start, e->token.length); break;
        case EXPR_BOOL: buf_str("Bool "); buf_tok(e->token.start, e->token.length); break;
        case EXPR_BINARY:
            buf_str("Binary "); ser_expr(e->binary.left);
            buf_str(" "); buf_tok(e->binary.op.start, e->binary.op.length);
            buf_str(" "); ser_expr(e->binary.right);
            break;
        case EXPR_UNARY:
            buf_str("Unary "); buf_tok(e->unary.op.start, e->unary.op.length);
            buf_str(" "); ser_expr(e->unary.operand);
            break;
        case EXPR_CALL:
            buf_str("Call "); ser_expr(e->call.callee); buf_str("(");
            for (int i = 0; i < e->call.arg_count; i++) {
                if (i > 0) buf_str(" ");
                ser_expr(e->call.args[i]);
            }
            buf_str(")");
            break;
        case EXPR_ASSIGN:
            buf_str("Binary (Ident "); buf_tok(e->assign.name.start, e->assign.name.length);
            buf_str(") = "); ser_expr(e->assign.value);
            break;
        default: buf_str("Expr"); break;
    }
    buf_str(")");
}

static void ser_stmt(Stmt* s) {
    if (!s) { buf_str("null"); return; }
    buf_str("(");
    switch (s->type) {
        case STMT_EXPR: buf_str("ExprStmt "); ser_expr(s->expr); break;
        case STMT_VAR:
            buf_str("Var ");
            if (s->var.name.start && s->var.name.length > 0)
                buf_tok(s->var.name.start, s->var.name.length);
            if (s->var.init) { buf_str("="); ser_expr(s->var.init); }
            break;
        case STMT_RETURN:
            buf_str("Return");
            if (s->ret.value) { buf_str(" "); ser_expr(s->ret.value); }
            break;
        case STMT_FN:
            buf_str("Fn ");
            if (s->fn.name.start && s->fn.name.length > 0)
                buf_tok(s->fn.name.start, s->fn.name.length);
            // Serialize parameters
            if (s->fn.param_count > 0) {
                buf_str(" (Params");
                for (int i = 0; i < s->fn.param_count; i++) {
                    buf_str(" (Param ");
                    buf_tok(s->fn.params[i].start, s->fn.params[i].length);
                    buf_str(")");
                }
                buf_str(")");
            }
            if (s->fn.body) { buf_str(" "); ser_stmt(s->fn.body); }
            break;
        case STMT_BLOCK:
            buf_str("Block");
            for (int i = 0; i < s->block.count; i++) { buf_str(" "); ser_stmt(s->block.stmts[i]); }
            break;
        case STMT_IF:
            buf_str("If "); ser_expr(s->if_stmt.condition);
            buf_str(" "); ser_stmt(s->if_stmt.then_branch);
            if (s->if_stmt.else_branch) { buf_str(" else "); ser_stmt(s->if_stmt.else_branch); }
            break;
        case STMT_WHILE:
            buf_str("While "); ser_expr(s->while_stmt.condition);
            buf_str(" "); ser_stmt(s->while_stmt.body);
            break;
        default: buf_str("Stmt"); break;
    }
    buf_str(")");
}

char* wyn_c_ast_to_string(int64_t ast_ptr) {
    Program* p = (Program*)(intptr_t)ast_ptr;
    if (!p) return "";
    buf_init();
    if (!g_ast_buffer) return "";
    buf_str("(Program");
    for (int i = 0; i < p->count; i++) { 
        buf_str(" "); 
        ser_stmt(p->stmts[i]); 
    }
    buf_str(")");
    return g_ast_buffer;
}

void wyn_c_free_ast(int64_t ast_ptr) {
    if ((Program*)(intptr_t)ast_ptr == g_program) g_program = NULL;
}

void wyn_c_parser_cleanup(void) {
    for (int i = 0; i < 10; i++) {
        if (g_error_messages[i]) { free(g_error_messages[i]); g_error_messages[i] = NULL; }
    }
    if (g_ast_buffer) { free(g_ast_buffer); g_ast_buffer = NULL; }
    g_program = NULL;
    g_error_count = 0;
}
