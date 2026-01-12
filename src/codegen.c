#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"
#include "ast.h"

// Forward declarations
void codegen_stmt(Stmt* stmt);
void codegen_match_statement(Stmt* stmt); // T1.4.4: Control Flow Agent addition

static FILE* out = NULL;

// Forward declaration
static void emit(const char* fmt, ...);

// Scope tracking for automatic cleanup
typedef struct {
    char* vars[256];
    char* types[256];  // Track type for proper cleanup
    int count;
} Scope;

static Scope scopes[32];
static int scope_depth = 0;

void init_codegen(FILE* output) {
    out = output;
    scope_depth = 0;
}

static void push_scope() {
    if (scope_depth < 32) {
        scopes[scope_depth].count = 0;
        scope_depth++;
    }
}

static void pop_scope() {
    if (scope_depth > 0) {
        scope_depth--;
        // Emit cleanup for this scope
        for (int i = 0; i < scopes[scope_depth].count; i++) {
            emit("    if(%s) free(%s);\n", scopes[scope_depth].vars[i], scopes[scope_depth].vars[i]);
        }
    }
}

static void track_string_var(const char* name, int len) {
    if (scope_depth > 0 && scopes[scope_depth - 1].count < 256) {
        char* var = malloc(len + 1);
        strncpy(var, name, len);
        var[len] = 0;
        scopes[scope_depth - 1].vars[scopes[scope_depth - 1].count++] = var;
    }
}

static void emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}

void codegen_expr(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_INT:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_FLOAT:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_STRING:
            emit("\"");
            // Handle string content - check for escape sequences
            for (int i = 1; i < expr->token.length - 1; i++) {
                char c = expr->token.start[i];
                if (c == '\\' && i + 1 < expr->token.length - 1) {
                    // Handle escape sequence
                    char next = expr->token.start[i + 1];
                    if (next == 'n') {
                        emit("\\n");
                        i++; // Skip the 'n'
                    } else if (next == 'r') {
                        emit("\\r");
                        i++; // Skip the 'r'
                    } else if (next == 't') {
                        emit("\\t");
                        i++; // Skip the 't'
                    } else if (next == '"') {
                        emit("\\\"");
                        i++; // Skip the '"'
                    } else if (next == '\\') {
                        emit("\\\\");
                        i++; // Skip the second '\'
                    } else {
                        // Unknown escape, emit as-is
                        emit("\\%c", next);
                        i++; // Skip the next char
                    }
                } else if (c == '"') {
                    // Unescaped quote - should not happen in well-formed string
                    emit("\\\"");
                } else {
                    emit("%c", c);
                }
            }
            emit("\"");
            break;
        case EXPR_CHAR:
            emit("'%.*s'", expr->token.length - 2, expr->token.start + 1);
            break;
        case EXPR_IDENT:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_BOOL:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_UNARY:
            if (expr->unary.op.type == TOKEN_NOT) {
                emit("!");
            } else {
                emit("%.*s", expr->unary.op.length, expr->unary.op.start);
            }
            codegen_expr(expr->unary.operand);
            break;
        case EXPR_BINARY:
            // Special handling for string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS) {
                // Check if either operand is a string (using type information from checker)
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                if (left_is_string || right_is_string) {
                    // Use string concatenation
                    emit("string_concat(");
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                    break;
                }
            }
            
            // Default binary expression handling
            emit("(");
            codegen_expr(expr->binary.left);
            if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP) {
                emit(" && ");
            } else if (expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE) {
                emit(" || ");
            } else {
                emit(" %.*s ", expr->binary.op.length, expr->binary.op.start);
            }
            codegen_expr(expr->binary.right);
            emit(")");
            break;
        case EXPR_CALL:
            // Special handling for print function
            if (expr->call.callee->type == EXPR_IDENT && 
                expr->call.callee->token.length == 5 &&
                memcmp(expr->call.callee->token.start, "print", 5) == 0) {
                
                if (expr->call.arg_count == 1) {
                    // Single argument - use the _Generic macro for type dispatch
                    emit("print(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else if (expr->call.arg_count >= 2 && 
                           expr->call.args[0]->type == EXPR_STRING) {
                    // Format string with {} placeholders: print("Value: {}", x)
                    const char* fmt = expr->call.args[0]->token.start + 1; // Skip opening quote
                    int fmt_len = expr->call.args[0]->token.length - 2; // Skip quotes
                    
                    emit("printf(\"");
                    int arg_idx = 1;
                    for (int i = 0; i < fmt_len; i++) {
                        if (i < fmt_len - 1 && fmt[i] == '{' && fmt[i+1] == '}') {
                            // Replace {} with appropriate format specifier
                            if (arg_idx < expr->call.arg_count) {
                                Expr* arg = expr->call.args[arg_idx];
                                
                                // Determine format specifier based on argument type
                                if (arg->expr_type && arg->expr_type->kind == TYPE_STRING) {
                                    emit("%%s");
                                } else if (arg->expr_type && arg->expr_type->kind == TYPE_FLOAT) {
                                    emit("%%f");
                                } else if (arg->expr_type && arg->expr_type->kind == TYPE_BOOL) {
                                    emit("%%s");  // Will use ternary for true/false
                                } else if (arg->type == EXPR_STRING) {
                                    // String literal
                                    emit("%%s");
                                } else if (arg->type == EXPR_IDENT) {
                                    // Variable - need better type detection
                                    // For now, check if it's likely a string parameter
                                    // This is a heuristic - proper solution needs symbol table lookup
                                    const char* var_name = arg->token.start;
                                    int var_len = arg->token.length;
                                    if ((var_len >= 4 && strncmp(var_name, "name", 4) == 0) ||
                                        (var_len >= 3 && strncmp(var_name, "msg", 3) == 0) ||
                                        (var_len >= 3 && strncmp(var_name, "str", 3) == 0) ||
                                        (var_len >= 4 && strncmp(var_name, "text", 4) == 0)) {
                                        emit("%%s");
                                    } else {
                                        emit("%%d");  // Default to int for other variables
                                    }
                                } else if (arg->type == EXPR_FIELD_ACCESS) {
                                    // Struct field access - check field name for string hints
                                    Token field_name = arg->field_access.field;
                                    if ((field_name.length >= 4 && strncmp(field_name.start, "name", 4) == 0) ||
                                        (field_name.length >= 3 && strncmp(field_name.start, "str", 3) == 0) ||
                                        (field_name.length >= 4 && strncmp(field_name.start, "text", 4) == 0) ||
                                        (field_name.length >= 5 && strncmp(field_name.start, "title", 5) == 0)) {
                                        emit("%%s");
                                    } else {
                                        emit("%%d");  // Default to int for other fields
                                    }
                                } else {
                                    // Default to int for backward compatibility
                                    emit("%%d");
                                }
                            }
                            i++; // Skip the }
                            arg_idx++;
                        } else if (fmt[i] == '%') {
                            emit("%%%%"); // Escape %
                        } else if (fmt[i] == '\\' && i < fmt_len - 1) {
                            emit("\\%c", fmt[i+1]);
                            i++;
                        } else {
                            emit("%c", fmt[i]);
                        }
                    }
                    emit("\\n\"");
                    // Add arguments
                    for (int i = 1; i < expr->call.arg_count; i++) {
                        emit(", ");
                        Expr* arg = expr->call.args[i];
                        // Handle boolean arguments that need string conversion
                        if (arg->expr_type && arg->expr_type->kind == TYPE_BOOL) {
                            emit("(");
                            codegen_expr(arg);
                            emit(" ? \"true\" : \"false\")");
                        } else {
                            codegen_expr(arg);
                        }
                    }
                    emit(")");
                } else {
                    // Multiple arguments - use individual print calls without newlines
                    emit("({ ");
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        if (i > 0) emit("printf(\" \"); ");
                        emit("print_no_nl(");
                        codegen_expr(expr->call.args[i]);
                        emit("); ");
                    }
                    emit("printf(\"\\n\"); })");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 3 &&
                       memcmp(expr->call.callee->token.start, "len", 3) == 0) {
                // Special handling for len() function
                if (expr->call.arg_count == 1) {
                    Expr* arg = expr->call.args[0];
                    if (arg->type == EXPR_ARRAY) {
                        // Array literal - count elements directly
                        emit("%d", arg->array.count);
                    } else {
                        // Variable - assume it's a dynamic array now
                        emit("(");
                        codegen_expr(arg);
                        emit(").count");
                    }
                } else {
                    emit("len(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                }
            } else {
                // Check for special functions that need address-taking
                bool is_array_push = (expr->call.callee->type == EXPR_IDENT && 
                                     expr->call.callee->token.length == 10 &&
                                     memcmp(expr->call.callee->token.start, "array_push", 10) == 0);
                bool is_array_pop = (expr->call.callee->type == EXPR_IDENT && 
                                    expr->call.callee->token.length == 9 &&
                                    memcmp(expr->call.callee->token.start, "array_pop", 9) == 0);
                
                // T1.5.3: Use mangled name only for actually overloaded functions
                if (expr->call.selected_overload) {
                    Symbol* overload = (Symbol*)expr->call.selected_overload;
                    // Only use mangled name if there are multiple overloads
                    if (overload->mangled_name && overload->next_overload) {
                        emit("%s", overload->mangled_name);
                    } else {
                        codegen_expr(expr->call.callee);
                    }
                } else {
                    codegen_expr(expr->call.callee);
                }
                
                emit("(");
                for (int i = 0; i < expr->call.arg_count; i++) {
                    if (i > 0) emit(", ");
                    
                    // For array_push, take address of first argument (the array)
                    // and cast second argument to void* for integers
                    // For array_pop, take address of first argument (the array)
                    if ((is_array_push || is_array_pop) && i == 0) {
                        emit("&");
                    } else if (is_array_push && i == 1) {
                        emit("(void*)(intptr_t)");
                    }
                    
                    codegen_expr(expr->call.args[i]);
                }
                emit(")");
            }
            break;
        case EXPR_METHOD_CALL: {
            // Check for built-in array/string/int/map methods
            Token method = expr->method_call.method;
            
            // First check if this is a module method call
            if (expr->method_call.object->type == EXPR_IDENT) {
                Token obj_name = expr->method_call.object->token;
                if (obj_name.length == 4 && memcmp(obj_name.start, "math", 4) == 0) {
                    // Generate direct function call for math module
                    emit("%.*s(", method.length, method.start);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        if (i > 0) emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            
            // First check if this is a string method
            bool is_string_method = (method.length == 6 && memcmp(method.start, "length", 6) == 0 && expr->method_call.arg_count == 0) ||
                                   (method.length == 9 && memcmp(method.start, "substring", 9) == 0 && expr->method_call.arg_count == 2) ||
                                   (method.length == 8 && memcmp(method.start, "contains", 8) == 0 && expr->method_call.arg_count == 1) ||
                                   (method.length == 6 && memcmp(method.start, "concat", 6) == 0 && expr->method_call.arg_count == 1) ||
                                   (method.length == 5 && memcmp(method.start, "upper", 5) == 0 && expr->method_call.arg_count == 0) ||
                                   (method.length == 5 && memcmp(method.start, "lower", 5) == 0 && expr->method_call.arg_count == 0);
            
            if (is_string_method) {
                // Handle string methods
                if (method.length == 6 && memcmp(method.start, "length", 6) == 0) {
                    emit("string_length(");
                    codegen_expr(expr->method_call.object);
                    emit(")");
                } else if (method.length == 9 && memcmp(method.start, "substring", 9) == 0) {
                    emit("string_substring(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(", ");
                    codegen_expr(expr->method_call.args[1]);
                    emit(")");
                } else if (method.length == 8 && memcmp(method.start, "contains", 8) == 0) {
                    emit("string_contains(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                } else if (method.length == 6 && memcmp(method.start, "concat", 6) == 0) {
                    emit("string_concat(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                } else if (method.length == 5 && memcmp(method.start, "upper", 5) == 0) {
                    emit("string_upper(");
                    codegen_expr(expr->method_call.object);
                    emit(")");
                } else if (method.length == 5 && memcmp(method.start, "lower", 5) == 0) {
                    emit("string_lower(");
                    codegen_expr(expr->method_call.object);
                    emit(")");
                }
            }
            // Then check if this is a map method by looking at common map method names
            else if ((method.length == 3 && memcmp(method.start, "get", 3) == 0 && expr->method_call.arg_count > 0) ||
                     (method.length == 3 && memcmp(method.start, "set", 3) == 0 && expr->method_call.arg_count > 1) ||
                     (method.length == 3 && memcmp(method.start, "has", 3) == 0 && expr->method_call.arg_count > 0) ||
                     (method.length == 4 && memcmp(method.start, "size", 4) == 0 && expr->method_call.arg_count == 0) ||
                     (method.length == 5 && memcmp(method.start, "clear", 5) == 0 && expr->method_call.arg_count == 0) ||
                     (method.length == 4 && memcmp(method.start, "keys", 4) == 0 && expr->method_call.arg_count == 0)) {
                // Handle map methods
                if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                    emit("map_get(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                } else if (method.length == 3 && memcmp(method.start, "set", 3) == 0) {
                    emit("map_set(&(");
                    codegen_expr(expr->method_call.object);
                    emit("), ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(", ");
                    codegen_expr(expr->method_call.args[1]);
                    emit(")");
                } else if (method.length == 3 && memcmp(method.start, "has", 3) == 0) {
                    emit("map_has(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                } else if (method.length == 4 && memcmp(method.start, "size", 4) == 0) {
                    emit("(");
                    codegen_expr(expr->method_call.object);
                    emit(").count");
                } else if (method.length == 5 && memcmp(method.start, "clear", 5) == 0) {
                    emit("map_clear(&(");
                    codegen_expr(expr->method_call.object);
                    emit("))");
                } else if (method.length == 4 && memcmp(method.start, "keys", 4) == 0) {
                    emit("map_keys(");
                    codegen_expr(expr->method_call.object);
                    emit(")");
                }
                break;  // Important: prevent fallthrough to array methods
            }
            // Array methods
            else if (method.length == 5 && memcmp(method.start, "first", 5) == 0) {
                codegen_expr(expr->method_call.object);
                emit("[0]");
            } else if (method.length == 4 && memcmp(method.start, "last", 4) == 0) {
                emit("(");
                codegen_expr(expr->method_call.object);
                emit(")[sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0])-1]");
            } else if (method.length == 6 && memcmp(method.start, "length", 6) == 0) {
                emit("(sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 5 && memcmp(method.start, "count", 5) == 0) {
                emit("(sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 4 && memcmp(method.start, "size", 4) == 0) {
                emit("(sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 7 && memcmp(method.start, "isEmpty", 7) == 0) {
                emit("(sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]) == 0)");
            } else if (method.length == 5 && memcmp(method.start, "empty", 5) == 0) {
                emit("(sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]) == 0)");
            } else if (method.length == 4 && memcmp(method.start, "sort", 4) == 0) {
                // arr.sort() - sorts in place and returns array
                emit("({ arr_sort(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0])); ");
                codegen_expr(expr->method_call.object);
                emit("; })");
            } else if (method.length == 7 && memcmp(method.start, "reverse", 7) == 0) {
                // arr.reverse() - reverses in place and returns array
                emit("({ arr_reverse(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0])); ");
                codegen_expr(expr->method_call.object);
                emit("; })");
            } else if (method.length == 3 && memcmp(method.start, "sum", 3) == 0) {
                emit("arr_sum(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 3 && memcmp(method.start, "max", 3) == 0) {
                emit("arr_max(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 3 && memcmp(method.start, "min", 3) == 0) {
                emit("arr_min(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 8 && memcmp(method.start, "contains", 8) == 0) {
                emit("arr_contains(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "find", 4) == 0) {
                emit("arr_find(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "count", 5) == 0) {
                emit("arr_count(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "fill", 4) == 0) {
                emit("({ arr_fill(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit("); ");
                codegen_expr(expr->method_call.object);
                emit("; })");
            } else if (method.length == 3 && memcmp(method.start, "all", 3) == 0) {
                emit("arr_all(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 3 && memcmp(method.start, "any", 3) == 0) {
                // any(val) - opposite of all
                emit("!arr_all(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "join", 4) == 0) {
                emit("arr_join(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 3 && memcmp(method.start, "map", 3) == 0) {
                // arr.map(value) - replace all elements with value
                emit("({ arr_fill(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit("); ");
                codegen_expr(expr->method_call.object);
                emit("; })");
            } else if (method.length == 6 && memcmp(method.start, "filter", 6) == 0) {
                // arr.filter(value) - count elements equal to value
                emit("arr_count(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 6 && memcmp(method.start, "reduce", 6) == 0) {
                // arr.reduce(op) - for now, just sum if op is "+"
                emit("arr_sum(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]))");
            } else if (method.length == 7 && memcmp(method.start, "forEach", 7) == 0) {
                // arr.forEach(val) - count how many times we'd call function with val
                emit("arr_count(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "some", 4) == 0) {
                // arr.some(val) - check if any element equals val
                emit("arr_contains(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "every", 5) == 0) {
                // arr.every(val) - check if all elements equal val
                emit("arr_all(");
                codegen_expr(expr->method_call.object);
                emit(", sizeof(");
                codegen_expr(expr->method_call.object);
                emit(")/sizeof((");
                codegen_expr(expr->method_call.object);
                emit(")[0]), ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "push", 4) == 0) {
                // arr.push(val) - use existing array_push function
                emit("array_push(&(");
                codegen_expr(expr->method_call.object);
                emit("), (void*)(intptr_t)");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 3 && memcmp(method.start, "pop", 3) == 0) {
                // arr.pop() - use existing array_pop function
                emit("(int)(intptr_t)array_pop(&(");
                codegen_expr(expr->method_call.object);
                emit("))");
            } else if (method.length == 5 && memcmp(method.start, "shift", 5) == 0) {
                // arr.shift() - remove first element
                emit("({ /* shift not implemented */ 0; })");
            } else if (method.length == 7 && memcmp(method.start, "unshift", 7) == 0) {
                // arr.unshift(val) - add to beginning
                emit("({ /* unshift not implemented */ 0; })");
            } else if (method.length == 6 && memcmp(method.start, "concat", 6) == 0) {
                // arr.concat(other) - combine arrays
                emit("({ /* concat not implemented */ ");
                codegen_expr(expr->method_call.object);
                emit("; })");
            // String methods
            } else if (method.length == 6 && memcmp(method.start, "length", 6) == 0) {
                emit("str_len(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "size", 4) == 0) {
                emit("str_len(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 3 && memcmp(method.start, "len", 3) == 0) {
                emit("str_len(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "chars", 5) == 0) {
                // Returns the string itself (for iteration)
                emit("(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "upper", 5) == 0) {
                emit("str_upper(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "lower", 5) == 0) {
                emit("str_lower(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "trim", 4) == 0) {
                emit("str_trim(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 8 && memcmp(method.start, "contains", 8) == 0) {
                emit("str_contains(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 10 && memcmp(method.start, "startsWith", 10) == 0) {
                emit("str_starts_with(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 8 && memcmp(method.start, "endsWith", 8) == 0) {
                emit("str_ends_with(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 6 && memcmp(method.start, "repeat", 6) == 0) {
                emit("str_repeat(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 7 && memcmp(method.start, "reverse", 7) == 0) {
                emit("str_reverse(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 7 && memcmp(method.start, "isEmpty", 7) == 0) {
                emit("(str_len(");
                codegen_expr(expr->method_call.object);
                emit(") == 0)");
            } else if (method.length == 5 && memcmp(method.start, "empty", 5) == 0) {
                emit("(str_len(");
                codegen_expr(expr->method_call.object);
                emit(") == 0)");
            } else if (method.length == 7 && memcmp(method.start, "replace", 7) == 0) {
                emit("str_replace(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "split", 5) == 0) {
                emit("str_split(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 9 && memcmp(method.start, "substring", 9) == 0) {
                emit("str_substring(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 7 && memcmp(method.start, "indexOf", 7) == 0) {
                emit("str_index_of(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "slice", 5) == 0) {
                emit("str_slice(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 8 && memcmp(method.start, "padStart", 8) == 0) {
                emit("str_pad_start(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 6 && memcmp(method.start, "padEnd", 6) == 0) {
                emit("str_pad_end(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 12 && memcmp(method.start, "removePrefix", 12) == 0) {
                emit("str_remove_prefix(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 12 && memcmp(method.start, "removeSuffix", 12) == 0) {
                emit("str_remove_suffix(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 11 && memcmp(method.start, "capitalize", 10) == 0) {
                emit("str_capitalize(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 6 && memcmp(method.start, "center", 6) == 0) {
                emit("str_center(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "lines", 5) == 0) {
                emit("str_lines(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "words", 5) == 0) {
                emit("str_words(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // Int methods
            } else if (method.length == 3 && memcmp(method.start, "abs", 3) == 0) {
                emit("abs_val(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "sqrt", 4) == 0) {
                emit("sqrt_int(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 6 && memcmp(method.start, "isEven", 6) == 0) {
                emit("is_even(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "isOdd", 5) == 0) {
                emit("is_odd(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "times", 5) == 0) {
                // n.times() - repeat n times (returns n for chaining)
                emit("({ int _n = ");
                codegen_expr(expr->method_call.object);
                emit("; for(int _i = 0; _i < _n; _i++); _n; })");
            } else if (method.length == 4 && memcmp(method.start, "upto", 4) == 0) {
                // n.upto(m) - range from n to m
                emit("({ int _start = ");
                codegen_expr(expr->method_call.object);
                emit("; int _end = ");
                codegen_expr(expr->method_call.args[0]);
                emit("; for(int _i = _start; _i <= _end; _i++); _end; })");
            } else if (method.length == 6 && memcmp(method.start, "downto", 6) == 0) {
                // n.downto(m) - range from n down to m
                emit("({ int _start = ");
                codegen_expr(expr->method_call.object);
                emit("; int _end = ");
                codegen_expr(expr->method_call.args[0]);
                emit("; for(int _i = _start; _i >= _end; _i--); _end; })");
            } else if (method.length == 6 && memcmp(method.start, "clamp", 6) == 0) {
                emit("clamp(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(", ");
                codegen_expr(expr->method_call.args[1]);
                emit(")");
            } else if (method.length == 3 && memcmp(method.start, "pow", 3) == 0) {
                emit("pow_int(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            } else if (method.length == 4 && memcmp(method.start, "sign", 4) == 0) {
                emit("sign(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 8 && memcmp(method.start, "toString", 8) == 0) {
                // Could be int.toString() or enum.toString()
                // For enums, we need to know the type - for now, try int_to_str
                // If it's an enum, the generated EnumName_toString will be called
                emit("int_to_str(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "toInt", 5) == 0) {
                // Enum to int - just return the value
                emit("(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // Float methods (if object is float)
            } else if (method.length == 4 && memcmp(method.start, "ceil", 4) == 0) {
                emit("ceil_int(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "floor", 5) == 0) {
                emit("floor_int(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else if (method.length == 5 && memcmp(method.start, "round", 5) == 0) {
                emit("round_int(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .print() method for all types
            } else if (method.length == 5 && memcmp(method.start, "print", 5) == 0) {
                emit("print(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .read() method for file paths (strings)
            } else if (method.length == 4 && memcmp(method.start, "read", 4) == 0) {
                emit("file_read(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .write() method for file paths (strings)
            } else if (method.length == 5 && memcmp(method.start, "write", 5) == 0) {
                emit("file_write(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            // .append() method for file paths
            } else if (method.length == 6 && memcmp(method.start, "append", 6) == 0) {
                emit("file_append(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            // .exists() method for file paths
            } else if (method.length == 6 && memcmp(method.start, "exists", 6) == 0) {
                emit("file_exists(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .delete() method for file paths
            } else if (method.length == 6 && memcmp(method.start, "delete", 6) == 0) {
                emit("file_delete(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .size() method for file paths
            } else if (method.length == 4 && memcmp(method.start, "size", 4) == 0) {
                emit("file_size(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .get() method for URLs (HTTP GET)
            } else if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                emit("http_get(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .gets() method for URLs (HTTPS GET)
            } else if (method.length == 4 && memcmp(method.start, "gets", 4) == 0) {
                emit("https_get(");
                codegen_expr(expr->method_call.object);
                emit(")");
            // .post() method for URLs (HTTP POST)
            } else if (method.length == 4 && memcmp(method.start, "post", 4) == 0) {
                emit("http_post(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            // .posts() method for URLs (HTTPS POST)
            } else if (method.length == 5 && memcmp(method.start, "posts", 5) == 0) {
                emit("https_post(");
                codegen_expr(expr->method_call.object);
                emit(", ");
                codegen_expr(expr->method_call.args[0]);
                emit(")");
            // Bool methods
            } else if (method.length == 8 && memcmp(method.start, "toString", 8) == 0) {
                // Works for both int and bool
                emit("int_to_str(");
                codegen_expr(expr->method_call.object);
                emit(")");
            } else {
                // Regular method call
                codegen_expr(expr->method_call.object);
                emit(".%.*s(", expr->method_call.method.length, expr->method_call.method.start);
                for (int i = 0; i < expr->method_call.arg_count; i++) {
                    if (i > 0) emit(", ");
                    codegen_expr(expr->method_call.args[i]);
                }
                emit(")");
            }
        }
            break;
        case EXPR_ARRAY:
            // Generate dynamic array creation with tagged union
            emit("({ WynArray __arr = array_new(); ");
            for (int i = 0; i < expr->array.count; i++) {
                if (expr->array.elements[i]->type == EXPR_ARRAY) {
                    // For nested arrays, allocate on heap and use array_push_array
                    emit("{ WynArray* __nested = malloc(sizeof(WynArray)); *__nested = ");
                    codegen_expr(expr->array.elements[i]);
                    emit("; array_push_array(&__arr, __nested); } ");
                } else if (expr->array.elements[i]->type == EXPR_STRING) {
                    // For strings, use array_push_str
                    emit("array_push_str(&__arr, ");
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                } else {
                    // For integers and other types, use array_push_int
                    emit("array_push_int(&__arr, ");
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                }
            }
            emit("__arr; })");;
            break;
        case EXPR_INDEX: {
            // Check if this is map indexing by looking at the object
            // For now, assume if index is a string, it's a map
            if (expr->index.index->type == EXPR_STRING) {
                // Map indexing: map["key"] -> map_get(map, "key")
                emit("map_get(");
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
            } else {
                // Array indexing with tagged union support
                // Check if this is chained indexing (matrix[0][1])
                if (expr->index.array->type == EXPR_INDEX) {
                    // This is chained indexing: arr[i][j]
                    if (expr->index.array->index.array->type == EXPR_INDEX) {
                        // Triple chained: arr[i][j][k]
                        emit("array_get_nested3_int(");
                        codegen_expr(expr->index.array->index.array->index.array);
                        emit(", ");
                        codegen_expr(expr->index.array->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else {
                        // Double chained: arr[i][j]
                        emit("array_get_nested_int(");
                        codegen_expr(expr->index.array->index.array);
                        emit(", ");
                        codegen_expr(expr->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    }
                } else {
                    // Single indexing: arr[i]
                    emit("array_get_int(");
                    codegen_expr(expr->index.array);
                    emit(", ");
                    codegen_expr(expr->index.index);
                    emit(")");
                }
            }
            break;
        }
        case EXPR_ASSIGN:
            emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
            codegen_expr(expr->assign.value);
            break;
        case EXPR_STRUCT_INIT:
            emit("(%.*s){", expr->struct_init.type_name.length, expr->struct_init.type_name.start);
            for (int i = 0; i < expr->struct_init.field_count; i++) {
                if (i > 0) emit(", ");
                emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                codegen_expr(expr->struct_init.field_values[i]);
            }
            emit("}");
            break;
        case EXPR_FIELD_ACCESS: {
            // Check if this is enum member access by looking at the pattern
            Token obj_name = expr->field_access.object->token;
            Token field_name = expr->field_access.field;
            
            // Simple heuristic: if object is an identifier starting with uppercase
            // and we're accessing a field that's all uppercase, it's likely enum access
            if (expr->field_access.object->type == EXPR_IDENT &&
                obj_name.length > 0 && obj_name.start[0] >= 'A' && obj_name.start[0] <= 'Z' &&
                field_name.length > 0 && field_name.start[0] >= 'A' && field_name.start[0] <= 'Z') {
                
                // Generate the enum constant name (EnumName_MEMBER)
                emit("%.*s_%.*s",
                     obj_name.length, obj_name.start,
                     field_name.length, field_name.start);
                return;
            }
            
            // Handle module.function calls
            if (obj_name.length == 4 && memcmp(obj_name.start, "math", 4) == 0) {
                // Generate direct function call for math module
                emit("%.*s", field_name.length, field_name.start);
            } else if (expr->field_access.field.length == 6 && 
                       memcmp(expr->field_access.field.start, "length", 6) == 0) {
                // Special case for array.length -> arr.count for dynamic arrays
                emit("(");
                codegen_expr(expr->field_access.object);
                emit(").count");
            } else {
                codegen_expr(expr->field_access.object);
                emit(".%.*s", expr->field_access.field.length, expr->field_access.field.start);
            }
            break;
        }
        case EXPR_MATCH: {
            emit("({ ");
            // Determine return type - for now assume int
            emit("int __match_result = 0; switch (");
            codegen_expr(expr->match.value);
            emit(") {\n");
            for (int i = 0; i < expr->match.arm_count; i++) {
                emit("        case %.*s: __match_result = ", 
                     expr->match.arms[i].pattern.length,
                     expr->match.arms[i].pattern.start);
                codegen_expr(expr->match.arms[i].result);
                emit("; break;\n");
            }
            emit("    } __match_result; })");
            break;
        }
        case EXPR_SOME:
            emit("some(");
            codegen_expr(expr->option.value);
            emit(")");
            break;
        case EXPR_NONE:
            emit("none()");
            break;
        case EXPR_OK:
            emit("ok(");
            codegen_expr(expr->option.value);
            emit(")");
            break;
        case EXPR_ERR:
            emit("err(");
            codegen_expr(expr->option.value);
            emit(")");
            break;
        case EXPR_TERNARY:
            emit("(");
            codegen_expr(expr->ternary.condition);
            emit(" ? ");
            codegen_expr(expr->ternary.then_expr);
            emit(" : ");
            codegen_expr(expr->ternary.else_expr);
            emit(")");
            break;
        case EXPR_PIPELINE: {
            // Generate nested function calls: f(g(h(x)))
            // For x |> f |> g |> h, generate h(g(f(x)))
            
            // Start from the rightmost function and work backwards
            for (int i = expr->pipeline.stage_count - 1; i >= 1; i--) {
                if (expr->pipeline.stages[i]->type == EXPR_IDENT) {
                    // Simple function call
                    emit("%.*s(", 
                         expr->pipeline.stages[i]->token.length,
                         expr->pipeline.stages[i]->token.start);
                } else if (expr->pipeline.stages[i]->type == EXPR_METHOD_CALL) {
                    // Method call - need to handle specially
                    // For now, just emit the method name as a function
                    emit("%.*s(", 
                         expr->pipeline.stages[i]->method_call.method.length,
                         expr->pipeline.stages[i]->method_call.method.start);
                }
            }
            
            // Emit the first stage (the value)
            codegen_expr(expr->pipeline.stages[0]);
            
            // Close all the function calls
            for (int i = 1; i < expr->pipeline.stage_count; i++) {
                emit(")");
            }
            break;
        }
        case EXPR_IF_EXPR:
            emit("(");
            codegen_expr(expr->if_expr.condition);
            emit(" ? ");
            if (expr->if_expr.then_expr) {
                codegen_expr(expr->if_expr.then_expr);
            } else {
                emit("0");
            }
            emit(" : ");
            if (expr->if_expr.else_expr) {
                codegen_expr(expr->if_expr.else_expr);
            } else {
                emit("0");
            }
            emit(")");
            break;
        case EXPR_STRING_INTERP: {
            // String interpolation: "Hello ${name}" -> sprintf format
            emit("({ char __buf[256]; sprintf(__buf, \"");
            
            // Build format string - use %s for everything and convert with _Generic
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.parts[i]) {
                    // String literal part
                    const char* part = expr->string_interp.parts[i];
                    while (*part) {
                        if (*part == '%') emit("%%"); // Escape % for sprintf
                        else emit("%c", *part);
                        part++;
                    }
                } else {
                    // Expression part - use %s and convert with to_string
                    emit("%%s");
                }
            }
            
            emit("\"");
            
            // Add arguments for expressions with type conversion
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.expressions[i]) {
                    emit(", to_string(");
                    codegen_expr(expr->string_interp.expressions[i]);
                    emit(")");
                }
            }
            
            emit("); __buf; })");
            break;
        }
        case EXPR_RANGE:
            // Generate range struct: {start, end}
            emit("({ struct { int start; int end; } __range = {");
            codegen_expr(expr->range.start);
            emit(", ");
            codegen_expr(expr->range.end);
            emit("}; __range; })");
            break;
        case EXPR_LAMBDA:
            // Generate inline function - simplified
            emit("({ /* lambda */ 0; })");
            break;
        case EXPR_MAP: {
            // Generate map using the typedef
            emit("({ ");
            emit("WynMap __map = {0}; ");
            emit("__map.count = %d; ", expr->map.count);
            if (expr->map.count > 0) {
                emit("__map.keys = malloc(sizeof(void*) * %d); ", expr->map.count);
                emit("__map.values = malloc(sizeof(void*) * %d); ", expr->map.count);
                for (int i = 0; i < expr->map.count; i++) {
                    emit("__map.keys[%d] = (void*)strdup(", i);
                    codegen_expr(expr->map.keys[i]);
                    emit("); ");
                    emit("__map.values[%d] = (void*)(intptr_t)", i);
                    codegen_expr(expr->map.values[i]);
                    emit("; ");
                }
            }
            emit("__map; })");
            break;
        }
        case EXPR_TUPLE: {
            // Generate tuple as a struct
            emit("({ struct { ");
            for (int i = 0; i < expr->tuple.count; i++) {
                emit("int item%d; ", i);
            }
            emit("} __tuple = { ");
            for (int i = 0; i < expr->tuple.count; i++) {
                if (i > 0) emit(", ");
                codegen_expr(expr->tuple.elements[i]);
            }
            emit(" }; __tuple; })");
            break;
        }
        case EXPR_INDEX_ASSIGN: {
            // Handle map["key"] = value -> map_set(&map, "key", value)
            if (expr->index_assign.index->type == EXPR_STRING) {
                emit("map_set(&(");
                codegen_expr(expr->index_assign.object);
                emit("), ");
                codegen_expr(expr->index_assign.index);
                emit(", ");
                codegen_expr(expr->index_assign.value);
                emit(")");
            } else {
                // Array assignment with tagged union support
                emit("{ WynArray* __arr_ptr = &(");
                codegen_expr(expr->index_assign.object);
                emit("); int __idx = ");
                codegen_expr(expr->index_assign.index);
                emit("; if (__idx >= 0 && __idx < __arr_ptr->count) { __arr_ptr->data[__idx].type = WYN_TYPE_INT; __arr_ptr->data[__idx].data.int_val = ");
                codegen_expr(expr->index_assign.value);
                emit("; } }");
            }
            break;
        }
        case EXPR_OPTIONAL_TYPE:
            // T2.5.1: Optional Type Implementation - For type expressions, just emit the inner type
            // In a real implementation, this would generate optional type metadata
            codegen_expr(expr->optional_type.inner_type);
            break;
        case EXPR_UNION_TYPE:
            // T2.5.2: Union Type Support - For type expressions, emit union type representation
            // In a real implementation, this would generate union type metadata
            emit("union { ");
            for (int i = 0; i < expr->union_type.type_count; i++) {
                if (i > 0) emit("; ");
                codegen_expr(expr->union_type.types[i]);
            }
            emit(" }");
            break;
        default:
            break;
    }
}

void codegen_c_header() {
    emit("#include <stdio.h>\n");
    emit("#include <stdlib.h>\n");
    emit("#include <stdint.h>\n");
    emit("#include <stdbool.h>\n");
    emit("#include <string.h>\n");
    emit("#include <math.h>\n");
    emit("#include <time.h>\n");
    emit("#include <ctype.h>\n");
    emit("#include <stdarg.h>\n");
    emit("#include <setjmp.h>\n");
    emit("#include <sys/socket.h>\n");
    emit("#include <netinet/in.h>\n");
    emit("#include <netdb.h>\n");
    emit("#include <unistd.h>\n");
    emit("#include <fcntl.h>\n");
    emit("#include <errno.h>\n\n");
    
    // Exception handling globals
    emit("jmp_buf* current_exception_buf = NULL;\n");
    emit("const char** current_exception_msg = NULL;\n\n");
    
    // Add map type definition
    emit("typedef struct { void** keys; void** values; int count; } WynMap;\n\n");
    
    // Array utility functions with tagged union support
    emit("typedef enum { WYN_TYPE_INT, WYN_TYPE_FLOAT, WYN_TYPE_STRING, WYN_TYPE_ARRAY } WynValueType;\n");
    emit("typedef struct {\n");
    emit("    WynValueType type;\n");
    emit("    union {\n");
    emit("        int int_val;\n");
    emit("        double float_val;\n");
    emit("        const char* string_val;\n");
    emit("        struct WynArray* array_val;\n");
    emit("    } data;\n");
    emit("} WynValue;\n\n");
    
    emit("typedef struct WynArray { WynValue* data; int count; int capacity; } WynArray;\n");
    emit("WynArray array_new() { WynArray arr = {0}; return arr; }\n");
    emit("void array_push_int(WynArray* arr, int value) {\n");
    emit("    if (arr->count >= arr->capacity) {\n");
    emit("        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;\n");
    emit("        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);\n");
    emit("    }\n");
    emit("    arr->data[arr->count].type = WYN_TYPE_INT;\n");
    emit("    arr->data[arr->count].data.int_val = value;\n");
    emit("    arr->count++;\n");
    emit("}\n");
    emit("void array_push_str(WynArray* arr, const char* value) {\n");
    emit("    if (arr->count >= arr->capacity) {\n");
    emit("        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;\n");
    emit("        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);\n");
    emit("    }\n");
    emit("    arr->data[arr->count].type = WYN_TYPE_STRING;\n");
    emit("    arr->data[arr->count].data.string_val = value;\n");
    emit("    arr->count++;\n");
    emit("}\n");
    emit("void array_push_array(WynArray* arr, WynArray* nested) {\n");
    emit("    if (arr->count >= arr->capacity) {\n");
    emit("        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;\n");
    emit("        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);\n");
    emit("    }\n");
    emit("    arr->data[arr->count].type = WYN_TYPE_ARRAY;\n");
    emit("    arr->data[arr->count].data.array_val = nested;\n");
    emit("    arr->count++;\n");
    emit("}\n");
    emit("int array_get_int(WynArray arr, int index) {\n");
    emit("    if (index < 0 || index >= arr.count) return 0;\n");
    emit("    if (arr.data[index].type == WYN_TYPE_INT) return arr.data[index].data.int_val;\n");
    emit("    return 0;\n");
    emit("}\n");
    emit("const char* array_get_str(WynArray arr, int index) {\n");
    emit("    if (index < 0 || index >= arr.count) return \"\";\n");
    emit("    if (arr.data[index].type == WYN_TYPE_STRING) return arr.data[index].data.string_val;\n");
    emit("    return \"\";\n");
    emit("}\n");
    emit("WynArray* array_get_array(WynArray arr, int index) {\n");
    emit("    if (index < 0 || index >= arr.count) return NULL;\n");
    emit("    if (arr.data[index].type == WYN_TYPE_ARRAY) return arr.data[index].data.array_val;\n");
    emit("    return NULL;\n");
    emit("}\n");
    emit("int array_get_nested_int(WynArray arr, int index1, int index2) {\n");
    emit("    WynArray* nested = array_get_array(arr, index1);\n");
    emit("    if (nested == NULL) return 0;\n");
    emit("    return array_get_int(*nested, index2);\n");
    emit("}\n");
    emit("int array_get_nested3_int(WynArray arr, int index1, int index2, int index3) {\n");
    emit("    WynArray* nested1 = array_get_array(arr, index1);\n");
    emit("    if (nested1 == NULL) return 0;\n");
    emit("    WynArray* nested2 = array_get_array(*nested1, index2);\n");
    emit("    if (nested2 == NULL) return 0;\n");
    emit("    return array_get_int(*nested2, index3);\n");
    emit("}\n\n");
    
    // Range utility function
    emit("typedef struct { int start; int end; int current; } WynRange;\n");
    emit("WynRange range(int start, int end) {\n");
    emit("    WynRange r = {start, end, start};\n");
    emit("    return r;\n");
    emit("}\n");
    emit("bool range_has_next(WynRange* r) { return r->current < r->end; }\n");
    emit("int range_next(WynRange* r) { return r->current++; }\n\n");
    
    // String utility functions
    emit("int string_length(const char* str) { return strlen(str); }\n");
    emit("char* string_substring(const char* str, int start, int end) {\n");
    emit("    int len = end - start;\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    strncpy(result, str + start, len);\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    emit("bool string_contains(const char* str, const char* substr) {\n");
    emit("    return strstr(str, substr) != NULL;\n");
    emit("}\n");
    emit("char* string_concat(const char* a, const char* b) {\n");
    emit("    int len_a = strlen(a), len_b = strlen(b);\n");
    emit("    char* result = malloc(len_a + len_b + 1);\n");
    emit("    strcpy(result, a);\n");
    emit("    strcat(result, b);\n");
    emit("    return result;\n");
    emit("}\n");
    emit("char* string_upper(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    for (int i = 0; i < len; i++) {\n");
    emit("        result[i] = toupper(str[i]);\n");
    emit("    }\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    emit("char* string_lower(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    for (int i = 0; i < len; i++) {\n");
    emit("        result[i] = tolower(str[i]);\n");
    emit("    }\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    // Map utility functions
    emit("int map_get(WynMap map, const char* key) {\n");
    emit("    for (int i = 0; i < map.count; i++) {\n");
    emit("        if (strcmp((char*)map.keys[i], key) == 0) {\n");
    emit("            return (int)(intptr_t)map.values[i];\n");
    emit("        }\n");
    emit("    }\n");
    emit("    return 0; // Not found\n");
    emit("}\n\n");
    
    emit("void map_set(WynMap* map, const char* key, int value) {\n");
    emit("    // Check if key exists\n");
    emit("    for (int i = 0; i < map->count; i++) {\n");
    emit("        if (strcmp((char*)map->keys[i], key) == 0) {\n");
    emit("            map->values[i] = (void*)(intptr_t)value;\n");
    emit("            return;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    // Add new key-value pair\n");
    emit("    map->keys = realloc(map->keys, sizeof(void*) * (map->count + 1));\n");
    emit("    map->values = realloc(map->values, sizeof(void*) * (map->count + 1));\n");
    emit("    map->keys[map->count] = (void*)strdup(key);\n");
    emit("    map->values[map->count] = (void*)(intptr_t)value;\n");
    emit("    map->count++;\n");
    emit("}\n\n");
    
    emit("bool map_has(WynMap map, const char* key) {\n");
    emit("    for (int i = 0; i < map.count; i++) {\n");
    emit("        if (strcmp((char*)map.keys[i], key) == 0) {\n");
    emit("            return true;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    return false;\n");
    emit("}\n\n");
    
    emit("void map_clear(WynMap* map) {\n");
    emit("    if (map->keys) {\n");
    emit("        for (int i = 0; i < map->count; i++) {\n");
    emit("            free(map->keys[i]); // Free strdup'd keys\n");
    emit("        }\n");
    emit("        free(map->keys);\n");
    emit("        free(map->values);\n");
    emit("    }\n");
    emit("    map->keys = NULL;\n");
    emit("    map->values = NULL;\n");
    emit("    map->count = 0;\n");
    emit("}\n\n");
    
    emit("char** map_keys(WynMap map) {\n");
    emit("    char** keys = malloc(sizeof(char*) * (map.count + 1));\n");
    emit("    for (int i = 0; i < map.count; i++) {\n");
    emit("        keys[i] = (char*)map.keys[i];\n");
    emit("    }\n");
    emit("    keys[map.count] = NULL; // Null terminate\n");
    emit("    return keys;\n");
    emit("}\n\n");
    
    // HTTP helper
    emit("struct HttpResponse { char* body; int status; size_t size; };\n");
    emit("char* http_headers[32] = {NULL};\n");
    emit("int http_header_count = 0;\n");
    emit("int http_last_status = 0;\n");
    emit("char http_last_error[256] = {0};\n");
    emit("char last_error[256] = {0};\n\n");
    
    // Enhanced HTTP client with chunked transfer, redirects, and large responses
    emit("char* http_request(const char* method, const char* url, const char* body) {\n");
    emit("    char hostname[256], path[1024];\n");
    emit("    int port = 80, is_https = 0;\n");
    emit("    http_last_error[0] = 0;\n");
    emit("    \n");
    emit("    // Parse URL\n");
    emit("    if(strncmp(url, \"https://\", 8) == 0) { url += 8; port = 443; is_https = 1; }\n");
    emit("    else if(strncmp(url, \"http://\", 7) == 0) url += 7;\n");
    emit("    \n");
    emit("    const char* slash = strchr(url, '/');\n");
    emit("    if(slash) {\n");
    emit("        int len = slash - url;\n");
    emit("        if(len >= 256) len = 255;\n");
    emit("        strncpy(hostname, url, len);\n");
    emit("        hostname[len] = 0;\n");
    emit("        strncpy(path, slash, 1023);\n");
    emit("        path[1023] = 0;\n");
    emit("    } else {\n");
    emit("        strncpy(hostname, url, 255);\n");
    emit("        hostname[255] = 0;\n");
    emit("        strcpy(path, \"/\");\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Check for port\n");
    emit("    char* colon = strchr(hostname, ':');\n");
    emit("    if(colon) { *colon = 0; port = atoi(colon + 1); }\n");
    emit("    \n");
    emit("    // Create socket\n");
    emit("    int sock = socket(AF_INET, SOCK_STREAM, 0);\n");
    emit("    if(sock < 0) { snprintf(http_last_error, 256, \"Socket creation failed\"); return NULL; }\n");
    emit("    \n");
    emit("    // Set timeout\n");
    emit("    struct timeval tv = {30, 0};\n");
    emit("    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));\n");
    emit("    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));\n");
    emit("    \n");
    emit("    // Resolve hostname\n");
    emit("    struct hostent* server = gethostbyname(hostname);\n");
    emit("    if(!server) { close(sock); snprintf(http_last_error, 256, \"Host not found: %%s\", hostname); return NULL; }\n");
    emit("    \n");
    emit("    // Connect\n");
    emit("    struct sockaddr_in addr;\n");
    emit("    memset(&addr, 0, sizeof(addr));\n");
    emit("    addr.sin_family = AF_INET;\n");
    emit("    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);\n");
    emit("    addr.sin_port = htons(port);\n");
    emit("    \n");
    emit("    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {\n");
    emit("        close(sock); snprintf(http_last_error, 256, \"Connection failed\"); return NULL;\n");
    emit("    }\n");
    emit("    \n");
    emit("    // HTTPS warning (TLS not implemented yet)\n");
    emit("    if(is_https) {\n");
    emit("        close(sock);\n");
    emit("        snprintf(http_last_error, 256, \"HTTPS not supported yet - use http:// instead\");\n");
    emit("        return NULL;\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Build request\n");
    emit("    char* request = malloc(8192);\n");
    emit("    int len = snprintf(request, 8192, \"%%s %%s HTTP/1.1\\r\\nHost: %%s\\r\\n\", method, path, hostname);\n");
    emit("    len += snprintf(request + len, 8192 - len, \"User-Agent: Wyn/1.4\\r\\n\");\n");
    emit("    len += snprintf(request + len, 8192 - len, \"Accept: */*\\r\\n\");\n");
    emit("    \n");
    emit("    // Add custom headers\n");
    emit("    for(int i = 0; i < http_header_count; i++) {\n");
    emit("        len += snprintf(request + len, 8192 - len, \"%%s\\r\\n\", http_headers[i]);\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Add body\n");
    emit("    if(body) {\n");
    emit("        len += snprintf(request + len, 8192 - len, \"Content-Length: %%d\\r\\n\", (int)strlen(body));\n");
    emit("        len += snprintf(request + len, 8192 - len, \"Content-Type: application/x-www-form-urlencoded\\r\\n\");\n");
    emit("    }\n");
    emit("    \n");
    emit("    len += snprintf(request + len, 8192 - len, \"Connection: close\\r\\n\\r\\n\");\n");
    emit("    if(body) len += snprintf(request + len, 8192 - len, \"%%s\", body);\n");
    emit("    \n");
    emit("    // Send\n");
    emit("    if(send(sock, request, len, 0) < 0) {\n");
    emit("        free(request); close(sock);\n");
    emit("        snprintf(http_last_error, 256, \"Send failed\");\n");
    emit("        return NULL;\n");
    emit("    }\n");
    emit("    free(request);\n");
    emit("    \n");
    emit("    // Read response with dynamic allocation\n");
    emit("    size_t capacity = 65536, total = 0;\n");
    emit("    char* response = malloc(capacity);\n");
    emit("    int n;\n");
    emit("    while((n = recv(sock, response + total, capacity - total - 1, 0)) > 0) {\n");
    emit("        total += n;\n");
    emit("        if(total >= capacity - 1024) {\n");
    emit("            capacity *= 2;\n");
    emit("            char* new_resp = realloc(response, capacity);\n");
    emit("            if(!new_resp) { free(response); close(sock); return NULL; }\n");
    emit("            response = new_resp;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    response[total] = 0;\n");
    emit("    close(sock);\n");
    emit("    \n");
    emit("    if(total == 0) {\n");
    emit("        free(response);\n");
    emit("        snprintf(http_last_error, 256, \"Empty response\");\n");
    emit("        return NULL;\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Parse status\n");
    emit("    http_last_status = 0;\n");
    emit("    if(strncmp(response, \"HTTP/\", 5) == 0) {\n");
    emit("        char* space = strchr(response, ' ');\n");
    emit("        if(space) http_last_status = atoi(space + 1);\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Find body\n");
    emit("    char* body_start = strstr(response, \"\\r\\n\\r\\n\");\n");
    emit("    if(body_start) {\n");
    emit("        body_start += 4;\n");
    emit("        \n");
    emit("        // Handle chunked transfer encoding\n");
    emit("        if(strstr(response, \"Transfer-Encoding: chunked\") || strstr(response, \"transfer-encoding: chunked\")) {\n");
    emit("            char* result = malloc(capacity);\n");
    emit("            char* dst = result;\n");
    emit("            char* src = body_start;\n");
    emit("            while(*src) {\n");
    emit("                int chunk_size;\n");
    emit("                if(sscanf(src, \"%%x\", &chunk_size) != 1) break;\n");
    emit("                if(chunk_size == 0) break;\n");
    emit("                src = strchr(src, '\\n');\n");
    emit("                if(!src) break;\n");
    emit("                src++;\n");
    emit("                memcpy(dst, src, chunk_size);\n");
    emit("                dst += chunk_size;\n");
    emit("                src += chunk_size + 2;\n");
    emit("            }\n");
    emit("            *dst = 0;\n");
    emit("            free(response);\n");
    emit("            return result;\n");
    emit("        }\n");
    emit("        \n");
    emit("        char* result = malloc(strlen(body_start) + 1);\n");
    emit("        strcpy(result, body_start);\n");
    emit("        free(response);\n");
    emit("        return result;\n");
    emit("    }\n");
    emit("    \n");
    emit("    return response;\n");
    emit("}\n\n");
    
    // Simple HTTPS support (basic TLS wrapper)
    emit("char* https_get(const char* url) {\n");
    emit("    // For HTTPS, we'll use a simple approach - call curl if available\n");
    emit("    char cmd[1024];\n");
    emit("    snprintf(cmd, 1024, \"curl -s '%%s' 2>/dev/null\", url);\n");
    emit("    FILE* fp = popen(cmd, \"r\");\n");
    emit("    if (!fp) return NULL;\n");
    emit("    \n");
    emit("    char* response = malloc(65536);\n");
    emit("    size_t len = fread(response, 1, 65535, fp);\n");
    emit("    response[len] = 0;\n");
    emit("    pclose(fp);\n");
    emit("    \n");
    emit("    return len > 0 ? response : NULL;\n");
    emit("}\n");
    emit("char* https_post(const char* url, const char* data) {\n");
    emit("    char cmd[2048];\n");
    emit("    snprintf(cmd, 2048, \"curl -s -X POST -d '%%s' '%%s' 2>/dev/null\", data, url);\n");
    emit("    FILE* fp = popen(cmd, \"r\");\n");
    emit("    if (!fp) return NULL;\n");
    emit("    \n");
    emit("    char* response = malloc(65536);\n");
    emit("    size_t len = fread(response, 1, 65535, fp);\n");
    emit("    response[len] = 0;\n");
    emit("    pclose(fp);\n");
    emit("    \n");
    emit("    return len > 0 ? response : NULL;\n");
    emit("}\n\n");
    
    emit("char* http_get(const char* url) { return http_request(\"GET\", url, NULL); }\n");
    emit("char* http_post(const char* url, const char* data) { return http_request(\"POST\", url, data); }\n");
    emit("char* http_put(const char* url, const char* data) { return http_request(\"PUT\", url, data); }\n");
    emit("char* http_delete(const char* url) { return http_request(\"DELETE\", url, NULL); }\n\n");
    
    emit("void http_set_header(const char* key, const char* val) {\n");
    emit("    if(http_header_count < 32) {\n");
    emit("        char* header = malloc(512);\n");
    emit("        snprintf(header, 512, \"%%s: %%s\", key, val);\n");
    emit("        http_headers[http_header_count++] = header;\n");
    emit("    }\n");
    emit("}\n\n");
    
    emit("void http_clear_headers() {\n");
    emit("    for(int i = 0; i < http_header_count; i++) free(http_headers[i]);\n");
    emit("    http_header_count = 0;\n");
    emit("}\n\n");
    
    emit("int http_status() { return http_last_status; }\n");
    emit("char* http_error() { return http_last_error[0] ? http_last_error : NULL; }\n");
    emit("char* last_error_get() { return last_error[0] ? last_error : NULL; }\n\n");
    
    // Simple JSON functions
    emit("char* json_get_str(const char* json, const char* key) {\n");
    emit("    char search[256];\n");
    emit("    snprintf(search, 256, \"\\\\\\\"%%s\\\\\\\":\", key);\n");
    emit("    char* pos = strstr(json, search);\n");
    emit("    if(!pos) return NULL;\n");
    emit("    pos += strlen(search);\n");
    emit("    while(*pos == ' ' || *pos == '\\\"') pos++;\n");
    emit("    char* end = pos;\n");
    emit("    while(*end && *end != '\\\"' && *end != ',' && *end != '}') end++;\n");
    emit("    int len = end - pos;\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    strncpy(result, pos, len);\n");
    emit("    result[len] = 0;\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    emit("int json_get_int(const char* json, const char* key) {\n");
    emit("    char* val = json_get_str(json, key);\n");
    emit("    return val ? atoi(val) : 0;\n");
    emit("}\n\n");
    
    emit("int json_get_bool(const char* json, const char* key) {\n");
    emit("    char* val = json_get_str(json, key);\n");
    emit("    if(!val) return 0;\n");
    emit("    return strcmp(val, \"true\") == 0 || strcmp(val, \"1\") == 0;\n");
    emit("}\n\n");
    
    emit("int json_has_key(const char* json, const char* key) {\n");
    emit("    char search[256];\n");
    emit("    snprintf(search, 256, \"\\\\\\\"%%s\\\\\\\":\", key);\n");
    emit("    return strstr(json, search) != NULL;\n");
    emit("}\n\n");
    
    emit("char* json_stringify_int(int val) {\n");
    emit("    char* r = malloc(20);\n");
    emit("    sprintf(r, \"%%d\", val);\n");
    emit("    return r;\n");
    emit("}\n\n");
    
    emit("char* json_stringify_str(const char* val) {\n");
    emit("    char* r = malloc(strlen(val) + 3);\n");
    emit("    sprintf(r, \"\\\\\\\"%%s\\\\\\\"\", val);\n");
    emit("    return r;\n");
    emit("}\n\n");
    
    emit("char* json_stringify_bool(int val) {\n");
    emit("    return val ? \"true\" : \"false\";\n");
    emit("}\n\n");
    
    emit("char* json_array_stringify(int* arr, int len) {\n");
    emit("    char* r = malloc(len * 20 + 10);\n");
    emit("    strcpy(r, \"[\");\n");
    emit("    for(int i = 0; i < len; i++) {\n");
    emit("        char buf[20];\n");
    emit("        sprintf(buf, \"%%d\", arr[i]);\n");
    emit("        strcat(r, buf);\n");
    emit("        if(i < len-1) strcat(r, \",\");\n");
    emit("    }\n");
    emit("    strcat(r, \"]\");\n");
    emit("    return r;\n");
    emit("}\n\n");
    
    emit("int json_array_length(const char* json) {\n");
    emit("    int count = 0;\n");
    emit("    int depth = 0;\n");
    emit("    for(const char* p = json; *p; p++) {\n");
    emit("        if(*p == '[') depth++;\n");
    emit("        else if(*p == ']') depth--;\n");
    emit("        else if(*p == ',' && depth == 1) count++;\n");
    emit("    }\n");
    emit("    return count > 0 ? count + 1 : 0;\n");
    emit("}\n\n");
    
    emit("char* json_array_get(const char* json, int index) {\n");
    emit("    int count = 0;\n");
    emit("    int depth = 0;\n");
    emit("    const char* start = NULL;\n");
    emit("    for(const char* p = json; *p; p++) {\n");
    emit("        if(*p == '[') { depth++; if(depth == 1 && count == index) start = p + 1; }\n");
    emit("        else if(*p == ']') depth--;\n");
    emit("        else if(*p == ',' && depth == 1) {\n");
    emit("            if(count == index && start) {\n");
    emit("                int len = p - start;\n");
    emit("                char* r = malloc(len + 1);\n");
    emit("                strncpy(r, start, len);\n");
    emit("                r[len] = 0;\n");
    emit("                return r;\n");
    emit("            }\n");
    emit("            count++;\n");
    emit("            if(count == index) start = p + 1;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    return NULL;\n");
    emit("}\n\n");
    
    emit("char* url_encode(const char* str) {\n");
    emit("    char* result = malloc(strlen(str) * 3 + 1);\n");
    emit("    char* p = result;\n");
    emit("    while(*str) {\n");
    emit("        if((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || (*str >= '0' && *str <= '9') || *str == '-' || *str == '_' || *str == '.' || *str == '~') {\n");
    emit("            *p++ = *str;\n");
    emit("        } else if(*str == ' ') {\n");
    emit("            *p++ = '+';\n");
    emit("        } else {\n");
    emit("            sprintf(p, \"%%%%%%02X\", (unsigned char)*str);\n");
    emit("            p += 3;\n");
    emit("        }\n");
    emit("        str++;\n");
    emit("    }\n");
    emit("    *p = 0;\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    emit("char* url_decode(const char* str) {\n");
    emit("    char* result = malloc(strlen(str) + 1);\n");
    emit("    char* p = result;\n");
    emit("    while(*str) {\n");
    emit("        if(*str == '%%' && str[1] && str[2]) {\n");
    emit("            int val;\n");
    emit("            sscanf(str + 1, \"%%2x\", &val);\n");
    emit("            *p++ = val;\n");
    emit("            str += 3;\n");
    emit("        } else if(*str == '+') {\n");
    emit("            *p++ = ' ';\n");
    emit("            str++;\n");
    emit("        } else {\n");
    emit("            *p++ = *str++;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    *p = 0;\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    emit("char* base64_encode(const char* str) {\n");
    emit("    static const char* b64 = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n");
    emit("    int len = strlen(str);\n");
    emit("    char* out = malloc(((len + 2) / 3) * 4 + 1);\n");
    emit("    int i = 0, j = 0;\n");
    emit("    while(i < len) {\n");
    emit("        uint32_t a = i < len ? (unsigned char)str[i++] : 0;\n");
    emit("        uint32_t b = i < len ? (unsigned char)str[i++] : 0;\n");
    emit("        uint32_t c = i < len ? (unsigned char)str[i++] : 0;\n");
    emit("        uint32_t triple = (a << 16) + (b << 8) + c;\n");
    emit("        out[j++] = b64[(triple >> 18) & 0x3F];\n");
    emit("        out[j++] = b64[(triple >> 12) & 0x3F];\n");
    emit("        out[j++] = b64[(triple >> 6) & 0x3F];\n");
    emit("        out[j++] = b64[triple & 0x3F];\n");
    emit("    }\n");
    emit("    for(int k = 0; k < (3 - len %% 3) %% 3; k++) out[j - 1 - k] = '=';\n");
    emit("    out[j] = 0;\n");
    emit("    return out;\n");
    emit("}\n\n");
    
    emit("int hash_string(const char* str) {\n");
    emit("    unsigned int hash = 5381;\n");
    emit("    int c;\n");
    emit("    while((c = *str++)) hash = ((hash << 5) + hash) + c;\n");
    emit("    return (int)hash;\n");
    emit("}\n\n");
    
    // Simple variadic print function
    emit("void print_args_impl(int count, ...) {\n");
    emit("    va_list args;\n");
    emit("    va_start(args, count);\n");
    emit("    for (int i = 0; i < count; i++) {\n");
    emit("        if (i > 0) printf(\" \");\n");
    emit("        void* arg = va_arg(args, void*);\n");
    emit("        // Simple heuristic: if it looks like a small integer, print as int\n");
    emit("        if ((intptr_t)arg >= -1000000 && (intptr_t)arg <= 1000000) {\n");
    emit("            printf(\"%%d\", (int)(intptr_t)arg);\n");
    emit("        } else {\n");
    emit("            printf(\"%%s\", (char*)arg);\n");
    emit("        }\n");
    emit("    }\n");
    emit("    printf(\"\\n\");\n");
    emit("    va_end(args);\n");
    emit("}\n\n");
    
    // Keep individual print functions for backward compatibility
    emit("void print_int(int x) { printf(\"%%d\\n\", x); }\n");
    emit("void print_float(double x) { printf(\"%%f\\n\", x); }\n");
    emit("void print_str(const char* s) { printf(\"%%s\\n\", s); }\n");
    emit("void print_bool(bool b) { printf(\"%%s\\n\", b ? \"true\" : \"false\"); }\n");
    emit("void print_int_no_nl(int x) { printf(\"%%d\", x); }\n");
    emit("void print_float_no_nl(double x) { printf(\"%%f\", x); }\n");
    emit("void print_str_no_nl(const char* s) { printf(\"%%s\", s); }\n");
    emit("void print_bool_no_nl(bool b) { printf(\"%%s\", b ? \"true\" : \"false\"); }\n");
    emit("void print_array(WynArray arr) { printf(\"[\"); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(\", \"); printf(\"%%d\", array_get_int(arr, i)); } printf(\"]\\n\"); }\n");
    emit("void print_array_no_nl(WynArray arr) { printf(\"[\"); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(\", \"); printf(\"%%d\", array_get_int(arr, i)); } printf(\"]\"); }\n");
    
    emit("#define print_no_nl(x) _Generic((x), \\\n");
    emit("    int: print_int_no_nl, \\\n");
    emit("    double: print_float_no_nl, \\\n");
    emit("    char*: print_str_no_nl, \\\n");
    emit("    const char*: print_str_no_nl, \\\n");
    emit("    bool: print_bool_no_nl, \\\n");
    emit("    WynArray: print_array_no_nl, \\\n");
    emit("    default: print_int_no_nl)(x)\n\n");
    emit("void print_hex(int x) { printf(\"0x%%x\\n\", x); }\n");
    emit("void print_bin(int x) { for(int i = 31; i >= 0; i--) printf(\"%%d\", (x >> i) & 1); printf(\"\\n\"); }\n");
    emit("void println() { printf(\"\\n\"); }\n");
    emit("void print_debug(const char* label, int val) { printf(\"%%s: %%d\\n\", label, val); }\n");
    
    // Generic print function for backward compatibility with method calls
    emit("#define print(x) _Generic((x), \\\n");
    emit("    int: print_int, \\\n");
    emit("    double: print_float, \\\n");
    emit("    char*: print_str, \\\n");
    emit("    const char*: print_str, \\\n");
    emit("    bool: print_bool, \\\n");
    emit("    WynArray: print_array, \\\n");
    emit("    default: print_int)(x)\n\n");
    emit("int input() { int x = 0; if (scanf(\"%%d\", &x) != 1) { while(getchar() != '\\n') { /* clear input buffer */ } return 0; } return x; }\n");
    emit("float input_float() { float x = 0.0f; if (scanf(\"%%f\", &x) != 1) { while(getchar() != '\\n') { /* clear input buffer */ } return 0.0f; } return x; }\n");
    emit("char* input_line() { static char buffer[1024]; if (fgets(buffer, sizeof(buffer), stdin)) { size_t len = strlen(buffer); if (len > 0 && buffer[len-1] == '\\n') buffer[len-1] = '\\0'; return buffer; } return \"\"; }\n");
    emit("void printf_wyn(const char* format, ...) { va_list args; va_start(args, format); vprintf(format, args); va_end(args); }\n");
    emit("double sin_approx(double x) { return x - (x*x*x)/6 + (x*x*x*x*x)/120; }\n");
    emit("double cos_approx(double x) { return 1 - (x*x)/2 + (x*x*x*x)/24; }\n");
    emit("double pi_const() { return 3.14159265359; }\n");
    emit("double e_const() { return 2.71828182846; }\n");
    emit("int str_len(const char* s) { return strlen(s); }\n");
    emit("int str_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }\n");
    emit("char* str_concat(const char* a, const char* b) { char* r = malloc(strlen(a) + strlen(b) + 1); strcpy(r, a); strcat(r, b); return r; }\n");
    emit("char* str_upper(const char* s) { char* r = malloc(strlen(s) + 1); for(int i = 0; s[i]; i++) r[i] = toupper(s[i]); r[strlen(s)] = 0; return r; }\n");
    emit("char* str_lower(const char* s) { char* r = malloc(strlen(s) + 1); for(int i = 0; s[i]; i++) r[i] = tolower(s[i]); r[strlen(s)] = 0; return r; }\n");
    emit("int str_contains(const char* s, const char* sub) { return strstr(s, sub) != NULL; }\n");
    emit("int str_starts_with(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }\n");
    emit("int str_ends_with(const char* s, const char* suffix) { int sl = strlen(s); int pl = strlen(suffix); return sl >= pl && strcmp(s + sl - pl, suffix) == 0; }\n");
    emit("char* str_trim(const char* s) { while(*s == ' ') s++; int len = strlen(s); while(len > 0 && s[len-1] == ' ') len--; char* r = malloc(len + 1); strncpy(r, s, len); r[len] = 0; return r; }\n");
    emit("char* str_repeat(const char* s, int count) { int len = strlen(s); char* r = malloc(len * count + 1); r[0] = 0; for(int i = 0; i < count; i++) strcat(r, s); return r; }\n");
    emit("char* str_reverse(const char* s) { int len = strlen(s); char* r = malloc(len + 1); for(int i = 0; i < len; i++) r[i] = s[len-1-i]; r[len] = 0; return r; }\n");
    
    // String conversion functions for interpolation
    emit("char* int_to_string(int x) { char* r = malloc(32); sprintf(r, \"%%d\", x); return r; }\n");
    emit("char* float_to_string(double x) { char* r = malloc(32); sprintf(r, \"%%g\", x); return r; }\n");
    emit("char* bool_to_string(bool x) { char* r = malloc(8); strcpy(r, x ? \"true\" : \"false\"); return r; }\n");
    emit("char* str_to_string(const char* x) { return (char*)x; }\n");
    
    emit("#define to_string(x) _Generic((x), \\\n");
    emit("    int: int_to_string, \\\n");
    emit("    double: float_to_string, \\\n");
    emit("    char*: str_to_string, \\\n");
    emit("    const char*: str_to_string, \\\n");
    emit("    bool: bool_to_string, \\\n");
    emit("    default: int_to_string)(x)\n\n");
    
    // Error handling functions
    emit("typedef struct { const char* message; const char* type; } WynError;\n");
    emit("WynError Error(const char* msg) { WynError e = {msg, \"Error\"}; return e; }\n");
    emit("WynError TypeError(const char* msg) { WynError e = {msg, \"TypeError\"}; return e; }\n");
    emit("WynError ValueError(const char* msg) { WynError e = {msg, \"ValueError\"}; return e; }\n");
    emit("WynError DivisionByZeroError(const char* msg) { WynError e = {msg, \"DivisionByZeroError\"}; return e; }\n");
    emit("void print_error(WynError err) { printf(\"%%s: %%s\", err.type, err.message); }\n\n");
    emit("char* str_substring(const char* s, int start, int end) { int len = strlen(s); if(start < 0) start = 0; if(end > len) end = len; if(start >= end) return malloc(1); int sublen = end - start; char* r = malloc(sublen + 1); strncpy(r, s + start, sublen); r[sublen] = 0; return r; }\n");
    emit("int str_index_of(const char* s, const char* sub) { char* p = strstr(s, sub); return p ? (int)(p - s) : -1; }\n");
    emit("char* str_slice(const char* s, int start, int end) { return str_substring(s, start, end); }\n");
    emit("char* str_pad_start(const char* s, int len, const char* pad) { int slen = strlen(s); if(slen >= len) { char* r = malloc(slen + 1); strcpy(r, s); return r; } int padlen = len - slen; char* r = malloc(len + 1); for(int i = 0; i < padlen; i++) r[i] = pad[0]; strcpy(r + padlen, s); return r; }\n");
    emit("char* str_pad_end(const char* s, int len, const char* pad) { int slen = strlen(s); if(slen >= len) { char* r = malloc(slen + 1); strcpy(r, s); return r; } char* r = malloc(len + 1); strcpy(r, s); for(int i = slen; i < len; i++) r[i] = pad[0]; r[len] = 0; return r; }\n");
    emit("char* str_remove_prefix(const char* s, const char* prefix) { int plen = strlen(prefix); if(strncmp(s, prefix, plen) == 0) { char* r = malloc(strlen(s) - plen + 1); strcpy(r, s + plen); return r; } char* r = malloc(strlen(s) + 1); strcpy(r, s); return r; }\n");
    emit("char* str_remove_suffix(const char* s, const char* suffix) { int slen = strlen(s); int suflen = strlen(suffix); if(slen >= suflen && strcmp(s + slen - suflen, suffix) == 0) { char* r = malloc(slen - suflen + 1); strncpy(r, s, slen - suflen); r[slen - suflen] = 0; return r; } char* r = malloc(slen + 1); strcpy(r, s); return r; }\n");
    emit("char* str_capitalize(const char* s) { char* r = malloc(strlen(s) + 1); strcpy(r, s); if(r[0]) r[0] = toupper(r[0]); for(int i = 1; r[i]; i++) r[i] = tolower(r[i]); return r; }\n");
    emit("char* str_center(const char* s, int width) { int len = strlen(s); if(len >= width) { char* r = malloc(len + 1); strcpy(r, s); return r; } int pad = (width - len) / 2; char* r = malloc(width + 1); for(int i = 0; i < pad; i++) r[i] = ' '; strcpy(r + pad, s); for(int i = pad + len; i < width; i++) r[i] = ' '; r[width] = 0; return r; }\n");
    emit("char** str_lines(const char* s) { char** lines = malloc(sizeof(char*)); lines[0] = malloc(strlen(s) + 1); strcpy(lines[0], s); return lines; }\n");
    emit("char** str_words(const char* s) { char** words = malloc(sizeof(char*)); words[0] = malloc(strlen(s) + 1); strcpy(words[0], s); return words; }\n");
    emit("void str_free(char* s) { if(s) free(s); }\n");
    emit("int str_parse_int(const char* s) { return atoi(s); }\n");
    emit("double str_parse_float(const char* s) { return atof(s); }\n");
    emit("int abs_val(int x) { return x < 0 ? -x : x; }\n");
    emit("int min(int a, int b) { return a < b ? a : b; }\n");
    emit("int max(int a, int b) { return a > b ? a : b; }\n");
    emit("int pow_int(int base, int exp) { int r = 1; for(int i = 0; i < exp; i++) r *= base; return r; }\n");
    emit("int clamp(int x, int min_val, int max_val) { return x < min_val ? min_val : (x > max_val ? max_val : x); }\n");
    emit("int sign(int x) { return x < 0 ? -1 : (x > 0 ? 1 : 0); }\n");
    emit("int gcd(int a, int b) { while(b) { int t = b; b = a %% b; a = t; } return a; }\n");
    emit("int lcm(int a, int b) { return a * b / gcd(a, b); }\n");
    emit("int is_even(int x) { return x %% 2 == 0; }\n");
    emit("int is_odd(int x) { return x %% 2 != 0; }\n");
    emit("char* file_read(const char* path) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* f = fopen(path, \"r\");\n");
    emit("    if(!f) { snprintf(last_error, 256, \"Cannot open file: %%s\", path); return NULL; }\n");
    emit("    fseek(f, 0, SEEK_END);\n");
    emit("    long sz = ftell(f);\n");
    emit("    fseek(f, 0, SEEK_SET);\n");
    emit("    char* buf = malloc(sz + 1);\n");
    emit("    if(!buf) { snprintf(last_error, 256, \"Out of memory\"); fclose(f); return NULL; }\n");
    emit("    fread(buf, 1, sz, f);\n");
    emit("    buf[sz] = 0;\n");
    emit("    fclose(f);\n");
    emit("    return buf;\n");
    emit("}\n");
    emit("int file_write(const char* path, const char* data) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* f = fopen(path, \"w\");\n");
    emit("    if(!f) { snprintf(last_error, 256, \"Cannot write file: %%s\", path); return 0; }\n");
    emit("    fputs(data, f);\n");
    emit("    fclose(f);\n");
    emit("    return 1;\n");
    emit("}\n");
    emit("int file_exists(const char* path) { FILE* f = fopen(path, \"r\"); if(f) { fclose(f); return 1; } return 0; }\n");
    emit("int file_size(const char* path) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* f = fopen(path, \"r\");\n");
    emit("    if(!f) { snprintf(last_error, 256, \"Cannot open file: %%s\", path); return -1; }\n");
    emit("    fseek(f, 0, SEEK_END);\n");
    emit("    long sz = ftell(f);\n");
    emit("    fclose(f);\n");
    emit("    return (int)sz;\n");
    emit("}\n");
    emit("int file_delete(const char* path) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    int result = remove(path);\n");
    emit("    if(result != 0) snprintf(last_error, 256, \"Cannot delete file: %%s\", path);\n");
    emit("    return result == 0;\n");
    emit("}\n");
    emit("int file_append(const char* path, const char* data) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* f = fopen(path, \"a\");\n");
    emit("    if(!f) { snprintf(last_error, 256, \"Cannot append to file: %%s\", path); return 0; }\n");
    emit("    fputs(data, f);\n");
    emit("    fclose(f);\n");
    emit("    return 1;\n");
    emit("}\n");
    emit("int file_copy(const char* src, const char* dst) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* s = fopen(src, \"r\");\n");
    emit("    if(!s) { snprintf(last_error, 256, \"Cannot open source: %%s\", src); return 0; }\n");
    emit("    FILE* d = fopen(dst, \"w\");\n");
    emit("    if(!d) { snprintf(last_error, 256, \"Cannot open destination: %%s\", dst); fclose(s); return 0; }\n");
    emit("    char buf[4096];\n");
    emit("    size_t n;\n");
    emit("    while((n = fread(buf, 1, 4096, s)) > 0) fwrite(buf, 1, n, d);\n");
    emit("    fclose(s);\n");
    emit("    fclose(d);\n");
    emit("    return 1;\n");
    emit("}\n");
    emit("int arr_sum(WynArray arr, int len) { int s = 0; for(int i = 0; i < len; i++) s += array_get_int(arr, i); return s; }\n");
    emit("int arr_max(WynArray arr, int len) { int m = array_get_int(arr, 0); for(int i = 1; i < len; i++) { int val = array_get_int(arr, i); if(val > m) m = val; } return m; }\n");
    emit("int arr_min(WynArray arr, int len) { int m = array_get_int(arr, 0); for(int i = 1; i < len; i++) { int val = array_get_int(arr, i); if(val < m) m = val; } return m; }\n");
    emit("int arr_contains(WynArray arr, int len, int val) { for(int i = 0; i < len; i++) if(array_get_int(arr, i) == val) return 1; return 0; }\n");
    emit("int arr_find(WynArray arr, int len, int val) { for(int i = 0; i < len; i++) if(array_get_int(arr, i) == val) return i; return -1; }\n");
    emit("void arr_reverse(int* arr, int len) { for(int i = 0; i < len/2; i++) { int t = arr[i]; arr[i] = arr[len-1-i]; arr[len-1-i] = t; } }\n");
    emit("void arr_sort(int* arr, int len) { for(int i = 0; i < len-1; i++) for(int j = 0; j < len-i-1; j++) if(arr[j] > arr[j+1]) { int t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t; } }\n");
    emit("int arr_count(int* arr, int len, int val) { int c = 0; for(int i = 0; i < len; i++) if(arr[i] == val) c++; return c; }\n");
    emit("void arr_fill(int* arr, int len, int val) { for(int i = 0; i < len; i++) arr[i] = val; }\n");
    emit("int arr_all(int* arr, int len, int val) { for(int i = 0; i < len; i++) if(arr[i] != val) return 0; return 1; }\n");
    emit("char* arr_join(int* arr, int len, const char* sep) { int total = 0; for(int i = 0; i < len; i++) { total += snprintf(NULL, 0, \"%%d\", arr[i]); if(i < len-1) total += strlen(sep); } char* r = malloc(total + 1); r[0] = 0; for(int i = 0; i < len; i++) { char buf[32]; snprintf(buf, 32, \"%%d\", arr[i]); strcat(r, buf); if(i < len-1) strcat(r, sep); } return r; }\n");
    emit("WynArray arr_map_double(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); array_push_int(&result, val * 2); } return result; }\n");
    emit("WynArray arr_map_square(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); array_push_int(&result, val * val); } return result; }\n");
    emit("WynArray arr_filter_positive(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val > 0) array_push_int(&result, val); } return result; }\n");
    emit("WynArray arr_filter_even(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val %% 2 == 0) array_push_int(&result, val); } return result; }\n");
    emit("WynArray arr_filter_greater_than_3(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val > 3) array_push_int(&result, val); } return result; }\n");
    emit("int arr_reduce_sum(WynArray arr) { int result = 0; for(int i = 0; i < arr.count; i++) { result += array_get_int(arr, i); } return result; }\n");
    emit("int arr_reduce_product(WynArray arr) { int result = 1; for(int i = 0; i < arr.count; i++) { result *= array_get_int(arr, i); } return result; }\n");
    emit("int random_int(int max) { return rand() %% max; }\n");
    emit("int random_range(int min, int max) { return min + rand() %% (max - min + 1); }\n");
    emit("double random_float() { return (double)rand() / RAND_MAX; }\n");
    emit("void seed_random(int seed) { srand(seed); }\n");
    emit("int time_now() { return (int)time(NULL); }\n");
    emit("char* time_format(int timestamp, const char* fmt) {\n");
    emit("    time_t t = (time_t)timestamp;\n");
    emit("    struct tm* tm_info = localtime(&t);\n");
    emit("    char* buffer = malloc(80);\n");
    emit("    strftime(buffer, 80, fmt, tm_info);\n");
    emit("    return buffer;\n");
    emit("}\n");
    emit("void assert_eq(int a, int b) { if (a != b) { printf(\"Assertion failed: %%d != %%d\\n\", a, b); exit(1); } }\n");
    emit("void assert_true(int cond) { if (!cond) { printf(\"Assertion failed\\n\"); exit(1); } }\n");
    emit("void assert_false(int cond) { if (cond) { printf(\"Assertion failed\\n\"); exit(1); } }\n");
    emit("void panic(const char* msg) { printf(\"Panic: %%s\\n\", msg); exit(1); }\n");
    emit("void todo(const char* msg) { printf(\"TODO: %%s\\n\", msg); exit(1); }\n");
    emit("void exit_program(int code) { exit(code); }\n");
    emit("void sleep_ms(int ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms %% 1000) * 1000000; nanosleep(&ts, NULL); }\n");
    emit("char* getenv_var(const char* name) { return getenv(name); }\n");
    emit("int setenv_var(const char* name, const char* val) { return setenv(name, val, 1) == 0; }\n");
    emit("int sqrt_int(int x) { return (int)sqrt(x); }\n");
    emit("int ceil_int(double x) { return (int)ceil(x); }\n");
    emit("int floor_int(double x) { return (int)floor(x); }\n");
    emit("int round_int(double x) { return (int)round(x); }\n");
    emit("double abs_float(double x) { return fabs(x); }\n");
    emit("char* str_replace(const char* s, const char* old, const char* new) { int count = 0; const char* p = s; int oldlen = strlen(old); int newlen = strlen(new); while((p = strstr(p, old))) { count++; p += oldlen; } int total = strlen(s) + count * (newlen - oldlen) + 1; char* r = malloc(total); char* dst = r; p = s; while(*p) { if(strncmp(p, old, oldlen) == 0) { memcpy(dst, new, newlen); dst += newlen; p += oldlen; } else { *dst++ = *p++; } } *dst = 0; return r; }\n");
    emit("char** str_split(const char* s, const char* delim, int* count) { char** r = malloc(100 * sizeof(char*)); *count = 0; char* copy = malloc(strlen(s) + 1); strcpy(copy, s); char* tok = strtok(copy, delim); while(tok && *count < 100) { r[*count] = malloc(strlen(tok) + 1); strcpy(r[*count], tok); (*count)++; tok = strtok(NULL, delim); } return r; }\n");
    emit("char* str_join(char** arr, int len, const char* sep) { int total = 0; for(int i = 0; i < len; i++) total += strlen(arr[i]); total += (len - 1) * strlen(sep) + 1; char* r = malloc(total); r[0] = 0; for(int i = 0; i < len; i++) { if(i > 0) strcat(r, sep); strcat(r, arr[i]); } return r; }\n");
    emit("char* int_to_str(int n) { char* r = malloc(12); sprintf(r, \"%%d\", n); return r; }\n");
    emit("int str_to_int(const char* s) { return atoi(s); }\n");
    emit("void swap(int* a, int* b) { int t = *a; *a = *b; *b = t; }\n");
    emit("double clamp_float(double x, double min_val, double max_val) { return x < min_val ? min_val : (x > max_val ? max_val : x); }\n");
    emit("double lerp(double a, double b, double t) { return a + t * (b - a); }\n");
    emit("double map_range(double x, double in_min, double in_max, double out_min, double out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }\n");
    emit("int bit_set(int x, int pos) { return x | (1 << pos); }\n");
    emit("int bit_clear(int x, int pos) { return x & ~(1 << pos); }\n");
    emit("int bit_toggle(int x, int pos) { return x ^ (1 << pos); }\n");
    emit("int bit_check(int x, int pos) { return (x >> pos) & 1; }\n");
    emit("int bit_count(int x) { int c = 0; while(x) { c += x & 1; x >>= 1; } return c; }\n\n");
    
    // ARC (Automatic Reference Counting) functions for struct lifecycle management
    emit("// ARC Object Header\n");
    emit("typedef struct WynObject {\n");
    emit("    int ref_count;\n");
    emit("    void (*destructor)(void*);\n");
    emit("    char data[];\n");
    emit("} WynObject;\n\n");
    
    emit("// ARC function implementations\n");
    emit("WynObject* wyn_arc_alloc(size_t size) {\n");
    emit("    WynObject* obj = malloc(sizeof(WynObject) + size);\n");
    emit("    if (obj) {\n");
    emit("        obj->ref_count = 1;\n");
    emit("        obj->destructor = NULL;\n");
    emit("    }\n");
    emit("    return obj;\n");
    emit("}\n\n");
    
    emit("WynObject* wyn_arc_retain(WynObject* obj) {\n");
    emit("    if (obj) {\n");
    emit("        obj->ref_count++;\n");
    emit("    }\n");
    emit("    return obj;\n");
    emit("}\n\n");
    
    emit("void wyn_arc_release(WynObject* obj) {\n");
    emit("    if (obj) {\n");
    emit("        obj->ref_count--;\n");
    emit("        if (obj->ref_count <= 0) {\n");
    emit("            if (obj->destructor) {\n");
    emit("                obj->destructor(obj->data);\n");
    emit("            }\n");
    emit("            free(obj);\n");
    emit("        }\n");
    emit("    }\n");
    emit("}\n\n");
}

void codegen_stmt(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_EXPR:
            codegen_expr(stmt->expr);
            emit(";\n");
            break;
        case STMT_VAR: {
            // Determine C type based on initializer
            const char* c_type = "int";
            if (stmt->var.init) {
                if (stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                } else if (stmt->var.init->type == EXPR_STRING_INTERP) {
                    c_type = "char*";
                } else if (stmt->var.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (stmt->var.init->type == EXPR_BOOL) {
                    c_type = "bool";
                } else if (stmt->var.init->type == EXPR_ARRAY) {
                    c_type = "WynArray";
                } else if (stmt->var.init->type == EXPR_MAP) {
                    // Map type - use the typedef
                    c_type = "WynMap";
                } else if (stmt->var.init->type == EXPR_CALL) {
                    // Function call - use type information from checker
                    if (stmt->var.init->expr_type) {
                        if (stmt->var.init->expr_type->kind == TYPE_ARRAY) {
                            c_type = "WynArray";
                        } else if (stmt->var.init->expr_type->kind == TYPE_STRING) {
                            c_type = "char*";
                        } else if (stmt->var.init->expr_type->kind == TYPE_FLOAT) {
                            c_type = "double";
                        } else if (stmt->var.init->expr_type->kind == TYPE_BOOL) {
                            c_type = "bool";
                        } else if (stmt->var.init->expr_type->kind == TYPE_STRUCT) {
                            // Use struct type name
                            static char struct_type[64];
                            snprintf(struct_type, 64, "%.*s", 
                                     stmt->var.init->expr_type->struct_type.name.length,
                                     stmt->var.init->expr_type->struct_type.name.start);
                            c_type = struct_type;
                        }
                        // Default to int for other types
                    }
                } else if (stmt->var.init->type == EXPR_STRUCT_INIT) {
                    // Use the struct type name
                    static char struct_type[64];
                    snprintf(struct_type, 64, "%.*s", 
                             stmt->var.init->struct_init.type_name.length,
                             stmt->var.init->struct_init.type_name.start);
                    c_type = struct_type;
                } else if (stmt->var.init->type == EXPR_BINARY) {
                    // Use type information from checker if available
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_STRING) {
                        c_type = "char*";
                    } else if (stmt->var.init->binary.op.type == TOKEN_PLUS) {
                        // Fallback: check if it's string concatenation
                        bool left_is_string = (stmt->var.init->binary.left->type == EXPR_STRING);
                        bool right_is_string = (stmt->var.init->binary.right->type == EXPR_STRING);
                        if (left_is_string || right_is_string) {
                            c_type = "char*";
                        }
                    }
                } else if (stmt->var.init->type == EXPR_CALL) {
                    // Check if it's a string-returning function
                    if (stmt->var.init->call.callee->type == EXPR_IDENT) {
                        Token fn_name = stmt->var.init->call.callee->token;
                        
                        // Check for array_new function
                        if (fn_name.length == 9 && memcmp(fn_name.start, "array_new", 9) == 0) {
                            c_type = "WynArray";
                        } else if (fn_name.length == 9 && memcmp(fn_name.start, "array_pop", 9) == 0) {
                            c_type = "void*";
                        } else {
                            const char* str_funcs[] = {"str_concat", "str_upper", "str_lower", "str_trim", 
                                                       "str_replace", "str_join", "int_to_str", "file_read",
                                                       "http_get", "http_post", "http_put", "http_delete", "getenv_var", 
                                                       "json_get_str", "json_stringify_int", "json_stringify_str", "json_array_get",
                                                       "url_encode", "url_decode", "base64_encode", "time_format"};
                            for (int i = 0; i < 21; i++) {
                                if (fn_name.length == (int)strlen(str_funcs[i]) &&
                                    memcmp(fn_name.start, str_funcs[i], fn_name.length) == 0) {
                                c_type = "char*";
                                break;
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_METHOD_CALL) {
                    // Check if it's a string method
                    Token method = stmt->var.init->method_call.method;
                    const char* str_methods[] = {"upper", "lower", "trim", "replace", "split", "toString", "join", "substring", "slice", "repeat", "reverse", "padStart", "padEnd", "removePrefix", "removeSuffix"};
                    for (int i = 0; i < 15; i++) {
                        if (method.length == (int)strlen(str_methods[i]) &&
                            memcmp(method.start, str_methods[i], method.length) == 0) {
                            c_type = "char*";
                            break;
                        }
                    }
                } else if (stmt->var.init->type == EXPR_PIPELINE) {
                    // For pipeline, check the last stage to determine type
                    if (stmt->var.init->pipeline.stage_count > 1) {
                        Expr* last_stage = stmt->var.init->pipeline.stages[stmt->var.init->pipeline.stage_count - 1];
                        if (last_stage->type == EXPR_IDENT) {
                            Token fn_name = last_stage->token;
                            const char* str_funcs[] = {"str_upper", "str_lower", "str_trim", "str_replace", "str_join", "int_to_str"};
                            for (int i = 0; i < 6; i++) {
                                if (fn_name.length == (int)strlen(str_funcs[i]) &&
                                    memcmp(fn_name.start, str_funcs[i], fn_name.length) == 0) {
                                    c_type = "char*";
                                    break;
                                }
                            }
                        }
                        }
                    }
                }
            }
            
            if (stmt->var.is_const) {
                emit("const %s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            } else {
                emit("%s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            }
            codegen_expr(stmt->var.init);
            emit(";\n");
            
            // Track string variables for automatic cleanup
            if (strcmp(c_type, "char*") == 0 && !stmt->var.is_const) {
                track_string_var(stmt->var.name.start, stmt->var.name.length);
            }
            break;
        }
        case STMT_RETURN:
            emit("return ");
            codegen_expr(stmt->ret.value);
            emit(";\n");
            break;
        case STMT_BREAK:
            emit("break;\n");
            break;
        case STMT_CONTINUE:
            emit("continue;\n");
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                emit("    ");
                codegen_stmt(stmt->block.stmts[i]);
            }
            break;
        case STMT_FN: {
            // Determine return type
            const char* return_type = "int"; // default
            if (stmt->fn.return_type) {
                if (stmt->fn.return_type->type == EXPR_IDENT) {
                    Token type_name = stmt->fn.return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        return_type = "int";
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        return_type = "char*";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        return_type = "WynArray";
                    }
                } else if (stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                    // T2.5.1: Handle optional return types - for now, treat as the inner type
                    Expr* inner = stmt->fn.return_type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT) {
                        Token type_name = inner->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            return_type = "int";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            return_type = "char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            return_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            return_type = "bool";
                        }
                    }
                }
            }
            
            emit("%s %.*s(", return_type, stmt->fn.name.length, stmt->fn.name.start);
            for (int i = 0; i < stmt->fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "int"; // default
                if (stmt->fn.param_types[i]) {
                    if (stmt->fn.param_types[i]->type == EXPR_IDENT) {
                        Token type_name = stmt->fn.param_types[i]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "int";
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = "WynArray";
                        }
                    } else if (stmt->fn.param_types[i]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (stmt->fn.param_types[i]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - for now, treat as the inner type
                        Expr* inner = stmt->fn.param_types[i]->optional_type.inner_type;
                        if (inner && inner->type == EXPR_IDENT) {
                            Token type_name = inner->token;
                            if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                param_type = "int";
                            } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                                param_type = "const char*";
                            } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                param_type = "const char*";
                            } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                param_type = "double";
                            } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                param_type = "bool";
                            }
                        }
                    }
                }
                
                emit("%s %.*s", param_type, stmt->fn.params[i].length, stmt->fn.params[i].start);
            }
            emit(") {\n");
            push_scope();  // Track allocations in this function
            codegen_stmt(stmt->fn.body);
            pop_scope();   // Auto-cleanup before function end
            emit("}\n\n");
            break;
        }
        case STMT_STRUCT:
            // T2.5.3: Enhanced struct system with ARC integration
            emit("typedef struct {\n");
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                // Convert Wyn type to C type
                const char* c_type = "int"; // default
                if (stmt->struct_decl.field_types[i]) {
                    if (stmt->struct_decl.field_types[i]->type == EXPR_IDENT) {
                        Token type_name = stmt->struct_decl.field_types[i]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            c_type = "int";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            c_type = "float";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            c_type = "bool";
                        }
                    } else if (stmt->struct_decl.field_types[i]->type == EXPR_ARRAY) {
                        // Array field type
                        c_type = "WynArray";
                    }
                }
                
                // Add ARC annotation comment for managed fields
                if (stmt->struct_decl.field_arc_managed[i]) {
                    emit("    %s %.*s; // ARC-managed\n", 
                         c_type,
                         stmt->struct_decl.fields[i].length,
                         stmt->struct_decl.fields[i].start);
                } else {
                    emit("    %s %.*s;\n", 
                         c_type,
                         stmt->struct_decl.fields[i].length,
                         stmt->struct_decl.fields[i].start);
                }
            }
            emit("} %.*s;\n\n", 
                 stmt->struct_decl.name.length,
                 stmt->struct_decl.name.start);
            
            // T2.5.3: Generate ARC cleanup function for struct
            emit("void %.*s_cleanup(%.*s* obj) {\n",
                 stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                 stmt->struct_decl.name.length, stmt->struct_decl.name.start);
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                if (stmt->struct_decl.field_arc_managed[i]) {
                    emit("    if (obj->%.*s) wyn_arc_release(obj->%.*s);\n",
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start,
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start);
                }
            }
            emit("}\n\n");
            break;
        case STMT_ENUM:
            emit("typedef enum {\n");
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                emit("    %.*s", 
                     stmt->enum_decl.variants[i].length,
                     stmt->enum_decl.variants[i].start);
                if (i < stmt->enum_decl.variant_count - 1) {
                    emit(",");
                }
                emit("\n");
            }
            emit("} %.*s;\n\n", 
                 stmt->enum_decl.name.length,
                 stmt->enum_decl.name.start);
            
            // Generate qualified constants for EnumName.MEMBER access
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                emit("#define %.*s_%.*s %d\n",
                     stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                     stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                     i);
            }
            emit("\n");
            
            // Generate toString function for enum
            emit("const char* %.*s_toString(%.*s val) {\n",
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start);
            emit("    switch(val) {\n");
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                emit("        case %.*s: return \"%.*s\";\n",
                     stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                     stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
            }
            emit("    }\n");
            emit("    return \"Unknown\";\n");
            emit("}\n\n");
            break;
        case STMT_TYPE_ALIAS:
            emit("typedef %.*s %.*s;\n\n",
                 stmt->type_alias.target.length,
                 stmt->type_alias.target.start,
                 stmt->type_alias.name.length,
                 stmt->type_alias.name.start);
            break;
        case STMT_IMPL:
            for (int i = 0; i < stmt->impl.method_count; i++) {
                FnStmt* method = stmt->impl.methods[i];
                emit("int %.*s_%.*s(", 
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                for (int j = 0; j < method->param_count; j++) {
                    if (j > 0) emit(", ");
                    emit("int %.*s", method->params[j].length, method->params[j].start);
                }
                emit(") {\n");
                codegen_stmt(method->body);
                emit("}\n\n");
            }
            break;
        case STMT_IF:
            emit("if (");
            codegen_expr(stmt->if_stmt.condition);
            emit(") {\n");
            push_scope();
            codegen_stmt(stmt->if_stmt.then_branch);
            pop_scope();
            emit("    }");
            if (stmt->if_stmt.else_branch) {
                emit(" else ");
                if (stmt->if_stmt.else_branch->type == STMT_IF) {
                    codegen_stmt(stmt->if_stmt.else_branch);
                } else {
                    emit("{\n");
                    push_scope();
                    codegen_stmt(stmt->if_stmt.else_branch);
                    pop_scope();
                    emit("    }\n");
                }
            } else {
                emit("\n");
            }
            break;
        case STMT_WHILE:
            emit("while (");
            codegen_expr(stmt->while_stmt.condition);
            emit(") {\n");
            push_scope();
            codegen_stmt(stmt->while_stmt.body);
            pop_scope();
            emit("    }\n");
            break;
        case STMT_FOR:
            // Check if this is a for-in loop (array iteration)
            if (stmt->for_stmt.array_expr) {
                // Generate for-in loop: for item in array
                emit("{\n");
                push_scope();
                emit("    WynArray __iter_array = ");
                codegen_expr(stmt->for_stmt.array_expr);
                emit(";\n");
                emit("    for (int __i = 0; __i < __iter_array.count; __i++) {\n");
                // Use a generic approach - check element type at runtime
                emit("        WynValue __elem = __iter_array.data[__i];\n");
                emit("        ");
                // Determine variable type based on heuristics
                const char* var_name = stmt->for_stmt.loop_var.start;
                int var_len = stmt->for_stmt.loop_var.length;
                if ((var_len >= 4 && strncmp(var_name, "name", 4) == 0) ||
                    (var_len >= 3 && strncmp(var_name, "str", 3) == 0) ||
                    (var_len >= 4 && strncmp(var_name, "text", 4) == 0)) {
                    // Likely a string variable
                    emit("const char* %.*s = (__elem.type == WYN_TYPE_STRING) ? __elem.data.string_val : \"\";\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                } else {
                    // Default to int
                    emit("int %.*s = (__elem.type == WYN_TYPE_INT) ? __elem.data.int_val : 0;\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                }
                codegen_stmt(stmt->for_stmt.body);
                emit("    }\n");
                pop_scope();
                emit("}\n");
            } else {
                // Regular for loop
                emit("for (");
                if (stmt->for_stmt.init) {
                    emit("int %.*s = ", stmt->for_stmt.init->var.name.length, stmt->for_stmt.init->var.name.start);
                    codegen_expr(stmt->for_stmt.init->var.init);
                }
                emit("; ");
                if (stmt->for_stmt.condition) {
                    codegen_expr(stmt->for_stmt.condition);
                }
                emit("; ");
                if (stmt->for_stmt.increment) {
                    codegen_expr(stmt->for_stmt.increment);
                }
                emit(") {\n");
                push_scope();
                codegen_stmt(stmt->for_stmt.body);
                pop_scope();
                emit("    }\n");
            }
            break;
        case STMT_EXPORT:
            // For now, just generate the exported statement normally
            // In a full implementation, we'd track exports for module linking
            codegen_stmt(stmt->export.stmt);
            break;
        case STMT_TRY:
            // Proper try/catch implementation using setjmp/longjmp
            emit("{\n");
            emit("    jmp_buf exception_buf;\n");
            emit("    const char* exception_msg = NULL;\n");
            emit("    if (setjmp(exception_buf) == 0) {\n");
            emit("        // Try block\n");
            emit("        current_exception_buf = &exception_buf;\n");
            emit("        current_exception_msg = &exception_msg;\n");
            codegen_stmt(stmt->try_stmt.try_block);
            emit("    } else {\n");
            emit("        // Catch block\n");
            emit("        const char* %.*s = exception_msg;\n", 
                 stmt->try_stmt.exception_var.length, 
                 stmt->try_stmt.exception_var.start);
            codegen_stmt(stmt->try_stmt.catch_block);
            emit("    }\n");
            emit("}\n");
            break;
        case STMT_THROW:
            // Proper throw implementation
            emit("if (current_exception_buf && current_exception_msg) {\n");
            emit("    *current_exception_msg = ");
            codegen_expr(stmt->throw_stmt.value);
            emit(";\n");
            emit("    longjmp(*current_exception_buf, 1);\n");
            emit("} else {\n");
            emit("    printf(\"Uncaught exception: %%s\\n\", ");
            codegen_expr(stmt->throw_stmt.value);
            emit(");\n");
            emit("    exit(1);\n");
            emit("}\n");
            break;
        case STMT_MATCH:  // T1.4.4: Control Flow Agent addition
            codegen_match_statement(stmt);
            break;
        default:
            break;
    }
}

void codegen_program(Program* prog) {
    bool has_main = false;
    bool has_math_import = false;
    
    // Check if math module is imported
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            ImportStmt* import = &prog->stmts[i]->import;
            if (import->module.length == 4 && memcmp(import->module.start, "math", 4) == 0) {
                has_math_import = true;
                break;
            }
        }
    }
    
    // Generate math functions if math module is imported
    if (has_math_import) {
        emit("// Math module functions\n");
        emit("int add(int a, int b) { return a + b; }\n");
        emit("int multiply(int a, int b) { return a * b; }\n");
        emit("double PI = 3.14159;\n\n");
    }
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            if (prog->stmts[i]->fn.name.length == 4 &&
                memcmp(prog->stmts[i]->fn.name.start, "main", 4) == 0) {
                has_main = true;
            }
        }
    }
    
    // Generate all structs and enums first
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT || prog->stmts[i]->type == STMT_ENUM) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate forward declarations for all functions
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            FnStmt* fn = &prog->stmts[i]->fn;
            
            // Determine return type
            const char* return_type = "int"; // default
            if (fn->return_type) {
                if (fn->return_type->type == EXPR_IDENT) {
                    Token type_name = fn->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        return_type = "int";
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        return_type = "char*";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        return_type = "WynArray";
                    }
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    // T2.5.1: Handle optional return types - for now, treat as the inner type
                    Expr* inner = fn->return_type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT) {
                        Token type_name = inner->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            return_type = "int";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            return_type = "char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            return_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            return_type = "bool";
                        }
                    }
                }
            }
            
            // Generate forward declaration
            emit("%s %.*s(", return_type, fn->name.length, fn->name.start);
            for (int j = 0; j < fn->param_count; j++) {
                if (j > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "int"; // default
                if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = fn->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "int";
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = "WynArray";
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - for now, treat as the inner type
                        Expr* inner = fn->param_types[j]->optional_type.inner_type;
                        if (inner && inner->type == EXPR_IDENT) {
                            Token type_name = inner->token;
                            if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                param_type = "int";
                            } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                                param_type = "const char*";
                            } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                param_type = "const char*";
                            } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                param_type = "double";
                            } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                param_type = "bool";
                            }
                        }
                    }
                }
                
                emit("%s %.*s", param_type, fn->params[j].length, fn->params[j].start);
            }
            emit(");\n");
        }
    }
    emit("\n");
    
    // Generate all functions
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // If no main function, create one that executes all statements
    if (!has_main) {
        emit("int main() {\n");
        
        // Special case: single expression should return its value
        if (prog->count == 1 && prog->stmts[0]->type == STMT_EXPR) {
            // Check if the expression is a function call that returns void
            Expr* expr = prog->stmts[0]->expr;
            if (expr->type == EXPR_CALL) {
                // Function calls are statements, not return values
                emit("    ");
                codegen_stmt(prog->stmts[0]);
                emit("    return 0;\n");
            } else {
                // Other expressions can be returned
                emit("    return ");
                codegen_expr(prog->stmts[0]->expr);
                emit(";\n");
            }
        } else {
            // Multiple statements or non-expression statements
            for (int i = 0; i < prog->count; i++) {
                if (prog->stmts[i]->type != STMT_FN) {
                    emit("    ");
                    codegen_stmt(prog->stmts[i]);
                }
            }
            emit("    return 0;\n");
        }
        emit("}\n");
    }
}

// T1.4.4: Control Flow Code Generation - Control Flow Agent addition
void codegen_match_statement(Stmt* stmt) {
    if (!stmt || stmt->type != STMT_MATCH) return;
    
    // Generate a temporary variable to hold the match value
    emit("{\n");
    emit("    auto __match_val = ");
    codegen_expr(stmt->match_stmt.value);
    emit(";\n");
    
    // Generate if-else chain for pattern matching
    for (int i = 0; i < stmt->match_stmt.case_count; i++) {
        MatchCase* match_case = &stmt->match_stmt.cases[i];
        
        if (i == 0) {
            emit("    if (");
        } else {
            emit("    } else if (");
        }
        
        // Simple pattern matching - just equality for now
        emit("__match_val == ");
        
        // Handle different pattern types
        if (match_case->pattern->type == PATTERN_LITERAL) {
            emit("/* literal pattern */");
        } else if (match_case->pattern->type == PATTERN_IDENT) {
            emit("true /* variable pattern */");
        } else {
            emit("false /* unsupported pattern */");
        }
        
        // Add guard clause if present
        if (match_case->guard) {
            emit(" && (");
            codegen_expr(match_case->guard);
            emit(")");
        }
        
        emit(") {\n");
        
        // Generate case body
        if (match_case->body) {
            emit("        ");
            codegen_stmt(match_case->body);
        }
    }
    
    // Close the if-else chain
    if (stmt->match_stmt.case_count > 0) {
        emit("    }\n");
    }
    
    emit("}\n");
}
