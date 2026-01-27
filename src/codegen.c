#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"
#include "ast.h"
#include "string_memory.h"
#include "arc_runtime.h"
#include "optional.h"
#include "result.h"
#include "modules.h"
#include "optimize.h"
#include "functional.h"
#include "hashmap.h"
#include "hashset.h"
#include "json.h"
#include "module_registry.h"
#include "module_aliases.h"
#include "scope.h"

// Forward declarations
void codegen_stmt(Stmt* stmt);

void codegen_match_statement(Stmt* stmt); // T1.4.4: Control Flow Agent addition
Type* make_type(TypeKind kind); // Forward declaration for type creation
extern bool is_builtin_module(const char* name);  // Module system

static FILE* out = NULL;

// Module emission tracking (reset per compilation)
static bool modules_emitted_this_compilation = false;

// Current module being emitted (for prefixing internal calls)
static const char* current_module_prefix = NULL;

// Identifier scope tracking for proper identifier resolution
static IdentScope* current_ident_scope = NULL;
static IdentScope* module_ident_scope = NULL;

// Convert module name to C identifier (network/http -> network_http)
static const char* module_to_c_ident(const char* module_name) {
    static char c_ident[256];
    int i = 0;
    for (const char* p = module_name; *p && i < 255; p++, i++) {
        c_ident[i] = (*p == '/') ? '_' : *p;
    }
    c_ident[i] = '\0';
    return c_ident;
}

// Simple function registry for current module
static char* module_functions[256];
static int module_function_count = 0;

// Map short module names to full paths (http -> network/http)
static struct {
    char short_name[64];
    char full_path[256];
} module_short_names[64];
static int module_short_name_count = 0;

static void register_module_short_name(const char* full_path) {
    // Extract short name (last part after /)
    const char* last_slash = strrchr(full_path, '/');
    const char* short_name = last_slash ? last_slash + 1 : full_path;
    
    if (module_short_name_count < 64) {
        strncpy(module_short_names[module_short_name_count].short_name, short_name, 63);
        strncpy(module_short_names[module_short_name_count].full_path, full_path, 255);
        module_short_name_count++;
    }
}

static const char* resolve_short_module_name(const char* short_name) {
    for (int i = 0; i < module_short_name_count; i++) {
        if (strcmp(module_short_names[i].short_name, short_name) == 0) {
            return module_short_names[i].full_path;
        }
    }
    return short_name;  // Not found, return as-is
}

// Parameter tracking for current function
static char* current_function_params[64];
static int current_param_count = 0;

// Local variable tracking for current function
static char* current_function_locals[256];
static int current_local_count = 0;

static void register_module_function(const char* name) {
    if (module_function_count < 256) {
        module_functions[module_function_count++] = strdup(name);
    }
}

static bool is_module_function(const char* name) {
    for (int i = 0; i < module_function_count; i++) {
        if (strcmp(module_functions[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void clear_module_functions() {
    for (int i = 0; i < module_function_count; i++) {
        free(module_functions[i]);
    }
    module_function_count = 0;
}

static void register_parameter(const char* name) {
    if (current_param_count < 64) {
        current_function_params[current_param_count++] = strdup(name);
    }
}

static bool is_parameter(const char* name) {
    for (int i = 0; i < current_param_count; i++) {
        if (strcmp(current_function_params[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void clear_parameters() {
    for (int i = 0; i < current_param_count; i++) {
        free(current_function_params[i]);
    }
    current_param_count = 0;
}

static void register_local_variable(const char* name) {
    if (current_local_count < 256) {
        current_function_locals[current_local_count++] = strdup(name);
    }
}

static bool is_local_variable(const char* name) {
    for (int i = 0; i < current_local_count; i++) {
        if (strcmp(current_function_locals[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void clear_local_variables() {
    for (int i = 0; i < current_local_count; i++) {
        free(current_function_locals[i]);
    }
    current_local_count = 0;
}

// Module alias tracking
static struct {
    char alias[64];
    char module[64];
} module_aliases[32];
static int module_alias_count = 0;

static void register_module_alias(const char* alias, const char* module) {
    if (module_alias_count < 32) {
        snprintf(module_aliases[module_alias_count].alias, 64, "%s", alias);
        snprintf(module_aliases[module_alias_count].module, 64, "%s", module);
        module_alias_count++;
    }
}

static const char* resolve_module_alias(const char* name) {
    for (int i = 0; i < module_alias_count; i++) {
        if (strcmp(module_aliases[i].alias, name) == 0) {
            return module_aliases[i].module;
        }
    }
    return name;  // Not an alias, return as-is
}

// Lambda function collection
typedef struct {
    char* code;
    int id;
    int param_count;
    char captured_vars[16][64];
    int capture_count;
} LambdaFunction;

static LambdaFunction lambda_functions[256];
static int lambda_count = 0;
static int lambda_id_counter = 0;

// Track lambda variable names and their captures for call site injection
typedef struct {
    char var_name[64];
    char captured_vars[16][64];
    int capture_count;
} LambdaVarInfo;

static LambdaVarInfo lambda_var_info[256];
static int lambda_var_count = 0;

// Spawn wrapper collection
typedef struct {
    char func_name[256];
} SpawnWrapper;

static SpawnWrapper spawn_wrappers[256];
static int spawn_wrapper_count = 0;

// Forward declaration
static void emit(const char* fmt, ...);

// Scope tracking for automatic cleanup with ARC integration
typedef struct {
    char* vars[256];
    char* types[256];  // Track type for proper cleanup
    WynObject* string_objects[256];  // Track ARC string objects
    int count;
    int string_count;
} Scope;

static Scope scopes[32];
static int scope_depth = 0;

void init_codegen(FILE* output) {
    out = output;
    scope_depth = 0;
    wyn_init_modules();  // Initialize module system
}

static void push_scope() {
    if (scope_depth < 32) {
        scopes[scope_depth].count = 0;
        scopes[scope_depth].string_count = 0;
        scope_depth++;
    }
}

static void pop_scope() {
    if (scope_depth > 0) {
        scope_depth--;
        
        // Emit cleanup for regular variables based on their type
        for (int i = 0; i < scopes[scope_depth].count; i++) {
            const char* var_name = scopes[scope_depth].vars[i];
            const char* var_type = scopes[scope_depth].types[i];
            
            if (var_type && strcmp(var_type, "WynArray") == 0) {
                // For WynArray, free the internal data array if it exists
                emit("    if(%s.data) free(%s.data);\n", var_name, var_name);
            } else if (var_type && (strcmp(var_type, "char*") == 0 || strcmp(var_type, "const char*") == 0)) {
                // For string pointers, only free if they're not string literals
                emit("    /* String cleanup handled by ARC */\n");
            } else {
                // For other pointer types, use regular free
                emit("    if(%s) free(%s);\n", var_name, var_name);
            }
        }
        
        // Release ARC string objects
        for (int i = 0; i < scopes[scope_depth].string_count; i++) {
            if (scopes[scope_depth].string_objects[i]) {
                wyn_arc_release(scopes[scope_depth].string_objects[i]);
                scopes[scope_depth].string_objects[i] = NULL;
            }
        }
    }
}

static void track_var_with_type(const char* name, int len, const char* type) {
    if (scope_depth > 0 && scopes[scope_depth - 1].count < 256) {
        char* var = malloc(len + 1);
        strncpy(var, name, len);
        var[len] = 0;
        scopes[scope_depth - 1].vars[scopes[scope_depth - 1].count] = var;
        
        // Store the type for proper cleanup
        if (type) {
            int type_len = strlen(type);
            char* var_type = malloc(type_len + 1);
            strcpy(var_type, type);
            scopes[scope_depth - 1].types[scopes[scope_depth - 1].count] = var_type;
        } else {
            scopes[scope_depth - 1].types[scopes[scope_depth - 1].count] = NULL;
        }
        
        scopes[scope_depth - 1].count++;
    }
}

static void track_string_var(const char* name, int len) {
    track_var_with_type(name, len, "char*");
}

static void track_string_object(WynObject* obj) {
    if (scope_depth > 0 && scopes[scope_depth - 1].string_count < 256) {
        scopes[scope_depth - 1].string_objects[scopes[scope_depth - 1].string_count++] = obj;
    }
}

static void emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}

// Global emit function for use by other modules
void wyn_emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}

// Helper to emit C type from type expression
static void emit_type_from_expr(Expr* type_expr) {
    if (type_expr->type == EXPR_IDENT) {
        Token type_name = type_expr->token;
        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
            emit("int");
        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
            emit("const char*");
        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
            emit("int");
        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
            emit("double");
        } else {
            emit("%.*s", type_name.length, type_name.start);
        }
    } else {
        emit("int"); // fallback
    }
}

void codegen_expr(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_INT: {
            // Handle binary literals (0b1010)
            if (expr->token.length > 2 && expr->token.start[0] == '0' && 
                (expr->token.start[1] == 'b' || expr->token.start[1] == 'B')) {
                // Convert binary to decimal
                long long value = 0;
                for (int i = 2; i < expr->token.length; i++) {
                    if (expr->token.start[i] == '0' || expr->token.start[i] == '1') {
                        value = value * 2 + (expr->token.start[i] - '0');
                    }
                }
                emit("%lld", value);
            }
            // Handle numbers with underscores (1_000)
            else if (memchr(expr->token.start, '_', expr->token.length)) {
                // Emit without underscores
                for (int i = 0; i < expr->token.length; i++) {
                    if (expr->token.start[i] != '_') {
                        emit("%c", expr->token.start[i]);
                    }
                }
            }
            // Regular int
            else {
                emit("%.*s", expr->token.length, expr->token.start);
            }
            break;
        }
        case EXPR_FLOAT:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_STRING: {
            // Check for multi-line string (""")
            bool is_multiline = (expr->token.length >= 6 && 
                                expr->token.start[0] == '"' && 
                                expr->token.start[1] == '"' && 
                                expr->token.start[2] == '"');
            
            int start_offset = is_multiline ? 3 : 1;
            int end_offset = is_multiline ? 3 : 1;
            
            // Emit string literal with proper C escape sequences
            emit("\"");
            for (int i = start_offset; i < expr->token.length - end_offset; i++) {
                char c = expr->token.start[i];
                if (c == '\\' && i + 1 < expr->token.length - end_offset) {
                    char next = expr->token.start[i + 1];
                    // Emit escape sequences as-is for C
                    emit("\\%c", next);
                    i++;
                } else if (c == '"') {
                    emit("\\\"");
                } else if (c == '\n') {
                    emit("\\n");
                } else {
                    emit("%c", c);
                }
            }
            emit("\"");
            break;
        }
        case EXPR_CHAR:
            emit("'%.*s'", expr->token.length - 2, expr->token.start + 1);
            break;
        case EXPR_IDENT: {
            // Convert :: to _ for C compatibility (e.g., Status::DONE -> Status_DONE)
            char* ident = malloc(expr->token.length + 256); // Extra space for alias resolution
            int offset = 0;
            
            // Check if this is a module.function call and resolve alias
            char temp_ident[512];
            memcpy(temp_ident, expr->token.start, expr->token.length);
            temp_ident[expr->token.length] = '\0';
            
            // If we're inside a module function, check if this identifier needs module prefix
            if (current_module_prefix && !strchr(temp_ident, ':') && !strchr(temp_ident, '.')) {
                // Check if this is a parameter - never prefix parameters
                if (is_parameter(temp_ident)) {
                    // This is a parameter, emit as-is
                    emit("%s", temp_ident);
                    free(ident);
                    break;
                }
                
                // Check if this is a local variable - never prefix local variables
                if (is_local_variable(temp_ident)) {
                    // This is a local variable, emit as-is
                    emit("%s", temp_ident);
                    free(ident);
                    break;
                }
                
                // Check if this is a simple identifier (no :: or .)
                // For module-level identifiers, we need to prefix them
                // Heuristic: uppercase = constant, lowercase = might be module var or local var
                bool looks_like_module_level = (temp_ident[0] >= 'A' && temp_ident[0] <= 'Z') || 
                                               (temp_ident[0] >= 'a' && temp_ident[0] <= 'z');
                
                // Don't prefix common local variable names or single-letter variables
                bool is_single_letter = (strlen(temp_ident) == 1);
                const char* common_locals[] = {"i", "j", "k", "x", "y", "z", "n", "result", "temp", "value", 
                                               "a", "b", "c", "d", "e", "f", "g", "h", "m", "p", "q", "r", "s", "t",
                                               "content", "path", "text", "count", "lines", "words", NULL};
                bool is_common_local = false;
                for (int i = 0; common_locals[i] != NULL; i++) {
                    if (strcmp(temp_ident, common_locals[i]) == 0) {
                        is_common_local = true;
                        break;
                    }
                }
                
                if (looks_like_module_level && !is_common_local && !is_single_letter) {
                    // Try prefixing - if it doesn't exist, C compiler will error
                    emit("%s_%s", current_module_prefix, temp_ident);
                    free(ident);
                    break;
                }
            }
            
            // Check if this is a module.function or module::function call and resolve alias
            char* dot = strchr(temp_ident, '.');
            char* colon = strstr(temp_ident, "::");
            
            if (colon) {
                // Handle module::function syntax
                char function_part[256];
                strcpy(function_part, colon + 2);  // Save function name
                *colon = '\0';  // Split at ::
                
                // Resolve short name (http -> network/http)
                const char* full_path = resolve_short_module_name(temp_ident);
                const char* resolved = resolve_module_alias(full_path);
                
                // Rebuild identifier with resolved module name
                snprintf(temp_ident, sizeof(temp_ident), "%s::%s", resolved, function_part);
            } else if (dot) {
                // Handle module.function syntax
                char function_part[256];
                strcpy(function_part, dot + 1);  // Save function name
                *dot = '\0';  // Split at dot
                const char* resolved = resolve_module_alias(temp_ident);
                // Rebuild identifier with resolved module name
                snprintf(temp_ident, sizeof(temp_ident), "%s.%s", resolved, function_part);
            }
            
            // Check if this is a C keyword that needs prefix
            const char* c_keywords[] = {"double", "float", "int", "char", "void", "return", "if", "else", "while", "for", NULL};
            bool is_c_keyword = false;
            for (int i = 0; c_keywords[i] != NULL; i++) {
                if (strlen(temp_ident) == strlen(c_keywords[i]) && 
                    strcmp(temp_ident, c_keywords[i]) == 0) {
                    is_c_keyword = true;
                    ident[0] = '_';
                    offset = 1;
                    break;
                }
            }
            
            strcpy(ident + offset, temp_ident);
            
            // Replace :: with _ and / with _
            for (int i = offset; ident[i] && ident[i+1]; i++) {
                if (ident[i] == ':' && ident[i+1] == ':') {
                    ident[i] = '_';
                    // Shift rest of string left by 1
                    memmove(ident + i + 1, ident + i + 2, strlen(ident + i + 2) + 1);
                } else if (ident[i] == '/') {
                    ident[i] = '_';
                }
            }
            // Check last character for /
            int len = strlen(ident);
            if (len > 0 && ident[len-1] == '/') {
                ident[len-1] = '_';
            }
            
            emit("%s", ident);
            free(ident);
            break;
        }
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
        case EXPR_AWAIT:
            // Await: call async function, block on future, cast result
            emit("*(int*)wyn_block_on(");
            codegen_expr(expr->await.expr);
            emit(")");
            break;
        case EXPR_BINARY:
            // Special handling for string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS) {
                // Check if either operand is actually a string type
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                bool left_is_int = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_INT);
                bool right_is_int = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_INT);
                
                if (left_is_string || right_is_string) {
                    // Use ARC-managed string concatenation with automatic conversion
                    emit("wyn_string_concat_safe(");
                    
                    // Convert left operand to string if it's an int
                    if (left_is_int && !left_is_string) {
                        emit("int_to_string(");
                        codegen_expr(expr->binary.left);
                        emit(")");
                    } else {
                        codegen_expr(expr->binary.left);
                    }
                    
                    emit(", ");
                    
                    // Convert right operand to string if it's an int
                    if (right_is_int && !right_is_string) {
                        emit("int_to_string(");
                        codegen_expr(expr->binary.right);
                        emit(")");
                    } else {
                        codegen_expr(expr->binary.right);
                    }
                    
                    emit(")");
                    break;
                }
            }
            
            // Special handling for string comparison operators
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ) {
                // Check if both operands are strings
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                if (left_is_string && right_is_string) {
                    // Use strcmp for string comparison
                    emit("(strcmp(");
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                    
                    // Map operator to strcmp result comparison
                    if (expr->binary.op.type == TOKEN_EQEQ) {
                        emit(" == 0");
                    } else if (expr->binary.op.type == TOKEN_BANGEQ) {
                        emit(" != 0");
                    } else if (expr->binary.op.type == TOKEN_LT) {
                        emit(" < 0");
                    } else if (expr->binary.op.type == TOKEN_GT) {
                        emit(" > 0");
                    } else if (expr->binary.op.type == TOKEN_LTEQ) {
                        emit(" <= 0");
                    } else if (expr->binary.op.type == TOKEN_GTEQ) {
                        emit(" >= 0");
                    }
                    emit(")");
                    break;
                }
            }
            
            // Default binary expression handling
            
            // Special handling for nil coalescing operator ??
            if (expr->binary.op.type == TOKEN_QUESTION_QUESTION) {
                // Generate: (left->has_value ? *(int*)left->value : right)
                emit("(");
                codegen_expr(expr->binary.left);
                emit("->has_value ? *(int*)");
                codegen_expr(expr->binary.left);
                emit("->value : ");
                codegen_expr(expr->binary.right);
                emit(")");
            } else {
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
            }
            break;
        case EXPR_CALL:
            // TASK-040: Special handling for higher-order functions
            if (expr->call.callee->type == EXPR_IDENT) {
                Token func_name = expr->call.callee->token;
                
                // Handle map function: map(array, lambda) - only if 2 args
                if (func_name.length == 3 && memcmp(func_name.start, "map", 3) == 0 && expr->call.arg_count == 2) {
                    emit("array_map(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // lambda
                    emit(")");
                    break;
                }
                
                // Handle filter function: filter(array, predicate) - only if 2 args
                if (func_name.length == 6 && memcmp(func_name.start, "filter", 6) == 0 && expr->call.arg_count == 2) {
                    emit("array_filter(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // predicate
                    emit(")");
                    break;
                }
                
                // Handle reduce function: reduce(array, func, initial)
                if (func_name.length == 6 && memcmp(func_name.start, "reduce", 6) == 0) {
                    emit("array_reduce(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // function
                    emit(", ");
                    codegen_expr(expr->call.args[2]); // initial value
                    emit(")");
                    break;
                }
            }
            
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
                       expr->call.callee->token.length == 8 &&
                       memcmp(expr->call.callee->token.start, "get_argc", 8) == 0) {
                // Special handling for get_argc() - call C interface function
                emit("wyn_get_argc()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 8 &&
                       memcmp(expr->call.callee->token.start, "get_argv", 8) == 0) {
                // Special handling for get_argv(index) - call C interface function
                if (expr->call.arg_count == 1) {
                    emit("wyn_get_argv(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("wyn_get_argv(0)");  // Default to index 0
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 17 &&
                       memcmp(expr->call.callee->token.start, "check_file_exists", 17) == 0) {
                // Special handling for check_file_exists(path) - call existing C function
                if (expr->call.arg_count == 1) {
                    emit("wyn_file_exists(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 17 &&
                       memcmp(expr->call.callee->token.start, "read_file_content", 17) == 0) {
                // Special handling for read_file_content(path) - call C interface function
                if (expr->call.arg_count == 1) {
                    emit("wyn_read_file(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("NULL");  // Return NULL if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 16 &&
                       memcmp(expr->call.callee->token.start, "is_content_valid", 16) == 0) {
                // Special handling for is_content_valid(content) - check if content is not NULL
                if (expr->call.arg_count == 1) {
                    emit("(");
                    codegen_expr(expr->call.args[0]);
                    emit(" != NULL)");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 10 &&
                       memcmp(expr->call.callee->token.start, "store_argv", 10) == 0) {
                // Special handling for store_argv(index) - store argument in global
                if (expr->call.arg_count == 1) {
                    emit("wyn_store_argv(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 24 &&
                       memcmp(expr->call.callee->token.start, "check_file_exists_stored", 24) == 0) {
                // Special handling for check_file_exists_stored() - check stored filename
                emit("wyn_file_exists(global_filename)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 18 &&
                       memcmp(expr->call.callee->token.start, "store_file_content", 18) == 0) {
                // Special handling for store_file_content() - store file content in global
                emit("wyn_store_file_content(global_filename)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 24 &&
                       memcmp(expr->call.callee->token.start, "get_stored_content_valid", 24) == 0) {
                // Special handling for get_stored_content_valid() - check stored content validity
                emit("wyn_get_content_valid()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_init_lexer", 12) == 0) {
                // Compiler function: c_init_lexer(source)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_init_lexer(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 12 &&
                       memcmp(expr->call.callee->token.start, "c_init_parser", 13) == 0) {
                // Compiler function: c_init_parser()
                emit("(wyn_c_init_parser(), 1)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_parse_program", 15) == 0) {
                // Compiler function: c_parse_program()
                emit("wyn_c_parse_program()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_init_checker", 14) == 0) {
                // Compiler function: c_init_checker()
                emit("(wyn_c_init_checker(), 1)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_check_program", 15) == 0) {
                // Compiler function: c_check_program(ast_ptr)
                if (expr->call.arg_count == 1) {
                    emit("(wyn_c_check_program(");
                    codegen_expr(expr->call.args[0]);
                    emit("), 1)");
                } else {
                    emit("0");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 18 &&
                       memcmp(expr->call.callee->token.start, "c_checker_had_error", 19) == 0) {
                // Compiler function: c_checker_had_error()
                emit("wyn_c_checker_had_error()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_generate_code", 15) == 0) {
                // Compiler function: c_generate_code(ast_ptr, filename)
                if (expr->call.arg_count == 2) {
                    emit("wyn_c_generate_code(");
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 19 &&
                       memcmp(expr->call.callee->token.start, "c_create_c_filename", 19) == 0) {
                // Compiler function: c_create_c_filename(filename)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_create_c_filename(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("NULL");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 19 &&
                       memcmp(expr->call.callee->token.start, "c_compile_to_binary", 19) == 0) {
                // Compiler function: c_compile_to_binary(c_filename, wyn_filename)
                if (expr->call.arg_count == 2) {
                    emit("wyn_c_compile_to_binary(");
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_remove_file", 13) == 0) {
                // Compiler function: c_remove_file(filename)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_remove_file(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("false");
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
                
                // Check if this is a generic function call
                if (expr->call.callee->type == EXPR_IDENT) {
                    Token func_name = expr->call.callee->token;
                    
                    if (wyn_is_generic_function_call(func_name)) {
                        // Collect argument types for generic instantiation
                        Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                        for (int i = 0; i < expr->call.arg_count; i++) {
                            // Infer type from expression if not already set
                            if (!expr->call.args[i]->expr_type) {
                                switch (expr->call.args[i]->type) {
                                    case EXPR_INT:
                                        expr->call.args[i]->expr_type = make_type(TYPE_INT);
                                        break;
                                    case EXPR_FLOAT:
                                        expr->call.args[i]->expr_type = make_type(TYPE_FLOAT);
                                        break;
                                    case EXPR_STRING:
                                        expr->call.args[i]->expr_type = make_type(TYPE_STRING);
                                        break;
                                    case EXPR_BOOL:
                                        expr->call.args[i]->expr_type = make_type(TYPE_BOOL);
                                        break;
                                    default:
                                        expr->call.args[i]->expr_type = make_type(TYPE_INT);
                                        break;
                                }
                            }
                            arg_types[i] = expr->call.args[i]->expr_type;
                        }
                        
                        // Generate monomorphic function name
                        char monomorphic_name[256];
                        wyn_generate_monomorphic_name(func_name, arg_types, expr->call.arg_count, 
                                                      monomorphic_name, sizeof(monomorphic_name));
                        
                        // Register this instantiation for later code generation
                        wyn_register_generic_instantiation(func_name, arg_types, expr->call.arg_count);
                        
                        // Emit call to monomorphic function
                        emit("%s", monomorphic_name);
                        free(arg_types);
                    } else {
                        // Regular function call
                        // T1.5.3: Use mangled name only for actually overloaded functions
                        if (expr->call.selected_overload) {
                            Symbol* overload = (Symbol*)expr->call.selected_overload;
                            // Only use mangled name if there are multiple overloads
                            if (overload->mangled_name && overload->next_overload) {
                                emit("%s", overload->mangled_name);
                            } else {
                                // Check if we're in a module and need to prefix
                                // BUT: don't prefix if the callee is already module-qualified (contains ::)
                                bool is_module_qualified = false;
                                if (expr->call.callee->type == EXPR_IDENT) {
                                    for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                                        if (expr->call.callee->token.start[i] == ':' && 
                                            expr->call.callee->token.start[i+1] == ':') {
                                            is_module_qualified = true;
                                            break;
                                        }
                                    }
                                }
                                
                                // Check if this is an internal module function call
                                bool is_internal_call = false;
                                if (current_module_prefix && !is_module_qualified && expr->call.callee->type == EXPR_IDENT) {
                                    char func_name[256];
                                    snprintf(func_name, 256, "%.*s", expr->call.callee->token.length, expr->call.callee->token.start);
                                    is_internal_call = is_module_function(func_name);
                                }
                                
                                // Only prefix if NOT an internal call
                                if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                    emit("%s_", current_module_prefix);
                                }
                                codegen_expr(expr->call.callee);
                            }
                        } else {
                            // Check if we're in a module and need to prefix
                            // BUT: don't prefix if the callee is already module-qualified (contains ::)
                            bool is_module_qualified = false;
                            if (expr->call.callee->type == EXPR_IDENT) {
                                for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                                    if (expr->call.callee->token.start[i] == ':' && 
                                        expr->call.callee->token.start[i+1] == ':') {
                                        is_module_qualified = true;
                                        break;
                                    }
                                }
                            }
                            
                            // Check if this is an internal module function call
                            bool is_internal_call = false;
                            if (current_module_prefix && !is_module_qualified && expr->call.callee->type == EXPR_IDENT) {
                                char func_name[256];
                                snprintf(func_name, 256, "%.*s", expr->call.callee->token.length, expr->call.callee->token.start);
                                is_internal_call = is_module_function(func_name);
                            }
                            
                            // Only prefix if NOT an internal call
                            if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                emit("%s_", current_module_prefix);
                            }
                            codegen_expr(expr->call.callee);
                        }
                    }
                } else {
                    // Non-identifier callee (e.g., function pointer)
                    codegen_expr(expr->call.callee);
                }
                
                emit("(");
                
                // Check if this is a lambda variable call - inject captured variables
                bool is_lambda_call = false;
                int lambda_var_idx = -1;
                if (expr->call.callee->type == EXPR_IDENT) {
                    char callee_name[64];
                    snprintf(callee_name, 64, "%.*s", 
                            expr->call.callee->token.length, expr->call.callee->token.start);
                    for (int i = 0; i < lambda_var_count; i++) {
                        if (strcmp(lambda_var_info[i].var_name, callee_name) == 0) {
                            is_lambda_call = true;
                            lambda_var_idx = i;
                            break;
                        }
                    }
                }
                
                // Emit captured variables first
                if (is_lambda_call && lambda_var_idx >= 0) {
                    for (int i = 0; i < lambda_var_info[lambda_var_idx].capture_count; i++) {
                        if (i > 0) emit(", ");
                        emit("%s", lambda_var_info[lambda_var_idx].captured_vars[i]);
                    }
                }
                
                for (int i = 0; i < expr->call.arg_count; i++) {
                    if (i > 0 || (is_lambda_call && lambda_var_idx >= 0 && lambda_var_info[lambda_var_idx].capture_count > 0)) {
                        emit(", ");
                    }
                    
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
            Token method = expr->method_call.method;
            
            // Extension methods on struct types - CHECK THIS FIRST
            if (expr->method_call.object->expr_type && 
                expr->method_call.object->expr_type->kind == TYPE_STRUCT) {
                Token type_name = expr->method_call.object->expr_type->struct_type.name;
                emit("%.*s_%.*s(", type_name.length, type_name.start, 
                     method.length, method.start);
                codegen_expr(expr->method_call.object);
                for (int i = 0; i < expr->method_call.arg_count; i++) {
                    emit(", ");
                    codegen_expr(expr->method_call.args[i]);
                }
                emit(")");
                break;
            }
            
            // Module method calls (module.function()) - CHECK THIS SECOND
            if (expr->method_call.object->type == EXPR_IDENT) {
                Token obj_name = expr->method_call.object->token;
                // Check if this is actually a module (not a variable)
                char module_name[256];
                int len = obj_name.length < 255 ? obj_name.length : 255;
                memcpy(module_name, obj_name.start, len);
                module_name[len] = '\0';
                
                // Treat as module if it's loaded OR if it's a built-in
                if (is_module_loaded(module_name) || is_builtin_module(module_name)) {
                    // Emit as: modulename_methodname(args)
                    emit("%.*s_%.*s(", obj_name.length, obj_name.start, method.length, method.start);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        if (i > 0) emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            
            // Type-aware method dispatch (Phase 4)
            Type* object_type = expr->method_call.object->expr_type;
            
            // Special handling for array.push() with struct elements
            if (object_type && object_type->kind == TYPE_ARRAY) {
                Token method = expr->method_call.method;
                if (method.length == 4 && memcmp(method.start, "push", 4) == 0) {
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type && elem_type->kind == TYPE_STRUCT) {
                        // Use macro for struct push
                        emit("array_push_struct(&(");
                        codegen_expr(expr->method_call.object);
                        emit("), ");
                        
                        // Emit the struct initializer
                        codegen_expr(expr->method_call.args[0]);
                        emit(", ");
                        
                        // Emit the type name for the macro's third argument
                        // This must match the type used in the struct initializer
                        Expr* init_expr = expr->method_call.args[0];
                        if (init_expr->type == EXPR_STRUCT_INIT) {
                            Token type_name = init_expr->struct_init.type_name;
                            
                            // The struct initializer codegen adds current_module_prefix
                            // We need to do the same here to match
                            if (current_module_prefix) {
                                emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                            } else {
                                emit("%.*s", type_name.length, type_name.start);
                            }
                        } else {
                            // Fallback
                            Token type_name = elem_type->struct_type.name;
                            if (current_module_prefix) {
                                emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                            } else {
                                emit("%.*s", type_name.length, type_name.start);
                            }
                        }
                        emit(")");
                        break;
                    }
                }
            }
            
            // Special handling for array.get() - use type-specific accessor
            if (object_type && object_type->kind == TYPE_ARRAY) {
                Token method = expr->method_call.method;
                if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                    // Determine element type and use appropriate accessor
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type) {
                        if (elem_type->kind == TYPE_STRING) {
                            emit("array_get_str(");
                        } else if (elem_type->kind == TYPE_STRUCT) {
                            // Use struct accessor
                            emit("array_get_struct(");
                        } else {
                            // Default to int accessor
                            emit("array_get_int(");
                        }
                    } else {
                        // No element type info, default to int
                        emit("array_get_int(");
                    }
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    if (elem_type && elem_type->kind == TYPE_STRUCT) {
                        emit(", ");
                        Token type_name = elem_type->struct_type.name;
                        emit("%.*s", type_name.length, type_name.start);
                    }
                    emit(")");
                    break;
                }
            }
            
            const char* receiver_type = get_receiver_type_string(object_type);
            
            // Debug: print what we got
            if (object_type) {
            } else {
            }
            if (receiver_type) {
            } else {
            }
            
            if (receiver_type) {
                char method_name[256];
                int len = method.length < 255 ? method.length : 255;
                memcpy(method_name, method.start, len);
                method_name[len] = '\0';
                
                MethodDispatch dispatch;
                if (dispatch_method(receiver_type, method_name, expr->method_call.arg_count, &dispatch)) {
                    emit("%s(", dispatch.c_function);
                    if (dispatch.pass_by_ref) {
                        emit("&(");
                        codegen_expr(expr->method_call.object);
                        emit(")");
                    } else {
                        codegen_expr(expr->method_call.object);
                    }
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            
            // Fallback: unknown method
            if (receiver_type) {
                fprintf(stderr, "Error: Unknown method '%.*s' for type '%s'\n", 
                        method.length, method.start, receiver_type);
                
                // Provide helpful hints
                if (strcmp(receiver_type, "map") == 0) {
                    fprintf(stderr, "Hint: Available HashMap methods: .has(key), .get(key), .remove(key), .len()\n");
                    fprintf(stderr, "      Or use indexing: map[\"key\"] to get/set values\n");
                } else if (strcmp(receiver_type, "set") == 0) {
                    fprintf(stderr, "Hint: Available HashSet methods: .add(item), .contains(item), .remove(item), .len()\n");
                } else if (strcmp(receiver_type, "array") == 0) {
                    fprintf(stderr, "Hint: Available array methods: .len(), .push(item), .pop(), .contains(item), .sort()\n");
                } else if (strcmp(receiver_type, "string") == 0) {
                    fprintf(stderr, "Hint: Available string methods: .len(), .upper(), .lower(), .trim(), .contains(substr)\n");
                }
            } else {
                fprintf(stderr, "Error: Unknown method '%.*s' (no type info)\n", 
                        method.length, method.start);
            }
            break;
        }
        case EXPR_ARRAY: {
            // Generate simple array creation
            static int arr_counter = 0;
            int arr_id = arr_counter++;
            emit("({ WynArray __arr_%d = array_new(); ", arr_id);
            for (int i = 0; i < expr->array.count; i++) {
                if (expr->array.elements[i]->type == EXPR_STRING) {
                    emit("array_push_str(&__arr_%d, ", arr_id);
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                } else {
                    emit("array_push_int(&__arr_%d, ", arr_id);
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                }
            }
            emit("__arr_%d; })", arr_id);
            break;
        }
        case EXPR_HASHMAP_LITERAL: {
            // v1.3.0: {} with initialization supporting multiple types
            if (expr->array.count == 0) {
                // Empty hashmap
                emit("hashmap_new()");
            } else {
                // HashMap with initial values
                static int map_counter = 0;
                int map_id = map_counter++;
                emit("({ WynHashMap* __map_%d = hashmap_new(); ", map_id);
                
                // Insert key-value pairs (stored as key, value, key, value...)
                for (int i = 0; i < expr->array.count; i += 2) {
                    Expr* value_expr = expr->array.elements[i+1];
                    
                    // Determine insert function based on value type
                    const char* insert_func = "hashmap_insert_int";
                    if (value_expr->type == EXPR_FLOAT) {
                        insert_func = "hashmap_insert_float";
                    } else if (value_expr->type == EXPR_STRING) {
                        insert_func = "hashmap_insert_string";
                    } else if (value_expr->type == EXPR_BOOL) {
                        insert_func = "hashmap_insert_bool";
                    }
                    
                    emit("%s(__map_%d, ", insert_func, map_id);
                    codegen_expr(expr->array.elements[i]);    // key
                    emit(", ");
                    codegen_expr(expr->array.elements[i+1]);  // value
                    emit("); ");
                }
                
                emit("__map_%d; })", map_id);
            }
            break;
        }
        case EXPR_HASHSET_LITERAL: {
            // v1.3.0: {:} with initialization
            if (expr->array.count == 0) {
                // Empty hashset
                emit("hashset_new()");
            } else {
                // HashSet with initial values
                static int set_counter = 0;
                int set_id = set_counter++;
                emit("({ WynHashSet* __set_%d = hashset_new(); ", set_id);
                
                // Add elements
                for (int i = 0; i < expr->array.count; i++) {
                    emit("hashset_add(__set_%d, ", set_id);
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                }
                
                emit("__set_%d; })", set_id);
            }
            break;
        }
        case EXPR_INDEX: {
            // Check if this is string indexing
            if (expr->index.array->expr_type && expr->index.array->expr_type->kind == TYPE_STRING) {
                // String indexing: s[i] -> wyn_string_charat(s, i)
                emit("wyn_string_charat(");
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
                break;
            }
            
            // Check if this is map indexing by looking at the array type
            bool is_map_index = false;
            if (expr->index.array->expr_type && expr->index.array->expr_type->kind == TYPE_MAP) {
                is_map_index = true;
            } else if (expr->index.index->type == EXPR_STRING || 
                      (expr->index.index->expr_type && expr->index.index->expr_type->kind == TYPE_STRING)) {
                is_map_index = true;
            }
            
            if (is_map_index) {
                // Map indexing: map["key"] -> hashmap_get_int(map, "key")
                emit("hashmap_get_int(");
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
            } else {
                // Array indexing with tagged union support
                // Determine if this is a string array by checking the source
                bool is_string_array = false;
                
                // Check method calls that return string arrays
                if (expr->index.array->type == EXPR_METHOD_CALL) {
                    Token method = expr->index.array->method_call.method;
                    if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "lines", 5) == 0)) {
                        is_string_array = true;
                    }
                }
                
                // Check function calls that return string arrays
                if (expr->index.array->type == EXPR_CALL) {
                    Token callee = expr->index.array->call.callee->token;
                    // System::args returns string array
                    if (callee.length == 12 && memcmp(callee.start, "System::args", 12) == 0) {
                        is_string_array = true;
                    }
                    // File::list_dir returns string array
                    if (callee.length == 14 && memcmp(callee.start, "File::list_dir", 14) == 0) {
                        is_string_array = true;
                    }
                }
                
                // Check identifiers - if variable was assigned from string array
                // For now, we track common patterns
                if (expr->index.array->type == EXPR_IDENT) {
                    Token var_name = expr->index.array->token;
                    // Common variable names for string arrays
                    if ((var_name.length == 4 && memcmp(var_name.start, "args", 4) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "files", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "names", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "parts", 5) == 0) ||
                        (var_name.length == 7 && memcmp(var_name.start, "entries", 7) == 0)) {
                        is_string_array = true;
                    }
                }
                
                // Check array literals - if first element is string, assume string array
                if (expr->index.array->type == EXPR_ARRAY) {
                    if (expr->index.array->array.count > 0) {
                        Expr* first = expr->index.array->array.elements[0];
                        if (first->type == EXPR_STRING) {
                            is_string_array = true;
                        }
                    }
                }
                
                // Check identifiers - look up in symbol table (TODO: needs type info)
                // For now, we can't determine this without better type tracking
                
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
                    // Check if this is a struct array
                    bool is_struct_array = false;
                    Type* elem_type = NULL;
                    if (expr->index.array->expr_type && 
                        expr->index.array->expr_type->kind == TYPE_ARRAY) {
                        elem_type = expr->index.array->expr_type->array_type.element_type;
                        if (elem_type && elem_type->kind == TYPE_STRUCT) {
                            is_struct_array = true;
                        }
                    }
                    
                    if (is_struct_array) {
                        emit("array_get_struct(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(", ");
                        Token type_name = elem_type->struct_type.name;
                        emit("%.*s", type_name.length, type_name.start);
                        emit(")");
                    } else if (is_string_array) {
                        emit("array_get_str(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else {
                        // Default to int array (most common case)
                        emit("array_get_int(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    }
                }
            }
            break;
        }
        case EXPR_ASSIGN: {
            // Check if we need to prefix the assignment target with module name
            char target_name[512];
            memcpy(target_name, expr->assign.name.start, expr->assign.name.length);
            target_name[expr->assign.name.length] = '\0';
            
            if (current_module_prefix && !strchr(target_name, ':') && !strchr(target_name, '.')) {
                // Check if this is a parameter - never prefix parameters
                if (is_parameter(target_name)) {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                    codegen_expr(expr->assign.value);
                    break;
                }
                
                // Check if this is a local variable - never prefix local variables
                if (is_local_variable(target_name)) {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                    codegen_expr(expr->assign.value);
                    break;
                }
                
                // Check if this looks like a module-level variable
                // Don't prefix common local variable names or single-letter variables
                bool is_single_letter = (strlen(target_name) == 1);
                const char* common_locals[] = {"i", "j", "k", "x", "y", "z", "n", "result", "temp", "value",
                                               "a", "b", "c", "d", "e", "f", "g", "h", "m", "p", "q", "r", "s", "t", NULL};
                bool is_common_local = false;
                for (int i = 0; common_locals[i] != NULL; i++) {
                    if (strcmp(target_name, common_locals[i]) == 0) {
                        is_common_local = true;
                        break;
                    }
                }
                
                if (!is_common_local && !is_single_letter) {
                    emit("%s_%.*s = ", current_module_prefix, expr->assign.name.length, expr->assign.name.start);
                } else {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                }
            } else {
                emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
            }
            codegen_expr(expr->assign.value);
            break;
        }
        case EXPR_STRUCT_INIT: {
            // Check if type needs ARC management
            bool needs_arc = false;
            Token type_name = expr->struct_init.type_name;
            
            // Use monomorphic name if available (for generic structs)
            const char* actual_type_name;
            int actual_type_name_len;
            static char prefixed_type_name[128];
            
            if (expr->struct_init.monomorphic_name) {
                actual_type_name = expr->struct_init.monomorphic_name;
                actual_type_name_len = strlen(actual_type_name);
            } else {
                // Check if type_name contains a module prefix (from member expression)
                // e.g., point.Point should become point_Point
                char temp_name[128];
                snprintf(temp_name, 128, "%.*s", type_name.length, type_name.start);
                
                // Check if there's a dot in the name (module.Type)
                char* dot = strchr(temp_name, '.');
                if (dot) {
                    // Replace dot with underscore: point.Point → point_Point
                    *dot = '_';
                    snprintf(prefixed_type_name, 128, "%s", temp_name);
                    actual_type_name = prefixed_type_name;
                    actual_type_name_len = strlen(prefixed_type_name);
                } else if (current_module_prefix) {
                    // Add module prefix if in module context
                    snprintf(prefixed_type_name, 128, "%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                    actual_type_name = prefixed_type_name;
                    actual_type_name_len = strlen(prefixed_type_name);
                } else {
                    actual_type_name = type_name.start;
                    actual_type_name_len = type_name.length;
                }
            }
            
            // Simple heuristic: types starting with uppercase likely need ARC
            if (actual_type_name_len > 0 && actual_type_name[0] >= 'A' && actual_type_name[0] <= 'Z') {
                needs_arc = true;
            }
            
            if (needs_arc) {
                // Generate ARC-managed struct initialization with cast
                emit("*(%.*s*)wyn_arc_new(sizeof(%.*s), &(%.*s){", 
                     actual_type_name_len, actual_type_name,
                     actual_type_name_len, actual_type_name,
                     actual_type_name_len, actual_type_name);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    if (i > 0) emit(", ");
                    emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                    codegen_expr(expr->struct_init.field_values[i]);
                }
                emit("})->data");
            } else {
                // Generate simple struct initialization
                emit("(%.*s){", actual_type_name_len, actual_type_name);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    if (i > 0) emit(", ");
                    emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                    codegen_expr(expr->struct_init.field_values[i]);
                }
                emit("}");
            }
            break;
        }
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
            // Check if this is a module call (resolve aliases)
            char module_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s", obj_name.length, obj_name.start);
            const char* resolved_module = resolve_module_alias(module_name);
            
            // Check if it's a known module
            extern bool is_module_loaded(const char* name);
            extern bool is_builtin_module(const char* name);
            if (is_module_loaded(resolved_module) || is_builtin_module(resolved_module)) {
                // Emit as module_function
                emit("%s_%.*s", resolved_module,
                     field_name.length, field_name.start);
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
            // Generate match expression using if-else chain
            static int match_counter = 0;
            int match_id = match_counter++;
            
            emit("({ ");
            
            // Get the type of the match value
            Type* match_type = expr->match.value->expr_type;
            const char* type_name = "int";  // Default fallback
            int type_name_len = 3;
            
            if (match_type && match_type->kind == TYPE_ENUM && match_type->name.length > 0) {
                type_name = match_type->name.start;
                type_name_len = match_type->name.length;
            }
            
            // Store match value in temp variable
            emit("%.*s __match_val_%d = ", type_name_len, type_name, match_id);
            codegen_expr(expr->match.value);
            emit("; ");
            
            // Generate result variable
            emit("int __match_result_%d; ", match_id);
            
            // Generate if-else chain for each arm
            for (int i = 0; i < expr->match.arm_count; i++) {
                Pattern* pat = expr->match.arms[i].pattern;
                
                if (i > 0) emit("else ");
                
                // Check pattern type
                if (pat->type == PATTERN_WILDCARD) {
                    // Wildcard always matches
                    emit("{ ");
                } else if (pat->type == PATTERN_LITERAL) {
                    emit("if (__match_val_%d == %.*s) { ",
                         match_id,
                         pat->literal.value.length,
                         pat->literal.value.start);
                } else if (pat->type == PATTERN_IDENT) {
                    // Check if this looks like an enum variant (contains underscore)
                    bool is_enum_variant = false;
                    for (int j = 0; j < pat->ident.name.length; j++) {
                        if (pat->ident.name.start[j] == '_') {
                            is_enum_variant = true;
                            break;
                        }
                    }
                    
                    if (is_enum_variant) {
                        // Enum variant - generate comparison
                        emit("if (__match_val_%d == %.*s) { ",
                             match_id,
                             pat->ident.name.length,
                             pat->ident.name.start);
                    } else {
                        // Variable binding - always matches, bind variable
                        emit("{ %.*s %.*s = __match_val_%d; ",
                             type_name_len, type_name,
                             pat->ident.name.length,
                             pat->ident.name.start,
                             match_id);
                    }
                } else if (pat->type == PATTERN_OPTION && pat->option.is_some) {
                    // Enum variant with data: Some(x), Ok(x), etc.
                    // Check tag matches variant
                    emit("if (__match_val_%d.tag == %.*s_TAG) { ",
                         match_id,
                         pat->option.variant_name.length,
                         pat->option.variant_name.start);
                    
                    // Bind inner variable if present
                    if (pat->option.inner && pat->option.inner->type == PATTERN_IDENT) {
                        // Extract variant name (e.g., "Some" from "Option_Some")
                        const char* variant_start = pat->option.variant_name.start;
                        int variant_len = pat->option.variant_name.length;
                        
                        // Find the last underscore to get the variant name
                        const char* underscore = NULL;
                        for (int j = 0; j < variant_len; j++) {
                            if (variant_start[j] == '_') {
                                underscore = variant_start + j;
                            }
                        }
                        
                        // Determine value type (heuristic: Err variants use const char*)
                        const char* value_type = "int";
                        int value_type_len = 3;
                        if (underscore) {
                            int short_variant_len = variant_len - (underscore - variant_start + 1);
                            const char* short_variant = underscore + 1;
                            // Check if variant name contains "Err"
                            if (short_variant_len >= 3 && 
                                memcmp(short_variant, "Err", 3) == 0) {
                                value_type = "const char*";
                                value_type_len = 11;
                            }
                        }
                        
                        if (underscore) {
                            // Use the part after the last underscore
                            int short_variant_len = variant_len - (underscore - variant_start + 1);
                            emit("%.*s %.*s = __match_val_%d.data.%.*s_value; ",
                                 value_type_len, value_type,
                                 pat->option.inner->ident.name.length,
                                 pat->option.inner->ident.name.start,
                                 match_id,
                                 short_variant_len,
                                 underscore + 1);
                        } else {
                            // No underscore, use full name
                            emit("%.*s %.*s = __match_val_%d.data.%.*s_value; ",
                                 value_type_len, value_type,
                                 pat->option.inner->ident.name.length,
                                 pat->option.inner->ident.name.start,
                                 match_id,
                                 variant_len,
                                 variant_start);
                        }
                    }
                } else {
                    // Unsupported pattern - treat as wildcard
                    emit("{ ");
                }
                
                // Generate result
                emit("__match_result_%d = ", match_id);
                codegen_expr(expr->match.arms[i].result);
                emit("; } ");
            }
            
            emit("__match_result_%d; })", match_id);
            break;
        };
        case EXPR_BLOCK: {
            // Generate block expression as compound statement
            emit("({ ");
            for (int i = 0; i < expr->block.stmt_count; i++) {
                codegen_stmt(expr->block.stmts[i]);
            }
            if (expr->block.result) {
                codegen_expr(expr->block.result);
            }
            emit("; })");
            break;
        }
        case EXPR_SOME: {
            // Generate Some constructor using generic some() function
            if (expr->option.value) {
                emit("some(");
                codegen_expr(expr->option.value);
                emit(")");
            } else {
                emit("wyn_none()");
            }
            break;
        }
        case EXPR_NONE: {
            // Generate None constructor
            emit("wyn_none()");
            break;
        }
        case EXPR_OK: {
            // Generate Ok value using ok_int() constructor
            if (expr->option.value) {
                emit("ok_int(");
                codegen_expr(expr->option.value);
                emit(")");
            } else {
                emit("ok_void()");
            }
            break;
        }
        case EXPR_ERR: {
            // Generate Err value using err_int() constructor
            if (expr->option.value) {
                emit("err_int(");
                codegen_expr(expr->option.value);
                emit(")");
            } else {
                emit("err_int(0)");
            }
            break;
        }
        case EXPR_TRY: {
            // TASK-028: Generate ? operator for error propagation
            emit("({ WynResult* _tmp_result = ");
            codegen_expr(expr->try_expr.value);
            emit("; if (wyn_result_is_err(_tmp_result)) return _tmp_result; wyn_result_unwrap(_tmp_result); })");
            break;
        }
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
        case EXPR_LAMBDA: {
            // Lambda function was already emitted in pre-scan
            // Just emit the function pointer reference
            // Find which lambda this is by matching the expression
            // For now, use a simple counter approach
            static int lambda_ref_counter = 0;
            lambda_ref_counter++;
            emit("__lambda_%d", lambda_ref_counter);
            break;
        }
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
            // Generate tuple as a struct literal (no compound statement)
            emit("(struct { ");
            for (int i = 0; i < expr->tuple.count; i++) {
                emit("int item%d; ", i);
            }
            emit("}){ ");
            for (int i = 0; i < expr->tuple.count; i++) {
                if (i > 0) emit(", ");
                codegen_expr(expr->tuple.elements[i]);
            }
            emit(" }");
            break;
        }
        case EXPR_TUPLE_INDEX: {
            // Access tuple element: tuple.0 -> tuple.item0
            emit("(");
            codegen_expr(expr->tuple_index.tuple);
            emit(").item%d", expr->tuple_index.index);
            break;
        }
        case EXPR_INDEX_ASSIGN: {
            // Check if this is map assignment
            bool is_map_assign = false;
            if (expr->index_assign.object->expr_type && expr->index_assign.object->expr_type->kind == TYPE_MAP) {
                is_map_assign = true;
            } else if (expr->index_assign.index->type == EXPR_STRING || 
                      (expr->index_assign.index->expr_type && expr->index_assign.index->expr_type->kind == TYPE_STRING)) {
                is_map_assign = true;
            }
            
            if (is_map_assign) {
                // Map assignment: map["key"] = value -> hashmap_insert_int(map, "key", value)
                emit("hashmap_insert_int(");
                codegen_expr(expr->index_assign.object);
                emit(", ");
                codegen_expr(expr->index_assign.index);
                emit(", ");
                codegen_expr(expr->index_assign.value);
                emit(")");
            } else {
                // ARC-managed array assignment
                emit("{ WynArray* __arr_ptr = &(");
                codegen_expr(expr->index_assign.object);
                emit("); int __idx = ");
                codegen_expr(expr->index_assign.index);
                emit("; if (__idx >= 0 && __idx < __arr_ptr->count) { ");
                
                // Release old value if it exists
                emit("if (__arr_ptr->data[__idx].type == WYN_TYPE_STRING && __arr_ptr->data[__idx].data.string_val) { ");
                emit("/* ARC release old string */ } ");
                
                // Set new value with proper type
                emit("__arr_ptr->data[__idx].type = WYN_TYPE_INT; __arr_ptr->data[__idx].data.int_val = ");
                codegen_expr(expr->index_assign.value);
                emit("; } }");
            }
            break;
        }
        case EXPR_FIELD_ASSIGN: {
            // Handle field assignment: obj.field = value
            emit("(");
            codegen_expr(expr->field_assign.object);
            emit(").%.*s = ", expr->field_assign.field.length, expr->field_assign.field.start);
            codegen_expr(expr->field_assign.value);
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
    emit("#define _POSIX_C_SOURCE 200809L\n");
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
    emit("#include <dirent.h>\n");
    emit("#include <sys/stat.h>\n");
    emit("#include <sys/socket.h>\n");
    emit("#include <sys/time.h>\n");
    emit("#include <netinet/in.h>\n");
    emit("#include <arpa/inet.h>\n");
    emit("#include <netdb.h>\n");
    emit("#include <unistd.h>\n");
    emit("#include <fcntl.h>\n");
    emit("#include <errno.h>\n");
    emit("#include \"wyn_interface.h\"\n");
    emit("#include \"io.h\"\n");
    emit("#include \"arc_runtime.h\"\n");
    emit("#include \"spawn.h\"\n");  // Spawn runtime
    emit("#include \"optional.h\"\n");
    emit("#include \"result.h\"\n");
    emit("#include \"hashmap.h\"\n");
    emit("#include \"hashset.h\"\n");
    emit("\n// Global argc/argv for System::args()\n");
    emit("int __wyn_argc = 0;\n");
    emit("char** __wyn_argv = NULL;\n\n");
    emit("#include \"json.h\"\n");
    emit("#include \"async_runtime.h\"\n\n");
    
    // Function declarations for C interface
    emit("int wyn_get_argc(void);\n");
    emit("const char* wyn_get_argv(int index);\n");
    emit("char* wyn_read_file(const char* path);\n");
    emit("int wyn_write_file(const char* path, const char* content);\n");
    emit("bool wyn_file_exists(const char* path);\n");
    emit("int wyn_store_argv(int index);\n");
    emit("int wyn_get_filename_valid(void);\n");
    emit("int wyn_store_file_content(const char* path);\n");
    emit("int wyn_get_content_valid(void);\n");
    
    // Compiler function declarations
    emit("bool wyn_c_init_lexer(const char* source);\n");
    emit("void wyn_c_init_parser();\n");
    emit("int wyn_c_parse_program();\n");
    emit("void wyn_c_init_checker();\n");
    emit("void wyn_c_check_program(int ast_ptr);\n");
    emit("bool wyn_c_checker_had_error();\n");
    emit("bool wyn_c_generate_code(int ast_ptr, const char* c_filename);\n");
    emit("char* wyn_c_create_c_filename(const char* filename);\n");
    emit("bool wyn_c_compile_to_binary(const char* c_filename, const char* wyn_filename);\n");
    emit("bool wyn_c_remove_file(const char* filename);\n\n");
    
    // String runtime functions
    emit("const char* wyn_string_concat_safe(const char* left, const char* right);\n\n");
    
    // Testing framework
    emit("// Test module\n");
    emit("void Test_init(const char* suite_name);\n");
    emit("void Test_assert(int condition, const char* message);\n");
    emit("void Test_assert_eq_int(int actual, int expected, const char* message);\n");
    emit("void Test_assert_eq_str(const char* actual, const char* expected, const char* message);\n");
    emit("void Test_assert_ne_int(int actual, int expected, const char* message);\n");
    emit("void Test_assert_gt(int actual, int threshold, const char* message);\n");
    emit("void Test_assert_lt(int actual, int threshold, const char* message);\n");
    emit("void Test_assert_gte(int actual, int threshold, const char* message);\n");
    emit("void Test_assert_lte(int actual, int threshold, const char* message);\n");
    emit("void Test_assert_contains(const char* haystack, const char* needle, const char* message);\n");
    emit("void Test_assert_null(void* ptr, const char* message);\n");
    emit("void Test_assert_not_null(void* ptr, const char* message);\n");
    emit("void Test_describe(const char* description);\n");
    emit("void Test_skip(const char* reason);\n");
    emit("int Test_summary();\n\n");
    
    // HTTP client
    emit("// Http module\n");
    emit("typedef struct HttpResponse HttpResponse;\n");
    emit("HttpResponse* Http_get(const char* url);\n");
    emit("HttpResponse* Http_post(const char* url, const char* body, const char* content_type);\n");
    emit("int Http_status(HttpResponse* resp);\n");
    emit("const char* Http_body(HttpResponse* resp);\n");
    emit("const char* Http_header(HttpResponse* resp, const char* name);\n");
    emit("void Http_free(HttpResponse* resp);\n\n");
    
    // TCP server
    emit("// TcpServer module\n");
    emit("typedef struct TcpServer TcpServer;\n");
    emit("TcpServer* TcpServer_new(int port);\n");
    emit("int TcpServer_listen(TcpServer* server);\n");
    emit("int TcpServer_accept(TcpServer* server);\n");
    emit("void TcpServer_close(TcpServer* server);\n\n");
    
    // Socket utilities
    emit("// Socket module\n");
    emit("int Socket_set_timeout(int sock, int seconds);\n");
    emit("int Socket_set_nonblocking(int sock);\n");
    emit("int Socket_poll_read(int sock, int timeout_ms);\n");
    emit("char* Socket_read_line(int sock);\n\n");
    
    // URL utilities
    emit("// Url module\n");
    emit("char* Url_encode(const char* str);\n");
    emit("char* Url_decode(const char* str);\n\n");
    
    // Standard library function declarations
    emit("// String module\n");
    emit("int wyn_string_len(const char* str);\n");
    emit("int wyn_string_contains(const char* str, const char* substr);\n");
    emit("int wyn_string_starts_with(const char* str, const char* prefix);\n");
    emit("int wyn_string_ends_with(const char* str, const char* suffix);\n");
    emit("char* wyn_string_to_upper(const char* str);\n");
    emit("char* wyn_string_to_lower(const char* str);\n");
    emit("char* wyn_string_trim(const char* str);\n");
    emit("char* wyn_str_replace(const char* str, const char* old, const char* new);\n");
    emit("char** wyn_string_split(const char* str, const char* delim, int* count);\n");
    emit("char* wyn_string_join(char** strings, int count, const char* delim);\n");
    emit("char* wyn_str_substring(const char* str, int start, int end);\n");
    emit("int wyn_string_index_of(const char* str, const char* substr);\n");
    emit("int wyn_string_last_index_of(const char* str, const char* substr);\n");
    emit("char* wyn_string_repeat(const char* str, int n);\n");
    emit("char* wyn_string_reverse(const char* str);\n\n");
    
    emit("// Json module\n");
    emit("typedef struct WynJson WynJson;\n");
    emit("WynJson* Json_parse(const char* text);\n");
    emit("char* Json_get_string(WynJson* json, const char* key);\n");
    emit("int Json_get_int(WynJson* json, const char* key);\n");
    emit("void Json_free(WynJson* json);\n\n");
    
    emit("// Time module wrappers\n");
    emit("long Time_now();\n");
    emit("long long Time_now_millis();\n");
    emit("void Time_sleep(int seconds);\n\n");
    
    emit("// Crypto module wrappers\n");
    emit("unsigned int Crypto_hash32(const char* data);\n");
    emit("unsigned long long Crypto_hash64(const char* data);\n\n");
    
    emit("// HashMap module\n");
    emit("int HashMap_new();\n");
    emit("void HashMap_insert(int map, const char* key, int value);\n");
    emit("int HashMap_get(int map, const char* key);\n");
    emit("int HashMap_contains(int map, const char* key);\n");
    emit("int HashMap_len(int map);\n");
    emit("int HashMap_remove(int map, const char* key);\n");
    emit("void HashMap_clear(int map);\n");
    emit("void HashMap_free(int map);\n");
    emit("int wyn_hashmap_new();\n");
    emit("void wyn_hashmap_insert_int(int map, const char* key, int value);\n");
    emit("int wyn_hashmap_get_int(int map, const char* key);\n");
    emit("int wyn_hashmap_has(int map, const char* key);\n");
    emit("int wyn_hashmap_len(int map);\n");
    emit("void wyn_hashmap_free(int map);\n\n");
    
    emit("// Arena module\n");
    emit("typedef struct WynArena WynArena;\n");
    emit("WynArena* wyn_arena_new();\n");
    emit("int* wyn_arena_alloc_int(WynArena* arena, int value);\n");
    emit("void wyn_arena_clear(WynArena* arena);\n");
    emit("void wyn_arena_free(WynArena* arena);\n\n");
    
    emit("// Array module\n");
    emit("int wyn_array_find(int* arr, int len, int (*pred)(int), int* found);\n");
    emit("int wyn_array_any(int* arr, int len, int (*pred)(int));\n");
    emit("int wyn_array_all(int* arr, int len, int (*pred)(int));\n");
    emit("void wyn_array_reverse(int* arr, int len);\n");
    emit("void wyn_array_sort(int* arr, int len);\n");
    emit("int wyn_array_contains(int* arr, int len, int value);\n");
    emit("int wyn_array_index_of(int* arr, int len, int value);\n");
    emit("int wyn_array_last_index_of(int* arr, int len, int value);\n");
    emit("int* wyn_array_slice(int* arr, int start, int end, int* out_len);\n");
    emit("int* wyn_array_concat(int* arr1, int len1, int* arr2, int len2, int* out_len);\n");
    emit("void wyn_array_fill(int* arr, int len, int value);\n");
    emit("int wyn_array_sum(int* arr, int len);\n");
    emit("int wyn_array_min(int* arr, int len);\n");
    emit("int wyn_array_max(int* arr, int len);\n");
    emit("double wyn_array_average(int* arr, int len);\n\n");
    
    emit("// Time module\n");
    emit("long wyn_time_now();\n");
    emit("long long wyn_time_now_millis();\n");
    emit("long long wyn_time_now_micros();\n");
    emit("void wyn_time_sleep(int seconds);\n");
    emit("void wyn_time_sleep_millis(int millis);\n");
    emit("void wyn_time_sleep_micros(int micros);\n");
    emit("char* wyn_time_format(long timestamp);\n");
    emit("long wyn_time_parse(const char* str);\n");
    emit("int wyn_time_year(long timestamp);\n");
    emit("int wyn_time_month(long timestamp);\n");
    emit("int wyn_time_day(long timestamp);\n");
    emit("int wyn_time_hour(long timestamp);\n");
    emit("int wyn_time_minute(long timestamp);\n");
    emit("int wyn_time_second(long timestamp);\n\n");
    
    emit("// Crypto module\n");
    emit("uint32_t wyn_crypto_hash32(const char* data, size_t len);\n");
    emit("uint64_t wyn_crypto_hash64(const char* data, size_t len);\n");
    emit("void wyn_crypto_md5(const char* data, size_t len, char* output);\n");
    emit("void wyn_crypto_sha256(const char* data, size_t len, char* output);\n");
    emit("char* wyn_crypto_base64_encode(const char* data, size_t len);\n");
    emit("char* wyn_crypto_base64_decode(const char* data, size_t* out_len);\n");
    emit("void wyn_crypto_random_bytes(char* buffer, size_t len);\n");
    emit("char* wyn_crypto_random_hex(size_t len);\n");
    emit("char* wyn_crypto_xor_cipher(const char* data, size_t len, const char* key, size_t key_len);\n\n");
    
    // Global variable declarations
    emit("extern char* global_filename;\n");
    emit("extern char* global_file_content;\n\n");
    
    // Exception handling globals
    emit("jmp_buf* current_exception_buf = NULL;\n");
    emit("const char** current_exception_msg = NULL;\n\n");
    
    // Add map type definition
    emit("typedef struct { void** keys; void** values; int count; } WynMap;\n\n");
    
    // Use ARC-compatible array types (don't redefine WYN_TYPE_* enums)
    emit("typedef struct {\n");
    emit("    WynTypeId type;\n");
    emit("    union {\n");
    emit("        int int_val;\n");
    emit("        double float_val;\n");
    emit("        const char* string_val;\n");
    emit("        struct WynArray* array_val;\n");
    emit("        void* struct_val;\n");
    emit("    } data;\n");
    emit("} WynValue;\n\n");
    
    emit("typedef struct WynArray { WynValue* data; int count; int capacity; } WynArray;\n");
    
    // Forward declarations for higher-order array functions
    emit("WynArray wyn_array_map(WynArray arr, int (*fn)(int));\n");
    emit("WynArray wyn_array_filter(WynArray arr, int (*fn)(int));\n");
    emit("int wyn_array_reduce(WynArray arr, int (*fn)(int, int), int initial);\n");
    
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
    emit("#define array_get_struct(arr, idx, T) (*(T*)arr.data[idx].data.struct_val)\n");
    emit("WynValue array_get(WynArray arr, int index) {\n");
    emit("    WynValue val = {0};\n");
    emit("    if (index >= 0 && index < arr.count) val = arr.data[index];\n");
    emit("    return val;\n");
    emit("}\n");
    emit("#define ARRAY_GET_STR(arr, idx) (array_get(arr, idx).data.string_val)\n");
    emit("#define ARRAY_GET_INT(arr, idx) (array_get(arr, idx).data.int_val)\n");
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
    
    // Array methods (Phase 4)
    emit("int array_len(WynArray arr) { return arr.count; }\n");
    emit("bool array_is_empty(WynArray arr) { return arr.count == 0; }\n");
    emit("bool array_contains(WynArray arr, int value) {\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        if (arr.data[i].type == WYN_TYPE_INT && arr.data[i].data.int_val == value) return true;\n");
    emit("    }\n");
    emit("    return false;\n");
    emit("}\n");
    emit("void array_push(WynArray* arr, int value) {\n");
    emit("    if (arr->count >= arr->capacity) {\n");
    emit("        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;\n");
    emit("        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);\n");
    emit("    }\n");
    emit("    arr->data[arr->count].type = WYN_TYPE_INT;\n");
    emit("    arr->data[arr->count].data.int_val = value;\n");
    emit("    arr->count++;\n");
    emit("}\n");
    emit("#define array_push_struct(arr, value, StructType) do { \\\n");
    emit("    StructType __temp_val = (value); \\\n");
    emit("    if ((arr)->count >= (arr)->capacity) { \\\n");
    emit("        (arr)->capacity = (arr)->capacity == 0 ? 4 : (arr)->capacity * 2; \\\n");
    emit("        (arr)->data = realloc((arr)->data, sizeof(WynValue) * (arr)->capacity); \\\n");
    emit("    } \\\n");
    emit("    (arr)->data[(arr)->count].type = WYN_TYPE_STRUCT; \\\n");
    emit("    (arr)->data[(arr)->count].data.struct_val = malloc(sizeof(StructType)); \\\n");
    emit("    memcpy((arr)->data[(arr)->count].data.struct_val, &__temp_val, sizeof(StructType)); \\\n");
    emit("    (arr)->count++; \\\n");
    emit("} while(0)\n");
    emit("int array_pop(WynArray* arr) {\n");
    emit("    if (arr->count == 0) return 0;\n");
    emit("    arr->count--;\n");
    emit("    return arr->data[arr->count].data.int_val;\n");
    emit("}\n");
    emit("int array_index_of(WynArray arr, int value) {\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        if (arr.data[i].type == WYN_TYPE_INT && arr.data[i].data.int_val == value) return i;\n");
    emit("    }\n");
    emit("    return -1;\n");
    emit("}\n");
    emit("void array_reverse(WynArray* arr) {\n");
    emit("    for (int i = 0; i < arr->count / 2; i++) {\n");
    emit("        WynValue temp = arr->data[i];\n");
    emit("        arr->data[i] = arr->data[arr->count - 1 - i];\n");
    emit("        arr->data[arr->count - 1 - i] = temp;\n");
    emit("    }\n");
    emit("}\n");
    emit("void array_sort(WynArray* arr) {\n");
    emit("    for (int i = 0; i < arr->count - 1; i++) {\n");
    emit("        for (int j = 0; j < arr->count - i - 1; j++) {\n");
    emit("            if (arr->data[j].data.int_val > arr->data[j + 1].data.int_val) {\n");
    emit("                WynValue temp = arr->data[j];\n");
    emit("                arr->data[j] = arr->data[j + 1];\n");
    emit("                arr->data[j + 1] = temp;\n");
    emit("            }\n");
    emit("        }\n");
    emit("    }\n");
    emit("}\n\n");
    
    // Array first/last - return int directly (0 if empty)
    emit("int array_first(WynArray arr) {\n");
    emit("    if (arr.count == 0) return 0;\n");
    emit("    return array_get_int(arr, 0);\n");
    emit("}\n");
    
    emit("int array_last(WynArray arr) {\n");
    emit("    if (arr.count == 0) return 0;\n");
    emit("    return array_get_int(arr, arr.count - 1);\n");
    emit("}\n");
    
    emit("int array_count(WynArray arr, int value) {\n");
    emit("    int count = 0;\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        if (array_get_int(arr, i) == value) count++;\n");
    emit("    }\n");
    emit("    return count;\n");
    emit("}\n");
    
    emit("void array_clear(WynArray* arr) {\n");
    emit("    arr->count = 0;\n");
    emit("}\n");
    
    emit("int array_min(WynArray arr) {\n");
    emit("    if (arr.count == 0) return 0;\n");
    emit("    int min = array_get_int(arr, 0);\n");
    emit("    for (int i = 1; i < arr.count; i++) {\n");
    emit("        int val = array_get_int(arr, i);\n");
    emit("        if (val < min) min = val;\n");
    emit("    }\n");
    emit("    return min;\n");
    emit("}\n");
    
    emit("int array_max(WynArray arr) {\n");
    emit("    if (arr.count == 0) return 0;\n");
    emit("    int max = array_get_int(arr, 0);\n");
    emit("    for (int i = 1; i < arr.count; i++) {\n");
    emit("        int val = array_get_int(arr, i);\n");
    emit("        if (val > max) max = val;\n");
    emit("    }\n");
    emit("    return max;\n");
    emit("}\n");
    
    emit("int array_sum(WynArray arr) {\n");
    emit("    int sum = 0;\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        sum += array_get_int(arr, i);\n");
    emit("    }\n");
    emit("    return sum;\n");
    emit("}\n");
    
    emit("int array_average(WynArray arr) {\n");
    emit("    if (arr.count == 0) return 0;\n");
    emit("    return array_sum(arr) / arr.count;\n");
    emit("}\n");
    
    emit("void array_remove_value(WynArray* arr, int value) {\n");
    emit("    int write_idx = 0;\n");
    emit("    for (int i = 0; i < arr->count; i++) {\n");
    emit("        if (array_get_int(*arr, i) != value) {\n");
    emit("            arr->data[write_idx++] = arr->data[i];\n");
    emit("        }\n");
    emit("    }\n");
    emit("    arr->count = write_idx;\n");
    emit("}\n");
    
    emit("void array_insert(WynArray* arr, int index, int value) {\n");
    emit("    if (index < 0 || index > arr->count) return;\n");
    emit("    if (arr->count >= arr->capacity) {\n");
    emit("        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;\n");
    emit("        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);\n");
    emit("    }\n");
    emit("    for (int i = arr->count; i > index; i--) {\n");
    emit("        arr->data[i] = arr->data[i-1];\n");
    emit("    }\n");
    emit("    arr->data[index].type = WYN_TYPE_INT;\n");
    emit("    arr->data[index].data.int_val = value;\n");
    emit("    arr->count++;\n");
    emit("}\n");
    
    emit("WynArray array_take(WynArray arr, int n) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    int count = (n < arr.count) ? n : arr.count;\n");
    emit("    for (int i = 0; i < count; i++) {\n");
    emit("        if (result.count >= result.capacity) {\n");
    emit("            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;\n");
    emit("            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);\n");
    emit("        }\n");
    emit("        result.data[result.count++] = arr.data[i];\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray array_skip(WynArray arr, int n) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    for (int i = n; i < arr.count; i++) {\n");
    emit("        if (result.count >= result.capacity) {\n");
    emit("            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;\n");
    emit("            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);\n");
    emit("        }\n");
    emit("        result.data[result.count++] = arr.data[i];\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray array_slice(WynArray arr, int start, int end) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    if (start < 0) start = 0;\n");
    emit("    if (end > arr.count) end = arr.count;\n");
    emit("    for (int i = start; i < end; i++) {\n");
    emit("        if (result.count >= result.capacity) {\n");
    emit("            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;\n");
    emit("            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);\n");
    emit("        }\n");
    emit("        result.data[result.count++] = arr.data[i];\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* array_join(WynArray arr, const char* sep) {\n");
    emit("    if (arr.count == 0) return \"\";\n");
    emit("    int total_len = 0;\n");
    emit("    int sep_len = strlen(sep);\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        if (arr.data[i].type == WYN_TYPE_STRING) {\n");
    emit("            total_len += strlen(arr.data[i].data.string_val);\n");
    emit("        }\n");
    emit("        if (i < arr.count - 1) total_len += sep_len;\n");
    emit("    }\n");
    emit("    char* result = malloc(total_len + 1);\n");
    emit("    result[0] = '\\0';\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        if (arr.data[i].type == WYN_TYPE_STRING) {\n");
    emit("            strcat(result, arr.data[i].data.string_val);\n");
    emit("        }\n");
    emit("        if (i < arr.count - 1) strcat(result, sep);\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray array_concat(WynArray arr1, WynArray arr2) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    for (int i = 0; i < arr1.count; i++) {\n");
    emit("        if (result.count >= result.capacity) {\n");
    emit("            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;\n");
    emit("            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);\n");
    emit("        }\n");
    emit("        result.data[result.count++] = arr1.data[i];\n");
    emit("    }\n");
    emit("    for (int i = 0; i < arr2.count; i++) {\n");
    emit("        if (result.count >= result.capacity) {\n");
    emit("            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;\n");
    emit("            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);\n");
    emit("        }\n");
    emit("        result.data[result.count++] = arr2.data[i];\n");
    emit("    }\n");
    emit("    return result;\n");
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
    emit("int string_contains(const char* str, const char* substr) {\n");
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
    emit("}\n");
    
    emit("int string_is_alpha(const char* str) {\n");
    emit("    if (!str || !*str) return 0;\n");
    emit("    for (int i = 0; str[i]; i++) {\n");
    emit("        if (!isalpha(str[i])) return 0;\n");
    emit("    }\n");
    emit("    return 1;\n");
    emit("}\n");
    
    emit("int string_is_digit(const char* str) {\n");
    emit("    if (!str || !*str) return 0;\n");
    emit("    for (int i = 0; str[i]; i++) {\n");
    emit("        if (!isdigit(str[i])) return 0;\n");
    emit("    }\n");
    emit("    return 1;\n");
    emit("}\n");
    
    emit("int string_is_alnum(const char* str) {\n");
    emit("    if (!str || !*str) return 0;\n");
    emit("    for (int i = 0; str[i]; i++) {\n");
    emit("        if (!isalnum(str[i])) return 0;\n");
    emit("    }\n");
    emit("    return 1;\n");
    emit("}\n");
    
    emit("int string_is_whitespace(const char* str) {\n");
    emit("    if (!str || !*str) return 0;\n");
    emit("    for (int i = 0; str[i]; i++) {\n");
    emit("        if (!isspace(str[i])) return 0;\n");
    emit("    }\n");
    emit("    return 1;\n");
    emit("}\n");
    
    emit("const char* string_char_at(const char* str, int index) {\n");
    emit("    int len = strlen(str);\n");
    emit("    if (index < 0 || index >= len) return \"\";\n");
    emit("    char* result = malloc(2);\n");
    emit("    result[0] = str[index];\n");
    emit("    result[1] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("int string_equals(const char* a, const char* b) {\n");
    emit("    return strcmp(a, b) == 0;\n");
    emit("}\n");
    
    emit("int string_count(const char* str, const char* substr) {\n");
    emit("    int count = 0;\n");
    emit("    int substr_len = strlen(substr);\n");
    emit("    if (substr_len == 0) return 0;\n");
    emit("    const char* pos = str;\n");
    emit("    while ((pos = strstr(pos, substr)) != NULL) {\n");
    emit("        count++;\n");
    emit("        pos += substr_len;\n");
    emit("    }\n");
    emit("    return count;\n");
    emit("}\n");
    
    emit("int string_is_numeric(const char* str) {\n");
    emit("    if (!str || !*str) return 0;\n");
    emit("    int has_dot = 0;\n");
    emit("    int i = 0;\n");
    emit("    if (str[0] == '-' || str[0] == '+') i = 1;\n");
    emit("    if (!str[i]) return 0;\n");
    emit("    for (; str[i]; i++) {\n");
    emit("        if (str[i] == '.') {\n");
    emit("            if (has_dot) return 0;\n");
    emit("            has_dot = 1;\n");
    emit("        } else if (!isdigit(str[i])) {\n");
    emit("            return 0;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    return 1;\n");
    emit("}\n");
    
    // Phase 2 Task 2.1: Additional string methods
    emit("char* string_capitalize(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    if (len > 0) result[0] = toupper(str[0]);\n");
    emit("    for (int i = 1; i < len; i++) result[i] = tolower(str[i]);\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_reverse(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    for (int i = 0; i < len; i++) result[i] = str[len - 1 - i];\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("int string_len(const char* str) { return strlen(str); }\n");
    emit("int string_is_empty(const char* str) { return str[0] == '\\0'; }\n");
    
    emit("int string_starts_with(const char* str, const char* prefix) {\n");
    emit("    return strncmp(str, prefix, strlen(prefix)) == 0;\n");
    emit("}\n");
    
    emit("int string_ends_with(const char* str, const char* suffix) {\n");
    emit("    int str_len = strlen(str);\n");
    emit("    int suffix_len = strlen(suffix);\n");
    emit("    if (suffix_len > str_len) return 0;\n");
    emit("    return strcmp(str + str_len - suffix_len, suffix) == 0;\n");
    emit("}\n");
    
    emit("int string_index_of(const char* str, const char* substr) {\n");
    emit("    const char* pos = strstr(str, substr);\n");
    emit("    return pos ? (int)(pos - str) : -1;\n");
    emit("}\n");
    
    emit("char* string_replace(const char* str, const char* old, const char* new) {\n");
    emit("    int count = 0;\n");
    emit("    const char* p = str;\n");
    emit("    int old_len = strlen(old);\n");
    emit("    while ((p = strstr(p, old))) { count++; p += old_len; }\n");
    emit("    int new_len = strlen(new);\n");
    emit("    int result_len = strlen(str) + count * (new_len - old_len);\n");
    emit("    char* result = malloc(result_len + 1);\n");
    emit("    char* r = result;\n");
    emit("    p = str;\n");
    emit("    while (*p) {\n");
    emit("        const char* match = strstr(p, old);\n");
    emit("        if (match) {\n");
    emit("            int len = match - p;\n");
    emit("            memcpy(r, p, len); r += len;\n");
    emit("            memcpy(r, new, new_len); r += new_len;\n");
    emit("            p = match + old_len;\n");
    emit("        } else {\n");
    emit("            strcpy(r, p); break;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    result[result_len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_replace_all(const char* str, const char* old, const char* new) {\n");
    emit("    return string_replace(str, old, new);\n");  // replace already replaces all
    emit("}\n");
    
    emit("int string_last_index_of(const char* str, const char* substr) {\n");
    emit("    const char* last = NULL;\n");
    emit("    const char* p = str;\n");
    emit("    while ((p = strstr(p, substr))) { last = p; p++; }\n");
    emit("    return last ? (int)(last - str) : -1;\n");
    emit("}\n");
    
    emit("char* string_slice(const char* str, int start, int end) {\n");
    emit("    int len = strlen(str);\n");
    emit("    if (start < 0) start = 0;\n");
    emit("    if (end > len) end = len;\n");
    emit("    if (start >= end) return strdup(\"\");\n");
    emit("    int slice_len = end - start;\n");
    emit("    char* result = malloc(slice_len + 1);\n");
    emit("    memcpy(result, str + start, slice_len);\n");
    emit("    result[slice_len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_repeat(const char* str, int count) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len * count + 1);\n");
    emit("    for (int i = 0; i < count; i++) memcpy(result + i * len, str, len);\n");
    emit("    result[len * count] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_title(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    int capitalize_next = 1;\n");
    emit("    for (int i = 0; i < len; i++) {\n");
    emit("        if (str[i] == ' ' || str[i] == '\\t' || str[i] == '\\n') {\n");
    emit("            result[i] = str[i];\n");
    emit("            capitalize_next = 1;\n");
    emit("        } else if (capitalize_next) {\n");
    emit("            result[i] = toupper(str[i]);\n");
    emit("            capitalize_next = 0;\n");
    emit("        } else {\n");
    emit("            result[i] = tolower(str[i]);\n");
    emit("        }\n");
    emit("    }\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_trim_left(const char* str) {\n");
    emit("    while (*str == ' ' || *str == '\\t' || *str == '\\n') str++;\n");
    emit("    return strdup(str);\n");
    emit("}\n");
    
    emit("char* string_trim_right(const char* str) {\n");
    emit("    int len = strlen(str);\n");
    emit("    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\\t' || str[len-1] == '\\n')) len--;\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    memcpy(result, str, len);\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_trim(const char* str) {\n");
    emit("    while (*str == ' ' || *str == '\\t' || *str == '\\n') str++;\n");
    emit("    int len = strlen(str);\n");
    emit("    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\\t' || str[len-1] == '\\n')) len--;\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    memcpy(result, str, len);\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray string_split(const char* str, const char* delim) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    char* copy = strdup(str);\n");
    emit("    char* token = strtok(copy, delim);\n");
    emit("    while (token != NULL) {\n");
    emit("        array_push_str(&arr, strdup(token));\n");
    emit("        token = strtok(NULL, delim);\n");
    emit("    }\n");
    emit("    free(copy);\n");
    emit("    return arr;\n");
    emit("}\n");
    
    emit("const char* wyn_string_charat(const char* str, int index) {\n");
    emit("    int len = strlen(str);\n");
    emit("    if (index < 0 || index >= len) return \"\";\n");
    emit("    char* result = malloc(2);\n");
    emit("    result[0] = str[index];\n");
    emit("    result[1] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray string_chars(const char* str) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    for (int i = 0; str[i] != '\\0'; i++) {\n");
    emit("        char* ch = malloc(2);\n");
    emit("        ch[0] = str[i];\n");
    emit("        ch[1] = '\\0';\n");
    emit("        array_push_str(&arr, ch);\n");
    emit("    }\n");
    emit("    return arr;\n");
    emit("}\n");
    
    emit("WynArray string_to_bytes(const char* str) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    for (int i = 0; str[i] != '\\0'; i++) {\n");
    emit("        array_push_int(&arr, (int)(unsigned char)str[i]);\n");
    emit("    }\n");
    emit("    return arr;\n");
    emit("}\n");
    
    emit("char* string_pad_left(const char* str, int width, const char* pad) {\n");
    emit("    int len = strlen(str);\n");
    emit("    if (len >= width) return strdup(str);\n");
    emit("    int pad_len = width - len;\n");
    emit("    char* result = malloc(width + 1);\n");
    emit("    for (int i = 0; i < pad_len; i++) result[i] = pad[0];\n");
    emit("    strcpy(result + pad_len, str);\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* string_pad_right(const char* str, int width, const char* pad) {\n");
    emit("    int len = strlen(str);\n");
    emit("    if (len >= width) return strdup(str);\n");
    emit("    int pad_len = width - len;\n");
    emit("    char* result = malloc(width + 1);\n");
    emit("    strcpy(result, str);\n");
    emit("    for (int i = len; i < width; i++) result[i] = pad[0];\n");
    emit("    result[width] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    emit("WynArray string_lines(const char* str) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    char* copy = strdup(str);\n");
    emit("    char* token = strtok(copy, \"\\n\");\n");
    emit("    while (token != NULL) {\n");
    emit("        array_push_str(&arr, strdup(token));\n");
    emit("        token = strtok(NULL, \"\\n\");\n");
    emit("    }\n");
    emit("    free(copy);\n");
    emit("    return arr;\n");
    emit("}\n");
    
    emit("WynArray string_words(const char* str) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    char* copy = strdup(str);\n");
    emit("    char* token = strtok(copy, \" \\t\\n\\r\");\n");
    emit("    while (token != NULL) {\n");
    emit("        array_push_str(&arr, strdup(token));\n");
    emit("        token = strtok(NULL, \" \\t\\n\\r\");\n");
    emit("    }\n");
    emit("    free(copy);\n");
    emit("    return arr;\n");
    emit("}\n\n");
    emit("void set_clear(WynHashSet* set) {\n");
    emit("    wyn_hashset_clear(set);\n");
    emit("}\n");
    emit("WynHashSet* set_union(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_union(set1, set2);\n");
    emit("}\n");
    emit("WynHashSet* set_intersection(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_intersection(set1, set2);\n");
    emit("}\n");
    emit("WynHashSet* set_difference(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_difference(set1, set2);\n");
    emit("}\n");
    emit("bool set_is_subset(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_is_subset(set1, set2);\n");
    emit("}\n");
    emit("bool set_is_superset(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_is_subset(set2, set1);\n");
    emit("}\n");
    emit("bool set_is_disjoint(WynHashSet* set1, WynHashSet* set2) {\n");
    emit("    return wyn_hashset_is_disjoint(set1, set2);\n");
    emit("}\n\n");
    
    // Phase 3 Task 3.1: Integer methods (conversion methods already exist for interpolation)
    emit("double int_to_float(int n) { return (double)n; }\n");
    emit("int int_abs(int n) { return n < 0 ? -n : n; }\n");
    emit("int int_pow(int base, int exp) {\n");
    emit("    int result = 1;\n");
    emit("    for (int i = 0; i < exp; i++) result *= base;\n");
    emit("    return result;\n");
    emit("}\n");
    emit("int int_min(int a, int b) { return a < b ? a : b; }\n");
    emit("int int_max(int a, int b) { return a > b ? a : b; }\n");
    emit("int int_clamp(int n, int min, int max) {\n");
    emit("    if (n < min) return min;\n");
    emit("    if (n > max) return max;\n");
    emit("    return n;\n");
    emit("}\n");
    emit("int int_is_even(int n) { return n %% 2 == 0; }\n");
    emit("int int_is_odd(int n) { return n %% 2 != 0; }\n");
    emit("int int_is_positive(int n) { return n > 0; }\n");
    emit("int int_is_negative(int n) { return n < 0; }\n");
    emit("int int_is_zero(int n) { return n == 0; }\n");
    emit("char* int_to_binary(int n) {\n");
    emit("    if (n == 0) return \"0\";\n");
    emit("    char* result = malloc(33);\n");
    emit("    int i = 0;\n");
    emit("    unsigned int num = (unsigned int)n;\n");
    emit("    while (num > 0) {\n");
    emit("        result[i++] = (num %% 2) + '0';\n");
    emit("        num /= 2;\n");
    emit("    }\n");
    emit("    result[i] = '\\0';\n");
    emit("    for (int j = 0; j < i/2; j++) {\n");
    emit("        char temp = result[j];\n");
    emit("        result[j] = result[i-1-j];\n");
    emit("        result[i-1-j] = temp;\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    emit("char* int_to_hex(int n) {\n");
    emit("    char* result = malloc(12);\n");
    emit("    sprintf(result, \"%%x\", n);\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    // Phase 3 Task 3.2: Float methods (conversion methods already exist for interpolation)
    emit("int float_to_int(double f) { return (int)f; }\n");
    emit("double float_round(double f) { return round(f); }\n");
    emit("double float_floor(double f) { return floor(f); }\n");
    emit("double float_ceil(double f) { return ceil(f); }\n");
    emit("double float_abs(double f) { return fabs(f); }\n");
    emit("double float_pow(double base, double exp) { return pow(base, exp); }\n");
    emit("double float_sqrt(double f) { return sqrt(f); }\n");
    emit("double float_min(double a, double b) { return a < b ? a : b; }\n");
    emit("double float_max(double a, double b) { return a > b ? a : b; }\n");
    emit("double float_clamp(double f, double min, double max) {\n");
    emit("    if (f < min) return min;\n");
    emit("    if (f > max) return max;\n");
    emit("    return f;\n");
    emit("}\n");
    emit("int float_is_nan(double f) { return isnan(f); }\n");
    emit("int float_is_infinite(double f) { return isinf(f); }\n");
    emit("int float_is_finite(double f) { return isfinite(f); }\n");
    emit("int float_is_positive(double f) { return f > 0.0; }\n");
    emit("int float_is_negative(double f) { return f < 0.0; }\n");
    emit("double float_sin(double f) { return sin(f); }\n");
    emit("double float_cos(double f) { return cos(f); }\n");
    emit("double float_tan(double f) { return tan(f); }\n");
    emit("double float_log(double f) { return log(f); }\n");
    emit("double float_exp(double f) { return exp(f); }\n\n");
    
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
    
    // New map methods for Task 3.3 - updated for v1.3.0 multi-type support
    emit("int map_get_or_default(WynHashMap* map, const char* key, int default_value) {\n");
    emit("    int result = hashmap_get_int(map, key);\n");
    emit("    return (result == -1) ? default_value : result;\n");
    emit("}\n");
    
    emit("void map_merge(WynHashMap* dest, WynHashMap* src) {\n");
    emit("    // Merge src into dest by iterating all buckets\n");
    emit("    for (int i = 0; i < 128; i++) {\n");
    emit("        void* entry = ((void**)src)[i];\n");
    emit("        while (entry) {\n");
    emit("            char* key = *(char**)entry;\n");
    emit("            int value = *((int*)((char*)entry + sizeof(char*)));\n");
    emit("            hashmap_insert_int(dest, key, value);\n");
    emit("            entry = *((void**)((char*)entry + sizeof(char*) + sizeof(int)));\n");
    emit("        }\n");
    emit("    }\n");
    emit("}\n");
    
    emit("int map_len(WynHashMap* map) {\n");
    emit("    int count = 0;\n");
    emit("    for (int i = 0; i < 128; i++) {\n");
    emit("        void* entry = ((void**)map)[i];\n");
    emit("        while (entry) {\n");
    emit("            count++;\n");
    emit("            entry = *((void**)((char*)entry + sizeof(char*) + sizeof(int)));\n");
    emit("        }\n");
    emit("    }\n");
    emit("    return count;\n");
    emit("}\n");
    
    emit("bool map_is_empty(WynHashMap* map) {\n");
    emit("    for (int i = 0; i < 128; i++) {\n");
    emit("        if (((void**)map)[i] != NULL) return false;\n");
    emit("    }\n");
    emit("    return true;\n");
    emit("}\n");
    
    emit("bool map_has(WynHashMap* map, const char* key) {\n");
    emit("    return hashmap_has(map, key);\n");
    emit("}\n");
    
    emit("void map_remove(WynHashMap* map, const char* key) {\n");
    emit("    hashmap_remove(map, key);\n");
    emit("}\n\n");
    
    // Phase 4: Array/Vec methods
    
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
    emit("    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);\n");
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
    
    // JSON functions moved to json.c (real implementation)
    // Old string-based stubs commented out - use json_parse() instead
    
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
    emit("void print_value(WynValue v) {\n");
    emit("    switch(v.type) {\n");
    emit("        case WYN_TYPE_INT: printf(\"%%d\\n\", v.data.int_val); break;\n");
    emit("        case WYN_TYPE_FLOAT: printf(\"%%g\\n\", v.data.float_val); break;\n");
    emit("        case WYN_TYPE_STRING: printf(\"%%s\\n\", v.data.string_val); break;\n");
    emit("        default: printf(\"<value>\\n\"); break;\n");
    emit("    }\n");
    emit("}\n");
    
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
    emit("    WynValue: print_value, \\\n");
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
    emit("int bool_to_int(bool x) { return x ? 1 : 0; }\n");
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
    // print_error function provided by error.c
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
    emit("char* File_read(const char* path) {\n");
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
    
    emit("WynArray File_list_dir(const char* path) {\n");
    emit("    WynArray arr = array_new();\n");
    emit("    DIR* dir = opendir(path);\n");
    emit("    if (!dir) return arr;\n");
    emit("    struct dirent* entry;\n");
    emit("    while ((entry = readdir(dir)) != NULL) {\n");
    emit("        if (strcmp(entry->d_name, \".\") == 0 || strcmp(entry->d_name, \"..\") == 0) continue;\n");
    emit("        char* name = malloc(strlen(entry->d_name) + 1);\n");
    emit("        strcpy(name, entry->d_name);\n");
    emit("        array_push_str(&arr, name);\n");
    emit("    }\n");
    emit("    closedir(dir);\n");
    emit("    return arr;\n");
    emit("}\n");
    
    emit("int File_is_file(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return 0;\n");
    emit("    return S_ISREG(st.st_mode);\n");
    emit("}\n");
    
    emit("int File_is_dir(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return 0;\n");
    emit("    return S_ISDIR(st.st_mode);\n");
    emit("}\n");
    
    emit("char* File_get_cwd() {\n");
    emit("    char* buf = malloc(1024);\n");
    emit("    if (getcwd(buf, 1024) == NULL) { free(buf); return \"\"; }\n");
    emit("    return buf;\n");
    emit("}\n");
    
    emit("int File_create_dir(const char* path) {\n");
    emit("    return mkdir(path, 0755) == 0;\n");
    emit("}\n");
    
    emit("int File_file_size(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return -1;\n");
    emit("    return (int)st.st_size;\n");
    emit("}\n");
    
    emit("char* File_path_join(const char* a, const char* b) {\n");
    emit("    int len_a = strlen(a);\n");
    emit("    int len_b = strlen(b);\n");
    emit("    int needs_sep = (len_a > 0 && a[len_a-1] != '/') ? 1 : 0;\n");
    emit("    char* result = malloc(len_a + len_b + needs_sep + 1);\n");
    emit("    strcpy(result, a);\n");
    emit("    if (needs_sep) strcat(result, \"/\");\n");
    emit("    strcat(result, b);\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* File_basename(const char* path) {\n");
    emit("    const char* last_slash = strrchr(path, '/');\n");
    emit("    if (last_slash == NULL) {\n");
    emit("        char* result = malloc(strlen(path) + 1);\n");
    emit("        strcpy(result, path);\n");
    emit("        return result;\n");
    emit("    }\n");
    emit("    char* result = malloc(strlen(last_slash));\n");
    emit("    strcpy(result, last_slash + 1);\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* File_dirname(const char* path) {\n");
    emit("    const char* last_slash = strrchr(path, '/');\n");
    emit("    if (last_slash == NULL) return \".\";\n");
    emit("    int len = last_slash - path;\n");
    emit("    if (len == 0) return \"/\";\n");
    emit("    char* result = malloc(len + 1);\n");
    emit("    strncpy(result, path, len);\n");
    emit("    result[len] = '\\0';\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("char* File_extension(const char* path) {\n");
    emit("    const char* last_dot = strrchr(path, '.');\n");
    emit("    const char* last_slash = strrchr(path, '/');\n");
    emit("    if (last_dot == NULL || (last_slash != NULL && last_dot < last_slash)) return \"\";\n");
    emit("    char* result = malloc(strlen(last_dot));\n");
    emit("    strcpy(result, last_dot + 1);\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("int File_write(const char* path, const char* data) {\n");
    emit("    last_error[0] = 0;\n");
    emit("    FILE* f = fopen(path, \"w\");\n");
    emit("    if(!f) { snprintf(last_error, 256, \"Cannot write file: %%s\", path); return 0; }\n");
    emit("    fputs(data, f);\n");
    emit("    fclose(f);\n");
    emit("    return 1;\n");
    emit("}\n");
    emit("int File_exists(const char* path) { FILE* f = fopen(path, \"r\"); if(f) { fclose(f); return 1; } return 0; }\n");
    emit("int File_delete(const char* path) { return remove(path) == 0; }\n");
    
    // File::copy - copy file from src to dst
    emit("int File_copy(const char* src, const char* dst) {\n");
    emit("    FILE* fsrc = fopen(src, \"rb\");\n");
    emit("    if(!fsrc) return 0;\n");
    emit("    FILE* fdst = fopen(dst, \"wb\");\n");
    emit("    if(!fdst) { fclose(fsrc); return 0; }\n");
    emit("    char buf[8192];\n");
    emit("    size_t n;\n");
    emit("    while((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {\n");
    emit("        if(fwrite(buf, 1, n, fdst) != n) { fclose(fsrc); fclose(fdst); return 0; }\n");
    emit("    }\n");
    emit("    fclose(fsrc); fclose(fdst);\n");
    emit("    return 1;\n");
    emit("}\n");
    
    // File::move - move/rename file
    emit("int File_move(const char* src, const char* dst) { return rename(src, dst) == 0; }\n");
    
    // File::size - get file size
    emit("int File_size(const char* path) {\n");
    emit("    FILE* f = fopen(path, \"rb\");\n");
    emit("    if(!f) return -1;\n");
    emit("    fseek(f, 0, SEEK_END);\n");
    emit("    long sz = ftell(f);\n");
    emit("    fclose(f);\n");
    emit("    return (int)sz;\n");
    emit("}\n");
    
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
    
    // File::modified_time - get file modification timestamp
    emit("long File_modified_time(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return -1;\n");
    emit("    return (long)st.st_mtime;\n");
    emit("}\n\n");
    
    // File::create_dir_all - recursive mkdir (like mkdir -p)
    emit("int File_create_dir_all(const char* path) {\n");
    emit("    if (!path || !*path) return 0;\n");
    emit("    char tmp[1024];\n");
    emit("    char *p = NULL;\n");
    emit("    size_t len = strlen(path);\n");
    emit("    if (len >= sizeof(tmp)) return 0;\n");
    emit("    snprintf(tmp, sizeof(tmp), \"%%s\", path);\n");
    emit("    if (tmp[len - 1] == '/') tmp[len - 1] = 0;\n");
    emit("    for (p = tmp + 1; *p; p++) {\n");
    emit("        if (*p == '/') {\n");
    emit("            *p = 0;\n");
    emit("            #ifdef _WIN32\n");
    emit("            mkdir(tmp);\n");
    emit("            #else\n");
    emit("            mkdir(tmp, 0755);\n");
    emit("            #endif\n");
    emit("            *p = '/';\n");
    emit("        }\n");
    emit("    }\n");
    emit("    #ifdef _WIN32\n");
    emit("    return mkdir(tmp) == 0 || errno == EEXIST;\n");
    emit("    #else\n");
    emit("    return mkdir(tmp, 0755) == 0 || errno == EEXIST;\n");
    emit("    #endif\n");
    emit("}\n\n");
    
    // File::remove_dir_all - recursive rmdir (like rm -rf)
    emit("int File_remove_dir_all(const char* path) {\n");
    emit("    if (!path || !*path) return 0;\n");
    emit("    DIR *d = opendir(path);\n");
    emit("    if (!d) return remove(path) == 0;\n");
    emit("    struct dirent *p;\n");
    emit("    int r = 0;\n");
    emit("    while (!r && (p = readdir(d))) {\n");
    emit("        if (!strcmp(p->d_name, \".\") || !strcmp(p->d_name, \"..\")) continue;\n");
    emit("        char buf[1024];\n");
    emit("        snprintf(buf, sizeof(buf), \"%%s/%%s\", path, p->d_name);\n");
    emit("        struct stat st;\n");
    emit("        if (stat(buf, &st) == 0) {\n");
    emit("            if (S_ISDIR(st.st_mode)) {\n");
    emit("                r = !File_remove_dir_all(buf);\n");
    emit("            } else {\n");
    emit("                r = remove(buf) != 0;\n");
    emit("            }\n");
    emit("        }\n");
    emit("    }\n");
    emit("    closedir(d);\n");
    emit("    return !r && rmdir(path) == 0;\n");
    emit("}\n\n");
    
    // System execution
    emit("char* System_exec(const char* cmd) {\n");
    emit("    FILE* pipe = popen(cmd, \"r\");\n");
    emit("    if (!pipe) return \"\";\n");
    emit("    char* result = malloc(65536);\n");
    emit("    result[0] = 0;\n");
    emit("    char buffer[1024];\n");
    emit("    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {\n");
    emit("        strcat(result, buffer);\n");
    emit("    }\n");
    emit("    pclose(pipe);\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("int System_exec_code(const char* cmd) {\n");
    emit("    int result = system(cmd);\n");
    emit("    #ifdef _WIN32\n");
    emit("    return result;\n");
    emit("    #else\n");
    emit("    return WEXITSTATUS(result);\n");
    emit("    #endif\n");
    emit("}\n");
    
    emit("void System_exit(int code) { exit(code); }\n");
    
    emit("char* System_env(const char* key) {\n");
    emit("    char* val = getenv(key);\n");
    emit("    if (!val) return \"\";\n");
    emit("    char* result = malloc(strlen(val) + 1);\n");
    emit("    strcpy(result, val);\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    emit("int System_set_env(const char* key, const char* value) {\n");
    emit("    #ifdef _WIN32\n");
    emit("    return _putenv_s(key, value) == 0;\n");
    emit("    #else\n");
    emit("    return setenv(key, value, 1) == 0;\n");
    emit("    #endif\n");
    emit("}\n\n");
    
    emit("WynArray System_args() {\n");
    emit("    WynArray arr;\n");
    emit("    arr.data = malloc(__wyn_argc * sizeof(WynValue));\n");
    emit("    arr.count = __wyn_argc;\n");
    emit("    arr.capacity = __wyn_argc;\n");
    emit("    for (int i = 0; i < __wyn_argc; i++) {\n");
    emit("        arr.data[i].type = WYN_TYPE_STRING;\n");
    emit("        arr.data[i].data.string_val = __wyn_argv[i];\n");
    emit("    }\n");
    emit("    return arr;\n");
    emit("}\n\n");
    
    // Env module functions
    emit("char* Env_get(const char* name) {\n");
    emit("    if (!name) return \"\";\n");
    emit("    char* value = getenv(name);\n");
    emit("    return value ? value : \"\";\n");
    emit("}\n");
    
    emit("int Env_set(const char* name, const char* value) {\n");
    emit("    if (!name || !value) return 0;\n");
    emit("#ifdef _WIN32\n");
    emit("    return _putenv_s(name, value) == 0;\n");
    emit("#else\n");
    emit("    return setenv(name, value, 1) == 0;\n");
    emit("#endif\n");
    emit("}\n");
    
    emit("WynArray Env_all() {\n");
    emit("    extern char **environ;\n");
    emit("    WynArray arr;\n");
    emit("    int count = 0;\n");
    emit("    for (char** env = environ; *env; env++) count++;\n");
    emit("    arr.data = malloc(count * sizeof(WynValue));\n");
    emit("    arr.count = count;\n");
    emit("    arr.capacity = count;\n");
    emit("    for (int i = 0; i < count; i++) {\n");
    emit("        arr.data[i].type = WYN_TYPE_STRING;\n");
    emit("        arr.data[i].data.string_val = environ[i];\n");
    emit("    }\n");
    emit("    return arr;\n");
    emit("}\n\n");
    
    // Queue module - FIFO (First In, First Out)
    emit("typedef struct { WynArray arr; } Queue;\n\n");
    
    emit("Queue* Queue_new() {\n");
    emit("    Queue* q = malloc(sizeof(Queue));\n");
    emit("    q->arr.data = NULL;\n");
    emit("    q->arr.count = 0;\n");
    emit("    q->arr.capacity = 0;\n");
    emit("    return q;\n");
    emit("}\n\n");
    
    emit("void Queue_push(Queue* q, int value) {\n");
    emit("    if (q->arr.count >= q->arr.capacity) {\n");
    emit("        q->arr.capacity = q->arr.capacity == 0 ? 4 : q->arr.capacity * 2;\n");
    emit("        q->arr.data = realloc(q->arr.data, sizeof(WynValue) * q->arr.capacity);\n");
    emit("    }\n");
    emit("    q->arr.data[q->arr.count].type = WYN_TYPE_INT;\n");
    emit("    q->arr.data[q->arr.count].data.int_val = value;\n");
    emit("    q->arr.count++;\n");
    emit("}\n\n");
    
    emit("int Queue_pop(Queue* q) {\n");
    emit("    if (q->arr.count == 0) return 0;\n");
    emit("    int value = q->arr.data[0].data.int_val;\n");
    emit("    for (int i = 1; i < q->arr.count; i++) {\n");
    emit("        q->arr.data[i-1] = q->arr.data[i];\n");
    emit("    }\n");
    emit("    q->arr.count--;\n");
    emit("    return value;\n");
    emit("}\n\n");
    
    emit("int Queue_peek(Queue* q) {\n");
    emit("    if (q->arr.count == 0) return 0;\n");
    emit("    return q->arr.data[0].data.int_val;\n");
    emit("}\n\n");
    
    emit("int Queue_len(Queue* q) {\n");
    emit("    return q->arr.count;\n");
    emit("}\n\n");
    
    emit("int Queue_is_empty(Queue* q) {\n");
    emit("    return q->arr.count == 0;\n");
    emit("}\n\n");
    
    // Stack module - LIFO (Last In, First Out)
    emit("typedef struct { WynArray arr; } Stack;\n\n");
    
    emit("Stack* Stack_new() {\n");
    emit("    Stack* s = malloc(sizeof(Stack));\n");
    emit("    s->arr.data = NULL;\n");
    emit("    s->arr.count = 0;\n");
    emit("    s->arr.capacity = 0;\n");
    emit("    return s;\n");
    emit("}\n\n");
    
    emit("void Stack_push(Stack* s, int value) {\n");
    emit("    if (s->arr.count >= s->arr.capacity) {\n");
    emit("        s->arr.capacity = s->arr.capacity == 0 ? 4 : s->arr.capacity * 2;\n");
    emit("        s->arr.data = realloc(s->arr.data, sizeof(WynValue) * s->arr.capacity);\n");
    emit("    }\n");
    emit("    s->arr.data[s->arr.count].type = WYN_TYPE_INT;\n");
    emit("    s->arr.data[s->arr.count].data.int_val = value;\n");
    emit("    s->arr.count++;\n");
    emit("}\n\n");
    
    emit("int Stack_pop(Stack* s) {\n");
    emit("    if (s->arr.count == 0) return 0;\n");
    emit("    s->arr.count--;\n");
    emit("    return s->arr.data[s->arr.count].data.int_val;\n");
    emit("}\n\n");
    
    emit("int Stack_peek(Stack* s) {\n");
    emit("    if (s->arr.count == 0) return 0;\n");
    emit("    return s->arr.data[s->arr.count - 1].data.int_val;\n");
    emit("}\n\n");
    
    emit("int Stack_len(Stack* s) {\n");
    emit("    return s->arr.count;\n");
    emit("}\n\n");
    
    emit("int Stack_is_empty(Stack* s) {\n");
    emit("    return s->arr.count == 0;\n");
    emit("}\n\n");
    
    // Math module functions
    emit("double Math_pow(double x, double y) { return pow(x, y); }\n");
    emit("double Math_sqrt(double x) { return sqrt(x); }\n");
    
    // Net module functions
    emit("int Net_listen(int port) {\n");
    emit("    int sockfd = socket(AF_INET, SOCK_STREAM, 0);\n");
    emit("    if (sockfd < 0) return -1;\n");
    emit("    int opt = 1;\n");
    emit("    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));\n");
    emit("    struct sockaddr_in addr;\n");
    emit("    addr.sin_family = AF_INET;\n");
    emit("    addr.sin_addr.s_addr = INADDR_ANY;\n");
    emit("    addr.sin_port = htons(port);\n");
    emit("    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {\n");
    emit("        close(sockfd);\n");
    emit("        return -1;\n");
    emit("    }\n");
    emit("    if (listen(sockfd, 5) < 0) {\n");
    emit("        close(sockfd);\n");
    emit("        return -1;\n");
    emit("    }\n");
    emit("    return sockfd;\n");
    emit("}\n\n");
    
    emit("int Net_connect(const char* host, int port) {\n");
    emit("    int sockfd = socket(AF_INET, SOCK_STREAM, 0);\n");
    emit("    if (sockfd < 0) return -1;\n");
    emit("    struct sockaddr_in addr;\n");
    emit("    addr.sin_family = AF_INET;\n");
    emit("    addr.sin_port = htons(port);\n");
    emit("    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {\n");
    emit("        close(sockfd);\n");
    emit("        return -1;\n");
    emit("    }\n");
    emit("    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {\n");
    emit("        close(sockfd);\n");
    emit("        return -1;\n");
    emit("    }\n");
    emit("    return sockfd;\n");
    emit("}\n\n");
    
    emit("int Net_send(int sockfd, const char* data) {\n");
    emit("    int len = strlen(data);\n");
    emit("    int sent = send(sockfd, data, len, 0);\n");
    emit("    return sent;\n");
    emit("}\n\n");
    
    emit("char* Net_recv(int sockfd) {\n");
    emit("    char* buffer = malloc(4096);\n");
    emit("    int received = recv(sockfd, buffer, 4095, 0);\n");
    emit("    if (received < 0) {\n");
    emit("        free(buffer);\n");
    emit("        return \"\";\n");
    emit("    }\n");
    emit("    buffer[received] = '\\0';\n");
    emit("    return buffer;\n");
    emit("}\n\n");
    
    emit("int Net_close(int sockfd) {\n");
    emit("    return close(sockfd) == 0 ? 1 : 0;\n");
    emit("}\n\n");
    
    // Time and Crypto functions are in stdlib_runtime.c
    
    emit("char* Time_format(int timestamp) {\n");
    emit("    time_t t = (time_t)timestamp;\n");
    emit("    struct tm* tm_info = localtime(&t);\n");
    emit("    char* buffer = malloc(64);\n");
    emit("    strftime(buffer, 64, \"%%Y-%%m-%%d %%H:%%M:%%S\", tm_info);\n");
    emit("    return buffer;\n");
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
    
    // Generic higher-order array functions
    emit("WynArray wyn_array_map(WynArray arr, int (*fn)(int)) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        int val = array_get_int(arr, i);\n");
    emit("        array_push_int(&result, fn(val));\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("WynArray wyn_array_filter(WynArray arr, int (*fn)(int)) {\n");
    emit("    WynArray result = array_new();\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        int val = array_get_int(arr, i);\n");
    emit("        if (fn(val)) array_push_int(&result, val);\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
    emit("int wyn_array_reduce(WynArray arr, int (*fn)(int, int), int initial) {\n");
    emit("    int result = initial;\n");
    emit("    for (int i = 0; i < arr.count; i++) {\n");
    emit("        int val = array_get_int(arr, i);\n");
    emit("        result = fn(result, val);\n");
    emit("    }\n");
    emit("    return result;\n");
    emit("}\n");
    
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
    emit("char* split_get(const char* s, const char* delim, int index) { int count = 0; char** parts = str_split(s, delim, &count); if (index < 0 || index >= count) return \"\"; return parts[index]; }\n");
    emit("int split_count(const char* s, const char* delim) { int count = 0; str_split(s, delim, &count); return count; }\n");
    emit("char* char_at(const char* s, int index) { if (index < 0 || index >= strlen(s)) return \"\"; static char buf[2]; buf[0] = s[index]; buf[1] = '\\0'; return buf; }\n");
    emit("int is_numeric(const char* s) { if (!s || !*s) return 0; int i = 0; if (s[0] == '-' || s[0] == '+') i++; if (!s[i]) return 0; while (s[i]) { if (s[i] < '0' || s[i] > '9') return 0; i++; } return 1; }\n");
    emit("int str_count(const char* s, const char* substr) { if (!s || !substr || !*substr) return 0; int count = 0; const char* p = s; while ((p = strstr(p, substr)) != NULL) { count++; p += strlen(substr); } return count; }\n");
    emit("int str_contains_substr(const char* s, const char* substr) { return strstr(s, substr) != NULL; }\n");
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
    
    // ARC function implementations are in arc_runtime.c - just declare what we need
    emit("// ARC functions are provided by arc_runtime.c\n\n");
}

// Track async function context
static bool in_async_function = false;

static void emit_function_with_prefix(Stmt* fn_stmt, const char* prefix) {
    if (!fn_stmt || fn_stmt->type != STMT_FN) {
        return;
    }
    
    // Convert module name to C identifier
    const char* c_prefix = module_to_c_ident(prefix);
    
    // Set module context for internal call prefixing
    const char* saved_prefix = current_module_prefix;
    current_module_prefix = prefix;
    
    // Register function parameters for scope tracking
    clear_parameters();
    clear_local_variables();
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        char param_name[256];
        snprintf(param_name, 256, "%.*s", fn_stmt->fn.params[i].length, fn_stmt->fn.params[i].start);
        register_parameter(param_name);
    }
    
    // Emit function signature with module prefix
    // Private functions are static (not accessible from outside)
    if (!fn_stmt->fn.is_public) {
        emit("static ");
    }
    
    // Determine return type
    const char* return_type = "int";
    static char custom_return_type[128];
    if (fn_stmt->fn.return_type) {
        if (fn_stmt->fn.return_type->type == EXPR_IDENT) {
            Token rt = fn_stmt->fn.return_type->token;
            if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) {
                return_type = "const char*";
            } else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) {
                return_type = "double";
            } else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) {
                return_type = "bool";
            } else if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) {
                return_type = "int";
            } else if (rt.length == 7 && memcmp(rt.start, "HashMap", 7) == 0) {
                return_type = "WynHashMap*";
            } else if (rt.length == 7 && memcmp(rt.start, "HashSet", 7) == 0) {
                return_type = "WynHashSet*";
            } else {
                // Custom struct type - add module prefix if in module context
                if (current_module_prefix) {
                    snprintf(custom_return_type, 128, "%s_%.*s", current_module_prefix, rt.length, rt.start);
                } else {
                    snprintf(custom_return_type, 128, "%.*s", rt.length, rt.start);
                }
                return_type = custom_return_type;
            }
        }
    }
    
    emit("%s %s_%.*s(", return_type, c_prefix, fn_stmt->fn.name.length, fn_stmt->fn.name.start);
    
    // Parameters
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        if (i > 0) emit(", ");
        
        // Check if parameter has a type expression
        if (fn_stmt->fn.param_types && fn_stmt->fn.param_types[i]) {
            Expr* param_type = fn_stmt->fn.param_types[i];
            if (param_type->type == EXPR_IDENT) {
                // Convert Wyn types to C types
                Token type_token = param_type->token;
                const char* c_type = "int";
                
                if (type_token.length == 6 && memcmp(type_token.start, "string", 6) == 0) {
                    c_type = "const char*";
                } else if (type_token.length == 5 && memcmp(type_token.start, "float", 5) == 0) {
                    c_type = "double";
                } else if (type_token.length == 4 && memcmp(type_token.start, "bool", 4) == 0) {
                    c_type = "bool";
                } else if (type_token.length == 5 && memcmp(type_token.start, "array", 5) == 0) {
                    c_type = "WynArray";
                } else if (type_token.length == 3 && memcmp(type_token.start, "int", 3) == 0) {
                    c_type = "int";
                } else if (type_token.length == 7 && memcmp(type_token.start, "HashMap", 7) == 0) {
                    c_type = "WynHashMap*";
                } else if (type_token.length == 7 && memcmp(type_token.start, "HashSet", 7) == 0) {
                    c_type = "WynHashSet*";
                } else {
                    // Custom struct type - add module prefix if in module context
                    if (current_module_prefix) {
                        emit("%s_%.*s ", current_module_prefix, type_token.length, type_token.start);
                    } else {
                        emit("%.*s ", type_token.length, type_token.start);
                    }
                    goto emit_param_name;
                }
                
                emit("%s ", c_type);
            } else {
                emit("int ");
            }
        } else {
            emit("int ");
        }
        
        emit_param_name:
        // Emit parameter name
        Token param_name = fn_stmt->fn.params[i];
        emit("%.*s", param_name.length, param_name.start);
    }
    emit(") ");
    
    // Body
    if (fn_stmt->fn.body) {
        if (fn_stmt->fn.body->type == STMT_BLOCK) {
            emit("{\n");
            codegen_stmt(fn_stmt->fn.body);
            emit("}\n");
        } else {
            codegen_stmt(fn_stmt->fn.body);
        }
    }
    emit("\n");
    
    // Restore context
    current_module_prefix = saved_prefix;
}

void codegen_stmt(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_EXPR:
            codegen_expr(stmt->expr);
            emit(";\n");
            break;
        case STMT_VAR: {
            // Determine C type based on explicit type annotation or initializer
            const char* c_type = "int";
            bool is_already_const = false;  // Track if type already has const
            bool needs_arc_management = false;
            
            // Check for explicit type annotation first
            if (stmt->var.type) {
                if (stmt->var.type->type == EXPR_OPTIONAL_TYPE) {
                    // Handle optional type annotation like int?
                    c_type = "WynOptional*";
                    needs_arc_management = true;
                } else if (stmt->var.type->type == EXPR_ARRAY) {
                    // Handle typed array annotation like [TokenType]
                    c_type = "WynArray";
                    needs_arc_management = false;
                } else if (stmt->var.type->type == EXPR_CALL) {
                    // Handle generic type instantiation: HashMap<K,V>, Option<T>, etc.
                    if (stmt->var.type->call.callee->type == EXPR_IDENT) {
                        Token type_name = stmt->var.type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            c_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            c_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            c_type = "WynOptional*";
                            needs_arc_management = true;
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            c_type = "WynResult*";
                            needs_arc_management = true;
                        }
                    }
                } else if (stmt->var.type->type == EXPR_IDENT) {
                    Token type_name = stmt->var.type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        c_type = "int";
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        c_type = "const char*";
                        is_already_const = true;  // String type already has const
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        c_type = "double";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        c_type = "bool";
                    } else {
                        // Custom struct/enum type - use the type name as-is
                        static char custom_type_buf[256];
                        int len = type_name.length < 255 ? type_name.length : 255;
                        memcpy(custom_type_buf, type_name.start, len);
                        custom_type_buf[len] = '\0';
                        c_type = custom_type_buf;
                    }
                }
            } else if (stmt->var.init) {
                // Infer type from initializer if no explicit type
                if (stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                    is_already_const = true;  // String literals already have const
                } else if (stmt->var.init->type == EXPR_STRING_INTERP) {
                    c_type = "char*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (stmt->var.init->type == EXPR_BOOL) {
                    c_type = "bool";
                } else if (stmt->var.init->type == EXPR_ARRAY) {
                    c_type = "WynArray";
                    needs_arc_management = false;  // Array expression already returns proper value
                } else if (stmt->var.init->type == EXPR_MAP) {
                    // Map type - use the typedef
                    c_type = "WynMap";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_METHOD_CALL) {
                    // Phase 1 Task 1.3: Infer type from method call
                    // First check if the expression has a type from the checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "int";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            case TYPE_ARRAY:
                                c_type = "WynArray";
                                break;
                            case TYPE_MAP:
                                c_type = "WynMap";
                                needs_arc_management = true;
                                break;
                            default:
                                c_type = "int";
                        }
                    } else {
                        // Fallback: Determine receiver type from expr_type (populated by checker)
                        const char* receiver_type = "int";  // default
                        
                        Expr* obj = stmt->var.init->method_call.object;
                        if (obj->expr_type) {
                            // Use type information from checker
                            switch (obj->expr_type->kind) {
                                case TYPE_STRING:
                                    receiver_type = "string";
                                    break;
                                case TYPE_INT:
                                    receiver_type = "int";
                                    break;
                                case TYPE_FLOAT:
                                    receiver_type = "float";
                                    break;
                                case TYPE_BOOL:
                                    receiver_type = "bool";
                                    break;
                                case TYPE_ARRAY:
                                    receiver_type = "array";
                                    break;
                                case TYPE_MAP:
                                    receiver_type = "map";
                                    break;
                                default:
                                    receiver_type = "int";
                            }
                        } else {
                            // Fallback: infer from expression type
                            if (obj->type == EXPR_STRING) {
                                receiver_type = "string";
                            } else if (obj->type == EXPR_FLOAT) {
                                receiver_type = "float";
                            } else if (obj->type == EXPR_INT) {
                                receiver_type = "int";
                            } else if (obj->type == EXPR_BOOL) {
                                receiver_type = "bool";
                            } else if (obj->type == EXPR_ARRAY) {
                                receiver_type = "array";
                            } else if (obj->type == EXPR_MAP) {
                                receiver_type = "map";
                            } else if (obj->type == EXPR_METHOD_CALL) {
                                // Nested method call (chaining) - assume string for now
                                receiver_type = "string";
                            }
                        }
                        
                        // Look up method return type
                        Token method = stmt->var.init->method_call.method;
                        char method_name[64];
                        snprintf(method_name, sizeof(method_name), "%.*s", method.length, method.start);
                        
                        const char* return_type = lookup_method_return_type(receiver_type, method_name);
                        if (return_type) {
                            if (strcmp(return_type, "string") == 0) {
                                c_type = "char*";
                                needs_arc_management = false;  // Disable ARC for now (Phase 1 focus)
                            } else if (strcmp(return_type, "int") == 0) {
                                c_type = "int";
                            } else if (strcmp(return_type, "float") == 0) {
                                c_type = "double";
                            } else if (strcmp(return_type, "bool") == 0) {
                                c_type = "bool";
                            } else if (strcmp(return_type, "array") == 0) {
                                c_type = "WynArray";
                            } else if (strcmp(return_type, "optional") == 0) {
                                c_type = "WynOptional*";
                                needs_arc_management = true;
                            } else if (strcmp(return_type, "void") == 0) {
                                c_type = "void";
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_STRUCT_INIT) {
                    // Use the struct type name (monomorphic if available)
                    static char struct_type[128];
                    if (stmt->var.init->struct_init.monomorphic_name) {
                        snprintf(struct_type, 128, "%s", stmt->var.init->struct_init.monomorphic_name);
                    } else {
                        // Check if type_name contains a module prefix (from member expression)
                        // e.g., point.Point should become point_Point
                        Token type_name = stmt->var.init->struct_init.type_name;
                        
                        char temp_name[128];
                        snprintf(temp_name, 128, "%.*s", type_name.length, type_name.start);
                        
                        // Check if there's a dot in the name (module.Type)
                        char* dot = strchr(temp_name, '.');
                        if (dot) {
                            // Replace dot with underscore: point.Point → point_Point
                            *dot = '_';
                            snprintf(struct_type, 128, "%s", temp_name);
                        } else if (current_module_prefix) {
                            // Add module prefix if in module context
                            snprintf(struct_type, 128, "%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                        } else {
                            snprintf(struct_type, 128, "%.*s", type_name.length, type_name.start);
                        }
                    }
                    c_type = struct_type;
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_SOME || stmt->var.init->type == EXPR_NONE) {
                    // Optional type
                    c_type = "WynOptional*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_OK || stmt->var.init->type == EXPR_ERR) {
                    // TASK-026: Result type
                    c_type = "WynResult*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_LAMBDA) {
                    // Lambda/closure type - function pointer
                    c_type = "int (*)(int, int)";  // Simplified: assume int params and return
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_HASHMAP_LITERAL) {
                    // v1.2.3: HashMap literal
                    c_type = "WynHashMap*";
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_HASHSET_LITERAL) {
                    // v1.2.3: HashSet literal
                    c_type = "WynHashSet*";
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_INDEX) {
                    // Array/map/string indexing - check expr_type from checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "int";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            default:
                                c_type = "int";
                        }
                    } else {
                        // Fallback to heuristics
                        if (stmt->var.init->index.array->type == EXPR_CALL) {
                            Token callee = stmt->var.init->index.array->call.callee->token;
                            if ((callee.length == 12 && memcmp(callee.start, "System::args", 12) == 0) ||
                                (callee.length == 14 && memcmp(callee.start, "File::list_dir", 14) == 0)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_METHOD_CALL) {
                            Token method = stmt->var.init->index.array->method_call.method;
                            if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "lines", 5) == 0)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_IDENT) {
                            Token var_name = stmt->var.init->index.array->token;
                            if ((var_name.length == 4 && memcmp(var_name.start, "args", 4) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "files", 5) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "names", 5) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "parts", 5) == 0) ||
                                (var_name.length == 7 && memcmp(var_name.start, "entries", 7) == 0)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_ARRAY) {
                            if (stmt->var.init->index.array->array.count > 0) {
                                if (stmt->var.init->index.array->array.elements[0]->type == EXPR_STRING) {
                                    c_type = "const char*";
                                    is_already_const = true;
                                }
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_TUPLE) {
                    // Tuple type - use __auto_type (GCC/Clang extension)
                    c_type = "__auto_type";
                } else if (stmt->var.init->type == EXPR_CALL) {
                    // Function call - use __auto_type to infer return type
                    c_type = "__auto_type";
                } else if (stmt->var.init->type == EXPR_BINARY) {
                    // Binary expression - check if it's string concatenation or arithmetic
                    if (stmt->var.init->binary.op.type == TOKEN_PLUS) {
                        // Check if either operand is explicitly a string literal
                        bool left_is_string = (stmt->var.init->binary.left->type == EXPR_STRING);
                        bool right_is_string = (stmt->var.init->binary.right->type == EXPR_STRING);
                        
                        if (left_is_string || right_is_string) {
                            // String concatenation
                            c_type = "const char*";
                            is_already_const = true;
                            needs_arc_management = true;
                        } else {
                            // Arithmetic - use __auto_type to infer from expression
                            c_type = "__auto_type";
                        }
                    } else {
                        // Other binary operations (-, *, /, ==, etc.) - use __auto_type
                        c_type = "__auto_type";
                    }
                }
                // ... rest of type determination logic
            }
            
            // Emit variable declaration - avoid double const
            // Special handling for function pointers (lambdas)
            if (stmt->var.init && stmt->var.init->type == EXPR_LAMBDA) {
                // Function pointer syntax: int (*name)(params...)
                // Check if name is a C keyword
                const char* c_keywords[] = {"double", "float", "int", "char", "void", "return", "if", "else", "while", "for", "switch", "case", NULL};
                bool is_c_keyword = false;
                for (int i = 0; c_keywords[i] != NULL; i++) {
                    if (stmt->var.name.length == strlen(c_keywords[i]) && 
                        memcmp(stmt->var.name.start, c_keywords[i], stmt->var.name.length) == 0) {
                        is_c_keyword = true;
                        break;
                    }
                }
                
                // Find this lambda in the lambda_functions array to get capture count
                int total_params = stmt->var.init->lambda.param_count;
                int lambda_idx = -1;
                for (int i = 0; i < lambda_count; i++) {
                    // Match by checking if this is the right lambda (use counter)
                    static int lambda_var_counter = 0;
                    if (i == lambda_var_counter) {
                        total_params += lambda_functions[i].capture_count;
                        lambda_idx = i;
                        lambda_var_counter++;
                        break;
                    }
                }
                
                // Store lambda variable info for call site injection
                if (lambda_idx >= 0 && lambda_var_count < 256) {
                    snprintf(lambda_var_info[lambda_var_count].var_name, 64, "%.*s", 
                            stmt->var.name.length, stmt->var.name.start);
                    lambda_var_info[lambda_var_count].capture_count = lambda_functions[lambda_idx].capture_count;
                    for (int i = 0; i < lambda_functions[lambda_idx].capture_count; i++) {
                        strcpy(lambda_var_info[lambda_var_count].captured_vars[i], 
                               lambda_functions[lambda_idx].captured_vars[i]);
                    }
                    lambda_var_count++;
                }
                
                emit("int (*%s%.*s)(", is_c_keyword ? "_" : "", stmt->var.name.length, stmt->var.name.start);
                for (int i = 0; i < total_params; i++) {
                    if (i > 0) emit(", ");
                    emit("int");
                }
                emit(") = ");
            } else if (stmt->var.is_const && !stmt->var.is_mutable && !is_already_const) {
                emit("const %s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            } else {
                emit("%s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            }
            
            // Register local variable for scope tracking (if inside a function)
            if (current_module_prefix) {
                char var_name[256];
                snprintf(var_name, 256, "%.*s", stmt->var.name.length, stmt->var.name.start);
                register_local_variable(var_name);
            }
            
            if (needs_arc_management) {
                emit("({ ");
                codegen_expr(stmt->var.init);
                emit("; /* ARC retain for %.*s */ })", stmt->var.name.length, stmt->var.name.start);
            } else {
                codegen_expr(stmt->var.init);
            }
            emit(";\n");
            
            // Track ARC-managed variables for automatic cleanup
            if (needs_arc_management && !stmt->var.is_const) {
                track_var_with_type(stmt->var.name.start, stmt->var.name.length, c_type);
            }
            break;
        }
        case STMT_RETURN:
            if (in_async_function) {
                emit("*temp = ");
                codegen_expr(stmt->ret.value);
                emit("; goto async_return;\n");
            } else {
                emit("return ");
                codegen_expr(stmt->ret.value);
                emit(";\n");
            }
            break;
        case STMT_BREAK:
            emit("break;\n");
            break;
        case STMT_CONTINUE:
            emit("continue;\n");
            break;
        case STMT_SPAWN: {
            // Spawn: lightweight tasks (not OS threads)
            // Wrapper functions are generated in pre-scan phase
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT &&
                stmt->spawn.call->call.arg_count == 0) {
                
                // Extract function name
                Expr* callee = stmt->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                // Generate spawn call
                emit("wyn_spawn(__spawn_wrapper_");
                emit(func_name);
                emit(", NULL);\n");
            } else {
                // Fallback: just call the function
                emit("/* spawn (fallback) */ ");
                codegen_expr(stmt->spawn.call);
                emit(";\n");
            }
            break;
        }
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                emit("    ");
                codegen_stmt(stmt->block.stmts[i]);
            }
            break;
        case STMT_UNSAFE:
            // Unsafe blocks are just regular blocks in C
            emit("/* unsafe */ {\n");
            for (int i = 0; i < stmt->block.count; i++) {
                emit("    ");
                codegen_stmt(stmt->block.stmts[i]);
            }
            emit("}\n");
            break;
        case STMT_FN: {
            // Determine return type
            const char* return_type = "int"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = stmt->fn.is_async;
            
            if (stmt->fn.return_type) {
                if (stmt->fn.return_type->type == EXPR_CALL) {
                    // Generic type instantiation: HashMap<K,V>, Option<T>, etc.
                    if (stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = stmt->fn.return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            return_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            return_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            return_type = "WynOptional*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            return_type = "WynResult*";
                        }
                    }
                } else if (stmt->fn.return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
                } else if (stmt->fn.return_type->type == EXPR_IDENT) {
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
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                        return_type = "WynHashMap*";
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                        return_type = "WynHashSet*";
                    } else {
                        // Assume it's a custom struct type
                        snprintf(return_type_buf, sizeof(return_type_buf), "%.*s", 
                               type_name.length, type_name.start);
                        return_type = return_type_buf;
                    }
                } else if (stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                    // T2.5.1: Handle optional return types - use WynOptional* for proper optional handling
                    return_type = "WynOptional*";
                }
            }
            
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (stmt->fn.name.length == 4 && 
                                   memcmp(stmt->fn.name.start, "main", 4) == 0);
            
            // For async functions, return WynFuture*
            if (is_async) {
                emit("WynFuture* %.*s(", stmt->fn.name.length, stmt->fn.name.start);
            } else if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (stmt->fn.is_extension) {
                // Extension method: fn Type.method() -> Type_method()
                emit("%s %.*s_%.*s(", return_type, 
                     stmt->fn.receiver_type.length, stmt->fn.receiver_type.start,
                     stmt->fn.name.length, stmt->fn.name.start);
            } else {
                emit("%s %.*s(", return_type, stmt->fn.name.length, stmt->fn.name.start);
            }
            for (int i = 0; i < stmt->fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "int"; // default
                char custom_type_buf[256] = {0};  // Buffer for custom types
                
                // FIX: For extension methods, first parameter (self) gets receiver type
                if (stmt->fn.is_extension && i == 0) {
                    snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s", 
                           stmt->fn.receiver_type.length, stmt->fn.receiver_type.start);
                    param_type = custom_type_buf;
                } else if (stmt->fn.param_types[i]) {
                    if (stmt->fn.param_types[i]->type == EXPR_FN_TYPE) {
                        // Function type: fn(T) -> R becomes function pointer
                        FnTypeExpr* fn_type = &stmt->fn.param_types[i]->fn_type;
                        
                        // Build return type
                        const char* ret_type = "int";
                        if (fn_type->return_type && fn_type->return_type->type == EXPR_IDENT) {
                            Token rt = fn_type->return_type->token;
                            if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) ret_type = "int";
                            else if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret_type = "char*";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret_type = "double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) ret_type = "bool";
                        }
                        
                        // Build parameter types
                        char params_buf[256] = "";
                        for (int j = 0; j < fn_type->param_count; j++) {
                            if (j > 0) strcat(params_buf, ", ");
                            const char* pt = "int";
                            if (fn_type->param_types[j] && fn_type->param_types[j]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[j]->token;
                                if (pt_tok.length == 3 && memcmp(pt_tok.start, "int", 3) == 0) pt = "int";
                                else if (pt_tok.length == 6 && memcmp(pt_tok.start, "string", 6) == 0) pt = "char*";
                                else if (pt_tok.length == 5 && memcmp(pt_tok.start, "float", 5) == 0) pt = "double";
                                else if (pt_tok.length == 4 && memcmp(pt_tok.start, "bool", 4) == 0) pt = "bool";
                            }
                            strcat(params_buf, pt);
                        }
                        
                        // Generate function pointer type: ret_type (*param_name)(params)
                        snprintf(custom_type_buf, sizeof(custom_type_buf), "%s (*", ret_type);
                        param_type = custom_type_buf;
                        emit("%s", param_type);
                        emit("%.*s)(", stmt->fn.params[i].length, stmt->fn.params[i].start);
                        emit("%s", params_buf);
                        emit(")");
                        continue; // Skip the normal emit below
                    } else if (stmt->fn.param_types[i]->type == EXPR_IDENT) {
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
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            param_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            param_type = "WynHashSet*";
                        } else {
                            // Assume it's a custom struct type
                            snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s", 
                                   type_name.length, type_name.start);
                            param_type = custom_type_buf;
                        }
                    } else if (stmt->fn.param_types[i]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (stmt->fn.param_types[i]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - use WynOptional* for proper optional handling
                        param_type = "WynOptional*";
                    }
                }
                
                emit("%s %.*s", param_type, stmt->fn.params[i].length, stmt->fn.params[i].start);
            }
            emit(") {\n");
            push_scope();  // Track allocations in this function
            
            // For async functions, wrap the body in a future
            if (is_async) {
                emit("    WynFuture* future = wyn_future_new();\n");
                emit("    %s* temp = malloc(sizeof(%s));\n", return_type, return_type);
                emit("    {\n");
                // Set async context for return statement handling
                bool prev_async = in_async_function;
                in_async_function = true;
                codegen_stmt(stmt->fn.body);
                in_async_function = prev_async;
                emit("    }\n");
                emit("async_return:\n");
                emit("    wyn_future_ready(future, temp);\n");
                emit("    return future;\n");
            } else {
                codegen_stmt(stmt->fn.body);
            }
            
            pop_scope();   // Auto-cleanup before function end
            emit("}\n\n");
            break;
        }
        case STMT_EXTERN: {
            // Generate extern function declaration
            const char* return_type = "int"; // default
            if (stmt->extern_fn.return_type && stmt->extern_fn.return_type->type == EXPR_IDENT) {
                Token ret_type = stmt->extern_fn.return_type->token;
                if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                    return_type = "int";
                } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                    return_type = "char*";
                } else if (ret_type.length == 4 && memcmp(ret_type.start, "void", 4) == 0) {
                    return_type = "void";
                }
            }
            
            emit("extern %s %.*s(", return_type, 
                 stmt->extern_fn.name.length, stmt->extern_fn.name.start);
            
            for (int i = 0; i < stmt->extern_fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                const char* param_type = "int"; // default
                if (stmt->extern_fn.param_types[i] && stmt->extern_fn.param_types[i]->type == EXPR_IDENT) {
                    Token type_name = stmt->extern_fn.param_types[i]->token;
                    if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        param_type = "const char*";
                    }
                }
                
                emit("%s", param_type);
            }
            
            if (stmt->extern_fn.is_variadic) {
                if (stmt->extern_fn.param_count > 0) emit(", ");
                emit("...");
            }
            
            emit(");\n");
            break;
        }
        case STMT_MACRO: {
            // Generate C macro
            emit("#define %.*s(", stmt->macro.name.length, stmt->macro.name.start);
            for (int i = 0; i < stmt->macro.param_count; i++) {
                if (i > 0) emit(", ");
                emit("%.*s", stmt->macro.params[i].length, stmt->macro.params[i].start);
            }
            emit(") %.*s\n", stmt->macro.body.length, stmt->macro.body.start);
            break;
        }
        case STMT_STRUCT:
            // Skip generic structs - they will be handled by monomorphization
            if (stmt->struct_decl.type_param_count > 0) {
                break;
            }
            
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
                        } else {
                            // Custom struct type - use the type name as-is
                            static char custom_type[64];
                            snprintf(custom_type, 64, "%.*s", type_name.length, type_name.start);
                            c_type = custom_type;
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
            // Emit struct name with module prefix if in module context
            if (current_module_prefix) {
                emit("} %s_%.*s;\n\n", 
                     current_module_prefix,
                     stmt->struct_decl.name.length,
                     stmt->struct_decl.name.start);
                
                // T2.5.3: Generate ARC cleanup function for struct
                emit("void %s_%.*s_cleanup(%s_%.*s* obj) {\n",
                     current_module_prefix,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     current_module_prefix,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
            } else {
                emit("} %.*s;\n\n", 
                     stmt->struct_decl.name.length,
                     stmt->struct_decl.name.start);
                
                // T2.5.3: Generate ARC cleanup function for struct
                emit("void %.*s_cleanup(%.*s* obj) {\n",
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
            }
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                if (stmt->struct_decl.field_arc_managed[i]) {
                    emit("    if (obj->%.*s) wyn_arc_release(obj->%.*s);\n",
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start,
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start);
                }
            }
            emit("}\n\n");
            
            // Generate methods defined in struct
            emit("/* Generating %d methods */\n", stmt->struct_decl.method_count);
            for (int i = 0; i < stmt->struct_decl.method_count; i++) {
                FnStmt* method = stmt->struct_decl.methods[i];
                // Generate as TypeName_methodname(TypeName self, ...)
                
                // Emit return type
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token type_name = method->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        emit("int");
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        emit("double");
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        emit("char*");
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        emit("bool");
                    } else {
                        emit("%.*s", type_name.length, type_name.start);
                    }
                } else {
                    emit("void");
                }
                
                emit(" %.*s_%.*s(%.*s self",
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     method->name.length, method->name.start,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                
                // Skip first param if it's 'self'
                int start_param = 0;
                if (method->param_count > 0 && 
                    method->params[0].length == 4 && 
                    memcmp(method->params[0].start, "self", 4) == 0) {
                    start_param = 1;
                }
                
                for (int j = start_param; j < method->param_count; j++) {
                    emit(", ");
                    // Emit parameter type
                    if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                        Token ptype = method->param_types[j]->token;
                        if (ptype.length == 3 && memcmp(ptype.start, "int", 3) == 0) {
                            emit("int");
                        } else if (ptype.length == 5 && memcmp(ptype.start, "float", 5) == 0) {
                            emit("double");
                        } else if (ptype.length == 6 && memcmp(ptype.start, "string", 6) == 0) {
                            emit("char*");
                        } else {
                            emit("%.*s", ptype.length, ptype.start);
                        }
                    }
                    emit(" %.*s", method->params[j].length, method->params[j].start);
                }
                emit(") ");
                
                // Emit method body
                if (method->body) {
                    emit("{\n");
                    emit("    /* body has %d statements */\n", method->body->block.count);
                    codegen_stmt(method->body);
                    emit("}\n");
                } else {
                    emit("{ /* no body */ }\n");
                }
                emit("\n");
            }
            
            break;
        case STMT_ENUM:
            // Check if any variant has data
            bool has_data = false;
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                if (stmt->enum_decl.variant_type_counts[i] > 0) {
                    has_data = true;
                    break;
                }
            }
            
            if (has_data) {
                // Generate tagged union for enum with data
                emit("typedef struct {\n");
                emit("    enum { ");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("%.*s_%.*s_TAG",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    if (i < stmt->enum_decl.variant_count - 1) emit(", ");
                }
                emit(" } tag;\n");
                emit("    union {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variant_type_counts[i] > 0) {
                        emit("        ");
                        // For now, only handle single-field variants
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" %.*s_value;\n",
                                 stmt->enum_decl.variants[i].length,
                                 stmt->enum_decl.variants[i].start);
                        }
                    }
                }
                emit("    } data;\n");
                emit("} %.*s;\n\n",
                     stmt->enum_decl.name.length,
                     stmt->enum_decl.name.start);
            } else {
                // Simple enum without data
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
            }
            
            // Generate qualified constants for EnumName.MEMBER access (only for simple enums)
            if (!has_data) {
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("#define %.*s_%.*s %d\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         i);
                }
            }
            emit("\n");
            
            // Generate constructor functions for enums with data
            if (has_data) {
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    // Constructor function for all variants (with or without data)
                    emit("%.*s %.*s_%.*s(",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    if (stmt->enum_decl.variant_type_counts[i] > 0) {
                        // Only handle single-field variants for now
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" value");
                        }
                    }
                    // else: zero-argument constructor
                    
                    emit(") {\n");
                    emit("    %.*s result;\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                    emit("    result.tag = %.*s_%.*s_TAG;\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    if (stmt->enum_decl.variant_type_counts[i] == 1) {
                        emit("    result.data.%.*s_value = value;\n",
                             stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    }
                    
                    emit("    return result;\n");
                    emit("}\n\n");
                }
            }
            
            // Generate toString function
            emit("const char* %.*s_toString(%.*s val) {\n",
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start);
            if (has_data) {
                emit("    switch(val.tag) {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("        case %.*s_%.*s_TAG: return \"%.*s\";\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                }
                emit("    }\n");
            } else {
                emit("    switch(val) {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("        case %.*s: return \"%.*s\";\n",
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                }
                emit("    }\n");
            }
            emit("    return \"Unknown\";\n");
            emit("}\n\n");
            
            // Generate unwrap function for Option enum
            if (stmt->enum_decl.name.length == 6 && memcmp(stmt->enum_decl.name.start, "Option", 6) == 0) {
                // Find the Some variant and its type
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variants[i].length == 4 && 
                        memcmp(stmt->enum_decl.variants[i].start, "Some", 4) == 0 &&
                        stmt->enum_decl.variant_type_counts[i] == 1) {
                        // Generate unwrap function
                        emit("int %.*s_unwrap(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Some_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Some_value;\n");
                        emit("    }\n");
                        emit("    fprintf(stderr, \"Error: unwrap() called on None\\n\");\n");
                        emit("    exit(1);\n");
                        emit("}\n\n");
                        
                        // Generate is_some function
                        emit("bool %.*s_is_some(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Some_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate is_none function
                        emit("bool %.*s_is_none(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_None_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate unwrap_or function
                        emit("int %.*s_unwrap_or(%.*s val, int default_val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Some_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Some_value;\n");
                        emit("    }\n");
                        emit("    return default_val;\n");
                        emit("}\n\n");
                        break;
                    }
                }
            }
            
            // Generate unwrap function for Result enum
            if (stmt->enum_decl.name.length == 6 && memcmp(stmt->enum_decl.name.start, "Result", 6) == 0) {
                // Find the Ok variant and its type
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variants[i].length == 2 && 
                        memcmp(stmt->enum_decl.variants[i].start, "Ok", 2) == 0 &&
                        stmt->enum_decl.variant_type_counts[i] == 1) {
                        // Generate unwrap function
                        emit("int %.*s_unwrap(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Ok_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Ok_value;\n");
                        emit("    }\n");
                        emit("    fprintf(stderr, \"Error: unwrap() called on Err\\n\");\n");
                        emit("    exit(1);\n");
                        emit("}\n\n");
                        
                        // Generate is_ok function
                        emit("bool %.*s_is_ok(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Ok_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate is_err function
                        emit("bool %.*s_is_err(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Err_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate unwrap_or function
                        emit("int %.*s_unwrap_or(%.*s val, int default_val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Ok_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Ok_value;\n");
                        emit("    }\n");
                        emit("    return default_val;\n");
                        emit("}\n\n");
                        break;
                    }
                }
            }
            
            break;
        case STMT_TYPE_ALIAS:
            // typedef target_type alias_name;
            emit("typedef %.*s %.*s;\n\n",
                 stmt->type_alias.target.length,
                 stmt->type_alias.target.start,
                 stmt->type_alias.name.length,
                 stmt->type_alias.name.start);
            break;
        case STMT_IMPL:
            for (int i = 0; i < stmt->impl.method_count; i++) {
                FnStmt* method = stmt->impl.methods[i];
                
                // Determine return type
                const char* return_type = "int";
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token ret_type = method->return_type->token;
                    if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                        return_type = "int";
                    } else if (ret_type.length == 5 && memcmp(ret_type.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (ret_type.length == 4 && memcmp(ret_type.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                        return_type = "const char*";
                    }
                }
                
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int j = 0; j < method->param_count; j++) {
                    if (j > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "int";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "int";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else {
                            // Custom struct type
                            snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s",
                                   type_name.length, type_name.start);
                            param_type = custom_type_buf;
                        }
                    }
                    
                    emit("%s %.*s", param_type, method->params[j].length, method->params[j].start);
                }
                emit(") {\n");
                push_scope();
                codegen_stmt(method->body);
                pop_scope();
                emit("}\n\n");
            }
            break;
        case STMT_TRAIT:
            // Generate trait definition as C interface
            emit("// trait %.*s\n", stmt->trait_decl.name.length, stmt->trait_decl.name.start);
            for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                FnStmt* method = stmt->trait_decl.methods[i];
                if (!stmt->trait_decl.method_has_default[i]) {
                    // Generate function pointer typedef for trait method
                    emit("typedef ");
                    if (method->return_type) {
                        if (method->return_type->type == EXPR_IDENT) {
                            Token ret_type = method->return_type->token;
                            if (ret_type.length == 3 && memcmp(ret_type.start, "str", 3) == 0) {
                                emit("char*");
                            } else if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                                emit("int");
                            } else {
                                emit("void*");
                            }
                        } else {
                            emit("void*");
                        }
                    } else {
                        emit("void");
                    }
                    emit(" (*%.*s_%.*s_fn)(void*", 
                         stmt->trait_decl.name.length, stmt->trait_decl.name.start,
                         method->name.length, method->name.start);
                    for (int j = 0; j < method->param_count; j++) {
                        emit(", ");
                        if (method->param_types && method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                            Token param_type = method->param_types[j]->token;
                            if (param_type.length == 3 && memcmp(param_type.start, "str", 3) == 0) {
                                emit("char*");
                            } else if (param_type.length == 3 && memcmp(param_type.start, "int", 3) == 0) {
                                emit("int");
                            } else {
                                emit("void*");
                            }
                        } else {
                            emit("void*");
                        }
                    }
                    emit(");\n");
                }
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
        case STMT_IMPORT: {
            // Extract module name
            char module_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s", 
                    stmt->import.module.length, stmt->import.module.start);
            
            // Handle selective imports: import { get, post } from module
            if (stmt->import.item_count > 0) {
                // Register each imported item with module prefix
                for (int i = 0; i < stmt->import.item_count; i++) {
                    char item_name[256];
                    snprintf(item_name, sizeof(item_name), "%.*s",
                            stmt->import.items[i].length, stmt->import.items[i].start);
                    
                    // Map item_name -> module_name::item_name
                    char full_name[512];
                    const char* c_mod = module_to_c_ident(module_name);
                    snprintf(full_name, sizeof(full_name), "%s_%s", c_mod, item_name);
                    register_module_alias(item_name, full_name);
                }
            }
            
            // Register alias if present
            if (stmt->import.alias.start != NULL) {
                char alias_name[256];
                snprintf(alias_name, sizeof(alias_name), "%.*s",
                        stmt->import.alias.length, stmt->import.alias.start);
                register_module_alias(alias_name, module_name);
            }
            
            extern bool is_builtin_module(const char* name);
            extern bool is_module_loaded(const char* name);
            extern char* resolve_relative_module_name(const char* name);
            
            // Resolve relative paths (crate/config -> config)
            char* resolved_module_name = resolve_relative_module_name(module_name);
            const char* lookup_name = resolved_module_name ? resolved_module_name : module_name;
            
            // Priority: User modules > Built-ins
            // This allows community to override/extend built-in modules
            if (is_module_loaded(lookup_name)) {
                // User module exists - emit all loaded modules once per compilation
                if (!modules_emitted_this_compilation) {
                    modules_emitted_this_compilation = true;
                    
                    extern int get_all_modules_raw(void** out_modules, int max_count);
                    void* all_modules_raw[64];
                    int module_count = get_all_modules_raw(all_modules_raw, 64);
                    
                    for (int m = 0; m < module_count; m++) {
                        typedef struct { char* name; Program* ast; } ModuleEntry;
                        ModuleEntry* mod = (ModuleEntry*)all_modules_raw[m];
                        
                        // Register short name mapping for nested modules
                        register_module_short_name(mod->name);
                        
                        // Register all functions in this module for internal call resolution
                        clear_module_functions();
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            if (s->type == STMT_FN) {
                                char func_name[256];
                                snprintf(func_name, 256, "%.*s", s->fn.name.length, s->fn.name.start);
                                register_module_function(func_name);
                            }
                        }
                        
                        // First: emit structs and enums with module prefix
                        const char* saved_prefix = current_module_prefix;
                        current_module_prefix = mod->name;
                        const char* c_mod_name = module_to_c_ident(mod->name);
                        
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            if ((s->type == STMT_STRUCT && s->struct_decl.is_public) ||
                                (s->type == STMT_ENUM && s->enum_decl.is_public)) {
                                codegen_stmt(s);
                            }
                        }
                        
                        current_module_prefix = saved_prefix;
                        
                        // Second: emit forward declarations for all functions
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            
                            if (s->type == STMT_FN) {
                                // Private functions are static
                                if (!s->fn.is_public) {
                                    emit("static ");
                                }
                                
                                // Determine return type
                                const char* return_type = "int";
                                static char custom_ret_type[128];
                                if (s->fn.return_type) {
                                    if (s->fn.return_type->type == EXPR_IDENT) {
                                        Token rt = s->fn.return_type->token;
                                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) {
                                            return_type = "const char*";
                                        } else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) {
                                            return_type = "double";
                                        } else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) {
                                            return_type = "bool";
                                        } else if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) {
                                            return_type = "int";
                                        } else if (rt.length == 7 && memcmp(rt.start, "HashMap", 7) == 0) {
                                            return_type = "WynHashMap*";
                                        } else if (rt.length == 7 && memcmp(rt.start, "HashSet", 7) == 0) {
                                            return_type = "WynHashSet*";
                                        } else {
                                            // Custom struct type - add module prefix
                                            snprintf(custom_ret_type, 128, "%s_%.*s", c_mod_name, rt.length, rt.start);
                                            return_type = custom_ret_type;
                                        }
                                    }
                                }
                                
                                emit("%s %s_%.*s(", return_type, c_mod_name, s->fn.name.length, s->fn.name.start);
                                
                                // Emit parameters with types
                                for (int j = 0; j < s->fn.param_count; j++) {
                                    if (j > 0) emit(", ");
                                    
                                    // Determine parameter type
                                    const char* param_type = "int";
                                    static char custom_param_type[128];
                                    if (s->fn.param_types[j]) {
                                        if (s->fn.param_types[j]->type == EXPR_IDENT) {
                                            Token pt = s->fn.param_types[j]->token;
                                            if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) {
                                                param_type = "const char*";
                                            } else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) {
                                                param_type = "double";
                                            } else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) {
                                                param_type = "bool";
                                            } else if (pt.length == 3 && memcmp(pt.start, "int", 3) == 0) {
                                                param_type = "int";
                                            } else if (pt.length == 5 && memcmp(pt.start, "array", 5) == 0) {
                                                param_type = "WynArray";
                                            } else {
                                                // Custom struct type - add module prefix
                                                snprintf(custom_param_type, 128, "%s_%.*s", c_mod_name, pt.length, pt.start);
                                                param_type = custom_param_type;
                                            }
                                        }
                                    }
                                    
                                    emit("%s %.*s", param_type, s->fn.params[j].length, s->fn.params[j].start);
                                }
                                
                                emit(");\n");
                            }
                        }
                        
                        // Second: emit constants
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            if (s->type == STMT_CONST) {
                                VarStmt* const_stmt = &s->const_stmt;
                                
                                // Determine type
                                if (const_stmt->init) {
                                    if (const_stmt->init->type == EXPR_STRING) {
                                        emit("const char* %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else if (const_stmt->init->type == EXPR_FLOAT) {
                                        emit("double %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else if (const_stmt->init->type == EXPR_BOOL) {
                                        emit("bool %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else {
                                        emit("int %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    }
                                    codegen_expr(const_stmt->init);
                                    emit(";\n");
                                }
                            } else if (s->type == STMT_VAR) {
                                // Module-level variables (mutable state)
                                VarStmt* var_stmt = &s->var;
                                
                                // Determine type
                                if (var_stmt->init) {
                                    if (var_stmt->init->type == EXPR_STRING) {
                                        emit("const char* %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else if (var_stmt->init->type == EXPR_FLOAT) {
                                        emit("double %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else if (var_stmt->init->type == EXPR_BOOL) {
                                        emit("bool %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else {
                                        emit("int %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    }
                                    codegen_expr(var_stmt->init);
                                    emit(";\n");
                                } else {
                                    // No initializer - default to 0
                                    emit("int %s_%.*s = 0;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                }
                            }
                        }
                        
                        // Third: emit functions
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            
                            if (s->type == STMT_FN) {
                                emit_function_with_prefix(s, mod->name);
                            }
                        }
                    }
                }
            } else if (is_builtin_module(lookup_name)) {
                // Built-in module (only if no user override)
                static bool builtin_modules_emitted = false;
                if (!builtin_modules_emitted) {
                    builtin_modules_emitted = true;
                    
                    if (strcmp(lookup_name, "math") == 0) {
                        // Math module - useful functions only (use +, -, *, / operators for basic arithmetic)
                        emit("#include <math.h>\n");
                        emit("double math_pow(double base, double exp) { return pow(base, exp); }\n");
                        emit("double math_sqrt(double x) { return sqrt(x); }\n");
                        emit("double math_abs(double x) { return fabs(x); }\n");
                        emit("double math_floor(double x) { return floor(x); }\n");
                        emit("double math_ceil(double x) { return ceil(x); }\n");
                        emit("double math_round(double x) { return round(x); }\n");
                        emit("double math_sin(double x) { return sin(x); }\n");
                        emit("double math_cos(double x) { return cos(x); }\n");
                        emit("double math_tan(double x) { return tan(x); }\n");
                        emit("double math_log(double x) { return log(x); }\n");
                        emit("double math_exp(double x) { return exp(x); }\n");
                        emit("double math_min(double a, double b) { return a < b ? a : b; }\n");
                        emit("double math_max(double a, double b) { return a > b ? a : b; }\n");
                        emit("const double math_pi = 3.14159265358979323846;\n");
                        emit("const double math_e = 2.71828182845904523536;\n");
                    }
                }
            }
            
            if (resolved_module_name) {
                free(resolved_module_name);
            }
            break;
        }
        case STMT_EXPORT:
            // Generate the exported statement normally (comment already generated)
            codegen_stmt(stmt->export.stmt);
            break;
        case STMT_TRY: {
            // TASK-026: Enhanced try/catch implementation with multiple catch blocks
            emit("{\n");
            emit("    jmp_buf exception_buf;\n");
            emit("    const char* exception_msg = NULL;\n");
            emit("    int exception_type = 0;\n");
            emit("    if (setjmp(exception_buf) == 0) {\n");
            emit("        // Try block\n");
            emit("        current_exception_buf = &exception_buf;\n");
            emit("        current_exception_msg = &exception_msg;\n");
            codegen_stmt(stmt->try_stmt.try_block);
            emit("    } else {\n");
            emit("        // Catch blocks\n");
            
            // Generate multiple catch blocks
            for (int i = 0; i < stmt->try_stmt.catch_count; i++) {
                if (i > 0) emit("        } else ");
                emit("        if (exception_type == %d) {\n", i);
                emit("            const char* %.*s = exception_msg;\n", 
                     stmt->try_stmt.exception_vars[i].length, 
                     stmt->try_stmt.exception_vars[i].start);
                codegen_stmt(stmt->try_stmt.catch_blocks[i]);
            }
            
            if (stmt->try_stmt.catch_count > 0) {
                emit("        }\n");
            }
            
            emit("    }\n");
            
            // Finally block
            if (stmt->try_stmt.finally_block) {
                emit("    // Finally block\n");
                codegen_stmt(stmt->try_stmt.finally_block);
            }
            
            emit("}\n");
            break;
        }
        case STMT_CATCH: {
            // TASK-026: Standalone catch statement
            emit("// Catch block for %.*s %.*s\n",
                 stmt->catch_stmt.exception_type.length, stmt->catch_stmt.exception_type.start,
                 stmt->catch_stmt.exception_var.length, stmt->catch_stmt.exception_var.start);
            codegen_stmt(stmt->catch_stmt.body);
            break;
        }
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
        case STMT_CONST: {
            // Module-level constants - emit as static const
            const char* c_type = "int";
            bool type_has_const = false;
            
            // Determine type from initializer
            if (stmt->const_stmt.init) {
                if (stmt->const_stmt.init->type == EXPR_STRING) {
                    c_type = "char*";  // Don't include const here, we'll add it below
                } else if (stmt->const_stmt.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (stmt->const_stmt.init->type == EXPR_BOOL) {
                    c_type = "bool";
                } else if (stmt->const_stmt.init->type == EXPR_INT) {
                    c_type = "int";
                }
            }
            
            // Emit as static const with module prefix
            if (current_module_prefix) {
                emit("static const %s %s_%.*s = ", c_type, current_module_prefix, 
                     stmt->const_stmt.name.length, stmt->const_stmt.name.start);
            } else {
                emit("static const %s %.*s = ", c_type, 
                     stmt->const_stmt.name.length, stmt->const_stmt.name.start);
            }
            
            codegen_expr(stmt->const_stmt.init);
            emit(";\n");
            break;
        }
        default:
            break;
    }
}

// Forward scan for lambdas in expressions
static void scan_expr_for_lambdas(Expr* expr);

static void scan_stmt_for_lambdas(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_VAR:
            if (stmt->var.init) scan_expr_for_lambdas(stmt->var.init);
            break;
        case STMT_CONST:
            if (stmt->const_stmt.init) scan_expr_for_lambdas(stmt->const_stmt.init);
            break;
        case STMT_RETURN:
            if (stmt->ret.value) scan_expr_for_lambdas(stmt->ret.value);
            break;
        case STMT_EXPR:
            scan_expr_for_lambdas(stmt->expr);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                scan_stmt_for_lambdas(stmt->block.stmts[i]);
            }
            break;
        case STMT_IF:
            scan_expr_for_lambdas(stmt->if_stmt.condition);
            scan_stmt_for_lambdas(stmt->if_stmt.then_branch);
            if (stmt->if_stmt.else_branch) scan_stmt_for_lambdas(stmt->if_stmt.else_branch);
            break;
        case STMT_WHILE:
            scan_expr_for_lambdas(stmt->while_stmt.condition);
            scan_stmt_for_lambdas(stmt->while_stmt.body);
            break;
        case STMT_SPAWN:
            // Collect spawn wrappers
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT &&
                stmt->spawn.call->call.arg_count == 0) {
                
                Expr* callee = stmt->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                // Add to spawn wrappers (avoid duplicates)
                bool already_added = false;
                for (int i = 0; i < spawn_wrapper_count; i++) {
                    if (strcmp(spawn_wrappers[i].func_name, func_name) == 0) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added && spawn_wrapper_count < 256) {
                    strcpy(spawn_wrappers[spawn_wrapper_count].func_name, func_name);
                    spawn_wrapper_count++;
                }
            }
            break;
        default:
            break;
    }
}

static void scan_expr_for_lambdas(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_LAMBDA:
            // Found a lambda! Generate it now
            lambda_id_counter++;
            int lambda_id = lambda_id_counter;
            
            // Detect captured variables (simple: check if body uses identifiers not in params)
            char captured_vars[16][64];
            int capture_count = 0;
            
            if (expr->lambda.body->type == EXPR_BINARY) {
                Expr* bin = expr->lambda.body;
                // Check left operand
                if (bin->binary.left->type == EXPR_IDENT) {
                    int is_param = 0;
                    for (int i = 0; i < expr->lambda.param_count; i++) {
                        if (expr->lambda.params[i].length == bin->binary.left->token.length &&
                            memcmp(expr->lambda.params[i].start, bin->binary.left->token.start, 
                                   expr->lambda.params[i].length) == 0) {
                            is_param = 1;
                            break;
                        }
                    }
                    if (!is_param && capture_count < 16) {
                        snprintf(captured_vars[capture_count], 64, "%.*s", 
                                bin->binary.left->token.length, bin->binary.left->token.start);
                        capture_count++;
                    }
                }
                // Check right operand
                if (bin->binary.right->type == EXPR_IDENT) {
                    int is_param = 0;
                    for (int i = 0; i < expr->lambda.param_count; i++) {
                        if (expr->lambda.params[i].length == bin->binary.right->token.length &&
                            memcmp(expr->lambda.params[i].start, bin->binary.right->token.start, 
                                   expr->lambda.params[i].length) == 0) {
                            is_param = 1;
                            break;
                        }
                    }
                    if (!is_param && capture_count < 16) {
                        // Check if already captured
                        int already = 0;
                        for (int i = 0; i < capture_count; i++) {
                            if (strcmp(captured_vars[i], "") != 0) {
                                char temp[64];
                                snprintf(temp, 64, "%.*s", bin->binary.right->token.length, 
                                        bin->binary.right->token.start);
                                if (strcmp(captured_vars[i], temp) == 0) {
                                    already = 1;
                                    break;
                                }
                            }
                        }
                        if (!already) {
                            snprintf(captured_vars[capture_count], 64, "%.*s", 
                                    bin->binary.right->token.length, bin->binary.right->token.start);
                            capture_count++;
                        }
                    }
                }
            }
            
            char* func_code = malloc(8192);
            int pos = 0;
            
            pos += snprintf(func_code + pos, 8192 - pos, "int __lambda_%d(", lambda_id);
            // Add captured variables as first parameters
            for (int i = 0; i < capture_count; i++) {
                if (i > 0) pos += snprintf(func_code + pos, 8192 - pos, ", ");
                pos += snprintf(func_code + pos, 8192 - pos, "int %s", captured_vars[i]);
            }
            // Add regular parameters
            for (int i = 0; i < expr->lambda.param_count; i++) {
                if (i > 0 || capture_count > 0) pos += snprintf(func_code + pos, 8192 - pos, ", ");
                pos += snprintf(func_code + pos, 8192 - pos, "int %.*s", 
                               expr->lambda.params[i].length, expr->lambda.params[i].start);
            }
            pos += snprintf(func_code + pos, 8192 - pos, ") {\n    return ");
            
            // For the body, we'll generate a simple expression
            // This is a simplified version - just handle basic cases
            if (expr->lambda.body->type == EXPR_BINARY) {
                Expr* bin = expr->lambda.body;
                // Left operand
                if (bin->binary.left->type == EXPR_IDENT) {
                    pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                                   bin->binary.left->token.length, bin->binary.left->token.start);
                } else if (bin->binary.left->type == EXPR_INT) {
                    pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                                   bin->binary.left->token.length, bin->binary.left->token.start);
                }
                // Operator
                if (bin->binary.op.type == TOKEN_PLUS) {
                    pos += snprintf(func_code + pos, 8192 - pos, " + ");
                } else if (bin->binary.op.type == TOKEN_STAR) {
                    pos += snprintf(func_code + pos, 8192 - pos, " * ");
                } else if (bin->binary.op.type == TOKEN_MINUS) {
                    pos += snprintf(func_code + pos, 8192 - pos, " - ");
                } else if (bin->binary.op.type == TOKEN_SLASH) {
                    pos += snprintf(func_code + pos, 8192 - pos, " / ");
                }
                // Right operand
                if (bin->binary.right->type == EXPR_IDENT) {
                    pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                                   bin->binary.right->token.length, bin->binary.right->token.start);
                } else if (bin->binary.right->type == EXPR_INT) {
                    pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                                   bin->binary.right->token.length, bin->binary.right->token.start);
                }
            } else if (expr->lambda.body->type == EXPR_IDENT) {
                // Simple identifier
                pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                               expr->lambda.body->token.length, expr->lambda.body->token.start);
            } else if (expr->lambda.body->type == EXPR_INT) {
                // Simple integer
                pos += snprintf(func_code + pos, 8192 - pos, "%.*s", 
                               expr->lambda.body->token.length, expr->lambda.body->token.start);
            }
            pos += snprintf(func_code + pos, 8192 - pos, ";\n}\n");
            
            if (lambda_count < 256) {
                lambda_functions[lambda_count].code = func_code;
                lambda_functions[lambda_count].id = lambda_id;
                lambda_functions[lambda_count].param_count = expr->lambda.param_count;
                lambda_functions[lambda_count].capture_count = capture_count;
                for (int i = 0; i < capture_count; i++) {
                    strcpy(lambda_functions[lambda_count].captured_vars[i], captured_vars[i]);
                }
                lambda_count++;
            }
            break;
        case EXPR_BINARY:
            scan_expr_for_lambdas(expr->binary.left);
            scan_expr_for_lambdas(expr->binary.right);
            break;
        case EXPR_CALL:
            scan_expr_for_lambdas(expr->call.callee);
            for (int i = 0; i < expr->call.arg_count; i++) {
                scan_expr_for_lambdas(expr->call.args[i]);
            }
            break;
        default:
            break;
    }
}

static void scan_for_lambdas(Stmt* body) {
    scan_stmt_for_lambdas(body);
}

void codegen_program(Program* prog) {
    bool has_main = false;
    bool has_math_import = false;
    
    // Reset module emission flag for this compilation
    modules_emitted_this_compilation = false;
    
    // Reset lambda collection
    lambda_count = 0;
    lambda_id_counter = 0;
    
    // Reset spawn wrapper collection
    spawn_wrapper_count = 0;
    
    // PASS 1: Pre-scan to collect all lambdas
    // We need to emit lambda functions before they're used
    // So we do a quick scan to find and generate them first
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            // Scan function body for lambdas
            scan_for_lambdas(prog->stmts[i]->fn.body);
        }
    }
    
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
    
    // Math functions are now handled by the module system
    
    // Collect generic instantiations (but don't generate yet)
    wyn_collect_generic_instantiations_from_program(prog);
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            if (prog->stmts[i]->fn.name.length == 4 &&
                memcmp(prog->stmts[i]->fn.name.start, "main", 4) == 0) {
                has_main = true;
            }
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            if (prog->stmts[i]->export.stmt->fn.name.length == 4 &&
                memcmp(prog->stmts[i]->export.stmt->fn.name.start, "main", 4) == 0) {
                has_main = true;
            }
        }
    }
    
    // Generate all structs, enums, and type aliases first
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT || prog->stmts[i]->type == STMT_ENUM || prog->stmts[i]->type == STMT_TYPE_ALIAS) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate module-level constants
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_CONST) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate forward declarations for struct methods
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT) {
            Stmt* stmt = prog->stmts[i];
            for (int j = 0; j < stmt->struct_decl.method_count; j++) {
                FnStmt* method = stmt->struct_decl.methods[j];
                
                // Emit return type
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token type_name = method->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        emit("int");
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        emit("double");
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        emit("char*");
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        emit("bool");
                    } else {
                        emit("%.*s", type_name.length, type_name.start);
                    }
                } else {
                    emit("void");
                }
                
                emit(" %.*s_%.*s(%.*s self",
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     method->name.length, method->name.start,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                
                int start_param = 0;
                if (method->param_count > 0 && 
                    method->params[0].length == 4 && 
                    memcmp(method->params[0].start, "self", 4) == 0) {
                    start_param = 1;
                }
                
                for (int k = start_param; k < method->param_count; k++) {
                    emit(", ");
                    if (method->param_types[k] && method->param_types[k]->type == EXPR_IDENT) {
                        Token ptype = method->param_types[k]->token;
                        if (ptype.length == 3 && memcmp(ptype.start, "int", 3) == 0) {
                            emit("int");
                        } else if (ptype.length == 5 && memcmp(ptype.start, "float", 5) == 0) {
                            emit("double");
                        } else if (ptype.length == 6 && memcmp(ptype.start, "string", 6) == 0) {
                            emit("char*");
                        } else {
                            emit("%.*s", ptype.length, ptype.start);
                        }
                    }
                    emit(" %.*s", method->params[k].length, method->params[k].start);
                }
                emit(");\n");
            }
        }
    }
    emit("\n");
    
    // Generate extern declarations
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_EXTERN) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate macros
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_MACRO) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Emit lambda functions that were collected in pre-scan
    if (lambda_count > 0) {
        emit("// Lambda functions\n");
        for (int i = 0; i < lambda_count; i++) {
            emit("%s\n", lambda_functions[i].code);
        }
        emit("\n");
    }
    
    // Pre-declare lambda functions with generic signatures
    // Actual definitions will be emitted right after this
    emit("// Lambda functions (defined before use)\n");
    // Don't emit forward declarations - just emit definitions here
    // But we can't because lambdas aren't collected yet!
    
    // Generate monomorphic instances of generic functions (after structs are defined)
    wyn_generate_monomorphic_instances_for_codegen(prog);
    
    // Generate impl blocks (extension methods)
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPL) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Process import and export statements
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            codegen_stmt(prog->stmts[i]);
        } else if (prog->stmts[i]->type == STMT_EXPORT) {
            // Generate export comment only (the function will be generated later)
            emit("// export ");
            if (prog->stmts[i]->export.stmt && prog->stmts[i]->export.stmt->type == STMT_FN) {
                emit("%.*s\n", prog->stmts[i]->export.stmt->fn.name.length, prog->stmts[i]->export.stmt->fn.name.start);
            } else {
                emit("statement\n");
            }
        }
    }
    
    // Generate forward declarations for all functions
    for (int i = 0; i < prog->count; i++) {
        FnStmt* fn = NULL;
        
        if (prog->stmts[i]->type == STMT_FN) {
            fn = &prog->stmts[i]->fn;
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            fn = &prog->stmts[i]->export.stmt->fn;
        }
        
        if (fn) {
            // Skip generic functions - they will be handled by monomorphization
            if (fn->type_param_count > 0) {
                continue;
            }
            
            // Determine return type
            const char* return_type = "int"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = fn->is_async;
            
            if (fn->return_type) {
                if (fn->return_type->type == EXPR_CALL) {
                    // Generic type instantiation: HashMap<K,V>, Option<T>, etc.
                    if (fn->return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = fn->return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            return_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            return_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            return_type = "WynOptional*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            return_type = "WynResult*";
                        }
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
                } else if (fn->return_type->type == EXPR_IDENT) {
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
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                        return_type = "WynHashMap*";
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                        return_type = "WynHashSet*";
                    } else {
                        // Assume it's a custom struct type
                        snprintf(return_type_buf, sizeof(return_type_buf), "%.*s", 
                               type_name.length, type_name.start);
                        return_type = return_type_buf;
                    }
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    // T2.5.1: Handle optional return types - use WynOptional* for proper optional handling
                    return_type = "WynOptional*";
                }
            }
            
            // Generate forward declaration
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (fn->name.length == 4 && 
                                   memcmp(fn->name.start, "main", 4) == 0);
            
            // For async functions, return WynFuture*
            if (is_async) {
                emit("WynFuture* %.*s(", fn->name.length, fn->name.start);
            } else if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (fn->is_extension) {
                // Extension method: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     fn->receiver_type.length, fn->receiver_type.start,
                     fn->name.length, fn->name.start);
            } else {
                emit("%s %.*s(", return_type, fn->name.length, fn->name.start);
            }
            for (int j = 0; j < fn->param_count; j++) {
                if (j > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "int"; // default
                char struct_type_name[256] = {0};
                bool is_struct_type = false;
                
                if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_FN_TYPE) {
                        // Function type: fn(T) -> R becomes function pointer
                        FnTypeExpr* fn_type = &fn->param_types[j]->fn_type;
                        
                        // Build return type
                        const char* ret_type = "int";
                        if (fn_type->return_type && fn_type->return_type->type == EXPR_IDENT) {
                            Token rt = fn_type->return_type->token;
                            if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) ret_type = "int";
                            else if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret_type = "char*";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret_type = "double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) ret_type = "bool";
                        }
                        
                        // Build parameter types
                        char params_buf[256] = "";
                        for (int k = 0; k < fn_type->param_count; k++) {
                            if (k > 0) strcat(params_buf, ", ");
                            const char* pt = "int";
                            if (fn_type->param_types[k] && fn_type->param_types[k]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[k]->token;
                                if (pt_tok.length == 3 && memcmp(pt_tok.start, "int", 3) == 0) pt = "int";
                                else if (pt_tok.length == 6 && memcmp(pt_tok.start, "string", 6) == 0) pt = "char*";
                                else if (pt_tok.length == 5 && memcmp(pt_tok.start, "float", 5) == 0) pt = "double";
                                else if (pt_tok.length == 4 && memcmp(pt_tok.start, "bool", 4) == 0) pt = "bool";
                            }
                            strcat(params_buf, pt);
                        }
                        
                        // Generate function pointer type: ret_type (*param_name)(params)
                        emit("%s (*%.*s)(", ret_type, fn->params[j].length, fn->params[j].start);
                        emit("%s)", params_buf);
                        continue; // Skip the normal emit below
                    } else if (fn->param_types[j]->type == EXPR_IDENT) {
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
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            param_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            param_type = "WynHashSet*";
                        } else {
                            // Assume it's a struct type
                            snprintf(struct_type_name, sizeof(struct_type_name), "%.*s", 
                                    type_name.length, type_name.start);
                            param_type = struct_type_name;
                            is_struct_type = true;
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - use WynOptional* for proper optional handling
                        param_type = "WynOptional*";
                    }
                }
                
                emit("%s %.*s", param_type, fn->params[j].length, fn->params[j].start);
            }
            emit(");\n");
        }
    }
    
    // Generate forward declarations for impl block methods
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPL) {
            Stmt* stmt = prog->stmts[i];
            for (int j = 0; j < stmt->impl.method_count; j++) {
                FnStmt* method = stmt->impl.methods[j];
                
                // Determine return type
                const char* return_type = "int";
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token ret_type = method->return_type->token;
                    if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                        return_type = "int";
                    } else if (ret_type.length == 5 && memcmp(ret_type.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (ret_type.length == 4 && memcmp(ret_type.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                        return_type = "const char*";
                    }
                }
                
                // Generate forward declaration: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int k = 0; k < method->param_count; k++) {
                    if (k > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "int";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[k] && method->param_types[k]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[k]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "int";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else {
                            // Custom struct type
                            snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s", 
                                   type_name.length, type_name.start);
                            param_type = custom_type_buf;
                        }
                    }
                    
                    emit("%s %.*s", param_type, method->params[k].length, method->params[k].start);
                }
                emit(");\n");
            }
        }
    }
    emit("\n");
    
    // Emit spawn wrapper functions (after forward declarations)
    if (spawn_wrapper_count > 0) {
        emit("// Spawn wrapper functions\n");
        for (int i = 0; i < spawn_wrapper_count; i++) {
            emit("void __spawn_wrapper_%s(void* arg) {\n", spawn_wrappers[i].func_name);
            emit("    %s();\n", spawn_wrappers[i].func_name);
            emit("}\n\n");
        }
    }
    
    // Lambda functions will be emitted at the end of the program
    
    // Generate all functions
    for (int i = 0; i < prog->count; i++) {
        FnStmt* fn = NULL;
        
        if (prog->stmts[i]->type == STMT_FN) {
            fn = &prog->stmts[i]->fn;
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            fn = &prog->stmts[i]->export.stmt->fn;
        }
        
        if (fn) {
            // Skip generic functions - they will be handled by monomorphization
            if (fn->type_param_count > 0) {
                continue;
            }
            
            if (prog->stmts[i]->type == STMT_EXPORT) {
                codegen_stmt(prog->stmts[i]->export.stmt);
            } else {
                codegen_stmt(prog->stmts[i]);
            }
        }
    }
    
    // If no main function, create one that executes all statements
    if (!has_main) {
        emit("int main(int argc, char** argv) {\n");
        emit("    __wyn_argc = argc;\n");
        emit("    __wyn_argv = argv;\n");
        
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
                if (prog->stmts[i]->type != STMT_FN && prog->stmts[i]->type != STMT_CONST) {
                    emit("    ");
                    codegen_stmt(prog->stmts[i]);
                }
            }
            emit("    return 0;\n");
        }
        emit("}\n");
    } else {
        // User defined main() is renamed to wyn_main during codegen
        // wyn_wrapper.c provides the actual main() that calls wyn_main()
    }
    
    // Note: main() wrapper is provided by wyn_wrapper.c, not generated here
}

// T1.4.4: Control Flow Code Generation - Control Flow Agent addition
void codegen_match_statement(Stmt* stmt) {
    if (!stmt || stmt->type != STMT_MATCH) return;
    
    // Check if this is a simple integer match that can use switch
    bool can_use_switch = true;
    bool has_wildcard = false;
    
    for (int i = 0; i < stmt->match_stmt.case_count; i++) {
        MatchCase* match_case = &stmt->match_stmt.cases[i];
        if (match_case->pattern->type == PATTERN_WILDCARD) {
            has_wildcard = true;
        } else if (match_case->pattern->type != PATTERN_LITERAL) {
            can_use_switch = false;
            break;
        } else if (match_case->pattern->literal.value.type != TOKEN_INT) {
            can_use_switch = false;
            break;
        }
        if (match_case->guard) {
            can_use_switch = false;
            break;
        }
    }
    
    if (can_use_switch) {
        // Generate C switch statement for simple integer matching
        emit("switch (");
        codegen_expr(stmt->match_stmt.value);
        emit(") {\n");
        
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            
            if (match_case->pattern->type == PATTERN_LITERAL) {
                emit("        case %.*s: ", 
                     match_case->pattern->literal.value.length,
                     match_case->pattern->literal.value.start);
                
                if (match_case->body) {
                    codegen_stmt(match_case->body);
                }
                emit(" break;\n");
            } else if (match_case->pattern->type == PATTERN_WILDCARD) {
                emit("        default: ");
                if (match_case->body) {
                    codegen_stmt(match_case->body);
                }
                emit(" break;\n");
            }
        }
        
        emit("    }\n");
    } else {
        // Generate if-else chain for complex patterns
        emit("{\n");
        
        // Determine the type of the match value
        bool is_enum_match = false;
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            if (match_case->pattern->type == PATTERN_OPTION && 
                match_case->pattern->option.variant_name.length > 0) {
                is_enum_match = true;
                break;
            }
        }
        
        if (is_enum_match) {
            // For enum matches, store the whole enum value
            emit("    __auto_type __match_val = ");
        } else {
            emit("    int __match_val = ");
        }
        codegen_expr(stmt->match_stmt.value);
        emit(";\n");
        
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            
            if (i == 0) {
                emit("    if (");
            } else {
                emit("    } else if (");
            }
            
            // Handle different pattern types
            if (match_case->pattern->type == PATTERN_LITERAL) {
                emit("__match_val == %.*s", 
                     match_case->pattern->literal.value.length,
                     match_case->pattern->literal.value.start);
            } else if (match_case->pattern->type == PATTERN_WILDCARD) {
                emit("1"); // Always true for wildcard
            } else if (match_case->pattern->type == PATTERN_OPTION) {
                if (match_case->pattern->option.variant_name.length > 0) {
                    // Enum variant match: check tag
                    if (match_case->pattern->option.enum_name.length > 0) {
                        // Full enum name available: Result::Ok
                        emit("__match_val.tag == %.*s_%.*s_TAG",
                             match_case->pattern->option.enum_name.length,
                             match_case->pattern->option.enum_name.start,
                             match_case->pattern->option.variant_name.length,
                             match_case->pattern->option.variant_name.start);
                    } else {
                        // Only variant name: Ok
                        emit("__match_val.tag == %.*s_TAG",
                             match_case->pattern->option.variant_name.length,
                             match_case->pattern->option.variant_name.start);
                    }
                } else if (match_case->pattern->option.is_some) {
                    emit("wyn_optional_is_some(__match_val)");
                } else {
                    emit("wyn_optional_is_none(__match_val)");
                }
            } else if (match_case->pattern->type == PATTERN_IDENT) {
                emit("1"); // Variable binding always matches
            } else {
                emit("0"); // Unsupported pattern
            }
            
            // Add guard clause if present
            if (match_case->guard) {
                emit(" && (");
                codegen_expr(match_case->guard);
                emit(")");
            }
            
            emit(") {\n");
            
            // Generate variable bindings for patterns that need them
            if (match_case->pattern->type == PATTERN_IDENT) {
                Token var_name = match_case->pattern->ident.name;
                emit("        int %.*s = __match_val;\n", var_name.length, var_name.start);
            } else if (match_case->pattern->type == PATTERN_OPTION && 
                       match_case->pattern->option.inner &&
                       match_case->pattern->option.inner->type == PATTERN_IDENT) {
                
                Token var_name = match_case->pattern->option.inner->ident.name;
                
                if (match_case->pattern->option.variant_name.length > 0) {
                    // Enum variant destructuring: extract from data union
                    emit("        __auto_type %.*s = __match_val.data.%.*s_value;\n", 
                         var_name.length, var_name.start,
                         match_case->pattern->option.variant_name.length,
                         match_case->pattern->option.variant_name.start);
                } else if (match_case->pattern->option.is_some) {
                    emit("        int %.*s = wyn_optional_unwrap(__match_val);\n", 
                         var_name.length, var_name.start);
                }
            }
            
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
}
