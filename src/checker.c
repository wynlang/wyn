#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ast.h"
#include "types.h"
#include "error.h"  // T1.5.3: For type_error_mismatch function
#include "optional.h"
#include "result.h"
#include "traits.h"
#include "module_loader.h"

// Forward declarations
extern Program* load_module(const char* module_name);  // From module.c
void check_stmt(Stmt* stmt, SymbolTable* scope);
Type* check_expr(Expr* expr, SymbolTable* scope);
void analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope);
void analyze_expr_captures(Expr* expr, LambdaExpr* lambda, SymbolTable* scope);

static SymbolTable* global_scope = NULL;
static Program* current_program = NULL;  // For looking up struct definitions
static Type* builtin_int = NULL;
static Type* builtin_float = NULL;
static Type* builtin_string = NULL;
static Type* builtin_bool = NULL;
static Type* builtin_void = NULL;
static Type* builtin_array = NULL;
static bool had_error = false;
static Type* current_function_return_type = NULL;
static Type* current_self_type = NULL; // receiver type for extension methods

// Module visibility tracking
static char current_module_name[256] = "";

typedef struct {
    char module_name[128];
    char function_name[128];
    bool is_public;
} FunctionVisibility;

static FunctionVisibility function_registry[512];
static int function_registry_count = 0;

// Module collision tracking
typedef struct {
    char short_name[128];
    char full_path[256];
    int line_number;
} ImportedModule;

static ImportedModule imported_modules[128];
static int imported_modules_count = 0;

void set_current_module(const char* name) {
    if (name) {
        strncpy(current_module_name, name, 255);
        current_module_name[255] = '\0';
    } else {
        current_module_name[0] = '\0';
    }
}

static void register_import(const char* full_path, int line) {
    // Extract short name (last component after /)
    const char* last_slash = strrchr(full_path, '/');
    const char* short_name = last_slash ? last_slash + 1 : full_path;
    
    // Just register - don't error yet
    // Error will happen at call site if short name is used ambiguously
    if (imported_modules_count < 128) {
        strncpy(imported_modules[imported_modules_count].short_name, short_name, 127);
        strncpy(imported_modules[imported_modules_count].full_path, full_path, 255);
        imported_modules[imported_modules_count].line_number = line;
        imported_modules_count++;
    }
}

static bool is_ambiguous_module(const char* name, char* first_path, int* first_line, char* second_path, int* second_line) {
    int count = 0;
    int indices[2] = {-1, -1};
    const char* first_full_path = NULL;
    
    for (int i = 0; i < imported_modules_count; i++) {
        if (strcmp(imported_modules[i].short_name, name) == 0) {
            // Check if this is a different full path
            if (count == 0) {
                first_full_path = imported_modules[i].full_path;
                indices[0] = i;
                count = 1;
            } else if (strcmp(imported_modules[i].full_path, first_full_path) != 0) {
                // Different full path - this is ambiguous
                if (count == 1) {
                    indices[1] = i;
                }
                count++;
            }
            // Same full path - duplicate import, not ambiguous
        }
    }
    
    if (count > 1) {
        strcpy(first_path, imported_modules[indices[0]].full_path);
        *first_line = imported_modules[indices[0]].line_number;
        strcpy(second_path, imported_modules[indices[1]].full_path);
        *second_line = imported_modules[indices[1]].line_number;
        return true;
    }
    return false;
}

static void register_function_visibility(const char* module, const char* func, bool is_public) {
    if (function_registry_count < 512) {
        strncpy(function_registry[function_registry_count].module_name, module, 127);
        strncpy(function_registry[function_registry_count].function_name, func, 127);
        function_registry[function_registry_count].is_public = is_public;
        function_registry_count++;
    }
}

static bool check_function_visibility(const char* module, const char* func) {
    // If calling from same module, always allowed
    if (strcmp(current_module_name, module) == 0) {
        return true;
    }
    
    // Check if function is public
    for (int i = 0; i < function_registry_count; i++) {
        if (strcmp(function_registry[i].module_name, module) == 0 &&
            strcmp(function_registry[i].function_name, func) == 0) {
            return function_registry[i].is_public;
        }
    }
    
    // Not found - assume public for backwards compatibility
    return true;
}

static void print_type_name(Type* type) {
    if (!type) {
        fprintf(stderr, "unknown");
        return;
    }
    switch (type->kind) {
        case TYPE_INT: fprintf(stderr, "int"); break;
        case TYPE_FLOAT: fprintf(stderr, "float"); break;
        case TYPE_STRING: fprintf(stderr, "string"); break;
        case TYPE_BOOL: fprintf(stderr, "bool"); break;
        case TYPE_VOID: fprintf(stderr, "void"); break;
        case TYPE_ARRAY: 
            fprintf(stderr, "[");
            if (type->array_type.element_type) {
                print_type_name(type->array_type.element_type);
            } else {
                fprintf(stderr, "unknown");
            }
            fprintf(stderr, "]");
            break;
        case TYPE_MAP:
            fprintf(stderr, "HashMap<string, int>");
            break;
        case TYPE_SET:
            fprintf(stderr, "HashSet<int>");
            break;
        case TYPE_STRUCT:
            if (type->struct_type.name.length > 0) {
                fprintf(stderr, "%.*s", type->struct_type.name.length, type->struct_type.name.start);
            } else {
                fprintf(stderr, "struct");
            }
            break;
        case TYPE_OPTIONAL: // T2.5.1: Optional Type Implementation
            fprintf(stderr, "Option<");
            print_type_name(type->optional_type.inner_type);
            fprintf(stderr, ">");
            break;
        case TYPE_RESULT: // TASK-026: Result Type Implementation
            fprintf(stderr, "Result<");
            print_type_name(type->result_type.ok_type);
            fprintf(stderr, ", ");
            print_type_name(type->result_type.err_type);
            fprintf(stderr, ">");
            break;
        case TYPE_FUNCTION:
            fprintf(stderr, "fn(");
            for (int i = 0; i < type->fn_type.param_count; i++) {
                if (i > 0) fprintf(stderr, ", ");
                print_type_name(type->fn_type.param_types[i]);
            }
            fprintf(stderr, ") -> ");
            print_type_name(type->fn_type.return_type);
            break;
        default: fprintf(stderr, "unknown"); break;
    }
}

Type* make_type(TypeKind kind) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = kind;
    return t;
}

// T2.5.1: Optional Type Implementation - Helper functions
static bool is_optional_type(Type* type) {
    return type && type->kind == TYPE_OPTIONAL;
}

// TASK-026: Result Type Implementation - Helper functions
static bool is_result_type(Type* type) {
    return type && type->kind == TYPE_RESULT;
}

static Type* make_result_type(Type* ok_type, Type* err_type) {
    Type* result_type = make_type(TYPE_RESULT);
    result_type->result_type.ok_type = ok_type;
    result_type->result_type.err_type = err_type;
    return result_type;
}

// T1.5.4: Parameter Validation Implementation
typedef enum {
    VALIDATION_SUCCESS,
    VALIDATION_PARAM_COUNT_MISMATCH,
    VALIDATION_TYPE_MISMATCH,
    VALIDATION_NULL_FUNCTION,
    VALIDATION_NULL_ARGS
} ValidationResult;

// Helper function to find struct definition by name
static StructStmt* find_struct_definition(Token struct_name) {
    if (!current_program) return NULL;
    
    for (int i = 0; i < current_program->count; i++) {
        Stmt* stmt = current_program->stmts[i];
        if (stmt->type == STMT_STRUCT) {
            Token name = stmt->struct_decl.name;
            if (name.length == struct_name.length &&
                memcmp(name.start, struct_name.start, name.length) == 0) {
                return &stmt->struct_decl;
            }
        }
    }
    return NULL;
}

static EnumStmt* find_enum_definition(Token enum_name) {
    if (!current_program) return NULL;
    
    for (int i = 0; i < current_program->count; i++) {
        Stmt* stmt = current_program->stmts[i];
        if (stmt->type == STMT_ENUM) {
            Token name = stmt->enum_decl.name;
            if (name.length == enum_name.length &&
                memcmp(name.start, enum_name.start, name.length) == 0) {
                return &stmt->enum_decl;
            }
        }
    }
    return NULL;
}

// Helper function to get field type from struct definition
static Type* get_struct_field_type(StructStmt* struct_def, Token field_name) {
    if (!struct_def) return NULL;
    
    for (int i = 0; i < struct_def->field_count; i++) {
        Token fname = struct_def->fields[i];
        if (fname.length == field_name.length &&
            memcmp(fname.start, field_name.start, fname.length) == 0) {
            // Found the field, now get its type
            Expr* field_type_expr = struct_def->field_types[i];
            
            // Convert the type expression to a Type*
            // For now, handle basic cases
            if (field_type_expr->type == EXPR_IDENT) {
                Token type_name = field_type_expr->token;
                
                // Check for built-in types
                if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                    return builtin_int;
                } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                    return builtin_string;
                } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                    return builtin_bool;
                } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                    return builtin_float;
                } else {
                    // Check if it's an enum type
                    Symbol* type_symbol = find_symbol(global_scope, type_name);
                    if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_ENUM) {
                        return type_symbol->type;
                    }
                    // User-defined type (struct)
                    Type* struct_type = make_type(TYPE_STRUCT);
                    struct_type->struct_type.name = type_name;
                    return struct_type;
                }
            } else if (field_type_expr->type == EXPR_ARRAY) {
                // Array type [ElementType]
                Type* array_type = make_type(TYPE_ARRAY);
                if (field_type_expr->array.count > 0) {
                    Expr* elem_type_expr = field_type_expr->array.elements[0];
                    if (elem_type_expr->type == EXPR_IDENT) {
                        Token elem_type_name = elem_type_expr->token;
                        if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                            array_type->array_type.element_type = builtin_int;
                        } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                            array_type->array_type.element_type = builtin_string;
                        } else {
                            // User-defined element type
                            Type* elem_struct_type = make_type(TYPE_STRUCT);
                            elem_struct_type->struct_type.name = elem_type_name;
                            array_type->array_type.element_type = elem_struct_type;
                        }
                    }
                }
                return array_type;
            }
            
            return NULL;
        }
    }
    return NULL;
}


static bool wyn_is_type_compatible(Type* expected, Type* actual) {
    if (!expected || !actual) {
        return false;
    }
    
    // Exact type match
    if (expected->kind == actual->kind) {
        return true;
    }
    
    // Allow int to float conversion
    if (expected->kind == TYPE_FLOAT && actual->kind == TYPE_INT) {
        return true;
    }
    
    return false;
}

static ValidationResult wyn_validate_function_call(Symbol* func_symbol, Expr** args, int arg_count, SymbolTable* scope) {
    if (!func_symbol || !func_symbol->type || func_symbol->type->kind != TYPE_FUNCTION) {
        return VALIDATION_NULL_FUNCTION;
    }
    
    if (!args && arg_count > 0) {
        return VALIDATION_NULL_ARGS;
    }
    
    Type* func_type = func_symbol->type;
    
    // Check parameter count
    if (arg_count != func_type->fn_type.param_count) {
        return VALIDATION_PARAM_COUNT_MISMATCH;
    }
    
    // Check type compatibility for each parameter
    for (int i = 0; i < func_type->fn_type.param_count; i++) {
        Type* expected_type = func_type->fn_type.param_types[i];
        Type* actual_type = check_expr(args[i], scope);
        
        if (!wyn_is_type_compatible(expected_type, actual_type)) {
            return VALIDATION_TYPE_MISMATCH;
        }
    }
    
    return VALIDATION_SUCCESS;
}

static const char* wyn_validation_error_message(ValidationResult result) {
    switch (result) {
        case VALIDATION_SUCCESS:
            return "Function call is valid";
        case VALIDATION_PARAM_COUNT_MISMATCH:
            return "Parameter count mismatch";
        case VALIDATION_TYPE_MISMATCH:
            return "Parameter type mismatch";
        case VALIDATION_NULL_FUNCTION:
            return "Function is null";
        case VALIDATION_NULL_ARGS:
            return "Arguments are null";
        default:
            return "Unknown validation error";
    }
}

// Forward declarations for T1.5.3: Function overloading
static bool types_equal(Type* a, Type* b);
static const char* type_to_string(Type* type);
static bool signatures_match(Type* type1, Type* type2);
static char* generate_mangled_name(Token name, Type* type);
static Symbol* find_function_overload(SymbolTable* scope, Token name, Type** arg_types, int arg_count);
static int calculate_match_score(Type* fn_type, Type** arg_types, int arg_count);
static bool can_convert_type(Type* from, Type* to);
static void add_function_overload(SymbolTable* scope, Token name, Type* type, bool is_mutable);

static Type* get_inner_type(Type* optional_type) {
    if (!is_optional_type(optional_type)) return optional_type;
    return optional_type->optional_type.inner_type;
}

void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable) {
    if (scope->count >= scope->capacity) {
        scope->capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        scope->symbols = realloc(scope->symbols, scope->capacity * sizeof(Symbol));
    }
    scope->symbols[scope->count].name = name;
    scope->symbols[scope->count].type = type;
    scope->symbols[scope->count].is_mutable = is_mutable;
    scope->symbols[scope->count].next_overload = NULL;  // T1.5.3: Initialize overload chain
    scope->symbols[scope->count].mangled_name = NULL;   // T1.5.3: Initialize mangled name
    scope->count++;
}

static void mark_used(SymbolTable* scope, Token name) {
    for (int i = 0; i < scope->count; i++) {
        if (scope->symbols[i].name.length == name.length &&
            memcmp(scope->symbols[i].name.start, name.start, name.length) == 0) {
            scope->symbols[i].is_mutable = true; // Reuse flag to mark as used
            return;
        }
    }
    if (scope->parent) mark_used(scope->parent, name);
}

void init_checker() {
    global_scope = calloc(1, sizeof(SymbolTable));
    global_scope->capacity = 128;
    global_scope->symbols = calloc(128, sizeof(Symbol));
    had_error = false;
    
    // Initialize trait system
    wyn_traits_init();
    
    builtin_int = make_type(TYPE_INT);
    builtin_float = make_type(TYPE_FLOAT);
    builtin_string = make_type(TYPE_STRING);
    builtin_bool = make_type(TYPE_BOOL);
    builtin_void = make_type(TYPE_VOID);
    builtin_array = make_type(TYPE_ARRAY);
    
    // Register collection types
    Type* builtin_map = make_type(TYPE_MAP);
    Token map_tok = {TOKEN_IDENT, "HashMap", 7, 0};
    add_symbol(global_scope, map_tok, builtin_map, false);
    
    Type* builtin_set = make_type(TYPE_SET);
    Token set_tok = {TOKEN_IDENT, "HashSet", 7, 0};
    add_symbol(global_scope, set_tok, builtin_set, false);
    
    // Register Result types
    Type* result_int_type = make_type(TYPE_STRUCT);
    result_int_type->struct_type.name = (Token){TOKEN_IDENT, "ResultInt", 9, 0};
    Token result_int_tok = {TOKEN_IDENT, "ResultInt", 9, 0};
    add_symbol(global_scope, result_int_tok, result_int_type, false);
    
    Type* result_string_type = make_type(TYPE_STRUCT);
    result_string_type->struct_type.name = (Token){TOKEN_IDENT, "ResultString", 12, 0};
    Token result_string_tok = {TOKEN_IDENT, "ResultString", 12, 0};
    add_symbol(global_scope, result_string_tok, result_string_type, false);
    
    // Add built-in functions
    const char* stdlib_funcs[] = {
        "print", "print_float", "print_str", "print_bool", "print_hex", "print_bin", "println", "print_debug", "input", "input_float", "input_line", "printf_wyn", "string_format", "sin_approx", "cos_approx", "pi_const", "e_const",
        "str_len", "str_eq", "str_concat", "str_upper", "str_lower", "str_contains", "str_starts_with", "str_ends_with", "str_trim",
        "str_replace", "str_split", "str_join", "int_to_str", "str_to_int", "str_repeat", "str_reverse", "str_parse_int", "str_parse_int_failed", "str_parse_float", "str_free",
        "split_get", "split_count", "char_at", "is_numeric", "str_count", "str_contains_substr",
        "string_char_at", "string_length",
        "abs_val", "min", "max", "pow_int", "clamp", "sign", "gcd", "lcm", "is_even", "is_odd",
        "sqrt_int", "ceil_int", "floor_int", "round_int", "abs_float",
        "swap", "clamp_float", "lerp", "map_range",
        "bit_set", "bit_clear", "bit_toggle", "bit_check", "bit_count",
        "arr_sum", "arr_max", "arr_min", "arr_contains", "arr_find", "arr_reverse", "arr_sort", "arr_count", "arr_fill", "arr_all", "arr_join", "arr_map_double", "arr_map_square", "arr_filter_positive", "arr_filter_even", "arr_filter_greater_than_3", "arr_reduce_sum", "arr_reduce_product",
        "file_exists", "file_size", "file_delete", "file_append", "file_copy", "last_error_get",
        "file_move", "file_list_dir", "file_mkdir", "file_rmdir", "file_is_file", "file_is_dir",
        // NOTE: file_read, file_write, sys_exec are registered separately with proper types
        "random_int", "random_range", "random_float", "seed_random", "time_now", "time_format",
        "range", "array_new", "array_push", "array_pop", "array_length_dyn", "len",
        "assert_eq", "assert_true", "assert_false", "panic", "todo",
        "exit_program", "sleep_ms", "getenv_var", "setenv_var",
        "Error", "TypeError", "ValueError", "DivisionByZeroError", "print_error",
        "http_get", "http_post", "http_put", "http_delete", "http_set_header", "http_clear_headers", "http_status", "http_error",
        "https_get", "https_post",
        "hashmap_new", "hashmap_insert", "hashmap_get", "hashmap_has", "hashmap_remove", "hashmap_free",
        "hashmap_insert_int", "hashmap_insert_float", "hashmap_insert_string", "hashmap_insert_bool",
        "hashmap_get_int", "hashmap_get_float", "hashmap_get_string", "hashmap_get_bool",
        "wyn_hashmap_new", "wyn_hashmap_insert_int", "wyn_hashmap_get_int", "wyn_hashmap_has", "wyn_hashmap_len", "wyn_hashmap_free",
        "hashset_new", "hashset_add", "hashset_contains", "hashset_remove", "hashset_free",
        "set_len", "set_is_empty", "set_clear", "set_union", "set_intersection", "set_difference", "set_is_subset", "set_is_superset",
        "json_parse", "json_get_string", "json_get_int", "json_free",
        "json_get_str", "json_get_int", "json_get_bool", "json_has_key", "json_stringify_int", "json_stringify_str", "json_stringify_bool", "json_array_stringify", "json_array_length", "json_array_get",
        "url_encode", "url_decode", "base64_encode", "hash_string",
        // v1.3.0 Standard Library
        "wyn_string_len", "wyn_string_contains", "wyn_string_starts_with", "wyn_string_ends_with",
        "wyn_string_to_upper", "wyn_string_to_lower", "wyn_string_trim", "wyn_str_replace",
        "wyn_string_split", "wyn_string_join", "wyn_str_substring", "wyn_string_index_of",
        "wyn_string_last_index_of", "wyn_string_repeat", "wyn_string_reverse",
        "wyn_string_pad_left", "wyn_string_pad_right",
        "wyn_string_pad_left_safe", "wyn_string_pad_right_safe",
        "wyn_array_map", "wyn_array_filter", "wyn_array_reduce", "wyn_array_find",
        "wyn_array_find_index", "wyn_array_unique", "wyn_array_join",
        "wyn_array_first", "wyn_array_last", "wyn_array_is_empty",
        "wyn_array_any", "wyn_array_all", "wyn_array_reverse", "wyn_array_sort",
        "wyn_array_contains", "wyn_array_index_of", "wyn_array_last_index_of",
        "wyn_array_slice", "wyn_array_concat", "wyn_array_fill",
        "wyn_array_sum", "wyn_array_min", "wyn_array_max", "wyn_array_average",
        "wyn_time_now", "wyn_time_now_millis", "wyn_time_now_micros",
        "wyn_time_sleep", "wyn_time_sleep_millis", "wyn_time_sleep_micros",
        "wyn_time_format", "wyn_time_parse",
        "wyn_time_year", "wyn_time_month", "wyn_time_day",
        "wyn_time_hour", "wyn_time_minute", "wyn_time_second",
        "wyn_crypto_hash32", "wyn_crypto_hash64", "wyn_crypto_md5", "wyn_crypto_sha256",
        "wyn_crypto_base64_encode", "wyn_crypto_base64_decode",
        "wyn_crypto_random_bytes", "wyn_crypto_random_hex", "wyn_crypto_xor_cipher",
        "wyn_math_abs", "wyn_math_min", "wyn_math_max", "wyn_math_pow",
        "wyn_math_sqrt", "wyn_math_floor", "wyn_math_ceil", "wyn_math_round"
    };
    
    for (int i = 0; i < 232; i++) {  // Updated count: 224 + 8 = 232
        Token tok = {TOKEN_IDENT, stdlib_funcs[i], (int)strlen(stdlib_funcs[i]), 0};
        add_symbol(global_scope, tok, builtin_int, false);
    }
    
    // Add C interface functions with correct function types
    Type* get_argc_type = make_type(TYPE_FUNCTION);
    get_argc_type->fn_type.param_count = 0;
    get_argc_type->fn_type.return_type = builtin_int;
    Token get_argc_tok = {TOKEN_IDENT, "get_argc", 8, 0};
    add_symbol(global_scope, get_argc_tok, get_argc_type, false);
    
    Type* get_argv_type = make_type(TYPE_FUNCTION);
    get_argv_type->fn_type.param_count = 1;
    get_argv_type->fn_type.param_types = malloc(sizeof(Type*));
    get_argv_type->fn_type.param_types[0] = builtin_int;
    get_argv_type->fn_type.return_type = builtin_string;
    Token get_argv_tok = {TOKEN_IDENT, "get_argv", 8, 0};
    add_symbol(global_scope, get_argv_tok, get_argv_type, false);
    
    Type* read_file_content_type = make_type(TYPE_FUNCTION);
    read_file_content_type->fn_type.param_count = 1;
    read_file_content_type->fn_type.param_types = malloc(sizeof(Type*));
    read_file_content_type->fn_type.param_types[0] = builtin_string;
    read_file_content_type->fn_type.return_type = builtin_string;
    Token read_file_content_tok = {TOKEN_IDENT, "read_file_content", 17, 0};
    add_symbol(global_scope, read_file_content_tok, read_file_content_type, false);
    
    Type* check_file_exists_type = make_type(TYPE_FUNCTION);
    check_file_exists_type->fn_type.param_count = 1;
    check_file_exists_type->fn_type.param_types = malloc(sizeof(Type*));
    check_file_exists_type->fn_type.param_types[0] = builtin_string;
    check_file_exists_type->fn_type.return_type = builtin_bool;
    Token check_file_exists_tok = {TOKEN_IDENT, "check_file_exists", 17, 0};
    add_symbol(global_scope, check_file_exists_tok, check_file_exists_type, false);
    
    Type* is_content_valid_type = make_type(TYPE_FUNCTION);
    is_content_valid_type->fn_type.param_count = 1;
    is_content_valid_type->fn_type.param_types = malloc(sizeof(Type*));
    is_content_valid_type->fn_type.param_types[0] = builtin_string;
    is_content_valid_type->fn_type.return_type = builtin_bool;
    Token is_content_valid_tok = {TOKEN_IDENT, "is_content_valid", 16, 0};
    add_symbol(global_scope, is_content_valid_tok, is_content_valid_type, false);
    
    // Add compiler interface functions
    Type* c_init_lexer_type = make_type(TYPE_FUNCTION);
    c_init_lexer_type->fn_type.param_count = 1;
    c_init_lexer_type->fn_type.param_types = malloc(sizeof(Type*));
    c_init_lexer_type->fn_type.param_types[0] = builtin_string;
    c_init_lexer_type->fn_type.return_type = builtin_bool;
    Token c_init_lexer_tok = {TOKEN_IDENT, "c_init_lexer", 12, 0};
    add_symbol(global_scope, c_init_lexer_tok, c_init_lexer_type, false);
    
    Type* c_init_parser_type = make_type(TYPE_FUNCTION);
    c_init_parser_type->fn_type.param_count = 0;
    c_init_parser_type->fn_type.return_type = builtin_int;
    Token c_init_parser_tok = {TOKEN_IDENT, "c_init_parser", 13, 0};
    add_symbol(global_scope, c_init_parser_tok, c_init_parser_type, false);
    
    Type* c_parse_program_type = make_type(TYPE_FUNCTION);
    c_parse_program_type->fn_type.param_count = 0;
    c_parse_program_type->fn_type.return_type = builtin_int;
    Token c_parse_program_tok = {TOKEN_IDENT, "c_parse_program", 15, 0};
    add_symbol(global_scope, c_parse_program_tok, c_parse_program_type, false);
    
    Type* c_init_checker_type = make_type(TYPE_FUNCTION);
    c_init_checker_type->fn_type.param_count = 0;
    c_init_checker_type->fn_type.return_type = builtin_int;
    Token c_init_checker_tok = {TOKEN_IDENT, "c_init_checker", 14, 0};
    add_symbol(global_scope, c_init_checker_tok, c_init_checker_type, false);
    
    Type* c_check_program_type = make_type(TYPE_FUNCTION);
    c_check_program_type->fn_type.param_count = 1;
    c_check_program_type->fn_type.param_types = malloc(sizeof(Type*));
    c_check_program_type->fn_type.param_types[0] = builtin_int;
    c_check_program_type->fn_type.return_type = builtin_int;
    Token c_check_program_tok = {TOKEN_IDENT, "c_check_program", 15, 0};
    add_symbol(global_scope, c_check_program_tok, c_check_program_type, false);
    
    Type* c_checker_had_error_type = make_type(TYPE_FUNCTION);
    c_checker_had_error_type->fn_type.param_count = 0;
    c_checker_had_error_type->fn_type.return_type = builtin_bool;
    Token c_checker_had_error_tok = {TOKEN_IDENT, "c_checker_had_error", 19, 0};
    add_symbol(global_scope, c_checker_had_error_tok, c_checker_had_error_type, false);
    
    Type* c_generate_code_type = make_type(TYPE_FUNCTION);
    c_generate_code_type->fn_type.param_count = 2;
    c_generate_code_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    c_generate_code_type->fn_type.param_types[0] = builtin_int;
    c_generate_code_type->fn_type.param_types[1] = builtin_string;
    c_generate_code_type->fn_type.return_type = builtin_bool;
    Token c_generate_code_tok = {TOKEN_IDENT, "c_generate_code", 15, 0};
    add_symbol(global_scope, c_generate_code_tok, c_generate_code_type, false);
    
    // Add wyn_math function types
    Type* wyn_math_abs_type = make_type(TYPE_FUNCTION);
    wyn_math_abs_type->fn_type.param_count = 1;
    wyn_math_abs_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_abs_type->fn_type.param_types[0] = builtin_float;
    wyn_math_abs_type->fn_type.return_type = builtin_float;
    Token wyn_math_abs_tok = {TOKEN_IDENT, "wyn_math_abs", 12, 0};
    add_symbol(global_scope, wyn_math_abs_tok, wyn_math_abs_type, false);
    
    Type* wyn_math_min_type = make_type(TYPE_FUNCTION);
    wyn_math_min_type->fn_type.param_count = 2;
    wyn_math_min_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_min_type->fn_type.param_types[0] = builtin_float;
    wyn_math_min_type->fn_type.param_types[1] = builtin_float;
    wyn_math_min_type->fn_type.return_type = builtin_float;
    Token wyn_math_min_tok = {TOKEN_IDENT, "wyn_math_min", 12, 0};
    add_symbol(global_scope, wyn_math_min_tok, wyn_math_min_type, false);
    
    Type* wyn_math_max_type = make_type(TYPE_FUNCTION);
    wyn_math_max_type->fn_type.param_count = 2;
    wyn_math_max_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_max_type->fn_type.param_types[0] = builtin_float;
    wyn_math_max_type->fn_type.param_types[1] = builtin_float;
    wyn_math_max_type->fn_type.return_type = builtin_float;
    Token wyn_math_max_tok = {TOKEN_IDENT, "wyn_math_max", 12, 0};
    add_symbol(global_scope, wyn_math_max_tok, wyn_math_max_type, false);
    
    Type* wyn_math_pow_type = make_type(TYPE_FUNCTION);
    wyn_math_pow_type->fn_type.param_count = 2;
    wyn_math_pow_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_pow_type->fn_type.param_types[0] = builtin_float;
    wyn_math_pow_type->fn_type.param_types[1] = builtin_float;
    wyn_math_pow_type->fn_type.return_type = builtin_float;
    Token wyn_math_pow_tok = {TOKEN_IDENT, "wyn_math_pow", 12, 0};
    add_symbol(global_scope, wyn_math_pow_tok, wyn_math_pow_type, false);
    
    Type* wyn_math_sqrt_type = make_type(TYPE_FUNCTION);
    wyn_math_sqrt_type->fn_type.param_count = 1;
    wyn_math_sqrt_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_sqrt_type->fn_type.param_types[0] = builtin_float;
    wyn_math_sqrt_type->fn_type.return_type = builtin_float;
    Token wyn_math_sqrt_tok = {TOKEN_IDENT, "wyn_math_sqrt", 13, 0};
    add_symbol(global_scope, wyn_math_sqrt_tok, wyn_math_sqrt_type, false);
    
    Type* wyn_math_floor_type = make_type(TYPE_FUNCTION);
    wyn_math_floor_type->fn_type.param_count = 1;
    wyn_math_floor_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_floor_type->fn_type.param_types[0] = builtin_float;
    wyn_math_floor_type->fn_type.return_type = builtin_float;
    Token wyn_math_floor_tok = {TOKEN_IDENT, "wyn_math_floor", 14, 0};
    add_symbol(global_scope, wyn_math_floor_tok, wyn_math_floor_type, false);
    
    Type* wyn_math_ceil_type = make_type(TYPE_FUNCTION);
    wyn_math_ceil_type->fn_type.param_count = 1;
    wyn_math_ceil_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_ceil_type->fn_type.param_types[0] = builtin_float;
    wyn_math_ceil_type->fn_type.return_type = builtin_float;
    Token wyn_math_ceil_tok = {TOKEN_IDENT, "wyn_math_ceil", 13, 0};
    add_symbol(global_scope, wyn_math_ceil_tok, wyn_math_ceil_type, false);
    
    Type* wyn_math_round_type = make_type(TYPE_FUNCTION);
    wyn_math_round_type->fn_type.param_count = 1;
    wyn_math_round_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_round_type->fn_type.param_types[0] = builtin_float;
    wyn_math_round_type->fn_type.return_type = builtin_float;
    Token wyn_math_round_tok = {TOKEN_IDENT, "wyn_math_round", 14, 0};
    add_symbol(global_scope, wyn_math_round_tok, wyn_math_round_type, false);
    
    Type* c_create_c_filename_type = make_type(TYPE_FUNCTION);
    c_create_c_filename_type->fn_type.param_count = 1;
    c_create_c_filename_type->fn_type.param_types = malloc(sizeof(Type*));
    c_create_c_filename_type->fn_type.param_types[0] = builtin_string;
    c_create_c_filename_type->fn_type.return_type = builtin_string;
    Token c_create_c_filename_tok = {TOKEN_IDENT, "c_create_c_filename", 19, 0};
    add_symbol(global_scope, c_create_c_filename_tok, c_create_c_filename_type, false);
    
    Type* c_compile_to_binary_type = make_type(TYPE_FUNCTION);
    c_compile_to_binary_type->fn_type.param_count = 2;
    c_compile_to_binary_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    c_compile_to_binary_type->fn_type.param_types[0] = builtin_string;
    c_compile_to_binary_type->fn_type.param_types[1] = builtin_string;
    c_compile_to_binary_type->fn_type.return_type = builtin_bool;
    Token c_compile_to_binary_tok = {TOKEN_IDENT, "c_compile_to_binary", 19, 0};
    add_symbol(global_scope, c_compile_to_binary_tok, c_compile_to_binary_type, false);
    
    Type* c_remove_file_type = make_type(TYPE_FUNCTION);
    c_remove_file_type->fn_type.param_count = 1;
    c_remove_file_type->fn_type.param_types = malloc(sizeof(Type*));
    c_remove_file_type->fn_type.param_types[0] = builtin_string;
    c_remove_file_type->fn_type.return_type = builtin_bool;
    Token c_remove_file_tok = {TOKEN_IDENT, "c_remove_file", 13, 0};
    add_symbol(global_scope, c_remove_file_tok, c_remove_file_type, false);
    
    // Register Result functions
    Type* result_int_ok_type = make_type(TYPE_FUNCTION);
    result_int_ok_type->fn_type.param_count = 1;
    result_int_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_ok_type->fn_type.param_types[0] = builtin_int;
    result_int_ok_type->fn_type.return_type = result_int_type;
    Token result_int_ok_tok = {TOKEN_IDENT, "ResultInt_Ok", 12, 0};
    add_symbol(global_scope, result_int_ok_tok, result_int_ok_type, false);
    
    Type* result_int_err_type = make_type(TYPE_FUNCTION);
    result_int_err_type->fn_type.param_count = 1;
    result_int_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_err_type->fn_type.param_types[0] = builtin_string;
    result_int_err_type->fn_type.return_type = result_int_type;
    Token result_int_err_tok = {TOKEN_IDENT, "ResultInt_Err", 13, 0};
    add_symbol(global_scope, result_int_err_tok, result_int_err_type, false);
    
    Type* result_int_is_ok_type = make_type(TYPE_FUNCTION);
    result_int_is_ok_type->fn_type.param_count = 1;
    result_int_is_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_is_ok_type->fn_type.param_types[0] = result_int_type;
    result_int_is_ok_type->fn_type.return_type = builtin_int;
    Token result_int_is_ok_tok = {TOKEN_IDENT, "ResultInt_is_ok", 15, 0};
    add_symbol(global_scope, result_int_is_ok_tok, result_int_is_ok_type, false);
    
    Type* result_int_is_err_type = make_type(TYPE_FUNCTION);
    result_int_is_err_type->fn_type.param_count = 1;
    result_int_is_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_is_err_type->fn_type.param_types[0] = result_int_type;
    result_int_is_err_type->fn_type.return_type = builtin_int;
    Token result_int_is_err_tok = {TOKEN_IDENT, "ResultInt_is_err", 16, 0};
    add_symbol(global_scope, result_int_is_err_tok, result_int_is_err_type, false);
    
    Type* result_string_ok_type = make_type(TYPE_FUNCTION);
    result_string_ok_type->fn_type.param_count = 1;
    result_string_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_ok_type->fn_type.param_types[0] = builtin_string;
    result_string_ok_type->fn_type.return_type = result_string_type;
    Token result_string_ok_tok = {TOKEN_IDENT, "ResultString_Ok", 15, 0};
    add_symbol(global_scope, result_string_ok_tok, result_string_ok_type, false);
    
    Type* result_string_err_type = make_type(TYPE_FUNCTION);
    result_string_err_type->fn_type.param_count = 1;
    result_string_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_err_type->fn_type.param_types[0] = builtin_string;
    result_string_err_type->fn_type.return_type = result_string_type;
    Token result_string_err_tok = {TOKEN_IDENT, "ResultString_Err", 16, 0};
    add_symbol(global_scope, result_string_err_tok, result_string_err_type, false);
    
    Type* result_string_is_ok_type = make_type(TYPE_FUNCTION);
    result_string_is_ok_type->fn_type.param_count = 1;
    result_string_is_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_is_ok_type->fn_type.param_types[0] = result_string_type;
    result_string_is_ok_type->fn_type.return_type = builtin_int;
    Token result_string_is_ok_tok = {TOKEN_IDENT, "ResultString_is_ok", 18, 0};
    add_symbol(global_scope, result_string_is_ok_tok, result_string_is_ok_type, false);
    
    Type* result_string_is_err_type = make_type(TYPE_FUNCTION);
    result_string_is_err_type->fn_type.param_count = 1;
    result_string_is_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_is_err_type->fn_type.param_types[0] = result_string_type;
    result_string_is_err_type->fn_type.return_type = builtin_int;
    Token result_string_is_err_tok = {TOKEN_IDENT, "ResultString_is_err", 19, 0};
    add_symbol(global_scope, result_string_is_err_tok, result_string_is_err_type, false);
    
    // Register Result unwrap functions
    Type* result_int_unwrap_type = make_type(TYPE_FUNCTION);
    result_int_unwrap_type->fn_type.param_count = 1;
    result_int_unwrap_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_unwrap_type->fn_type.param_types[0] = result_int_type;
    result_int_unwrap_type->fn_type.return_type = builtin_int;
    Token result_int_unwrap_tok = {TOKEN_IDENT, "ResultInt_unwrap", 16, 0};
    add_symbol(global_scope, result_int_unwrap_tok, result_int_unwrap_type, false);
    
    Type* result_int_unwrap_err_type = make_type(TYPE_FUNCTION);
    result_int_unwrap_err_type->fn_type.param_count = 1;
    result_int_unwrap_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_unwrap_err_type->fn_type.param_types[0] = result_int_type;
    result_int_unwrap_err_type->fn_type.return_type = builtin_string;
    Token result_int_unwrap_err_tok = {TOKEN_IDENT, "ResultInt_unwrap_err", 20, 0};
    add_symbol(global_scope, result_int_unwrap_err_tok, result_int_unwrap_err_type, false);
    
    Type* result_string_unwrap_type = make_type(TYPE_FUNCTION);
    result_string_unwrap_type->fn_type.param_count = 1;
    result_string_unwrap_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_unwrap_type->fn_type.param_types[0] = result_string_type;
    result_string_unwrap_type->fn_type.return_type = builtin_string;
    Token result_string_unwrap_tok = {TOKEN_IDENT, "ResultString_unwrap", 19, 0};
    add_symbol(global_scope, result_string_unwrap_tok, result_string_unwrap_type, false);
    
    Type* result_string_unwrap_err_type = make_type(TYPE_FUNCTION);
    result_string_unwrap_err_type->fn_type.param_count = 1;
    result_string_unwrap_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_unwrap_err_type->fn_type.param_types[0] = result_string_type;
    result_string_unwrap_err_type->fn_type.return_type = builtin_string;
    Token result_string_unwrap_err_tok = {TOKEN_IDENT, "ResultString_unwrap_err", 23, 0};
    add_symbol(global_scope, result_string_unwrap_err_tok, result_string_unwrap_err_type, false);

    // OptionInt type and functions
    Type* option_int_type = make_type(TYPE_STRUCT);
    Token option_int_name = {TOKEN_IDENT, "OptionInt", 9, 0};
    option_int_type->struct_type.name = option_int_name;
    add_symbol(global_scope, option_int_name, option_int_type, false);

    Type* oi_some_t = make_type(TYPE_FUNCTION);
    oi_some_t->fn_type.param_count = 1;
    oi_some_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_some_t->fn_type.param_types[0] = builtin_int;
    oi_some_t->fn_type.return_type = option_int_type;
    Token oi_some_tok = {TOKEN_IDENT, "OptionInt_Some", 14, 0};
    add_symbol(global_scope, oi_some_tok, oi_some_t, false);

    Type* oi_none_t = make_type(TYPE_FUNCTION);
    oi_none_t->fn_type.param_count = 0;
    oi_none_t->fn_type.param_types = NULL;
    oi_none_t->fn_type.return_type = option_int_type;
    Token oi_none_tok = {TOKEN_IDENT, "OptionInt_None", 14, 0};
    add_symbol(global_scope, oi_none_tok, oi_none_t, false);

    Type* oi_is_some_t = make_type(TYPE_FUNCTION);
    oi_is_some_t->fn_type.param_count = 1;
    oi_is_some_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_is_some_t->fn_type.param_types[0] = option_int_type;
    oi_is_some_t->fn_type.return_type = builtin_int;
    Token oi_is_some_tok = {TOKEN_IDENT, "OptionInt_is_some", 17, 0};
    add_symbol(global_scope, oi_is_some_tok, oi_is_some_t, false);

    Type* oi_is_none_t = make_type(TYPE_FUNCTION);
    oi_is_none_t->fn_type.param_count = 1;
    oi_is_none_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_is_none_t->fn_type.param_types[0] = option_int_type;
    oi_is_none_t->fn_type.return_type = builtin_int;
    Token oi_is_none_tok = {TOKEN_IDENT, "OptionInt_is_none", 17, 0};
    add_symbol(global_scope, oi_is_none_tok, oi_is_none_t, false);

    Type* oi_unwrap_t = make_type(TYPE_FUNCTION);
    oi_unwrap_t->fn_type.param_count = 1;
    oi_unwrap_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_unwrap_t->fn_type.param_types[0] = option_int_type;
    oi_unwrap_t->fn_type.return_type = builtin_int;
    Token oi_unwrap_tok = {TOKEN_IDENT, "OptionInt_unwrap", 16, 0};
    add_symbol(global_scope, oi_unwrap_tok, oi_unwrap_t, false);

    Type* oi_unwrap_or_t = make_type(TYPE_FUNCTION);
    oi_unwrap_or_t->fn_type.param_count = 2;
    oi_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    oi_unwrap_or_t->fn_type.param_types[0] = option_int_type;
    oi_unwrap_or_t->fn_type.param_types[1] = builtin_int;
    oi_unwrap_or_t->fn_type.return_type = builtin_int;
    Token oi_unwrap_or_tok = {TOKEN_IDENT, "OptionInt_unwrap_or", 19, 0};
    add_symbol(global_scope, oi_unwrap_or_tok, oi_unwrap_or_t, false);

    // OptionString type and functions
    Type* option_string_type = make_type(TYPE_STRUCT);
    Token option_string_name = {TOKEN_IDENT, "OptionString", 12, 0};
    option_string_type->struct_type.name = option_string_name;
    add_symbol(global_scope, option_string_name, option_string_type, false);

    Type* os_some_t = make_type(TYPE_FUNCTION);
    os_some_t->fn_type.param_count = 1;
    os_some_t->fn_type.param_types = malloc(sizeof(Type*));
    os_some_t->fn_type.param_types[0] = builtin_string;
    os_some_t->fn_type.return_type = option_string_type;
    Token os_some_tok = {TOKEN_IDENT, "OptionString_Some", 17, 0};
    add_symbol(global_scope, os_some_tok, os_some_t, false);

    Type* os_none_t = make_type(TYPE_FUNCTION);
    os_none_t->fn_type.param_count = 0;
    os_none_t->fn_type.param_types = NULL;
    os_none_t->fn_type.return_type = option_string_type;
    Token os_none_tok = {TOKEN_IDENT, "OptionString_None", 17, 0};
    add_symbol(global_scope, os_none_tok, os_none_t, false);

    Type* os_is_some_t = make_type(TYPE_FUNCTION);
    os_is_some_t->fn_type.param_count = 1;
    os_is_some_t->fn_type.param_types = malloc(sizeof(Type*));
    os_is_some_t->fn_type.param_types[0] = option_string_type;
    os_is_some_t->fn_type.return_type = builtin_int;
    Token os_is_some_tok = {TOKEN_IDENT, "OptionString_is_some", 20, 0};
    add_symbol(global_scope, os_is_some_tok, os_is_some_t, false);

    Type* os_is_none_t = make_type(TYPE_FUNCTION);
    os_is_none_t->fn_type.param_count = 1;
    os_is_none_t->fn_type.param_types = malloc(sizeof(Type*));
    os_is_none_t->fn_type.param_types[0] = option_string_type;
    os_is_none_t->fn_type.return_type = builtin_int;
    Token os_is_none_tok = {TOKEN_IDENT, "OptionString_is_none", 20, 0};
    add_symbol(global_scope, os_is_none_tok, os_is_none_t, false);

    Type* os_unwrap_t = make_type(TYPE_FUNCTION);
    os_unwrap_t->fn_type.param_count = 1;
    os_unwrap_t->fn_type.param_types = malloc(sizeof(Type*));
    os_unwrap_t->fn_type.param_types[0] = option_string_type;
    os_unwrap_t->fn_type.return_type = builtin_string;
    Token os_unwrap_tok = {TOKEN_IDENT, "OptionString_unwrap", 19, 0};
    add_symbol(global_scope, os_unwrap_tok, os_unwrap_t, false);

    Type* os_unwrap_or_t = make_type(TYPE_FUNCTION);
    os_unwrap_or_t->fn_type.param_count = 2;
    os_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    os_unwrap_or_t->fn_type.param_types[0] = option_string_type;
    os_unwrap_or_t->fn_type.param_types[1] = builtin_string;
    os_unwrap_or_t->fn_type.return_type = builtin_string;
    Token os_unwrap_or_tok = {TOKEN_IDENT, "OptionString_unwrap_or", 22, 0};
    add_symbol(global_scope, os_unwrap_or_tok, os_unwrap_or_t, false);

    // System functions
    Type* sys_exec_t = make_type(TYPE_FUNCTION);
    sys_exec_t->fn_type.param_count = 1;
    sys_exec_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exec_t->fn_type.param_types[0] = builtin_string;
    sys_exec_t->fn_type.return_type = builtin_string;
    Token sys_exec_tok = {TOKEN_IDENT, "System_exec", 11, 0};
    add_symbol(global_scope, sys_exec_tok, sys_exec_t, false);

    Type* sys_exec_code_t = make_type(TYPE_FUNCTION);
    sys_exec_code_t->fn_type.param_count = 1;
    sys_exec_code_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exec_code_t->fn_type.param_types[0] = builtin_string;
    sys_exec_code_t->fn_type.return_type = builtin_int;
    Token sys_exec_code_tok = {TOKEN_IDENT, "System_exec_code", 16, 0};
    add_symbol(global_scope, sys_exec_code_tok, sys_exec_code_t, false);

    Type* sys_exit_t = make_type(TYPE_FUNCTION);
    sys_exit_t->fn_type.param_count = 1;
    sys_exit_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exit_t->fn_type.param_types[0] = builtin_int;
    sys_exit_t->fn_type.return_type = builtin_void;
    Token sys_exit_tok = {TOKEN_IDENT, "System_exit", 11, 0};
    add_symbol(global_scope, sys_exit_tok, sys_exit_t, false);

    Type* sys_env_t = make_type(TYPE_FUNCTION);
    sys_env_t->fn_type.param_count = 1;
    sys_env_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_env_t->fn_type.param_types[0] = builtin_string;
    sys_env_t->fn_type.return_type = builtin_string;
    Token sys_env_tok = {TOKEN_IDENT, "System_env", 10, 0};
    add_symbol(global_scope, sys_env_tok, sys_env_t, false);

    // Conversion functions
    Type* itos_t = make_type(TYPE_FUNCTION);
    itos_t->fn_type.param_count = 1;
    itos_t->fn_type.param_types = malloc(sizeof(Type*));
    itos_t->fn_type.param_types[0] = builtin_int;
    itos_t->fn_type.return_type = builtin_string;
    Token itos_tok = {TOKEN_IDENT, "int_to_string", 13, 0};
    add_symbol(global_scope, itos_tok, itos_t, false);

    Type* ftos_t = make_type(TYPE_FUNCTION);
    ftos_t->fn_type.param_count = 1;
    ftos_t->fn_type.param_types = malloc(sizeof(Type*));
    ftos_t->fn_type.param_types[0] = builtin_float;
    ftos_t->fn_type.return_type = builtin_string;
    Token ftos_tok = {TOKEN_IDENT, "float_to_string", 15, 0};
    add_symbol(global_scope, ftos_tok, ftos_t, false);

    // Math stdlib - register all Math_ functions
    struct { const char* name; int nlen; int param_count; Type* p1; Type* p2; Type* ret; } math_fns[] = {
        {"Math_abs", 8, 1, builtin_float, NULL, builtin_float},
        {"Math_max", 8, 2, builtin_float, builtin_float, builtin_float},
        {"Math_min", 8, 2, builtin_float, builtin_float, builtin_float},
        {"Math_pow", 8, 2, builtin_float, builtin_float, builtin_float},
        {"Math_sqrt", 9, 1, builtin_float, NULL, builtin_float},
        {"Math_floor", 10, 1, builtin_float, NULL, builtin_float},
        {"Math_ceil", 9, 1, builtin_float, NULL, builtin_float},
        {"Math_round", 10, 1, builtin_float, NULL, builtin_float},
        {"Math_sin", 8, 1, builtin_float, NULL, builtin_float},
        {"Math_cos", 8, 1, builtin_float, NULL, builtin_float},
        {"Math_tan", 8, 1, builtin_float, NULL, builtin_float},
        {"Math_random", 11, 0, NULL, NULL, builtin_float},
    };
    for (int i = 0; i < 13; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = math_fns[i].param_count;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        if (math_fns[i].p1) ft->fn_type.param_types[0] = math_fns[i].p1;
        if (math_fns[i].p2) ft->fn_type.param_types[1] = math_fns[i].p2;
        ft->fn_type.return_type = math_fns[i].ret;
        Token tok = {TOKEN_IDENT, math_fns[i].name, math_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // String stdlib - function-style str_ functions
    struct { const char* name; int nlen; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } str_fns[] = {
        {"str_len", 7, 1, builtin_string, NULL, NULL, builtin_int},
        {"str_upper", 9, 1, builtin_string, NULL, NULL, builtin_string},
        {"str_lower", 9, 1, builtin_string, NULL, NULL, builtin_string},
        {"str_trim", 8, 1, builtin_string, NULL, NULL, builtin_string},
        {"str_contains", 12, 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_starts_with", 15, 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_ends_with", 13, 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_index_of", 12, 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_replace", 11, 3, builtin_string, builtin_string, builtin_string, builtin_string},
        {"str_substring", 13, 3, builtin_string, builtin_int, builtin_int, builtin_string},
        {"str_repeat", 10, 2, builtin_string, builtin_int, NULL, builtin_string},
        {"str_concat", 10, 2, builtin_string, builtin_string, NULL, builtin_string},
        {"str_to_int", 10, 1, builtin_string, NULL, NULL, builtin_int},
    };
    for (int i = 0; i < 13; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = str_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (str_fns[i].p1) ft->fn_type.param_types[0] = str_fns[i].p1;
        if (str_fns[i].p2) ft->fn_type.param_types[1] = str_fns[i].p2;
        if (str_fns[i].p3) ft->fn_type.param_types[2] = str_fns[i].p3;
        ft->fn_type.return_type = str_fns[i].ret;
        Token tok = {TOKEN_IDENT, str_fns[i].name, str_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Path stdlib
    struct { const char* name; int nlen; } path_fns[] = {
        {"Path_basename", 13}, {"Path_dirname", 12},
        {"Path_extension", 14}, {"Path_join", 9},
    };
    for (int i = 0; i < 4; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = (i == 3) ? 2 : 1; // Path_join takes 2
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = builtin_string;
        if (i == 3) ft->fn_type.param_types[1] = builtin_string;
        ft->fn_type.return_type = builtin_string;
        Token tok = {TOKEN_IDENT, path_fns[i].name, path_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }
    
    // JSON stdlib
    Type* json_obj_type = make_type(TYPE_MAP);
    struct { const char* name; int nlen; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } json_fns[] = {
        {"json_new", 8, 0, NULL, NULL, NULL, json_obj_type},
        {"json_set_string", 15, 3, json_obj_type, builtin_string, builtin_string, builtin_void},
        {"json_set_int", 12, 3, json_obj_type, builtin_string, builtin_int, builtin_void},
        {"json_get_string", 15, 2, json_obj_type, builtin_string, NULL, builtin_string},
        {"json_get_int", 12, 2, json_obj_type, builtin_string, NULL, builtin_int},
        {"json_stringify", 14, 1, json_obj_type, NULL, NULL, builtin_string},
        {"Regex_match", 11, 2, builtin_string, builtin_string, NULL, builtin_int},
        {"Regex_replace", 13, 3, builtin_string, builtin_string, builtin_string, builtin_string},
    };
    for (int i = 0; i < 8; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = json_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (json_fns[i].p1) ft->fn_type.param_types[0] = json_fns[i].p1;
        if (json_fns[i].p2) ft->fn_type.param_types[1] = json_fns[i].p2;
        if (json_fns[i].p3) ft->fn_type.param_types[2] = json_fns[i].p3;
        ft->fn_type.return_type = json_fns[i].ret;
        Token tok = {TOKEN_IDENT, json_fns[i].name, json_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Register namespace identifiers so checker doesn't reject File.read() etc.
    // Also register their methods with proper return types
    const char* namespaces[] = {"File", "Path", "DateTime", "Json", "Http", "HashMap", "HashSet", "Regex", "System", "Terminal", "Test", "Math", "Env", "Net", "Url", "Task", NULL};
    for (int i = 0; namespaces[i]; i++) {
        Token ns_tok = {TOKEN_IDENT, namespaces[i], (int)strlen(namespaces[i]), 0};
        if (!find_symbol(global_scope, ns_tok)) {
            add_symbol(global_scope, ns_tok, builtin_int, false);
        }
    }

    // File namespace methods
    struct { const char* name; int nlen; int pc; Type* p1; Type* p2; Type* ret; } file_ns_fns[] = {
        {"File_read", 9, 1, builtin_string, NULL, builtin_string},
        {"File_write", 10, 2, builtin_string, builtin_string, builtin_int},
        {"File_exists", 11, 1, builtin_string, NULL, builtin_int},
        {"File_delete", 11, 1, builtin_string, NULL, builtin_int},
        {"File_copy", 9, 2, builtin_string, builtin_string, builtin_int},
        {"File_move", 9, 2, builtin_string, builtin_string, builtin_int},
    };
    for (int i = 0; i < 6; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = file_ns_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = file_ns_fns[i].p1;
        if (file_ns_fns[i].p2) ft->fn_type.param_types[1] = file_ns_fns[i].p2;
        ft->fn_type.return_type = file_ns_fns[i].ret;
        Token tok = {TOKEN_IDENT, file_ns_fns[i].name, file_ns_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // HashMap namespace: HashMap.new() -> HashMap_new()
    Type* map_type = make_type(TYPE_MAP);
    {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 0;
        ft->fn_type.param_types = NULL;
        ft->fn_type.return_type = map_type;
        Token tok = {TOKEN_IDENT, "HashMap_new", 11, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // HashSet namespace: HashSet.new() -> HashSet_new()
    Type* set_type = make_type(TYPE_SET);
    {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 0;
        ft->fn_type.param_types = NULL;
        ft->fn_type.return_type = set_type;
        Token tok = {TOKEN_IDENT, "HashSet_new", 11, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Json namespace
    Type* json_type = make_type(TYPE_MAP); // opaque pointer
    struct { const char* name; int nlen; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } json_ns_fns[] = {
        {"Json_new", 8, 0, NULL, NULL, NULL, json_type},
        {"Json_set_string", 15, 3, json_type, builtin_string, builtin_string, builtin_void},
        {"Json_set_int", 12, 3, json_type, builtin_string, builtin_int, builtin_void},
        {"Json_get_string", 15, 2, json_type, builtin_string, NULL, builtin_string},
        {"Json_get_int", 12, 2, json_type, builtin_string, NULL, builtin_int},
        {"Json_stringify", 14, 1, json_type, NULL, NULL, builtin_string},
    };
    for (int i = 0; i < 6; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = json_ns_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (json_ns_fns[i].p1) ft->fn_type.param_types[0] = json_ns_fns[i].p1;
        if (json_ns_fns[i].p2) ft->fn_type.param_types[1] = json_ns_fns[i].p2;
        if (json_ns_fns[i].p3) ft->fn_type.param_types[2] = json_ns_fns[i].p3;
        ft->fn_type.return_type = json_ns_fns[i].ret;
        Token tok = {TOKEN_IDENT, json_ns_fns[i].name, json_ns_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Http namespace
    {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 1;
        ft->fn_type.param_types = malloc(sizeof(Type*));
        ft->fn_type.param_types[0] = builtin_string;
        ft->fn_type.return_type = builtin_string;
        Token tok = {TOKEN_IDENT, "Http_get", 8, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Regex namespace
    {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 2;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = builtin_string;
        ft->fn_type.param_types[1] = builtin_string;
        ft->fn_type.return_type = builtin_int;
        Token tok = {TOKEN_IDENT, "Regex_match", 11, 0};
        add_symbol(global_scope, tok, ft, false);
    }
    {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 3;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        ft->fn_type.param_types[0] = builtin_string;
        ft->fn_type.param_types[1] = builtin_string;
        ft->fn_type.param_types[2] = builtin_string;
        ft->fn_type.return_type = builtin_string;
        Token tok = {TOKEN_IDENT, "Regex_replace", 13, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Terminal namespace
    struct { const char* name; int nlen; int pc; Type* p1; Type* p2; Type* ret; } term_fns[] = {
        {"Terminal_cols", 13, 0, NULL, NULL, builtin_int},
        {"Terminal_rows", 13, 0, NULL, NULL, builtin_int},
        {"Terminal_raw_mode", 17, 0, NULL, NULL, builtin_void},
        {"Terminal_restore", 16, 0, NULL, NULL, builtin_void},
        {"Terminal_read_key", 17, 0, NULL, NULL, builtin_int},
        {"Terminal_clear", 14, 0, NULL, NULL, builtin_void},
        {"Terminal_write", 14, 1, builtin_string, NULL, builtin_void},
        {"Terminal_move", 13, 2, builtin_int, builtin_int, builtin_void},
    };
    for (int i = 0; i < 8; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = term_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        if (term_fns[i].p1) ft->fn_type.param_types[0] = term_fns[i].p1;
        if (term_fns[i].p2) ft->fn_type.param_types[1] = term_fns[i].p2;
        ft->fn_type.return_type = term_fns[i].ret;
        Token tok = {TOKEN_IDENT, term_fns[i].name, term_fns[i].nlen, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // DateTime stdlib
    Type* dt_now_t = make_type(TYPE_FUNCTION);
    dt_now_t->fn_type.param_count = 0;
    dt_now_t->fn_type.param_types = NULL;
    dt_now_t->fn_type.return_type = builtin_int;
    Token dt_now_tok = {TOKEN_IDENT, "DateTime_now", 12, 0};
    add_symbol(global_scope, dt_now_tok, dt_now_t, false);
    
    Type* dt_format_t = make_type(TYPE_FUNCTION);
    dt_format_t->fn_type.param_count = 2;
    dt_format_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    dt_format_t->fn_type.param_types[0] = builtin_int;
    dt_format_t->fn_type.param_types[1] = builtin_string;
    dt_format_t->fn_type.return_type = builtin_string;
    Token dt_format_tok = {TOKEN_IDENT, "DateTime_format", 15, 0};
    add_symbol(global_scope, dt_format_tok, dt_format_t, false);
    
    Type* dt_sleep_t = make_type(TYPE_FUNCTION);
    dt_sleep_t->fn_type.param_count = 1;
    dt_sleep_t->fn_type.param_types = malloc(sizeof(Type*));
    dt_sleep_t->fn_type.param_types[0] = builtin_int;
    dt_sleep_t->fn_type.return_type = builtin_void;
    Token dt_sleep_tok = {TOKEN_IDENT, "DateTime_sleep", 14, 0};
    add_symbol(global_scope, dt_sleep_tok, dt_sleep_t, false);
}

Symbol* find_symbol(SymbolTable* scope, Token name) {
    for (int i = 0; i < scope->count; i++) {
        if (scope->symbols[i].name.length == name.length &&
            memcmp(scope->symbols[i].name.start, name.start, name.length) == 0) {
            return &scope->symbols[i];
        }
    }
    if (scope->parent) return find_symbol(scope->parent, name);
    return NULL;
}

// T1.5.3: Function overloading support
static void add_function_overload(SymbolTable* scope, Token name, Type* type, bool is_mutable) {
    // Check if function with this name already exists
    Symbol* existing = find_symbol(scope, name);
    
    if (existing && existing->type->kind == TYPE_FUNCTION && type->kind == TYPE_FUNCTION) {
        // Check for exact signature match (error)
        if (signatures_match(existing->type, type)) {
            char func_name[256];
            snprintf(func_name, sizeof(func_name), "%.*s", name.length, name.start);
            type_error_mismatch("unique function signature", "duplicate signature", func_name, name.line, 0);
            return;
        }
        
        // Add to overload chain
        Symbol* current = existing;
        while (current->next_overload) {
            if (signatures_match(current->next_overload->type, type)) {
                char func_name[256];
                snprintf(func_name, sizeof(func_name), "%.*s", name.length, name.start);
                type_error_mismatch("unique function signature", "duplicate signature", func_name, name.line, 0);
                return;
            }
            current = current->next_overload;
        }
        
        // Create new overload
        current->next_overload = malloc(sizeof(Symbol));
        current->next_overload->name = name;
        current->next_overload->type = type;
        current->next_overload->is_mutable = is_mutable;
        current->next_overload->next_overload = NULL;
        current->next_overload->mangled_name = generate_mangled_name(name, type);
    } else {
        // First function with this name or non-function symbol
        add_symbol(scope, name, type, is_mutable);
        if (type->kind == TYPE_FUNCTION) {
            scope->symbols[scope->count - 1].mangled_name = generate_mangled_name(name, type);
        }
    }
}

// T1.5.3: Check if two function signatures match
static bool signatures_match(Type* type1, Type* type2) {
    if (type1->kind != TYPE_FUNCTION || type2->kind != TYPE_FUNCTION) return false;
    
    FunctionType* fn1 = &type1->fn_type;
    FunctionType* fn2 = &type2->fn_type;
    
    if (fn1->param_count != fn2->param_count) return false;
    
    for (int i = 0; i < fn1->param_count; i++) {
        if (!types_equal(fn1->param_types[i], fn2->param_types[i])) {
            return false;
        }
    }
    
    return true;
}

// T1.5.3: Generate mangled name for function overloading
static char* generate_mangled_name(Token name, Type* type) {
    if (type->kind != TYPE_FUNCTION) return NULL;
    
    char* mangled = malloc(256);
    int pos = 0;
    
    // Start with function name
    pos += snprintf(mangled + pos, 256 - pos, "%.*s", name.length, name.start);
    
    // Add parameter types
    FunctionType* fn_type = &type->fn_type;
    for (int i = 0; i < fn_type->param_count; i++) {
        const char* type_name = type_to_string(fn_type->param_types[i]);
        pos += snprintf(mangled + pos, 256 - pos, "_%s", type_name);
    }
    
    return mangled;
}

// T1.5.3: Find best matching overload for function call
static Symbol* find_function_overload(SymbolTable* scope, Token name, Type** arg_types, int arg_count) {
    Symbol* symbol = find_symbol(scope, name);
    if (!symbol) {
        return NULL;
    }
    if (symbol->type->kind != TYPE_FUNCTION) {
        return symbol;
    }
    
    Symbol* best_match = NULL;
    int best_score = -1;
    
    // Check all overloads
    Symbol* current = symbol;
    while (current) {
        if (current->type->kind == TYPE_FUNCTION) {
            int score = calculate_match_score(current->type, arg_types, arg_count);
            if (score > best_score) {
                best_score = score;
                best_match = current;
            }
        }
        current = current->next_overload;
    }
    
    return best_match;
}

// T1.5.3: Calculate how well a function signature matches the arguments
static int calculate_match_score(Type* fn_type, Type** arg_types, int arg_count) {
    if (fn_type->kind != TYPE_FUNCTION) return -1;
    
    FunctionType* func = &fn_type->fn_type;
    
    // For variadic functions, allow arg_count >= param_count
    if (func->is_variadic) {
        if (arg_count < func->param_count) return -1;
    } else {
        if (func->param_count != arg_count) return -1;  // Exact count match required for non-variadic
    }
    
    int score = 0;
    // Only check types for the declared parameters
    for (int i = 0; i < func->param_count && i < arg_count; i++) {
        if (types_equal(func->param_types[i], arg_types[i])) {
            score += 10;  // Exact match
        } else if (can_convert_type(arg_types[i], func->param_types[i])) {
            score += 5;   // Convertible match
        } else {
            return -1;    // No match possible
        }
    }
    
    // For variadic functions, give a small bonus for extra args (they're allowed)
    if (func->is_variadic && arg_count > func->param_count) {
        score += 1;  // Small bonus for variadic match
    }
    
    return score;
}

// T1.5.3: Check if one type can be converted to another
static bool can_convert_type(Type* from, Type* to) {
    if (types_equal(from, to)) return true;
    
    // Allow int -> float conversion
    if (from->kind == TYPE_INT && to->kind == TYPE_FLOAT) return true;
    
    // Add more conversion rules as needed
    return false;
}

Type* check_expr(Expr* expr, SymbolTable* scope) {
    if (!expr) return NULL;
    
    // Handle generic type instantiation: HashMap<K,V>, Option<T>, etc.
    // Parser represents this as EXPR_CALL with type arguments
    if (expr->type == EXPR_CALL && expr->call.callee->type == EXPR_IDENT) {
        Token type_name = expr->call.callee->token;
        
        // Check if this is a known generic type
        if ((type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) ||
            (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) ||
            (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) ||
            (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0)) {
            
            // For now, treat all generic instantiations as their base type
            // HashMap<K,V> -> TYPE_MAP, Option<T> -> TYPE_OPTIONAL, etc.
            Type* base_type = NULL;
            if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                base_type = make_type(TYPE_MAP);
            } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                base_type = make_type(TYPE_SET);
            } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                base_type = make_type(TYPE_OPTIONAL);
            } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                base_type = make_type(TYPE_RESULT);
            }
            
            expr->expr_type = base_type;
            return base_type;
        }
    }
    
    switch (expr->type) {
        case EXPR_INT:
            expr->expr_type = builtin_int;
            return builtin_int;
        case EXPR_FLOAT:
            expr->expr_type = builtin_float;
            return builtin_float;
        case EXPR_STRING:
            expr->expr_type = builtin_string;
            return builtin_string;
        case EXPR_BOOL:
            expr->expr_type = builtin_bool;
            return builtin_bool;
        case EXPR_IDENT: {
            // Handle 'self' in extension methods
            if (current_self_type && expr->token.length == 4 && 
                memcmp(expr->token.start, "self", 4) == 0) {
                expr->expr_type = current_self_type;
                return current_self_type;
            }
            
            // Check for built-in Option/Result constants
            if (expr->token.length == 4 && memcmp(expr->token.start, "none", 4) == 0) {
                expr->expr_type = builtin_int;  // Return type is pointer to Option
                return builtin_int;
            }
            
            // Check for boolean literals
            if (expr->token.length == 4 && memcmp(expr->token.start, "true", 4) == 0) {
                expr->expr_type = builtin_int;  // Booleans are ints (1)
                return builtin_int;
            }
            if (expr->token.length == 5 && memcmp(expr->token.start, "false", 5) == 0) {
                expr->expr_type = builtin_int;  // Booleans are ints (0)
                return builtin_int;
            }
            
            // T2.5.1: Handle built-in type names
            if (expr->token.length == 3 && memcmp(expr->token.start, "int", 3) == 0) {
                expr->expr_type = builtin_int;
                return builtin_int;
            }
            if (expr->token.length == 5 && memcmp(expr->token.start, "float", 5) == 0) {
                expr->expr_type = builtin_float;
                return builtin_float;
            }
            if (expr->token.length == 6 && memcmp(expr->token.start, "string", 6) == 0) {
                expr->expr_type = builtin_string;
                return builtin_string;
            }
            if (expr->token.length == 3 && memcmp(expr->token.start, "str", 3) == 0) {
                expr->expr_type = builtin_string;
                return builtin_string;
            }
            if (expr->token.length == 4 && memcmp(expr->token.start, "bool", 4) == 0) {
                expr->expr_type = builtin_bool;
                return builtin_bool;
            }
            if (expr->token.length == 4 && memcmp(expr->token.start, "void", 4) == 0) {
                expr->expr_type = builtin_void;
                return builtin_void;
            }
            
            Symbol* sym = find_symbol(scope, expr->token);
            if (!sym) {
                // Check if this is a module-qualified name (e.g., math::add or math.add)
                // If so, allow it - it will be resolved at codegen time
                bool is_qualified = false;
                for (int i = 0; i < expr->token.length - 1; i++) {
                    if (expr->token.start[i] == ':' && expr->token.start[i+1] == ':') {
                        is_qualified = true;
                        break;
                    }
                    if (expr->token.start[i] == '.') {
                        // Any identifier with a dot is assumed to be a module call
                        is_qualified = true;
                        break;
                    }
                }
                
                // Check if this might be a module alias (will be used with dot later)
                // This is a heuristic - assume short names might be module aliases
                extern const char* resolve_parser_module_alias(const char* name);
                if (!is_qualified) {
                    char name_buf[256];
                    int len = expr->token.length < 255 ? expr->token.length : 255;
                    memcpy(name_buf, expr->token.start, len);
                    name_buf[len] = '\0';
                    if (resolve_parser_module_alias(name_buf) != NULL) {
                        is_qualified = true;
                    }
                }
                
                if (is_qualified) {
                    // Module-qualified name - assume it's valid
                    expr->expr_type = builtin_int;  // Default type
                    return builtin_int;
                }
                
                fprintf(stderr, "\nError at line %d: Undefined variable '%.*s'\n", 
                        expr->token.line, expr->token.length, expr->token.start);
                
                // Extract variable name for enhanced error reporting
                char var_name[256];
                snprintf(var_name, sizeof(var_name), "%.*s", expr->token.length, expr->token.start);
                type_error_undefined_variable(var_name, expr->token.line, 0);
                
                // Suggest similar names
                fprintf(stderr, "  Available variables in scope:\n");
                int suggestions = 0;
                for (int i = 0; i < scope->count && suggestions < 3; i++) {
                    fprintf(stderr, "    - %.*s\n",
                            scope->symbols[i].name.length, scope->symbols[i].name.start);
                    suggestions++;
                }
                if (suggestions == 0) {
                    fprintf(stderr, "    (none)\n");
                }
                
                had_error = true;
                return NULL;
            }
            mark_used(scope, expr->token);
            expr->expr_type = sym->type;  // Store type in AST
            return sym->type;
        }
        case EXPR_BINARY: {
            Type* left = check_expr(expr->binary.left, scope);
            Type* right = check_expr(expr->binary.right, scope);
            
            if (!left || !right) return NULL;
            
            // Nil coalescing operator ??
            if (expr->binary.op.type == TOKEN_QUESTION_QUESTION) {
                // Left should be Option<T>, right should be T
                // Return type is T
                expr->expr_type = right;
                return right;
            }
            
            // Allow bool operations (both && and 'and', || and 'or')
            if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_OR ||
                expr->binary.op.type == TOKEN_AMPAMP || expr->binary.op.type == TOKEN_PIPEPIPE) {
                // Both sides should be bool-compatible (bool or int)
                // In C, int and bool are interchangeable in boolean context
                bool left_ok = (left->kind == TYPE_BOOL || left->kind == TYPE_INT);
                bool right_ok = (right->kind == TYPE_BOOL || right->kind == TYPE_INT);
                
                if (!left_ok || !right_ok) {
                    fprintf(stderr, "Error at line %d: Boolean operation requires bool or int operands\n", 
                            expr->binary.op.line);
                    had_error = true;
                    return NULL;
                }
                // Return int (which works as bool in C)
                expr->expr_type = builtin_int;
                return builtin_int;
            }
            
            // Comparison operators return bool
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ) {
                // Allow comparing compatible types
                // Int, bool, and enum are all compatible for comparison
                bool types_compatible = (left->kind == right->kind) ||
                                       (left->kind == TYPE_INT && right->kind == TYPE_BOOL) ||
                                       (left->kind == TYPE_BOOL && right->kind == TYPE_INT) ||
                                       (left->kind == TYPE_ENUM && right->kind == TYPE_INT) ||
                                       (left->kind == TYPE_INT && right->kind == TYPE_ENUM) ||
                                       (left->kind == TYPE_ENUM && right->kind == TYPE_ENUM);
                
                if (!types_compatible) {
                    fprintf(stderr, "Error at line %d: Cannot compare different types\n", 
                            expr->binary.op.line);
                    had_error = true;
                    return NULL;
                }
                expr->expr_type = builtin_int;  // Return int (works as bool)
                return builtin_int;
            }
            
            // Allow string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS) {
                // Allow string + string, string + int, int + string
                bool left_is_string = (left->kind == TYPE_STRING);
                bool right_is_string = (right->kind == TYPE_STRING);
                bool left_is_int = (left->kind == TYPE_INT);
                bool right_is_int = (right->kind == TYPE_INT);
                
                if ((left_is_string && right_is_string) ||
                    (left_is_string && right_is_int) ||
                    (left_is_int && right_is_string)) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
            }
            
            if (left->kind != right->kind) {
                // Use enhanced error reporting with detailed type information
                char left_type[256], right_type[256];
                snprintf(left_type, sizeof(left_type), "%s", type_to_string(left));
                snprintf(right_type, sizeof(right_type), "%s", type_to_string(right));
                
                type_error_mismatch(left_type, right_type, "binary expression", 
                                  expr->binary.op.line, 0);
                had_error = true;
                return NULL;
            }
            expr->expr_type = left;
            return left;
        }
        case EXPR_CALL: {
            // T3.1.1: Enhanced generic function call handling
            if (expr->call.callee->type == EXPR_IDENT) {
                Token func_name = expr->call.callee->token;
                
                // Check for built-in Option/Result constructors
                char name_buf[256];
                int name_len = func_name.length < 255 ? func_name.length : 255;
                strncpy(name_buf, func_name.start, name_len);
                name_buf[name_len] = '\0';
                
                if (strcmp(name_buf, "some") == 0 || strcmp(name_buf, "ok") == 0) {
                    // some(value) or ok(value) - returns Option<T> or Result<T, E>
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: '%s' expects 1 argument, got %d\n",
                                func_name.line, name_buf, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    Type* arg_type = check_expr(expr->call.args[0], scope);
                    expr->expr_type = arg_type;  // Return type is pointer to Option/Result
                    return arg_type;
                } else if (strcmp(name_buf, "none") == 0) {
                    // none() - returns Option<T>
                    if (expr->call.arg_count != 0) {
                        fprintf(stderr, "Error at line %d: 'none' expects 0 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    expr->expr_type = builtin_int;  // Return type is pointer to Option
                    return builtin_int;
                } else if (strcmp(name_buf, "err") == 0) {
                    // err(error) - returns Result<T, E>
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'err' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    Type* arg_type = check_expr(expr->call.args[0], scope);
                    expr->expr_type = arg_type;  // Return type is pointer to Result
                    return arg_type;
                } else if (strcmp(name_buf, "file_write") == 0 || strcmp(name_buf, "file_append") == 0) {
                    // file_write(path, content) or file_append(path, content) - returns int
                    if (expr->call.arg_count != 2) {
                        fprintf(stderr, "Error at line %d: '%s' expects 2 arguments, got %d\n",
                                func_name.line, name_buf, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    check_expr(expr->call.args[1], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "file_exists") == 0) {
                    // file_exists(path) - returns int
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: '%s' expects 1 argument, got %d\n",
                                func_name.line, name_buf, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "print") == 0 || strcmp(name_buf, "println") == 0) {
                    // print(value) or println(value) - accepts any number of arguments
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_void;
                    return builtin_void;
                } else if (strcmp(name_buf, "assert") == 0) {
                    // assert(condition) or assert(condition, message)
                    if (expr->call.arg_count < 1 || expr->call.arg_count > 2) {
                        fprintf(stderr, "Error at line %d: 'assert' expects 1 or 2 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_void;
                    }
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_void;
                    return builtin_void;
                }
                
                // Math functions
                if (strcmp(name_buf, "min") == 0 || strcmp(name_buf, "max") == 0) {
                    if (expr->call.arg_count != 2) {
                        fprintf(stderr, "Error at line %d: '%s' expects 2 arguments, got %d\n",
                                func_name.line, name_buf, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    check_expr(expr->call.args[1], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "abs") == 0) {
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'abs' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "len") == 0) {
                    // len(array) or len(string) - returns int
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'len' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "typeof") == 0) {
                    // typeof(value) - returns string
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'typeof' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_string;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
                
                // Utility functions
                if (strcmp(name_buf, "exit") == 0) {
                    // exit(code) - exits program
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'exit' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_void;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_void;
                    return builtin_void;
                } else if (strcmp(name_buf, "panic") == 0) {
                    // panic(message) - panic with message
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'panic' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_void;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_void;
                    return builtin_void;
                } else if (strcmp(name_buf, "sleep") == 0) {
                    // sleep(ms) - sleep for milliseconds
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'sleep' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_void;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_void;
                    return builtin_void;
                } else if (strcmp(name_buf, "rand") == 0) {
                    // rand() - random number
                    if (expr->call.arg_count != 0) {
                        fprintf(stderr, "Error at line %d: 'rand' expects 0 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "time_now") == 0) {
                    // time_now() - current time in seconds
                    if (expr->call.arg_count != 0) {
                        fprintf(stderr, "Error at line %d: 'time_now' expects 0 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "system") == 0) {
                    // system(cmd) - run shell command
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'system' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                }
                
                // String functions
                if (strcmp(name_buf, "str_concat") == 0) {
                    // str_concat(s1, s2) - concatenate strings
                    if (expr->call.arg_count != 2) {
                        fprintf(stderr, "Error at line %d: 'str_concat' expects 2 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_string;
                    }
                    check_expr(expr->call.args[0], scope);
                    check_expr(expr->call.args[1], scope);
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "str_contains") == 0) {
                    // str_contains(haystack, needle) - check if string contains substring
                    if (expr->call.arg_count != 2) {
                        fprintf(stderr, "Error at line %d: 'str_contains' expects 2 arguments, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    check_expr(expr->call.args[0], scope);
                    check_expr(expr->call.args[1], scope);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "str_upper") == 0) {
                    // str_upper(s) - convert to uppercase
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'str_upper' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_string;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "str_lower") == 0) {
                    // str_lower(s) - convert to lowercase
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'str_lower' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_string;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "wyn_str_substring") == 0) {
                    // wyn_str_substring(s, start, end) - returns substring
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "str_eq") == 0 || strcmp(name_buf, "string_length") == 0 ||
                           strcmp(name_buf, "str_len") == 0) {
                    // str_eq(a, b), string_length(s), str_len(s) - returns int
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(name_buf, "str_concat") == 0) {
                    // str_concat(a, b) - returns string
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "int_to_str") == 0) {
                    // int_to_str(n) - returns string
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "file_read") == 0) {
                    // file_read(path) - returns string
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "char_at") == 0 || strcmp(name_buf, "string_char_at") == 0) {
                    // char_at(s, index) - returns string (single char)
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "split_get") == 0) {
                    // split_get(s, delim, index) - returns string
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(name_buf, "str_upper") == 0 || strcmp(name_buf, "str_lower") == 0 ||
                           strcmp(name_buf, "str_trim") == 0 || strcmp(name_buf, "str_repeat") == 0 ||
                           strcmp(name_buf, "str_reverse") == 0 || strcmp(name_buf, "str_replace") == 0) {
                    // String transformation functions - return string
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
                
                // Check if this is a generic function call
                if (wyn_is_generic_function_call(func_name)) {
                    // Collect argument types for generic instantiation
                    Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        arg_types[i] = check_expr(expr->call.args[i], scope);
                    }
                    
                    // Infer generic type and create monomorphic instance
                    Type* return_type = wyn_infer_generic_call_type(func_name, expr->call.args, expr->call.arg_count);
                    
                    // Register this instantiation for code generation
                    // wyn_register_generic_instantiation(func_name, arg_types, expr->call.arg_count);
                    
                    expr->expr_type = return_type;
                    free(arg_types);
                    return return_type;
                }
                
                
                // T1.5.3: Function overloading support with T1.5.4: Parameter validation
                // Collect argument types for overload resolution
                Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                for (int i = 0; i < expr->call.arg_count; i++) {
                    arg_types[i] = check_expr(expr->call.args[i], scope);
                }
                
                // Find best matching overload
                Symbol* best_match = find_function_overload(scope, expr->call.callee->token, arg_types, expr->call.arg_count);
                
                // Check if this is a module-qualified function call (e.g., math::add)
                bool is_qualified = false;
                char qual_module[128] = "";
                char qual_func[128] = "";
                
                for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                    if (expr->call.callee->token.start[i] == ':' && expr->call.callee->token.start[i+1] == ':') {
                        is_qualified = true;
                        // Extract module and function names
                        int mod_len = i;
                        int func_len = expr->call.callee->token.length - i - 2;
                        snprintf(qual_module, 128, "%.*s", mod_len, expr->call.callee->token.start);
                        snprintf(qual_func, 128, "%.*s", func_len, expr->call.callee->token.start + i + 2);
                        break;
                    }
                }
                
                // Check visibility for module-qualified calls
                if (is_qualified && qual_module[0] != '\0') {
                    // Check if module name is ambiguous
                    char first_path[256], second_path[256];
                    int first_line, second_line;
                    if (is_ambiguous_module(qual_module, first_path, &first_line, second_path, &second_line)) {
                        fprintf(stderr, "Error at line %d: Ambiguous module name '%s'\n",
                                expr->call.callee->token.line, qual_module);
                        fprintf(stderr, "  Could refer to:\n");
                        fprintf(stderr, "    - %s (imported at line %d)\n", first_path, first_line);
                        fprintf(stderr, "    - %s (imported at line %d)\n", second_path, second_line);
                        fprintf(stderr, "  Use full path to disambiguate:\n");
                        
                        char c_ident1[256], c_ident2[256];
                        strcpy(c_ident1, first_path);
                        strcpy(c_ident2, second_path);
                        for (char* p = c_ident1; *p; p++) if (*p == '/') *p = '_';
                        for (char* p = c_ident2; *p; p++) if (*p == '/') *p = '_';
                        
                        fprintf(stderr, "    - %s::%s()\n", c_ident1, qual_func);
                        fprintf(stderr, "    - %s::%s()\n", c_ident2, qual_func);
                        had_error = true;
                        free(arg_types);
                        return builtin_int;
                    }
                    
                    if (!check_function_visibility(qual_module, qual_func)) {
                        fprintf(stderr, "Error at line %d: Function '%s' in module '%s' is private\n",
                                expr->call.callee->token.line, qual_func, qual_module);
                        fprintf(stderr, "  Note: Only 'pub' functions can be called from outside the module\n");
                        had_error = true;
                        free(arg_types);
                        return builtin_int;
                    }
                }
                
                if (is_qualified && !best_match) {
                    // Module-qualified function - check for known return types
                    if (strcmp(qual_module, "C_Parser") == 0) {
                        // C_Parser module functions
                        if (strcmp(qual_func, "ast_to_string") == 0) {
                            expr->expr_type = builtin_string;
                            free(arg_types);
                            return builtin_string;
                        }
                        // Other C_Parser functions return int/void
                    }
                    if (strcmp(qual_module, "HashMap") == 0) {
                        if (strcmp(qual_func, "new") == 0) {
                            Type* map_type = make_type(TYPE_MAP);
                            expr->expr_type = map_type;
                            free(arg_types);
                            return map_type;
                        }
                    }
                    expr->expr_type = builtin_int;  // Default return type
                    free(arg_types);
                    return builtin_int;
                }
                
                if (best_match && best_match->type->kind == TYPE_FUNCTION) {
                    // T1.5.4: Validate the function call with detailed parameter checking
                    ValidationResult validation = wyn_validate_function_call(best_match, expr->call.args, expr->call.arg_count, scope);
                    
                    if (validation != VALIDATION_SUCCESS) {
                        char func_name[256];
                        snprintf(func_name, sizeof(func_name), "%.*s", 
                                expr->call.callee->token.length, expr->call.callee->token.start);
                        fprintf(stderr, "Error: Function call validation failed for '%s': %s\n",
                                func_name, wyn_validation_error_message(validation));
                        had_error = true;
                    }
                    
                    // Store the selected overload for code generation
                    expr->call.selected_overload = (void*)best_match;
                    expr->expr_type = best_match->type->fn_type.return_type;
                    free(arg_types);
                    return best_match->type->fn_type.return_type;
                } else if (!best_match) {
                    char func_name[256];
                    snprintf(func_name, sizeof(func_name), "%.*s", 
                            expr->call.callee->token.length, expr->call.callee->token.start);
                    
                    // Search scope for similar names (typo detection)
                    int min_dist = 999;
                    char closest[256] = {0};
                    SymbolTable* s = scope;
                    while (s) {
                        for (int si = 0; si < s->count; si++) {
                            char sn[256];
                            int sl = s->symbols[si].name.length < 255 ? s->symbols[si].name.length : 255;
                            memcpy(sn, s->symbols[si].name.start, sl);
                            sn[sl] = '\0';
                            // Simple distance: count differing chars
                            int fl = strlen(func_name);
                            int diff = abs(fl - sl);
                            if (diff <= 2 && sl > 1) {
                                int match_chars = 0;
                                int ml = fl < sl ? fl : sl;
                                for (int ci = 0; ci < ml; ci++) {
                                    if (func_name[ci] == sn[ci]) match_chars++;
                                }
                                int d = (ml - match_chars) + diff;
                                if (d < min_dist && d <= 2 && d > 0) {
                                    min_dist = d;
                                    strcpy(closest, sn);
                                }
                            }
                        }
                        s = s->parent;
                    }
                    if (closest[0]) {
                        fprintf(stderr, "\n  Did you mean: %s?\n", closest);
                    }
                    
                    type_error_undefined_function(func_name, expr->call.callee->token.line, 0);
                    had_error = true;
                }
                
                free(arg_types);
            }
            
            // Fallback to original logic for non-identifier callees with T1.5.4 validation
            Type* callee_type = check_expr(expr->call.callee, scope);
            
            // T1.5.4: Enhanced parameter validation for function types
            if (callee_type && callee_type->kind == TYPE_FUNCTION) {
                // For variadic functions, allow any number of arguments >= param_count
                if (callee_type->fn_type.is_variadic) {
                    if (expr->call.arg_count < callee_type->fn_type.param_count) {
                        char func_name[256];
                        snprintf(func_name, sizeof(func_name), "%.*s", 
                                expr->call.callee->token.length, expr->call.callee->token.start);
                        type_error_wrong_arg_count(func_name, callee_type->fn_type.param_count, 
                                                  expr->call.arg_count, expr->call.callee->token.line, 0);
                        had_error = true;
                    }
                } else if (expr->call.arg_count != callee_type->fn_type.param_count) {
                    char func_name[256];
                    snprintf(func_name, sizeof(func_name), "%.*s", 
                            expr->call.callee->token.length, expr->call.callee->token.start);
                    type_error_wrong_arg_count(func_name, callee_type->fn_type.param_count, 
                                              expr->call.arg_count, expr->call.callee->token.line, 0);
                    had_error = true;
                }
                
                // Check type compatibility for each argument (only for non-variadic params)
                int params_to_check = callee_type->fn_type.param_count;
                for (int i = 0; i < expr->call.arg_count && i < params_to_check; i++) {
                    Type* expected_type = callee_type->fn_type.param_types[i];
                    Type* actual_type = check_expr(expr->call.args[i], scope);
                    
                    if (!wyn_is_type_compatible(expected_type, actual_type)) {
                        char expected_str[256], actual_str[256], context[256];
                        snprintf(expected_str, sizeof(expected_str), "%s", type_to_string(expected_type));
                        snprintf(actual_str, sizeof(actual_str), "%s", type_to_string(actual_type));
                        snprintf(context, sizeof(context), "argument %d", i + 1);
                        
                        type_error_mismatch(expected_str, actual_str, context, 
                                          expr->call.args[i]->token.line, 0);
                        had_error = true;
                    }
                }
            }
            
            // Check all arguments
            for (int i = 0; i < expr->call.arg_count; i++) {
                check_expr(expr->call.args[i], scope);
            }
            
            if (callee_type && callee_type->kind == TYPE_FUNCTION) {
                expr->expr_type = callee_type->fn_type.return_type;
                return callee_type->fn_type.return_type;
            }
            expr->expr_type = builtin_int;
            return builtin_int;
        }
        case EXPR_METHOD_CALL: {
            Type* object_type = check_expr(expr->method_call.object, scope);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                check_expr(expr->method_call.args[i], scope);
            }
            
            Token method = expr->method_call.method;
            char method_name[256];
            int len = method.length < 255 ? method.length : 255;
            memcpy(method_name, method.start, len);
            method_name[len] = '\0';
            
            // Check for namespace method calls: File.read() -> File_read
            // Only for known namespaces, not regular variables
            if (expr->method_call.object->type == EXPR_IDENT) {
                char obj_name[256];
                snprintf(obj_name, sizeof(obj_name), "%.*s",
                    expr->method_call.object->token.length,
                    expr->method_call.object->token.start);
                // Only treat as namespace if it's a known builtin module
                extern bool is_builtin_module(const char* name);
                if (is_builtin_module(obj_name)) {
                    char ns_method[256];
                    snprintf(ns_method, sizeof(ns_method), "%s_%s", obj_name, method_name);
                    Token ns_tok = {TOKEN_IDENT, ns_method, (int)strlen(ns_method), 0};
                    Symbol* ns_sym = find_symbol(global_scope, ns_tok);
                    if (ns_sym && ns_sym->type && ns_sym->type->kind == TYPE_FUNCTION) {
                        Type* ret = ns_sym->type->fn_type.return_type;
                        if (ret) {
                            expr->expr_type = ret;
                            return ret;
                        }
                    }
                }
            }
            
            // Handle string methods
            if (object_type && object_type->kind == TYPE_STRING) {
                if (strcmp(method_name, "contains") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "upper") == 0 || strcmp(method_name, "lower") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "len") == 0 || strcmp(method_name, "length") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "starts_with") == 0 || strcmp(method_name, "ends_with") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "trim") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "replace") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "substring") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "split_at") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "repeat") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "index_of") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "split_count") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "to_int") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "to_float") == 0) {
                    expr->expr_type = builtin_float;
                    return builtin_float;
                }
            }
            
            // Handle int methods
            if (object_type == builtin_int) {
                if (strcmp(method_name, "abs") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "to_string") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "min") == 0 || strcmp(method_name, "max") == 0) {
                    expr->expr_type = builtin_int;
                    return builtin_int;
                }
            }
            
            // Handle bool methods
            if (object_type == builtin_bool || object_type == builtin_int) {
                if (strcmp(method_name, "to_string") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
            }
            
            // Special handling for array.get() - return element type
            if (object_type && object_type->kind == TYPE_ARRAY) {
                if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                    // Return the element type if known
                    if (object_type->array_type.element_type) {
                        expr->expr_type = object_type->array_type.element_type;
                        return object_type->array_type.element_type;
                    }
                }
            }
            
            // Use method signature table for type inference (Phase 1)
            const char* receiver_type = get_receiver_type_string(object_type);
            if (receiver_type) {
                memcpy(method_name, method.start, len);
                method_name[len] = '\0';
                
                const char* return_type_str = lookup_method_return_type(receiver_type, method_name);
                if (return_type_str) {
                    // Map return type string to Type*
                    if (strcmp(return_type_str, "string") == 0) {
                        expr->expr_type = builtin_string;
                        return builtin_string;
                    } else if (strcmp(return_type_str, "int") == 0) {
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    } else if (strcmp(return_type_str, "float") == 0) {
                        expr->expr_type = builtin_float;
                        return builtin_float;
                    } else if (strcmp(return_type_str, "bool") == 0) {
                        expr->expr_type = builtin_bool;
                        return builtin_bool;
                    } else if (strcmp(return_type_str, "array") == 0) {
                        // Check if this is a string method that returns string array
                        if (object_type && object_type->kind == TYPE_STRING) {
                            if (strcmp(method_name, "split") == 0 ||
                                strcmp(method_name, "chars") == 0 ||
                                strcmp(method_name, "words") == 0 ||
                                strcmp(method_name, "lines") == 0) {
                                Type* string_array = make_type(TYPE_ARRAY);
                                string_array->array_type.element_type = builtin_string;
                                expr->expr_type = string_array;
                                return string_array;
                            }
                        }
                        expr->expr_type = builtin_array;
                        return builtin_array;
                    } else if (strcmp(return_type_str, "json") == 0) {
                        Type* json_type = make_type(TYPE_JSON);
                        expr->expr_type = json_type;
                        return json_type;
                    } else if (strcmp(return_type_str, "void") == 0) {
                        expr->expr_type = builtin_void;
                        return builtin_void;
                    }
                }
            }
            
            // Look up user-defined extension methods: Type_method in symbol table
            if (object_type && object_type->kind == TYPE_STRUCT) {
                Token type_name = object_type->struct_type.name;
                char ext_fn_name[256];
                snprintf(ext_fn_name, sizeof(ext_fn_name), "%.*s_%.*s",
                        type_name.length, type_name.start,
                        (int)method.length, method.start);
                Token ext_tok = {TOKEN_IDENT, ext_fn_name, (int)strlen(ext_fn_name), 0};
                Symbol* ext_sym = find_symbol(global_scope, ext_tok);
                if (ext_sym && ext_sym->type && ext_sym->type->kind == TYPE_FUNCTION) {
                    Type* ret = ext_sym->type->fn_type.return_type;
                    if (ret) {
                        expr->expr_type = ret;
                        return ret;
                    }
                }
            }

            // Fallback to int
            expr->expr_type = builtin_int;
            return builtin_int;
        }
        case EXPR_ARRAY: {
            // Check array elements and ensure type consistency
            if (expr->array.count > 0) {
                Type* element_type = check_expr(expr->array.elements[0], scope);
                
                // Check all elements have the same type
                for (int i = 1; i < expr->array.count; i++) {
                    Type* elem_type = check_expr(expr->array.elements[i], scope);
                    if (elem_type && element_type && elem_type->kind != element_type->kind) {
                        fprintf(stderr, "Error: Array elements must have consistent types\n");
                        had_error = true;
                        return NULL;
                    }
                }
                
                // Create array type with element type tracking
                Type* array_type = make_type(TYPE_ARRAY);
                array_type->array_type.element_type = element_type;
                expr->expr_type = array_type;
                return array_type;
            }
            
            expr->expr_type = builtin_array;
            return builtin_array;
        }
        case EXPR_HASHMAP_LITERAL: {
            // v1.3.0: {} creates a hashmap with proper type
            Type* map_type = make_type(TYPE_MAP);
            expr->expr_type = map_type;
            return map_type;
        }
        case EXPR_HASHSET_LITERAL: {
            // v1.3.1: {:} creates a hashset with TYPE_SET
            Type* set_type = make_type(TYPE_SET);
            expr->expr_type = set_type;
            return set_type;
        }
        case EXPR_INDEX: {
            Type* array_type = check_expr(expr->index.array, scope);
            Type* idx_type = check_expr(expr->index.index, scope);
            
            // Check if this is string indexing
            if (array_type && array_type->kind == TYPE_STRING) {
                if (idx_type && idx_type->kind != TYPE_INT) {
                    fprintf(stderr, "Error: String index must be int\n");
                    return NULL;
                }
                expr->expr_type = builtin_string; // Return single-char string
                return builtin_string;
            }
            
            // Allow string indices for maps, int indices for arrays
            if (array_type && array_type->kind == TYPE_MAP) {
                // Map indexing - allow string keys
                if (idx_type && idx_type->kind != TYPE_STRING) {
                    fprintf(stderr, "Error: Map index must be string\n");
                    return NULL;
                }
                expr->expr_type = builtin_int; // Map value type (simplified)
                return builtin_int;
            } else {
                // Array indexing - require int indices
                if (idx_type && idx_type->kind != TYPE_INT) {
                    fprintf(stderr, "Error: Array index must be int\n");
                    return NULL;
                }
                
                // Try to infer element type from array source
                // Check if array came from a function that returns string array
                if (expr->index.array->type == EXPR_CALL) {
                    Token callee = expr->index.array->call.callee->token;
                    // System::args returns string array
                    if (callee.length == 12 && memcmp(callee.start, "System::args", 12) == 0) {
                        expr->expr_type = builtin_string;
                        expr->expr_type = builtin_string;
                        return builtin_string;
                    }
                    // File::list_dir returns string array
                    if (callee.length == 14 && memcmp(callee.start, "File::list_dir", 14) == 0) {
                        expr->expr_type = builtin_string;
                        return builtin_string;
                    }
                }
                
                // Check if indexing a variable - look up its source
                if (expr->index.array->type == EXPR_IDENT) {
                    // For now, use heuristic based on variable name
                    Token var_name = expr->index.array->token;
                    if ((var_name.length == 4 && memcmp(var_name.start, "args", 4) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "files", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "names", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "parts", 5) == 0) ||
                        (var_name.length == 7 && memcmp(var_name.start, "entries", 7) == 0)) {
                        expr->expr_type = builtin_string;
                        return builtin_string;
                    }
                }
                
                // Check if array came from string method that returns string array
                if (expr->index.array->type == EXPR_METHOD_CALL) {
                    Token method = expr->index.array->method_call.method;
                    if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "lines", 5) == 0)) {
                        expr->expr_type = builtin_string;
                        return builtin_string;
                    }
                }
                
                // Check array literals - if first element is string, assume string array
                if (expr->index.array->type == EXPR_ARRAY) {
                    if (expr->index.array->array.count > 0) {
                        Type* first_type = check_expr(expr->index.array->array.elements[0], scope);
                        if (first_type && first_type->kind == TYPE_STRING) {
                            expr->expr_type = builtin_string;
                            return builtin_string;
                        }
                    }
                }
                
                // Check if array has tracked element type from type annotation
                if (array_type && array_type->kind == TYPE_ARRAY && array_type->array_type.element_type) {
                    expr->expr_type = array_type->array_type.element_type;
                    return array_type->array_type.element_type;
                }
                
                // Default to int for unknown arrays
                expr->expr_type = builtin_int;
                return builtin_int;
            }
        }
        case EXPR_ASSIGN: {
            Symbol* sym = find_symbol(scope, expr->assign.name);
            if (!sym) {
                fprintf(stderr, "Error: Undefined variable '%.*s'\n",
                        expr->assign.name.length, expr->assign.name.start);
                return NULL;
            }
            Type* val_type = check_expr(expr->assign.value, scope);
            
            // T2.5.1: Null safety enforcement
            if (val_type && sym->type) {
                // Check if assigning optional to non-optional
                if (!is_optional_type(sym->type) && is_optional_type(val_type)) {
                    fprintf(stderr, "Error: Cannot assign optional type to non-optional variable '%.*s'\n",
                            expr->assign.name.length, expr->assign.name.start);
                    had_error = true;
                    return NULL;
                }
                // Check type compatibility (considering optionality)
                Type* sym_inner = get_inner_type(sym->type);
                Type* val_inner = get_inner_type(val_type);
                if (sym_inner->kind != val_inner->kind) {
                    fprintf(stderr, "Error: Type mismatch in assignment\n");
                    return NULL;
                }
            }
            
            return sym->type;
        }
        case EXPR_PIPELINE: {
            // Check all stages
            for (int i = 0; i < expr->pipeline.stage_count; i++) {
                check_expr(expr->pipeline.stages[i], scope);
            }
            return builtin_int;
        }
        case EXPR_IF_EXPR: {
            check_expr(expr->if_expr.condition, scope);
            Type* then_type = NULL;
            Type* else_type = NULL;
            if (expr->if_expr.then_expr) {
                then_type = check_expr(expr->if_expr.then_expr, scope);
            }
            if (expr->if_expr.else_expr) {
                else_type = check_expr(expr->if_expr.else_expr, scope);
            }
            // Return the type of the then branch (or else if no then)
            if (then_type) {
                expr->expr_type = then_type;
                return then_type;
            }
            if (else_type) {
                expr->expr_type = else_type;
                return else_type;
            }
            return builtin_int;
        }
        case EXPR_STRING_INTERP:
            return builtin_string;
        case EXPR_RANGE:
            return builtin_int; // Range type
        case EXPR_LAMBDA: {
            // TASK-040: Lambda expression type checking and capture analysis
            
            // Create new scope for lambda parameters
            SymbolTable lambda_scope = {0};
            lambda_scope.parent = scope;
            
            // Add parameters to lambda scope
            for (int i = 0; i < expr->lambda.param_count; i++) {
                add_symbol(&lambda_scope, expr->lambda.params[i], builtin_int, false);
            }
            
            // Check lambda body in the new scope
            Type* body_type = check_expr(expr->lambda.body, &lambda_scope);
            if (!body_type) {
                had_error = true;
                return NULL;
            }
            
            // Perform capture analysis - find free variables in lambda body
            analyze_lambda_captures(&expr->lambda, expr->lambda.body, scope);
            
            // Create function type for lambda
            Type* lambda_type = make_type(TYPE_FUNCTION);
            lambda_type->fn_type.param_count = expr->lambda.param_count;
            lambda_type->fn_type.param_types = malloc(sizeof(Type*) * expr->lambda.param_count);
            
            // For now, assume all parameters are int (simplified)
            for (int i = 0; i < expr->lambda.param_count; i++) {
                lambda_type->fn_type.param_types[i] = builtin_int;
            }
            
            lambda_type->fn_type.return_type = body_type;
            expr->expr_type = lambda_type;
            return lambda_type;
        }
        case EXPR_MAP: {
            // Create a proper map type
            Type* map_type = make_type(TYPE_MAP);
            map_type->map_type.key_type = builtin_string;   // For now, assume string keys
            map_type->map_type.value_type = builtin_int;    // For now, assume int values
            
            // Check all keys and values
            for (int i = 0; i < expr->map.count; i++) {
                check_expr(expr->map.keys[i], scope);
                check_expr(expr->map.values[i], scope);
            }
            return map_type;
        }
        case EXPR_TUPLE: {
            // Check all tuple elements
            for (int i = 0; i < expr->tuple.count; i++) {
                check_expr(expr->tuple.elements[i], scope);
            }
            return builtin_int; // Tuple type (simplified for now)
        }
        case EXPR_TUPLE_INDEX: {
            // Check tuple and return element type
            check_expr(expr->tuple_index.tuple, scope);
            return builtin_int; // Element type (simplified for now)
        }
        case EXPR_FIELD_ACCESS: {
            // Handle enum member access and module.function access
            Type* object_type = check_expr(expr->field_access.object, scope);  // Validate object and get type
            
            // Check if this is enum member access (EnumName.MEMBER)
            if (expr->field_access.object->type == EXPR_IDENT) {
                Token enum_name = expr->field_access.object->token;
                Token member_name = expr->field_access.field;
                
                // Create qualified name to check if it exists in symbol table
                char qualified_member[128];
                snprintf(qualified_member, 128, "%.*s.%.*s",
                        enum_name.length, enum_name.start,
                        member_name.length, member_name.start);
                
                Token qualified_token = {TOKEN_IDENT, qualified_member, (int)strlen(qualified_member), 0};
                Symbol* enum_member_symbol = find_symbol(global_scope, qualified_token);
                
                if (enum_member_symbol) {
                    // This is a valid enum member access
                    expr->field_access.is_enum_access = true;
                    return builtin_int; // Enum values are integers
                }
            }
            
            // Check if this is struct field access (struct_var.field)
            if (object_type && object_type->kind == TYPE_STRUCT) {
                Token struct_name = object_type->struct_type.name;
                Token field_name = expr->field_access.field;
                
                // Find the struct definition
                StructStmt* struct_def = find_struct_definition(struct_name);
                if (struct_def) {
                    // Get the field type
                    Type* field_type = get_struct_field_type(struct_def, field_name);
                    if (field_type) {
                        // Set the expr_type so codegen can use it
                        expr->expr_type = field_type;
                        return field_type;
                    }
                }
            }
            
            // Create qualified name for module lookup
            Token obj_name = expr->field_access.object->token;
            Token field_name = expr->field_access.field;
            
            char qualified_name[128];
            snprintf(qualified_name, 128, "%.*s.%.*s", 
                     obj_name.length, obj_name.start,
                     field_name.length, field_name.start);
            
            Token qualified_token = {TOKEN_IDENT, qualified_name, (int)strlen(qualified_name), 0};
            Symbol* sym = find_symbol(scope, qualified_token);
            if (sym) {
                return sym->type;
            }
            
            return builtin_int; // Default
        }
        case EXPR_INDEX_ASSIGN: {
            // Check index assignment
            check_expr(expr->index_assign.object, scope);
            check_expr(expr->index_assign.index, scope);
            check_expr(expr->index_assign.value, scope);
            return builtin_int; // Assignment returns int (simplified)
        }
        case EXPR_FIELD_ASSIGN: {
            // Check field assignment
            check_expr(expr->field_assign.object, scope);
            check_expr(expr->field_assign.value, scope);
            return builtin_int; // Assignment returns int (simplified)
        }
        case EXPR_STRUCT_INIT: {
            // Check if this is a generic struct instantiation
            Token struct_name = expr->struct_init.type_name;
            
            if (wyn_is_generic_struct(struct_name)) {
                // This is a generic struct - infer type arguments from field values
                Type** type_args = malloc(sizeof(Type*) * expr->struct_init.field_count);
                int type_arg_count = 0;
                
                // Infer types from field values
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    Type* field_type = check_expr(expr->struct_init.field_values[i], scope);
                    if (field_type && type_arg_count == 0) {
                        // Use first field type as type argument (simplified)
                        type_args[type_arg_count++] = field_type;
                    }
                }
                
                // Generate monomorphic struct name
                char monomorphic_name[256];
                wyn_generate_monomorphic_struct_name(struct_name, type_args, type_arg_count, 
                                                     monomorphic_name, sizeof(monomorphic_name));
                
                // Create struct type
                Type* struct_type = make_type(TYPE_STRUCT);
                struct_type->struct_type.name = struct_name;  // Set name in struct_type union
                expr->expr_type = struct_type;
                
                free(type_args);
                return struct_type;
            } else {
                // Regular struct initialization
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    check_expr(expr->struct_init.field_values[i], scope);
                }
                // Create struct type with name
                Type* struct_type = make_type(TYPE_STRUCT);
                struct_type->struct_type.name = struct_name;  // Set name in struct_type union
                expr->expr_type = struct_type;
                return struct_type;
            }
        }
        case EXPR_OPTIONAL_TYPE: {
            // T2.5.1: Optional Type Implementation - Type System Agent addition
            Type* inner_type = check_expr(expr->optional_type.inner_type, scope);
            if (!inner_type) return NULL;
            
            // Create optional type
            Type* optional_type = make_type(TYPE_OPTIONAL);
            optional_type->optional_type.inner_type = inner_type;
            expr->expr_type = optional_type;
            return optional_type;
        }
        case EXPR_UNION_TYPE: {
            // T2.5.2: Union Type Support - Type System Agent addition
            if (expr->union_type.type_count < 2) return NULL;
            
            // Check all union member types
            Type** member_types = malloc(sizeof(Type*) * expr->union_type.type_count);
            for (int i = 0; i < expr->union_type.type_count; i++) {
                member_types[i] = check_expr(expr->union_type.types[i], scope);
                if (!member_types[i]) {
                    free(member_types);
                    return NULL;
                }
            }
            
            // Create union type
            Type* union_type = make_type(TYPE_UNION);
            union_type->union_type.types = member_types;
            union_type->union_type.type_count = expr->union_type.type_count;
            expr->expr_type = union_type;
            return union_type;
        }
        case EXPR_OK: {
            if (!expr->option.value) {
                fprintf(stderr, "Error: Ok() requires a value\n");
                had_error = true;
                return NULL;
            }
            Type* value_type = check_expr(expr->option.value, scope);
            if (!value_type) return NULL;
            // Resolve to concrete ResultInt or ResultString
            Token concrete_name;
            if (value_type == builtin_string) {
                concrete_name = (Token){TOKEN_IDENT, "ResultString", 12, 0};
            } else {
                concrete_name = (Token){TOKEN_IDENT, "ResultInt", 9, 0};
            }
            Symbol* sym = find_symbol(global_scope, concrete_name);
            Type* result_type = sym ? sym->type : make_result_type(value_type, builtin_string);
            expr->expr_type = result_type;
            return result_type;
        }
        case EXPR_ERR: {
            if (!expr->option.value) {
                fprintf(stderr, "Error: Err() requires an error value\n");
                had_error = true;
                return NULL;
            }
            check_expr(expr->option.value, scope);
            // Default to ResultInt (Err always takes string, result type from context)
            Token concrete_name = {TOKEN_IDENT, "ResultInt", 9, 0};
            Symbol* sym = find_symbol(global_scope, concrete_name);
            Type* result_type = sym ? sym->type : make_result_type(builtin_void, builtin_string);
            expr->expr_type = result_type;
            return result_type;
        }
        case EXPR_TRY: {
            // TASK-026: ? operator for error propagation
            if (!expr->try_expr.value) {
                fprintf(stderr, "Error: ? operator requires an expression\n");
                had_error = true;
                return NULL;
            }
            
            Type* value_type = check_expr(expr->try_expr.value, scope);
            if (!value_type || !is_result_type(value_type)) {
                fprintf(stderr, "Error: ? operator can only be used on Result types\n");
                had_error = true;
                return NULL;
            }
            
            // Return the Ok type from Result<T,E>
            expr->expr_type = value_type->result_type.ok_type;
            return value_type->result_type.ok_type;
        }
        case EXPR_RESULT_TYPE: {
            // TASK-026: Result<T,E> type expression
            Type* ok_type = check_expr(expr->result_type.ok_type, scope);
            Type* err_type = check_expr(expr->result_type.err_type, scope);
            
            if (!ok_type || !err_type) return NULL;
            
            Type* result_type = make_result_type(ok_type, err_type);
            expr->expr_type = result_type;
            return result_type;
        }
        case EXPR_SOME: {
            if (!expr->option.value) {
                fprintf(stderr, "Error: Some() requires a value\n");
                had_error = true;
                return NULL;
            }
            Type* inner_type = check_expr(expr->option.value, scope);
            if (!inner_type) return NULL;
            Token concrete_name;
            if (inner_type == builtin_string) {
                concrete_name = (Token){TOKEN_IDENT, "OptionString", 12, 0};
            } else {
                concrete_name = (Token){TOKEN_IDENT, "OptionInt", 9, 0};
            }
            Symbol* sym = find_symbol(global_scope, concrete_name);
            Type* opt_type = sym ? sym->type : make_type(TYPE_OPTIONAL);
            expr->expr_type = opt_type;
            return opt_type;
        }
        case EXPR_NONE: {
            // Default to OptionInt - context would refine this
            Token concrete_name = {TOKEN_IDENT, "OptionInt", 9, 0};
            Symbol* sym = find_symbol(global_scope, concrete_name);
            Type* opt_type = sym ? sym->type : make_type(TYPE_OPTIONAL);
            expr->expr_type = opt_type;
            return opt_type;
        }
        case EXPR_UNARY: {
            // Type-check unary expressions (!, -, etc.)
            Type* operand_type = check_expr(expr->unary.operand, scope);
            if (!operand_type) return NULL;
            
            // For boolean NOT (!), expect bool and return bool
            if (expr->unary.op.type == TOKEN_BANG) {
                expr->expr_type = builtin_bool;
                return builtin_bool;
            }
            
            // For numeric negation (-), return the operand type
            if (expr->unary.op.type == TOKEN_MINUS) {
                expr->expr_type = operand_type;
                return operand_type;
            }
            
            // Default: return operand type
            expr->expr_type = operand_type;
            return operand_type;
        }
        case EXPR_MATCH: {
            // Type-check match expression with exhaustiveness checking
            Type* match_value_type = check_expr(expr->match.value, scope);
            if (!match_value_type) return NULL;
            
            Type* result_type = NULL;
            bool has_wildcard = false;
            
            // Check each match arm
            for (int i = 0; i < expr->match.arm_count; i++) {
                MatchArm* arm = &expr->match.arms[i];
                
                // Check if this is a wildcard pattern
                if (arm->pattern && arm->pattern->type == PATTERN_WILDCARD) {
                    has_wildcard = true;
                }
                
                // Create a new scope for this arm to hold pattern bindings
                SymbolTable arm_scope = {0};
                arm_scope.parent = scope;
                
                // Add pattern bindings to scope
                if (arm->pattern) {
                    Pattern* pat = arm->pattern;
                    
                    // Unwrap guard pattern
                    if (pat->type == PATTERN_GUARD) {
                        pat = pat->guard.pattern;
                    }
                    
                    if (pat->type == PATTERN_IDENT) {
                        // Simple variable binding
                        add_symbol(&arm_scope, pat->ident.name, match_value_type, false);
                    } else if (pat->type == PATTERN_STRUCT) {
                        // Struct destructuring - bind each field
                        for (int j = 0; j < pat->struct_pat.field_count; j++) {
                            Token field_name = pat->struct_pat.field_names[j];
                            // Get field type from struct
                            Type* field_type = builtin_int; // Default to int
                            if (match_value_type->kind == TYPE_STRUCT) {
                                for (int k = 0; k < match_value_type->struct_type.field_count; k++) {
                                    if (match_value_type->struct_type.field_names[k].length == field_name.length &&
                                        memcmp(match_value_type->struct_type.field_names[k].start, field_name.start, field_name.length) == 0) {
                                        field_type = match_value_type->struct_type.field_types[k];
                                        break;
                                    }
                                }
                            }
                            add_symbol(&arm_scope, field_name, field_type, false);
                        }
                    } else if (pat->type == PATTERN_OPTION && pat->option.inner) {
                        // Enum variant with inner pattern
                        if (pat->option.inner->type == PATTERN_IDENT) {
                            // For now, assume int type for inner value
                            // TODO: Get actual type from enum variant
                            add_symbol(&arm_scope, pat->option.inner->ident.name, builtin_int, false);
                        }
                    }
                    
                    // If this is a guard pattern, check the guard expression with bindings in scope
                    if (arm->pattern->type == PATTERN_GUARD) {
                        Type* guard_type = check_expr(arm->pattern->guard.guard, &arm_scope);
                        if (!guard_type) return NULL;
                    }
                }
                
                // Type-check the result expression with pattern bindings in scope
                Type* arm_type = check_expr(arm->result, &arm_scope);
                if (!arm_type) return NULL;
                
                // All arms must have the same type
                if (result_type == NULL) {
                    result_type = arm_type;
                } else if (!types_equal(result_type, arm_type)) {
                    fprintf(stderr, "Error: Match arms have different types\n");
                    had_error = true;
                    return NULL;
                }
            }
            
            // TODO: Check exhaustiveness for enum types
            // For now, just require a wildcard or assume exhaustive
            if (match_value_type->kind == TYPE_ENUM && !has_wildcard) {
                // Simplified: just warn, don't error
                // fprintf(stderr, "Warning: Match may not be exhaustive\n");
            }
            
            expr->expr_type = result_type ? result_type : builtin_void;
            return expr->expr_type;
        }
        case EXPR_BLOCK: {
            // Check block expression
            for (int i = 0; i < expr->block.stmt_count; i++) {
                check_stmt(expr->block.stmts[i], scope);
            }
            if (expr->block.result) {
                expr->expr_type = check_expr(expr->block.result, scope);
            } else {
                expr->expr_type = builtin_void;
            }
            return expr->expr_type;
        }
        case EXPR_FN_TYPE: {
            // Function type: fn(T1, T2) -> R
            Type* fn_type = make_type(TYPE_FUNCTION);
            fn_type->fn_type.param_count = expr->fn_type.param_count;
            fn_type->fn_type.param_types = malloc(sizeof(Type*) * expr->fn_type.param_count);
            
            for (int i = 0; i < expr->fn_type.param_count; i++) {
                fn_type->fn_type.param_types[i] = check_expr(expr->fn_type.param_types[i], scope);
            }
            
            fn_type->fn_type.return_type = check_expr(expr->fn_type.return_type, scope);
            fn_type->fn_type.is_variadic = false;
            
            expr->expr_type = fn_type;
            return fn_type;
        }
        default:
            return builtin_int;
    }
}

void check_stmt(Stmt* stmt, SymbolTable* scope) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_VAR: {
            Type* init_type = NULL;
            
            // Enhanced type inference for T2.5.4
            if (stmt->var.type) {
                // Explicit type annotation provided - convert Expr* to Type*
                if (stmt->var.type->type == EXPR_ARRAY) {
                    // Handle typed array annotation like [ASTNode], [Token], etc.
                    Type* array_type = make_type(TYPE_ARRAY);
                    if (stmt->var.type->array.count > 0) {
                        // Get element type from array type annotation
                        Expr* elem_type_expr = stmt->var.type->array.elements[0];
                        if (elem_type_expr->type == EXPR_IDENT) {
                            Token elem_type_name = elem_type_expr->token;
                            // Look up the struct type
                            StructStmt* struct_def = find_struct_definition(elem_type_name);
                            if (struct_def) {
                                Type* elem_type = make_type(TYPE_STRUCT);
                                elem_type->struct_type.name = elem_type_name;
                                array_type->array_type.element_type = elem_type;
                            } else {
                                // Try enum types
                                EnumStmt* enum_def = find_enum_definition(elem_type_name);
                                if (enum_def) {
                                    Type* elem_type = make_type(TYPE_ENUM);
                                    elem_type->name = elem_type_name;
                                    elem_type->enum_type.variants = enum_def->variants;
                                    elem_type->enum_type.variant_count = enum_def->variant_count;
                                    array_type->array_type.element_type = elem_type;
                                } else {
                                    // Try built-in types
                                    if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                        array_type->array_type.element_type = builtin_int;
                                    } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                        array_type->array_type.element_type = builtin_string;
                                    } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                        array_type->array_type.element_type = builtin_float;
                                    } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                        array_type->array_type.element_type = builtin_bool;
                                    }
                                }
                            }
                        }
                    }
                    init_type = array_type;
                } else {
                    init_type = check_expr(stmt->var.type, scope);
                }
                
                // IMPORTANT: Still check the init expression to resolve method calls
                // and propagate type information, even though we have an explicit type
                if (stmt->var.init) {
                    Type* actual_type = check_expr(stmt->var.init, scope);
                    // Task 1.1: Check type mismatch between declared and actual type
                    if (init_type && actual_type && !types_equal(init_type, actual_type)) {
                        char expected_str[128], actual_str[128];
                        snprintf(expected_str, sizeof(expected_str), "%s", type_to_string(init_type));
                        snprintf(actual_str, sizeof(actual_str), "%s", type_to_string(actual_type));
                        type_error_mismatch(expected_str, actual_str, "variable declaration",
                            stmt->var.name.line, 0);
                        had_error = true;
                    }
                }
            } else if (stmt->var.init) {
                // Always check the expression to populate expr_type
                Type* checked_type = check_expr(stmt->var.init, scope);
                
                // Try enhanced type inference
                Type* inferred_type = wyn_infer_variable_type(stmt->var.init, scope);
                
                // Use inferred type if available, otherwise use checked type
                init_type = inferred_type ? inferred_type : checked_type;
            }
            
            // T3.3.2: Handle pattern-based variable declarations
            if (stmt->var.uses_pattern && stmt->var.pattern) {
                // Process let binding with pattern matching
                if (stmt->var.init) {
                    if (!wyn_process_let_binding(stmt->var.pattern, stmt->var.init, scope)) {
                        printf("Error: Failed to process pattern in let binding\n");
                        had_error = true;
                    }
                    
                    // Check pattern completeness
                    if (init_type) {
                        wyn_check_let_pattern_completeness(stmt->var.pattern, init_type);
                    }
                } else {
                    printf("Error: Pattern-based let binding requires initialization\n");
                    had_error = true;
                }
            } else {
                // Traditional single variable declaration
                if (init_type) {
                    add_symbol(scope, stmt->var.name, init_type, !stmt->var.is_const);
                }
            }
            break;
        }
        case STMT_EXPR:
            check_expr(stmt->expr, scope);
            break;
        case STMT_RETURN:
            if (stmt->ret.value) {
                Type* return_expr_type = check_expr(stmt->ret.value, scope);
                // Validate return type matches function return type
                if (current_function_return_type && return_expr_type) {
                    // Skip type checking for Result types (allows implicit conversion)
                    if (return_expr_type->kind == TYPE_RESULT) {
                        break;
                    }
                    // Allow returning concrete Result/Option structs from generic-typed functions
                    if ((current_function_return_type->kind == TYPE_RESULT ||
                         current_function_return_type->kind == TYPE_OPTIONAL) &&
                        return_expr_type->kind == TYPE_STRUCT) {
                        break;
                    }
                    // Allow int/bool interchangeability (comparisons return int but work as bool)
                    bool types_match = (current_function_return_type->kind == return_expr_type->kind) ||
                        (current_function_return_type->kind == TYPE_BOOL && return_expr_type->kind == TYPE_INT) ||
                        (current_function_return_type->kind == TYPE_INT && return_expr_type->kind == TYPE_BOOL);
                    if (!types_match) {
                        fprintf(stderr, "Error: Return type mismatch. Expected ");
                        print_type_name(current_function_return_type);
                        fprintf(stderr, ", got ");
                        print_type_name(return_expr_type);
                        fprintf(stderr, "\n");
                        had_error = true;
                    }
                }
            }
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                check_stmt(stmt->block.stmts[i], scope);
            }
            break;
        case STMT_IF:
            check_expr(stmt->if_stmt.condition, scope);
            check_stmt(stmt->if_stmt.then_branch, scope);
            if (stmt->if_stmt.else_branch) {
                check_stmt(stmt->if_stmt.else_branch, scope);
            }
            break;
        case STMT_WHILE:
            check_expr(stmt->while_stmt.condition, scope);
            check_stmt(stmt->while_stmt.body, scope);
            break;
        case STMT_FOR: {
            if (stmt->for_stmt.init) {
                check_stmt(stmt->for_stmt.init, scope);
            }
            check_expr(stmt->for_stmt.condition, scope);
            check_expr(stmt->for_stmt.increment, scope);
            
            // For array iteration, add the loop variable to scope
            if (stmt->for_stmt.array_expr) {
                // Determine element type from array expression
                Type* array_type = check_expr(stmt->for_stmt.array_expr, scope);
                Type* elem_type = builtin_int; // default
                if (array_type && array_type->kind == TYPE_ARRAY && array_type->array_type.element_type) {
                    elem_type = array_type->array_type.element_type;
                }
                add_symbol(scope, stmt->for_stmt.loop_var, elem_type, false);
            }
            
            check_stmt(stmt->for_stmt.body, scope);
            break;
        }
        case STMT_FN: {
            // Handle function definitions inside modules
            FnStmt* fn = &stmt->fn;
            
            // Create function scope for parameter type checking
            SymbolTable fn_scope = {0};
            fn_scope.parent = scope;
            fn_scope.capacity = 32;
            fn_scope.symbols = calloc(32, sizeof(Symbol));
            fn_scope.count = 0;
            
            // Add parameters to function scope with proper types
            for (int j = 0; j < fn->param_count; j++) {
                Type* param_type = builtin_int; // default
                
                if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle typed array parameters like [Token], [ASTNode]
                        Type* array_type = make_type(TYPE_ARRAY);
                        if (fn->param_types[j]->array.count > 0) {
                            // Get element type from array type annotation
                            Expr* elem_type_expr = fn->param_types[j]->array.elements[0];
                            if (elem_type_expr->type == EXPR_IDENT) {
                                Token elem_type_name = elem_type_expr->token;
                                // Look up the struct type
                                StructStmt* struct_def = find_struct_definition(elem_type_name);
                                if (struct_def) {
                                    Type* elem_type = make_type(TYPE_STRUCT);
                                    elem_type->struct_type.name = elem_type_name;
                                    array_type->array_type.element_type = elem_type;
                                } else {
                                    // Try built-in types
                                    if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                        array_type->array_type.element_type = builtin_int;
                                    } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                        array_type->array_type.element_type = builtin_string;
                                    } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                        array_type->array_type.element_type = builtin_float;
                                    } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                        array_type->array_type.element_type = builtin_bool;
                                    }
                                }
                            }
                        }
                        param_type = array_type;
                    } else if (fn->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = fn->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = builtin_int;
                        } else if ((type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) ||
                                   (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0)) {
                            param_type = builtin_string;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = builtin_float;
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = builtin_bool;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = builtin_array;
                        }
                    }
                }
                
                add_symbol(&fn_scope, fn->params[j], param_type, true);
            }
            
            // Check function body with parameters in scope
            if (fn->body) {
                check_stmt(fn->body, &fn_scope);
            }
            
            free(fn_scope.symbols);
            break;
        }
        case STMT_CONST: {
            // Handle module-level constants
            VarStmt* const_stmt = &stmt->const_stmt;
            Type* const_type = builtin_int;
            
            if (const_stmt->init) {
                if (const_stmt->init->type == EXPR_STRING) {
                    const_type = builtin_string;
                } else if (const_stmt->init->type == EXPR_FLOAT) {
                    const_type = builtin_float;
                } else if (const_stmt->init->type == EXPR_BOOL) {
                    const_type = builtin_bool;
                } else if (const_stmt->init->type == EXPR_INT) {
                    const_type = builtin_int;
                }
            }
            
            add_symbol(scope, const_stmt->name, const_type, true);
            break;
        }
        case STMT_EXPORT:
            // Check the exported statement
            check_stmt(stmt->export.stmt, scope);
            break;
        case STMT_TRY:
            // Check try block
            check_stmt(stmt->try_stmt.try_block, scope);
            // Check catch blocks with exception variables in scope
            for (int i = 0; i < stmt->try_stmt.catch_count; i++) {
                SymbolTable catch_scope = {0};
                catch_scope.parent = scope;
                // Add exception variable to catch scope
                add_symbol(&catch_scope, stmt->try_stmt.exception_vars[i], builtin_string, false);
                check_stmt(stmt->try_stmt.catch_blocks[i], &catch_scope);
            }
            
            // Check finally block if present
            if (stmt->try_stmt.finally_block) {
                check_stmt(stmt->try_stmt.finally_block, scope);
            }
            break;
        case STMT_THROW:
            // Check the thrown expression
            if (stmt->throw_stmt.value) {
                check_expr(stmt->throw_stmt.value, scope);
            }
            break;
        case STMT_STRUCT:
            // T3.1.2: Register generic structs
            if (stmt->struct_decl.type_param_count > 0) {
                wyn_register_generic_struct(&stmt->struct_decl);
                
                // Create a new scope with type parameters for field type checking
                SymbolTable struct_scope = {0};
                struct_scope.parent = scope;
                
                // Add type parameters to the scope as type symbols
                for (int i = 0; i < stmt->struct_decl.type_param_count; i++) {
                    Type* type_param_type = make_type(TYPE_GENERIC);
                    add_symbol(&struct_scope, stmt->struct_decl.type_params[i], type_param_type, false);
                }
                
                // Check all field types with the extended scope
                for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                    check_expr(stmt->struct_decl.field_types[i], &struct_scope);
                }
            } else {
                // Non-generic struct - check field types with current scope
                for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                    check_expr(stmt->struct_decl.field_types[i], scope);
                }
            }
            
            // T2.5.3: Enhanced struct type checking with ARC integration
            // Struct type already registered in Pass 0
            break;
        case STMT_IMPL:
            // T2.5.3: Method definitions on structs
            // Register each method as an extension method
            for (int i = 0; i < stmt->impl.method_count; i++) {
                FnStmt* method = stmt->impl.methods[i];
                
                // Create function type with proper parameter count
                Type* fn_type = make_type(TYPE_FUNCTION);
                fn_type->fn_type.param_count = method->param_count;
                fn_type->fn_type.param_types = malloc(sizeof(Type*) * method->param_count);
                for (int j = 0; j < method->param_count; j++) {
                    Type* param_type = builtin_int; // default
                    
                    // Check if first parameter is 'self' - if so, use the impl type
                    if (j == 0 && method->param_count > 0 && 
                        method->params[0].length == 4 && 
                        memcmp(method->params[0].start, "self", 4) == 0) {
                        Symbol* type_symbol = find_symbol(global_scope, stmt->impl.type_name);
                        if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                            param_type = type_symbol->type;
                        }
                    } else if (method->param_types[j]) {
                        // Look up parameter type
                        if (method->param_types[j]->type == EXPR_IDENT) {
                            Token type_name = method->param_types[j]->token;
                            if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                param_type = builtin_int;
                            } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                param_type = builtin_string;
                            } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                param_type = builtin_float;
                            } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                param_type = builtin_bool;
                            } else {
                                // Check if it's a struct type
                                Symbol* type_symbol = find_symbol(global_scope, type_name);
                                if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                                    param_type = type_symbol->type;
                                }
                            }
                        }
                    }
                    
                    fn_type->fn_type.param_types[j] = param_type;
                }
                fn_type->fn_type.return_type = builtin_int; // Simplified
                
                // Register as extension method: Type_method
                char* ext_name = malloc(stmt->impl.type_name.length + 1 + method->name.length + 1);
                memcpy(ext_name, stmt->impl.type_name.start, stmt->impl.type_name.length);
                ext_name[stmt->impl.type_name.length] = '_';
                memcpy(ext_name + stmt->impl.type_name.length + 1, method->name.start, method->name.length);
                ext_name[stmt->impl.type_name.length + 1 + method->name.length] = '\0';
                
                Token function_name;
                function_name.start = ext_name;
                function_name.length = stmt->impl.type_name.length + 1 + method->name.length;
                function_name.type = TOKEN_IDENT;
                function_name.line = method->name.line;
                
                add_function_overload(global_scope, function_name, fn_type, false);
                
                // Check method body
                if (method->body) {
                    SymbolTable method_scope = {0};
                    method_scope.parent = scope;
                    
                    // Add parameters to scope
                    for (int j = 0; j < method->param_count; j++) {
                        Type* param_type = builtin_int; // default
                        
                        // Special handling for 'self' parameter - use the impl type
                        if (j == 0 && method->param_count > 0 && 
                            method->params[0].length == 4 && 
                            memcmp(method->params[0].start, "self", 4) == 0) {
                            Symbol* type_symbol = find_symbol(global_scope, stmt->impl.type_name);
                            if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                                param_type = type_symbol->type;
                            }
                        } else if (method->param_types[j]) {
                            // Look up parameter type for other parameters
                            if (method->param_types[j]->type == EXPR_IDENT) {
                                Token type_name = method->param_types[j]->token;
                                if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                    param_type = builtin_int;
                                } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                    param_type = builtin_string;
                                } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                    param_type = builtin_float;
                                } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                    param_type = builtin_bool;
                                } else {
                                    // Check if it's a struct type
                                    Symbol* type_symbol = find_symbol(global_scope, type_name);
                                    if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                                        param_type = type_symbol->type;
                                    }
                                }
                            }
                        }
                        
                        add_symbol(&method_scope, method->params[j], param_type, method->param_mutable[j]);
                    }
                    
                    check_stmt(method->body, &method_scope);
                }
            }
            break;
        case STMT_TRAIT:
            // T3.2.1: Trait definition handling
            wyn_register_trait(&stmt->trait_decl);
            
            // Check trait methods
            for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                FnStmt* method = stmt->trait_decl.methods[i];
                
                // Create method scope for trait method
                SymbolTable method_scope = {0};
                method_scope.parent = scope;
                
                // Add method parameters to scope
                for (int j = 0; j < method->param_count; j++) {
                    Type* param_type = builtin_int; // Simplified
                    bool is_mutable = method->param_mutable ? method->param_mutable[j] : false;
                    add_symbol(&method_scope, method->params[j], param_type, is_mutable);
                }
                
                // Check method body if it has a default implementation
                if (stmt->trait_decl.method_has_default[i] && method->body) {
                    check_stmt(method->body, &method_scope);
                }
            }
            break;
        case STMT_ENUM:
            // Create proper enum type
            {
                Type* enum_type = make_type(TYPE_ENUM);
                enum_type->name = stmt->enum_decl.name;
                enum_type->enum_type.variants = stmt->enum_decl.variants;
                enum_type->enum_type.variant_count = stmt->enum_decl.variant_count;
                
                // Register enum type in global scope
                add_symbol(global_scope, stmt->enum_decl.name, enum_type, false);
                
                // Register each enum variant BOTH as qualified and unqualified
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    // Register unqualified variant (e.g., DONE)
                    add_symbol(global_scope, stmt->enum_decl.variants[i], enum_type, false);
                    
                    // Register qualified variant with . (e.g., Status.DONE)
                    char qualified_member_dot[128];
                    snprintf(qualified_member_dot, 128, "%.*s.%.*s",
                            stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                            stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    Token qualified_token_dot = {TOKEN_IDENT, strdup(qualified_member_dot), (int)strlen(qualified_member_dot), 0};
                    add_symbol(global_scope, qualified_token_dot, enum_type, false);
                    
                    // Register qualified variant with :: (e.g., Status::DONE) - maps to Status_DONE in C
                    char qualified_member_colon[128];
                    snprintf(qualified_member_colon, 128, "%.*s::%.*s",
                            stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                            stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    Token qualified_token_colon = {TOKEN_IDENT, strdup(qualified_member_colon), (int)strlen(qualified_member_colon), 0};
                    add_symbol(global_scope, qualified_token_colon, enum_type, false);
                    
                    // Also register with _ for C compatibility (e.g., Status_DONE)
                    char qualified_member_underscore[128];
                    snprintf(qualified_member_underscore, 128, "%.*s_%.*s",
                            stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                            stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    Token qualified_token_underscore = {TOKEN_IDENT, strdup(qualified_member_underscore), (int)strlen(qualified_member_underscore), 0};
                    add_symbol(global_scope, qualified_token_underscore, enum_type, false);
                    
                    // Register constructor function for variants with data
                    if (stmt->enum_decl.variant_type_counts[i] > 0) {
                        // Register EnumName_VariantName as a function
                        char constructor_name[128];
                        snprintf(constructor_name, 128, "%.*s_%.*s",
                                stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                                stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                        
                        Token constructor_token = {TOKEN_IDENT, strdup(constructor_name), (int)strlen(constructor_name), 0};
                        
                        Type* constructor_type = make_type(TYPE_FUNCTION);
                        constructor_type->fn_type.param_count = stmt->enum_decl.variant_type_counts[i];
                        constructor_type->fn_type.param_types = malloc(sizeof(Type*) * constructor_type->fn_type.param_count);
                        
                        // For now, just set all params to int (simplified)
                        for (int j = 0; j < constructor_type->fn_type.param_count; j++) {
                            constructor_type->fn_type.param_types[j] = builtin_int;
                        }
                        
                        constructor_type->fn_type.return_type = enum_type;
                        add_symbol(global_scope, constructor_token, constructor_type, false);
                    }
                }
                
                // Register toString function: EnumName_toString
                char tostring_name[128];
                snprintf(tostring_name, 128, "%.*s_toString",
                        stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                
                Token tostring_token = {TOKEN_IDENT, strdup(tostring_name), (int)strlen(tostring_name), 0};
                
                Type* tostring_type = make_type(TYPE_FUNCTION);
                tostring_type->fn_type.param_count = 1;
                tostring_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
                tostring_type->fn_type.param_types[0] = enum_type;
                tostring_type->fn_type.return_type = builtin_string;
                add_symbol(global_scope, tostring_token, tostring_type, false);
            }
            break;
        case STMT_TYPE_ALIAS:
            // Register type alias in global scope
            add_symbol(global_scope, stmt->type_alias.name, builtin_int, false);
            break;
        case STMT_IMPORT:
            // Register module namespace in scope
            add_symbol(scope, stmt->import.module, builtin_int, false);
            // Check for collision
            register_import(stmt->import.module.start, stmt->import.module.line);
            break;
        case STMT_MATCH: {
            // Type-check match statement with exhaustiveness checking
            Type* match_value_type = check_expr(stmt->match_stmt.value, scope);
            if (!match_value_type) return;
            
            bool has_wildcard = false;
            
            // Check each match case
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* match_case = &stmt->match_stmt.cases[i];
                
                // Check if this is a wildcard pattern
                if (match_case->pattern && match_case->pattern->type == PATTERN_WILDCARD) {
                    has_wildcard = true;
                }
                
                // Create a new scope for this match arm to hold bound variables
                SymbolTable arm_scope = {0};
                arm_scope.parent = scope;
                
                // If this is a destructuring pattern, bind the variable
                if (match_case->pattern && match_case->pattern->type == PATTERN_OPTION) {
                    if (match_case->pattern->option.inner && 
                        match_case->pattern->option.inner->type == PATTERN_IDENT) {
                        Token var_name = match_case->pattern->option.inner->ident.name;
                        
                        // For now, assume the bound variable has type int
                        // TODO: Get actual type from enum variant
                        Type* bound_type = calloc(1, sizeof(Type));
                        bound_type->kind = TYPE_INT;
                        
                        add_symbol(&arm_scope, var_name, bound_type, false);
                    }
                }
                
                // Type-check the case body in the arm scope
                if (match_case->body) {
                    check_stmt(match_case->body, &arm_scope);
                }
            }
            
            // For now, check exhaustiveness by looking for enum-like patterns
            // even if the type is int (since enum variants are treated as ints)
            if (!has_wildcard) {
                // Try to find if this looks like an enum match by checking pattern names
                // Look for patterns that match known enum variants
                bool looks_like_enum_match = false;
                char enum_name[64] = {0};
                int enum_variant_count = 0;
                
                // Scan global scope for enum types and see if patterns match
                for (int s = 0; s < global_scope->count; s++) {
                    Symbol* sym = &global_scope->symbols[s];
                    if (sym->type && sym->type->kind == TYPE_ENUM) {
                        // Check if any of our patterns match this enum's variants
                        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                            MatchCase* match_case = &stmt->match_stmt.cases[i];
                            Token pattern_name;
                            bool has_pattern_name = false;
                            
                            if (match_case->pattern && match_case->pattern->type == PATTERN_IDENT) {
                                pattern_name = match_case->pattern->ident.name;
                                has_pattern_name = true;
                            } else if (match_case->pattern && match_case->pattern->type == PATTERN_OPTION) {
                                pattern_name = match_case->pattern->option.variant_name;
                                has_pattern_name = true;
                            }
                            
                            if (has_pattern_name) {
                                // Check if this pattern matches any variant of this enum
                                for (int v = 0; v < sym->type->enum_type.variant_count; v++) {
                                    Token variant = sym->type->enum_type.variants[v];
                                    if (pattern_name.length == variant.length &&
                                        memcmp(pattern_name.start, variant.start, variant.length) == 0) {
                                        looks_like_enum_match = true;
                                        strncpy(enum_name, sym->name.start, 
                                               sym->name.length < 63 ? sym->name.length : 63);
                                        enum_variant_count = sym->type->enum_type.variant_count;
                                        break;
                                    }
                                }
                                if (looks_like_enum_match) break;
                            }
                        }
                        if (looks_like_enum_match) break;
                    }
                }
                
                if (looks_like_enum_match) {
                    // Find the enum type again to check exhaustiveness
                    for (int s = 0; s < global_scope->count; s++) {
                        Symbol* sym = &global_scope->symbols[s];
                        if (sym->type && sym->type->kind == TYPE_ENUM && 
                            strncmp(enum_name, sym->name.start, sym->name.length) == 0) {
                            
                            bool* covered = calloc(sym->type->enum_type.variant_count, sizeof(bool));
                            
                            // Check which variants are covered
                            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                                MatchCase* match_case = &stmt->match_stmt.cases[i];
                                Token pattern_name;
                                bool has_pattern_name = false;
                                
                                if (match_case->pattern && match_case->pattern->type == PATTERN_IDENT) {
                                    pattern_name = match_case->pattern->ident.name;
                                    has_pattern_name = true;
                                } else if (match_case->pattern && match_case->pattern->type == PATTERN_OPTION) {
                                    pattern_name = match_case->pattern->option.variant_name;
                                    has_pattern_name = true;
                                }
                                
                                if (has_pattern_name) {
                                    for (int v = 0; v < sym->type->enum_type.variant_count; v++) {
                                        Token variant = sym->type->enum_type.variants[v];
                                        if (pattern_name.length == variant.length &&
                                            memcmp(pattern_name.start, variant.start, variant.length) == 0) {
                                            covered[v] = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            // Check if any variants are missing
                            for (int v = 0; v < sym->type->enum_type.variant_count; v++) {
                                if (!covered[v]) {
                                    Token variant = sym->type->enum_type.variants[v];
                                    fprintf(stderr, "Error: non-exhaustive match, missing case: %.*s\n",
                                            variant.length, variant.start);
                                    had_error = true;
                                }
                            }
                            
                            free(covered);
                            break;
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

void check_program(Program* prog) {
    // Set global pointer for struct field type lookup
    current_program = prog;
    
    // Pass 0: Register all struct types, enums, and constants first (so functions can reference them)
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT) {
            StructStmt* struct_decl = &prog->stmts[i]->struct_decl;
            Type* struct_type = make_type(TYPE_STRUCT);
            struct_type->struct_type.name = struct_decl->name;
            struct_type->struct_type.field_count = struct_decl->field_count;
            add_symbol(global_scope, struct_decl->name, struct_type, false);
        } else if (prog->stmts[i]->type == STMT_ENUM) {
            // Register enum type and variants early so functions can use them
            EnumStmt* enum_decl = &prog->stmts[i]->enum_decl;
            Type* enum_type = make_type(TYPE_ENUM);
            enum_type->name = enum_decl->name;
            enum_type->enum_type.variants = enum_decl->variants;
            enum_type->enum_type.variant_count = enum_decl->variant_count;
            
            // Register enum type in global scope
            add_symbol(global_scope, enum_decl->name, enum_type, false);
            
            // Register each enum variant
            for (int j = 0; j < enum_decl->variant_count; j++) {
                // Register unqualified variant (e.g., ADD)
                add_symbol(global_scope, enum_decl->variants[j], enum_type, false);
                
                // Register qualified variant with :: (e.g., Operation::ADD)
                char qualified[128];
                snprintf(qualified, 128, "%.*s::%.*s",
                        enum_decl->name.length, enum_decl->name.start,
                        enum_decl->variants[j].length, enum_decl->variants[j].start);
                Token qualified_token = {TOKEN_IDENT, strdup(qualified), (int)strlen(qualified), 0};
                add_symbol(global_scope, qualified_token, enum_type, false);
                
                // Register constructor function for all variants (with or without data)
                // For mixed enums (Some(T) + None), we need constructors for all
                bool has_any_data = false;
                for (int k = 0; k < enum_decl->variant_count; k++) {
                    if (enum_decl->variant_type_counts && enum_decl->variant_type_counts[k] > 0) {
                        has_any_data = true;
                        break;
                    }
                }
                
                if (has_any_data) {
                    // This is a tagged union enum - register constructors for ALL variants
                    char constructor_name[128];
                    snprintf(constructor_name, 128, "%.*s_%.*s",
                            enum_decl->name.length, enum_decl->name.start,
                            enum_decl->variants[j].length, enum_decl->variants[j].start);
                    
                    Token constructor_token = {TOKEN_IDENT, strdup(constructor_name), (int)strlen(constructor_name), 0};
                    
                    Type* constructor_type = make_type(TYPE_FUNCTION);
                    int param_count = (enum_decl->variant_type_counts && enum_decl->variant_type_counts[j] > 0) 
                                      ? enum_decl->variant_type_counts[j] : 0;
                    constructor_type->fn_type.param_count = param_count;
                    constructor_type->fn_type.param_types = malloc(sizeof(Type*) * (param_count > 0 ? param_count : 1));
                    
                    // Parse parameter types from variant_types
                    for (int k = 0; k < param_count; k++) {
                        if (enum_decl->variant_types && enum_decl->variant_types[j] && enum_decl->variant_types[j][k]) {
                            Expr* type_expr = enum_decl->variant_types[j][k];
                            if (type_expr->type == EXPR_IDENT) {
                                Token type_name = type_expr->token;
                                if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                    constructor_type->fn_type.param_types[k] = builtin_int;
                                } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                    constructor_type->fn_type.param_types[k] = builtin_string;
                                } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                    constructor_type->fn_type.param_types[k] = builtin_bool;
                                } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                    constructor_type->fn_type.param_types[k] = builtin_float;
                                } else {
                                    constructor_type->fn_type.param_types[k] = builtin_int; // fallback
                                }
                            } else {
                                constructor_type->fn_type.param_types[k] = builtin_int; // fallback
                            }
                        } else {
                            constructor_type->fn_type.param_types[k] = builtin_int; // fallback
                        }
                    }
                    
                    constructor_type->fn_type.return_type = enum_type;
                    add_symbol(global_scope, constructor_token, constructor_type, false);
                }
            }
            
            // Register toString function
            char tostring_name[128];
            snprintf(tostring_name, 128, "%.*s_toString",
                    enum_decl->name.length, enum_decl->name.start);
            
            Token tostring_token = {TOKEN_IDENT, strdup(tostring_name), (int)strlen(tostring_name), 0};
            
            Type* tostring_type = make_type(TYPE_FUNCTION);
            tostring_type->fn_type.param_count = 1;
            tostring_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
            tostring_type->fn_type.param_types[0] = enum_type;
            tostring_type->fn_type.return_type = builtin_string;
            add_symbol(global_scope, tostring_token, tostring_type, false);
        } else if (prog->stmts[i]->type == STMT_EXTERN) {
            // Register extern function
            ExternStmt* ext = &prog->stmts[i]->extern_fn;
            Type* fn_type = make_type(TYPE_FUNCTION);
            fn_type->fn_type.param_count = ext->param_count;
            fn_type->fn_type.is_variadic = ext->is_variadic;
            fn_type->fn_type.param_types = malloc(sizeof(Type*) * ext->param_count);
            for (int j = 0; j < ext->param_count; j++) {
                // Convert type expression to Type*
                if (ext->param_types[j] && ext->param_types[j]->type == EXPR_IDENT) {
                    Token type_name = ext->param_types[j]->token;
                    if (strncmp(type_name.start, "string", type_name.length) == 0) {
                        fn_type->fn_type.param_types[j] = builtin_string;
                    } else if (strncmp(type_name.start, "int", type_name.length) == 0) {
                        fn_type->fn_type.param_types[j] = builtin_int;
                    } else {
                        fn_type->fn_type.param_types[j] = builtin_int; // Default fallback
                    }
                } else {
                    fn_type->fn_type.param_types[j] = builtin_int; // Fallback
                }
            }
            fn_type->fn_type.return_type = builtin_int; // Simplified
            add_function_overload(global_scope, ext->name, fn_type, false);
        } else if (prog->stmts[i]->type == STMT_MACRO) {
            // Register macro as function
            MacroStmt* macro = &prog->stmts[i]->macro;
            Type* fn_type = make_type(TYPE_FUNCTION);
            fn_type->fn_type.param_count = macro->param_count;
            fn_type->fn_type.param_types = malloc(sizeof(Type*) * macro->param_count);
            for (int j = 0; j < macro->param_count; j++) {
                fn_type->fn_type.param_types[j] = builtin_int; // Simplified
            }
            fn_type->fn_type.return_type = builtin_int; // Simplified
            add_function_overload(global_scope, macro->name, fn_type, false);
        } else if (prog->stmts[i]->type == STMT_CONST) {
            // Register module-level constants early so functions can use them
            VarStmt* const_stmt = &prog->stmts[i]->const_stmt;
            
            // Determine type from initializer
            Type* const_type = builtin_int; // default
            if (const_stmt->init) {
                if (const_stmt->init->type == EXPR_STRING) {
                    const_type = builtin_string;
                } else if (const_stmt->init->type == EXPR_FLOAT) {
                    const_type = builtin_float;
                } else if (const_stmt->init->type == EXPR_BOOL) {
                    const_type = builtin_bool;
                } else if (const_stmt->init->type == EXPR_INT) {
                    const_type = builtin_int;
                }
            }
            
            add_symbol(global_scope, const_stmt->name, const_type, false);
        }
    }
    
    // Register standard library modules (always available, no import needed)
    {
        // File module
        Token file_read_tok = {TOKEN_IDENT, "File::read", 10, 0};
        Type* file_read_type = make_type(TYPE_FUNCTION);
        file_read_type->fn_type.param_count = 1;
        file_read_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_read_type->fn_type.param_types[0] = builtin_string;
        file_read_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_read_tok, file_read_type, false);
        
        Token file_write_tok = {TOKEN_IDENT, "File::write", 11, 0};
        Type* file_write_type = make_type(TYPE_FUNCTION);
        file_write_type->fn_type.param_count = 2;
        file_write_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        file_write_type->fn_type.param_types[0] = builtin_string;
        file_write_type->fn_type.param_types[1] = builtin_string;
        file_write_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_write_tok, file_write_type, false);
        
        Token file_exists_tok = {TOKEN_IDENT, "File::exists", 12, 0};
        Type* file_exists_type = make_type(TYPE_FUNCTION);
        file_exists_type->fn_type.param_count = 1;
        file_exists_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_exists_type->fn_type.param_types[0] = builtin_string;
        file_exists_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_exists_tok, file_exists_type, false);
        
        Token file_delete_tok = {TOKEN_IDENT, "File::delete", 12, 0};
        Type* file_delete_type = make_type(TYPE_FUNCTION);
        file_delete_type->fn_type.param_count = 1;
        file_delete_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_delete_type->fn_type.param_types[0] = builtin_string;
        file_delete_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_delete_tok, file_delete_type, false);
        
        Token file_list_dir_tok = {TOKEN_IDENT, "File::list_dir", 14, 0};
        Type* file_list_dir_type = make_type(TYPE_FUNCTION);
        file_list_dir_type->fn_type.param_count = 1;
        file_list_dir_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_list_dir_type->fn_type.param_types[0] = builtin_string;
        // Return type is array of strings
        Type* string_array_list = make_type(TYPE_ARRAY);
        string_array_list->array_type.element_type = builtin_string;
        file_list_dir_type->fn_type.return_type = string_array_list;
        add_symbol(global_scope, file_list_dir_tok, file_list_dir_type, false);
        
        Token file_is_file_tok = {TOKEN_IDENT, "File::is_file", 13, 0};
        Type* file_is_file_type = make_type(TYPE_FUNCTION);
        file_is_file_type->fn_type.param_count = 1;
        file_is_file_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_is_file_type->fn_type.param_types[0] = builtin_string;
        file_is_file_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_is_file_tok, file_is_file_type, false);
        
        Token file_is_dir_tok = {TOKEN_IDENT, "File::is_dir", 12, 0};
        Type* file_is_dir_type = make_type(TYPE_FUNCTION);
        file_is_dir_type->fn_type.param_count = 1;
        file_is_dir_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_is_dir_type->fn_type.param_types[0] = builtin_string;
        file_is_dir_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_is_dir_tok, file_is_dir_type, false);
        
        Token file_get_cwd_tok = {TOKEN_IDENT, "File::get_cwd", 13, 0};
        Type* file_get_cwd_type = make_type(TYPE_FUNCTION);
        file_get_cwd_type->fn_type.param_count = 0;
        file_get_cwd_type->fn_type.param_types = NULL;
        file_get_cwd_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_get_cwd_tok, file_get_cwd_type, false);
        
        Token file_create_dir_tok = {TOKEN_IDENT, "File::create_dir", 16, 0};
        Type* file_create_dir_type = make_type(TYPE_FUNCTION);
        file_create_dir_type->fn_type.param_count = 1;
        file_create_dir_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_create_dir_type->fn_type.param_types[0] = builtin_string;
        file_create_dir_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_create_dir_tok, file_create_dir_type, false);
        
        Token file_file_size_tok = {TOKEN_IDENT, "File::file_size", 15, 0};
        Type* file_file_size_type = make_type(TYPE_FUNCTION);
        file_file_size_type->fn_type.param_count = 1;
        file_file_size_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_file_size_type->fn_type.param_types[0] = builtin_string;
        file_file_size_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_file_size_tok, file_file_size_type, false);
        
        Token file_path_join_tok = {TOKEN_IDENT, "File::path_join", 15, 0};
        Type* file_path_join_type = make_type(TYPE_FUNCTION);
        file_path_join_type->fn_type.param_count = 2;
        file_path_join_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        file_path_join_type->fn_type.param_types[0] = builtin_string;
        file_path_join_type->fn_type.param_types[1] = builtin_string;
        file_path_join_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_path_join_tok, file_path_join_type, false);
        
        Token file_basename_tok = {TOKEN_IDENT, "File::basename", 14, 0};
        Type* file_basename_type = make_type(TYPE_FUNCTION);
        file_basename_type->fn_type.param_count = 1;
        file_basename_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_basename_type->fn_type.param_types[0] = builtin_string;
        file_basename_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_basename_tok, file_basename_type, false);
        
        Token file_dirname_tok = {TOKEN_IDENT, "File::dirname", 13, 0};
        Type* file_dirname_type = make_type(TYPE_FUNCTION);
        file_dirname_type->fn_type.param_count = 1;
        file_dirname_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_dirname_type->fn_type.param_types[0] = builtin_string;
        file_dirname_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_dirname_tok, file_dirname_type, false);
        
        Token file_extension_tok = {TOKEN_IDENT, "File::extension", 15, 0};
        Type* file_extension_type = make_type(TYPE_FUNCTION);
        file_extension_type->fn_type.param_count = 1;
        file_extension_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_extension_type->fn_type.param_types[0] = builtin_string;
        file_extension_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, file_extension_tok, file_extension_type, false);
        
        // New file system utility functions
        Token file_move_tok = {TOKEN_IDENT, "File::move", 10, 0};
        Type* file_move_type = make_type(TYPE_FUNCTION);
        file_move_type->fn_type.param_count = 2;
        file_move_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        file_move_type->fn_type.param_types[0] = builtin_string;
        file_move_type->fn_type.param_types[1] = builtin_string;
        file_move_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_move_tok, file_move_type, false);
        
        Token file_mkdir_tok = {TOKEN_IDENT, "File::mkdir", 11, 0};
        Type* file_mkdir_type = make_type(TYPE_FUNCTION);
        file_mkdir_type->fn_type.param_count = 1;
        file_mkdir_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_mkdir_type->fn_type.param_types[0] = builtin_string;
        file_mkdir_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_mkdir_tok, file_mkdir_type, false);
        
        Token file_rmdir_tok = {TOKEN_IDENT, "File::rmdir", 11, 0};
        Type* file_rmdir_type = make_type(TYPE_FUNCTION);
        file_rmdir_type->fn_type.param_count = 1;
        file_rmdir_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        file_rmdir_type->fn_type.param_types[0] = builtin_string;
        file_rmdir_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, file_rmdir_tok, file_rmdir_type, false);
        
        // System module
        Token sys_exec_tok = {TOKEN_IDENT, "System::exec", 12, 0};
        Type* sys_exec_type = make_type(TYPE_FUNCTION);
        sys_exec_type->fn_type.param_count = 1;
        sys_exec_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        sys_exec_type->fn_type.param_types[0] = builtin_string;
        sys_exec_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, sys_exec_tok, sys_exec_type, false);
        
        Token sys_exec_code_tok = {TOKEN_IDENT, "System::exec_code", 17, 0};
        Type* sys_exec_code_type = make_type(TYPE_FUNCTION);
        sys_exec_code_type->fn_type.param_count = 1;
        sys_exec_code_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        sys_exec_code_type->fn_type.param_types[0] = builtin_string;
        sys_exec_code_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, sys_exec_code_tok, sys_exec_code_type, false);
        
        Token sys_exit_tok = {TOKEN_IDENT, "System::exit", 12, 0};
        Type* sys_exit_type = make_type(TYPE_FUNCTION);
        sys_exit_type->fn_type.param_count = 1;
        sys_exit_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        sys_exit_type->fn_type.param_types[0] = builtin_int;
        sys_exit_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, sys_exit_tok, sys_exit_type, false);
        
        Token sys_args_tok = {TOKEN_IDENT, "System::args", 12, 0};
        Type* sys_args_type = make_type(TYPE_FUNCTION);
        sys_args_type->fn_type.param_count = 0;
        sys_args_type->fn_type.param_types = NULL;
        // Return type is array of strings
        Type* string_array = make_type(TYPE_ARRAY);
        string_array->array_type.element_type = builtin_string;
        sys_args_type->fn_type.return_type = string_array;
        add_symbol(global_scope, sys_args_tok, sys_args_type, false);
        
        Token sys_env_tok = {TOKEN_IDENT, "System::env", 11, 0};
        Type* sys_env_type = make_type(TYPE_FUNCTION);
        sys_env_type->fn_type.param_count = 1;
        sys_env_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        sys_env_type->fn_type.param_types[0] = builtin_string;
        sys_env_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, sys_env_tok, sys_env_type, false);
        
        Token sys_set_env_tok = {TOKEN_IDENT, "System::set_env", 15, 0};
        Type* sys_set_env_type = make_type(TYPE_FUNCTION);
        sys_set_env_type->fn_type.param_count = 2;
        sys_set_env_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        sys_set_env_type->fn_type.param_types[0] = builtin_string;
        sys_set_env_type->fn_type.param_types[1] = builtin_string;
        sys_set_env_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, sys_set_env_tok, sys_set_env_type, false);
        
        // Math module (update to Math:: from math.)
        Token math_pow_tok = {TOKEN_IDENT, "Math::pow", 9, 0};
        Type* math_pow_type = make_type(TYPE_FUNCTION);
        math_pow_type->fn_type.param_count = 2;
        math_pow_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        math_pow_type->fn_type.param_types[0] = builtin_float;
        math_pow_type->fn_type.param_types[1] = builtin_float;
        math_pow_type->fn_type.return_type = builtin_float;
        add_symbol(global_scope, math_pow_tok, math_pow_type, false);
        
        Token math_sqrt_tok = {TOKEN_IDENT, "Math::sqrt", 10, 0};
        Type* math_sqrt_type = make_type(TYPE_FUNCTION);
        math_sqrt_type->fn_type.param_count = 1;
        math_sqrt_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        math_sqrt_type->fn_type.param_types[0] = builtin_float;
        math_sqrt_type->fn_type.return_type = builtin_float;
        add_symbol(global_scope, math_sqrt_tok, math_sqrt_type, false);
        
        // Time module
        Token time_now_tok = {TOKEN_IDENT, "Time::now", 9, 0};
        Type* time_now_type = make_type(TYPE_FUNCTION);
        time_now_type->fn_type.param_count = 0;
        time_now_type->fn_type.param_types = NULL;
        time_now_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, time_now_tok, time_now_type, false);
        
        Token time_sleep_tok = {TOKEN_IDENT, "Time::sleep", 11, 0};
        Type* time_sleep_type = make_type(TYPE_FUNCTION);
        time_sleep_type->fn_type.param_count = 1;
        time_sleep_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        time_sleep_type->fn_type.param_types[0] = builtin_int;
        time_sleep_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, time_sleep_tok, time_sleep_type, false);
        
        Token time_format_tok = {TOKEN_IDENT, "Time::format", 12, 0};
        Type* time_format_type = make_type(TYPE_FUNCTION);
        time_format_type->fn_type.param_count = 1;
        time_format_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        time_format_type->fn_type.param_types[0] = builtin_int;
        time_format_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, time_format_tok, time_format_type, false);
        
        // Net module
        Token net_listen_tok = {TOKEN_IDENT, "Net::listen", 11, 0};
        Type* net_listen_type = make_type(TYPE_FUNCTION);
        net_listen_type->fn_type.param_count = 1;
        net_listen_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        net_listen_type->fn_type.param_types[0] = builtin_int;
        net_listen_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, net_listen_tok, net_listen_type, false);
        
        Token net_connect_tok = {TOKEN_IDENT, "Net::connect", 12, 0};
        Type* net_connect_type = make_type(TYPE_FUNCTION);
        net_connect_type->fn_type.param_count = 2;
        net_connect_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        net_connect_type->fn_type.param_types[0] = builtin_string;
        net_connect_type->fn_type.param_types[1] = builtin_int;
        net_connect_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, net_connect_tok, net_connect_type, false);
        
        Token net_send_tok = {TOKEN_IDENT, "Net::send", 9, 0};
        Type* net_send_type = make_type(TYPE_FUNCTION);
        net_send_type->fn_type.param_count = 2;
        net_send_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        net_send_type->fn_type.param_types[0] = builtin_int;
        net_send_type->fn_type.param_types[1] = builtin_string;
        net_send_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, net_send_tok, net_send_type, false);
        
        Token net_recv_tok = {TOKEN_IDENT, "Net::recv", 9, 0};
        Type* net_recv_type = make_type(TYPE_FUNCTION);
        net_recv_type->fn_type.param_count = 1;
        net_recv_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        net_recv_type->fn_type.param_types[0] = builtin_int;
        net_recv_type->fn_type.return_type = builtin_string;
        add_symbol(global_scope, net_recv_tok, net_recv_type, false);
        
        Token net_close_tok = {TOKEN_IDENT, "Net::close", 10, 0};
        Type* net_close_type = make_type(TYPE_FUNCTION);
        net_close_type->fn_type.param_count = 1;
        net_close_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        net_close_type->fn_type.param_types[0] = builtin_int;
        net_close_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, net_close_tok, net_close_type, false);
        
        // HashMap module
        Token hashmap_new_tok = {TOKEN_IDENT, "HashMap::new", 12, 0};
        Type* hashmap_new_type = make_type(TYPE_FUNCTION);
        hashmap_new_type->fn_type.param_count = 0;
        hashmap_new_type->fn_type.param_types = NULL;
        hashmap_new_type->fn_type.return_type = make_type(TYPE_MAP);
        add_symbol(global_scope, hashmap_new_tok, hashmap_new_type, false);
        
        // HashSet module
        Token hashset_new_tok = {TOKEN_IDENT, "HashSet::new", 12, 0};
        Type* hashset_new_type = make_type(TYPE_FUNCTION);
        hashset_new_type->fn_type.param_count = 0;
        hashset_new_type->fn_type.param_types = NULL;
        hashset_new_type->fn_type.return_type = make_type(TYPE_SET);
        add_symbol(global_scope, hashset_new_tok, hashset_new_type, false);
        
        Token hashmap_insert_tok = {TOKEN_IDENT, "HashMap::insert", 15, 0};
        Type* hashmap_insert_type = make_type(TYPE_FUNCTION);
        hashmap_insert_type->fn_type.param_count = 3;
        hashmap_insert_type->fn_type.param_types = malloc(sizeof(Type*) * 3);
        hashmap_insert_type->fn_type.param_types[0] = builtin_int;
        hashmap_insert_type->fn_type.param_types[1] = builtin_string;
        hashmap_insert_type->fn_type.param_types[2] = builtin_int;
        hashmap_insert_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_insert_tok, hashmap_insert_type, false);
        
        Token hashmap_get_tok = {TOKEN_IDENT, "HashMap::get", 12, 0};
        Type* hashmap_get_type = make_type(TYPE_FUNCTION);
        hashmap_get_type->fn_type.param_count = 2;
        hashmap_get_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        hashmap_get_type->fn_type.param_types[0] = builtin_int;
        hashmap_get_type->fn_type.param_types[1] = builtin_string;
        hashmap_get_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_get_tok, hashmap_get_type, false);
        
        Token hashmap_contains_tok = {TOKEN_IDENT, "HashMap::contains", 17, 0};
        Type* hashmap_contains_type = make_type(TYPE_FUNCTION);
        hashmap_contains_type->fn_type.param_count = 2;
        hashmap_contains_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        hashmap_contains_type->fn_type.param_types[0] = builtin_int;
        hashmap_contains_type->fn_type.param_types[1] = builtin_string;
        hashmap_contains_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_contains_tok, hashmap_contains_type, false);
        
        Token hashmap_len_tok = {TOKEN_IDENT, "HashMap::len", 12, 0};
        Type* hashmap_len_type = make_type(TYPE_FUNCTION);
        hashmap_len_type->fn_type.param_count = 1;
        hashmap_len_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        hashmap_len_type->fn_type.param_types[0] = builtin_int;
        hashmap_len_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_len_tok, hashmap_len_type, false);
        
        Token hashmap_remove_tok = {TOKEN_IDENT, "HashMap::remove", 15, 0};
        Type* hashmap_remove_type = make_type(TYPE_FUNCTION);
        hashmap_remove_type->fn_type.param_count = 2;
        hashmap_remove_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        hashmap_remove_type->fn_type.param_types[0] = builtin_int;
        hashmap_remove_type->fn_type.param_types[1] = builtin_string;
        hashmap_remove_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_remove_tok, hashmap_remove_type, false);
        
        Token hashmap_free_tok = {TOKEN_IDENT, "HashMap::free", 13, 0};
        Type* hashmap_free_type = make_type(TYPE_FUNCTION);
        hashmap_free_type->fn_type.param_count = 1;
        hashmap_free_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        hashmap_free_type->fn_type.param_types[0] = builtin_int;
        hashmap_free_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, hashmap_free_tok, hashmap_free_type, false);
        
        // Lowercase hashmap functions (for compatibility)
        Token hashmap_new_lc_tok = {TOKEN_IDENT, "wyn_hashmap_new", 15, 0};
        Type* hashmap_new_lc_type = make_type(TYPE_FUNCTION);
        hashmap_new_lc_type->fn_type.param_count = 0;
        hashmap_new_lc_type->fn_type.param_types = NULL;
        hashmap_new_lc_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_new_lc_tok, hashmap_new_lc_type, false);
        
        Token hashmap_insert_int_lc_tok = {TOKEN_IDENT, "wyn_hashmap_insert_int", 22, 0};
        Type* hashmap_insert_int_lc_type = make_type(TYPE_FUNCTION);
        hashmap_insert_int_lc_type->fn_type.param_count = 3;
        hashmap_insert_int_lc_type->fn_type.param_types = malloc(sizeof(Type*) * 3);
        hashmap_insert_int_lc_type->fn_type.param_types[0] = builtin_int;
        hashmap_insert_int_lc_type->fn_type.param_types[1] = builtin_string;
        hashmap_insert_int_lc_type->fn_type.param_types[2] = builtin_int;
        hashmap_insert_int_lc_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, hashmap_insert_int_lc_tok, hashmap_insert_int_lc_type, false);
        
        Token hashmap_get_int_lc_tok = {TOKEN_IDENT, "wyn_hashmap_get_int", 19, 0};
        Type* hashmap_get_int_lc_type = make_type(TYPE_FUNCTION);
        hashmap_get_int_lc_type->fn_type.param_count = 2;
        hashmap_get_int_lc_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        hashmap_get_int_lc_type->fn_type.param_types[0] = builtin_int;
        hashmap_get_int_lc_type->fn_type.param_types[1] = builtin_string;
        hashmap_get_int_lc_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_get_int_lc_tok, hashmap_get_int_lc_type, false);
        
        Token hashmap_has_lc_tok = {TOKEN_IDENT, "wyn_hashmap_has", 15, 0};
        Type* hashmap_has_lc_type = make_type(TYPE_FUNCTION);
        hashmap_has_lc_type->fn_type.param_count = 2;
        hashmap_has_lc_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        hashmap_has_lc_type->fn_type.param_types[0] = builtin_int;
        hashmap_has_lc_type->fn_type.param_types[1] = builtin_string;
        hashmap_has_lc_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_has_lc_tok, hashmap_has_lc_type, false);
        
        Token hashmap_len_lc_tok = {TOKEN_IDENT, "wyn_hashmap_len", 15, 0};
        Type* hashmap_len_lc_type = make_type(TYPE_FUNCTION);
        hashmap_len_lc_type->fn_type.param_count = 1;
        hashmap_len_lc_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        hashmap_len_lc_type->fn_type.param_types[0] = builtin_int;
        hashmap_len_lc_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, hashmap_len_lc_tok, hashmap_len_lc_type, false);
        
        Token hashmap_free_lc_tok = {TOKEN_IDENT, "wyn_hashmap_free", 16, 0};
        Type* hashmap_free_lc_type = make_type(TYPE_FUNCTION);
        hashmap_free_lc_type->fn_type.param_count = 1;
        hashmap_free_lc_type->fn_type.param_types = malloc(sizeof(Type*) * 1);
        hashmap_free_lc_type->fn_type.param_types[0] = builtin_int;
        hashmap_free_lc_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, hashmap_free_lc_tok, hashmap_free_lc_type, false);
        
        // Arena functions
        Token wyn_arena_new_tok = {TOKEN_IDENT, "wyn_arena_new", 13, 0};
        Type* wyn_arena_new_type = make_type(TYPE_FUNCTION);
        wyn_arena_new_type->fn_type.param_count = 0;
        wyn_arena_new_type->fn_type.param_types = NULL;
        wyn_arena_new_type->fn_type.return_type = builtin_int;
        add_symbol(global_scope, wyn_arena_new_tok, wyn_arena_new_type, false);
    }
    
    // First pass: process imports and load modules
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            ImportStmt* import = &prog->stmts[i]->import;
            
            // Build module name
            char module_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s", import->module.length, import->module.start);
            
            // Load module from same directory as current file
            Program* module = load_module(module_name);
            
            if (module) {
                // Check if this is a whole-module import (no items specified)
                if (import->item_count == 0) {
                    // Whole module import - register functions with module prefix
                    char module_name_str[256];
                    snprintf(module_name_str, sizeof(module_name_str), "%.*s", import->module.length, import->module.start);
                    
                    // Register all exported functions with qualified names
                    for (int j = 0; j < module->count; j++) {
                        Stmt* stmt = module->stmts[j];
                        if (stmt->type == STMT_EXPORT && stmt->export.stmt && stmt->export.stmt->type == STMT_FN) {
                            FnStmt* fn = &stmt->export.stmt->fn;
                            
                            // Create qualified name: module::function
                            char* qualified_name = malloc(strlen(module_name_str) + 2 + fn->name.length + 1);
                            sprintf(qualified_name, "%s::%.*s", module_name_str, fn->name.length, fn->name.start);
                            
                            Token qualified_token = fn->name;
                            qualified_token.start = qualified_name;
                            qualified_token.length = strlen(qualified_name);
                            
                            // Create function type
                            Type* fn_type = make_type(TYPE_FUNCTION);
                            fn_type->fn_type.param_count = fn->param_count;
                            fn_type->fn_type.param_types = malloc(sizeof(Type*) * fn->param_count);
                            Type* int_type = make_type(TYPE_INT);
                            for (int k = 0; k < fn->param_count; k++) {
                                fn_type->fn_type.param_types[k] = int_type;
                            }
                            fn_type->fn_type.return_type = int_type;
                            
                            // Register with qualified name
                            Symbol* existing = find_symbol(global_scope, qualified_token);
                            if (!existing) {
                                add_symbol(global_scope, qualified_token, fn_type, false);
                            }
                        }
                    }
                }
                
                // Merge exported functions into current program for codegen
                // Symbols are already registered by check_all_modules
                merge_module_exports(module, prog, import);
            } else {
                fprintf(stderr, "Warning: Could not load module '%s'\n", module_name);
            }
        }
    }
    
    // Continue with function registration
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            FnStmt* fn = &prog->stmts[i]->fn;
            
            // T3.1.1: Register generic functions
            if (fn->type_param_count > 0) {
                wyn_register_generic_function(fn);
            }
            
            // Create function type
            Type* fn_type = make_type(TYPE_FUNCTION);
            fn_type->fn_type.param_count = fn->param_count;
            fn_type->fn_type.param_types = malloc(sizeof(Type*) * fn->param_count);
            for (int j = 0; j < fn->param_count; j++) {
                // Determine parameter type from type annotation
                Type* param_type = builtin_int; // default
                
                // FIX: For extension methods, first parameter defaults to receiver type
                if (fn->is_extension && j == 0 && !fn->param_types[j]) {
                    // Look up the receiver struct type
                    Symbol* receiver_symbol = find_symbol(global_scope, fn->receiver_type);
                    if (receiver_symbol && receiver_symbol->type && receiver_symbol->type->kind == TYPE_STRUCT) {
                        param_type = receiver_symbol->type;
                    } else {
                        param_type = builtin_int; // Fallback
                    }
                } else if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_FN_TYPE) {
                        // Handle function type parameters: fn(T) -> R
                        param_type = check_expr(fn->param_types[j], global_scope);
                    } else if (fn->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = fn->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = builtin_int;
                        } else if ((type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) ||
                                   (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0)) {
                            param_type = builtin_string;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = builtin_float;
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = builtin_bool;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = builtin_array;
                        } else {
                            // Check if it's a user-defined type (struct or enum)
                            Symbol* type_symbol = find_symbol(global_scope, type_name);
                            if (type_symbol && type_symbol->type) {
                                param_type = type_symbol->type;
                            }
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type]
                        Type* array_type = make_type(TYPE_ARRAY);
                        if (fn->param_types[j]->array.count > 0 && fn->param_types[j]->array.elements[0]) {
                            Expr* elem_type_expr = fn->param_types[j]->array.elements[0];
                            if (elem_type_expr->type == EXPR_IDENT) {
                                Token elem_type_name = elem_type_expr->token;
                                if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                    array_type->array_type.element_type = builtin_int;
                                } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                    array_type->array_type.element_type = builtin_string;
                                } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                    array_type->array_type.element_type = builtin_float;
                                } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                    array_type->array_type.element_type = builtin_bool;
                                } else {
                                    // Check if it's a user-defined type (struct or enum)
                                    Symbol* type_symbol = find_symbol(global_scope, elem_type_name);
                                    if (type_symbol && type_symbol->type) {
                                        array_type->array_type.element_type = type_symbol->type;
                                    }
                                }
                            }
                        }
                        param_type = array_type;
                    }
                }
                fn_type->fn_type.param_types[j] = param_type;
            }
            
            // Determine return type from function signature or infer from body
            fn_type->fn_type.return_type = builtin_int; // default
            if (fn->return_type) {
                if (fn->return_type->type == EXPR_CALL) {
                    // Generic type like HashMap<K,V>
                    if (fn->return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = fn->return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            fn_type->fn_type.return_type = make_type(TYPE_MAP);
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            fn_type->fn_type.return_type = make_type(TYPE_SET);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            // Resolve Option<int> -> OptionInt, Option<string> -> OptionString
                            Token concrete = {TOKEN_IDENT, "OptionInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0) {
                                    concrete = (Token){TOKEN_IDENT, "OptionString", 12, 0};
                                }
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            fn_type->fn_type.return_type = sym ? sym->type : builtin_int;
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            // Resolve Result<int, string> -> ResultInt, Result<string, string> -> ResultString
                            Token concrete = {TOKEN_IDENT, "ResultInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0) {
                                    concrete = (Token){TOKEN_IDENT, "ResultString", 12, 0};
                                }
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            fn_type->fn_type.return_type = sym ? sym->type : builtin_int;
                        }
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    Type* array_type = make_type(TYPE_ARRAY);
                    if (fn->return_type->array.count > 0 && fn->return_type->array.elements[0]) {
                        Expr* elem_type_expr = fn->return_type->array.elements[0];
                        if (elem_type_expr->type == EXPR_IDENT) {
                            Token elem_type_name = elem_type_expr->token;
                            if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                array_type->array_type.element_type = builtin_int;
                            } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                array_type->array_type.element_type = builtin_string;
                            } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                array_type->array_type.element_type = builtin_float;
                            } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                array_type->array_type.element_type = builtin_bool;
                            }
                        }
                    }
                    fn_type->fn_type.return_type = array_type;
                } else if (fn->return_type->type == EXPR_IDENT) {
                    Token type_name = fn->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        fn_type->fn_type.return_type = builtin_int;
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        fn_type->fn_type.return_type = builtin_string;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        fn_type->fn_type.return_type = builtin_float;
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        fn_type->fn_type.return_type = builtin_bool;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        fn_type->fn_type.return_type = builtin_array;
                    } else {
                        // Check if it's a user-defined type (struct or enum)
                        Symbol* type_symbol = find_symbol(global_scope, type_name);
                        if (type_symbol && type_symbol->type) {
                            fn_type->fn_type.return_type = type_symbol->type;
                        }
                    }
                }
            }
            
            // Register function name (or Type_method for extension methods)
            Token function_name = fn->name;
            if (fn->is_extension) {
                // Create Type_method name
                char* ext_name = malloc(fn->receiver_type.length + 1 + fn->name.length + 1);
                memcpy(ext_name, fn->receiver_type.start, fn->receiver_type.length);
                ext_name[fn->receiver_type.length] = '_';
                memcpy(ext_name + fn->receiver_type.length + 1, fn->name.start, fn->name.length);
                ext_name[fn->receiver_type.length + 1 + fn->name.length] = '\0';
                function_name.start = ext_name;
                function_name.length = fn->receiver_type.length + 1 + fn->name.length;
            }
            
            // T1.5.3: Register function with overload support
            // Check if already registered (e.g., from imported module)
            Symbol* existing = find_symbol(global_scope, function_name);
            if (!existing || !signatures_match(existing->type, fn_type)) {
                add_function_overload(global_scope, function_name, fn_type, false);
            }
        } else if (prog->stmts[i]->type == STMT_EXPORT) {
            // Handle exported statements
            Stmt* exported = prog->stmts[i]->export.stmt;
            if (exported->type == STMT_FN) {
                FnStmt* fn = &exported->fn;
                
                // Create function type
                Type* fn_type = make_type(TYPE_FUNCTION);
                fn_type->fn_type.param_count = fn->param_count;
                fn_type->fn_type.param_types = malloc(sizeof(Type*) * fn->param_count);
                for (int j = 0; j < fn->param_count; j++) {
                    Type* param_type = builtin_int; // default
                    if (fn->param_types[j]) {
                        if (fn->param_types[j]->type == EXPR_FN_TYPE) {
                            param_type = check_expr(fn->param_types[j], global_scope);
                        } else if (fn->param_types[j]->type == EXPR_IDENT) {
                            Token type_name = fn->param_types[j]->token;
                            if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                                param_type = builtin_int;
                            } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                                param_type = builtin_string;
                            } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                                param_type = builtin_float;
                            } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                                param_type = builtin_bool;
                            } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                                param_type = builtin_array;
                            } else {
                                // Check if it's a struct type
                                Symbol* type_symbol = find_symbol(global_scope, type_name);
                                if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                                    param_type = type_symbol->type;
                                }
                            }
                        }
                    }
                    fn_type->fn_type.param_types[j] = param_type;
                }
                
                // Determine return type from function signature
                fn_type->fn_type.return_type = builtin_int; // default
                if (fn->return_type && fn->return_type->type == EXPR_IDENT) {
                    Token type_name = fn->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        fn_type->fn_type.return_type = builtin_int;
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        fn_type->fn_type.return_type = builtin_string;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        fn_type->fn_type.return_type = builtin_float;
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        fn_type->fn_type.return_type = builtin_bool;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        fn_type->fn_type.return_type = builtin_array;
                    } else {
                        // Check if it's a struct type
                        Symbol* type_symbol = find_symbol(global_scope, type_name);
                        if (type_symbol && type_symbol->type && type_symbol->type->kind == TYPE_STRUCT) {
                            fn_type->fn_type.return_type = type_symbol->type;
                        }
                    }
                }
                
                add_symbol(global_scope, fn->name, fn_type, false);
            } else if (exported->type == STMT_VAR) {
                // Handle exported variables
                Type* init_type = builtin_int; // Simplified for now
                add_symbol(global_scope, exported->var.name, init_type, !exported->var.is_const);
            }
        } else if (prog->stmts[i]->type == STMT_ENUM) {
            // Enum types are already registered in Pass 0 with proper TYPE_ENUM
            // No need to re-register here (would overwrite with builtin_int)
            // The enum type and variants are already in global_scope
        }
    }
    
    // Second pass: check function bodies
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            SymbolTable local_scope;
            local_scope.parent = global_scope;
            local_scope.capacity = 32;
            local_scope.symbols = calloc(32, sizeof(Symbol));
            local_scope.count = 0;
            
            FnStmt* fn = &prog->stmts[i]->fn;
            
            // Set current function return type for return statement validation
            current_function_return_type = builtin_int; // default
            if (fn->return_type) {
                if (fn->return_type->type == EXPR_FN_TYPE) {
                    // Function type: fn(T1, T2) -> R
                    current_function_return_type = check_expr(fn->return_type, &local_scope);
                } else if (fn->return_type->type == EXPR_CALL) {
                    // Generic type instantiation: HashMap<K,V>, Option<T>, etc.
                    if (fn->return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = fn->return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            current_function_return_type = make_type(TYPE_MAP);
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            current_function_return_type = make_type(TYPE_SET);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            Token concrete = {TOKEN_IDENT, "OptionInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT &&
                                fn->return_type->call.args[0]->token.length == 6 &&
                                memcmp(fn->return_type->call.args[0]->token.start, "string", 6) == 0) {
                                concrete = (Token){TOKEN_IDENT, "OptionString", 12, 0};
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            current_function_return_type = sym ? sym->type : make_type(TYPE_OPTIONAL);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            Token concrete = {TOKEN_IDENT, "ResultInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT &&
                                fn->return_type->call.args[0]->token.length == 6 &&
                                memcmp(fn->return_type->call.args[0]->token.start, "string", 6) == 0) {
                                concrete = (Token){TOKEN_IDENT, "ResultString", 12, 0};
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            current_function_return_type = sym ? sym->type : make_type(TYPE_RESULT);
                        }
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    Type* array_type = make_type(TYPE_ARRAY);
                    if (fn->return_type->array.count > 0 && fn->return_type->array.elements[0]) {
                        // Get element type
                        Expr* elem_type_expr = fn->return_type->array.elements[0];
                        if (elem_type_expr->type == EXPR_IDENT) {
                            Token elem_type_name = elem_type_expr->token;
                            if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                array_type->array_type.element_type = builtin_int;
                            } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                array_type->array_type.element_type = builtin_string;
                            } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                array_type->array_type.element_type = builtin_float;
                            } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                array_type->array_type.element_type = builtin_bool;
                            }
                        }
                    }
                    current_function_return_type = array_type;
                } else if (fn->return_type->type == EXPR_IDENT) {
                    Token type_name = fn->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        current_function_return_type = builtin_int;
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        current_function_return_type = builtin_string;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        current_function_return_type = builtin_float;
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        current_function_return_type = builtin_bool;
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        current_function_return_type = builtin_array;
                    } else {
                        // Check if it's a user-defined type (struct or enum)
                        Symbol* type_symbol = find_symbol(global_scope, type_name);
                        if (type_symbol && type_symbol->type) {
                            current_function_return_type = type_symbol->type;
                        }
                    }
                }
            }
            
            for (int j = 0; j < fn->param_count; j++) {
                // Determine parameter type from type annotation
                Type* param_type = builtin_int; // default
                if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = fn->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = builtin_int;
                        } else if ((type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) ||
                                   (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0)) {
                            param_type = builtin_string;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = builtin_float;
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = builtin_bool;
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = builtin_array;
                        } else {
                            // Check if it's a user-defined type (struct or enum)
                            Symbol* type_symbol = find_symbol(global_scope, type_name);
                            if (type_symbol && type_symbol->type) {
                                param_type = type_symbol->type;
                            }
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type]
                        Type* array_type = make_type(TYPE_ARRAY);
                        if (fn->param_types[j]->array.count > 0 && fn->param_types[j]->array.elements[0]) {
                            Expr* elem_type_expr = fn->param_types[j]->array.elements[0];
                            if (elem_type_expr->type == EXPR_IDENT) {
                                Token elem_type_name = elem_type_expr->token;
                                if (elem_type_name.length == 3 && memcmp(elem_type_name.start, "int", 3) == 0) {
                                    array_type->array_type.element_type = builtin_int;
                                } else if (elem_type_name.length == 6 && memcmp(elem_type_name.start, "string", 6) == 0) {
                                    array_type->array_type.element_type = builtin_string;
                                } else if (elem_type_name.length == 5 && memcmp(elem_type_name.start, "float", 5) == 0) {
                                    array_type->array_type.element_type = builtin_float;
                                } else if (elem_type_name.length == 4 && memcmp(elem_type_name.start, "bool", 4) == 0) {
                                    array_type->array_type.element_type = builtin_bool;
                                } else {
                                    // Check if it's a user-defined type (struct or enum)
                                    Symbol* type_symbol = find_symbol(global_scope, elem_type_name);
                                    if (type_symbol && type_symbol->type) {
                                        array_type->array_type.element_type = type_symbol->type;
                                    }
                                }
                            }
                        }
                        param_type = array_type;
                    }
                }
                
                // T1.5.2: Type check default parameter values
                if (fn->param_defaults && fn->param_defaults[j]) {
                    Type* default_type = check_expr(fn->param_defaults[j], &local_scope);
                    if (default_type && !types_equal(param_type, default_type)) {
                        char param_name[256];
                        snprintf(param_name, sizeof(param_name), "%.*s", 
                                fn->params[j].length, fn->params[j].start);
                        type_error_mismatch(type_to_string(param_type), 
                                          type_to_string(default_type),
                                          param_name, 
                                          fn->params[j].line, 
                                          0);  // Column not available in Token
                    }
                }
                
                add_symbol(&local_scope, fn->params[j], param_type, true);
            }
            
            // T2.5.4: Enhanced return type inference
            if (!fn->return_type) {
                // No explicit return type - infer from function body
                Type* inferred_return = wyn_infer_function_return_type(fn->body, &local_scope);
                if (inferred_return) {
                    current_function_return_type = inferred_return;
                }
            }
            
            // Register function visibility if in a module
            if (current_module_name[0] != '\0') {
                char func_name[128];
                snprintf(func_name, 128, "%.*s", fn->name.length, fn->name.start);
                register_function_visibility(current_module_name, func_name, fn->is_public);
            }
            
            // Set self type for extension methods
            if (fn->is_extension) {
                Symbol* recv = find_symbol(global_scope, fn->receiver_type);
                current_self_type = (recv && recv->type) ? recv->type : NULL;
            }
            
            check_stmt(fn->body, &local_scope);
            current_function_return_type = NULL;
            current_self_type = NULL;
            free(local_scope.symbols);
        } else {
            check_stmt(prog->stmts[i], global_scope);
        }
    }
}

SymbolTable* get_global_scope() {
    return global_scope;
}

bool checker_had_error() {
    return had_error;
}

// T1.5.2: Helper functions for default parameter type checking
bool types_equal(Type* a, Type* b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_VOID:
            return true;
        case TYPE_FUNCTION: {
            // Compare function signatures
            if (a->fn_type.param_count != b->fn_type.param_count) return false;
            for (int i = 0; i < a->fn_type.param_count; i++) {
                if (!types_equal(a->fn_type.param_types[i], b->fn_type.param_types[i])) {
                    return false;
                }
            }
            return types_equal(a->fn_type.return_type, b->fn_type.return_type);
        }
        case TYPE_ARRAY:
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_MAP:
        case TYPE_OPTIONAL:
        case TYPE_UNION:
            // For now, just compare kinds - more detailed comparison can be added later
            return true;
        default:
            return false;
    }
}

const char* type_to_string(Type* type) {
    if (!type) return "unknown";
    
    switch (type->kind) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_STRING: return "string";
        case TYPE_BOOL: return "bool";
        case TYPE_VOID: return "void";
        case TYPE_ARRAY: return "array";
        case TYPE_STRUCT: return "struct";
        case TYPE_ENUM: return "enum";
        case TYPE_FUNCTION: return "function";
        case TYPE_MAP: return "map";
        case TYPE_OPTIONAL: return "optional";
        case TYPE_UNION: return "union";
        default: return "unknown";
    }
}

// TASK-040: Capture analysis for lambda expressions
void analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope) {
    if (!lambda || !body || !scope) return;
    
    // Simple capture analysis - find free variables in lambda body
    // This is a simplified implementation that captures identifiers not in parameters
    
    // Initialize capture arrays
    lambda->captured_vars = malloc(sizeof(Token) * 8);
    lambda->capture_by_move = malloc(sizeof(bool) * 8);
    lambda->captured_count = 0;
    
    // Recursively analyze the body expression for free variables
    analyze_expr_captures(body, lambda, scope);
}

// Helper function to recursively analyze expressions for captures
void analyze_expr_captures(Expr* expr, LambdaExpr* lambda, SymbolTable* scope) {
    if (!expr || !lambda) return;
    
    switch (expr->type) {
        case EXPR_IDENT: {
            // Check if this identifier is a free variable (not a parameter)
            bool is_param = false;
            for (int i = 0; i < lambda->param_count; i++) {
                if (expr->token.length == lambda->params[i].length &&
                    memcmp(expr->token.start, lambda->params[i].start, expr->token.length) == 0) {
                    is_param = true;
                    break;
                }
            }
            
            if (!is_param && lambda->captured_count < 8) {
                // Check if already captured
                bool already_captured = false;
                for (int i = 0; i < lambda->captured_count; i++) {
                    if (expr->token.length == lambda->captured_vars[i].length &&
                        memcmp(expr->token.start, lambda->captured_vars[i].start, expr->token.length) == 0) {
                        already_captured = true;
                        break;
                    }
                }
                
                if (!already_captured) {
                    lambda->captured_vars[lambda->captured_count] = expr->token;
                    lambda->capture_by_move[lambda->captured_count] = false; // Default to reference
                    lambda->captured_count++;
                }
            }
            break;
        }
        case EXPR_BINARY:
            analyze_expr_captures(expr->binary.left, lambda, scope);
            analyze_expr_captures(expr->binary.right, lambda, scope);
            break;
        case EXPR_CALL:
            analyze_expr_captures(expr->call.callee, lambda, scope);
            for (int i = 0; i < expr->call.arg_count; i++) {
                analyze_expr_captures(expr->call.args[i], lambda, scope);
            }
            break;
        case EXPR_UNARY:
            analyze_expr_captures(expr->unary.operand, lambda, scope);
            break;
        // Add more cases as needed for other expression types
        default:
            break;
    }
}
