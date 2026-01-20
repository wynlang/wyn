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

// Forward declarations
void check_stmt(Stmt* stmt, SymbolTable* scope);
Type* check_expr(Expr* expr, SymbolTable* scope);
void analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope);
void analyze_expr_captures(Expr* expr, LambdaExpr* lambda, SymbolTable* scope);

static SymbolTable* global_scope = NULL;
static Type* builtin_int = NULL;
static Type* builtin_float = NULL;
static Type* builtin_string = NULL;
static Type* builtin_bool = NULL;
static Type* builtin_void = NULL;
static Type* builtin_array = NULL;
static bool had_error = false;
static Type* current_function_return_type = NULL;

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
        case TYPE_ARRAY: fprintf(stderr, "array"); break;
        case TYPE_STRUCT:
            if (type->struct_type.name.length > 0) {
                fprintf(stderr, "%.*s", type->struct_type.name.length, type->struct_type.name.start);
            } else {
                fprintf(stderr, "struct");
            }
            break;
        case TYPE_OPTIONAL: // T2.5.1: Optional Type Implementation
            print_type_name(type->optional_type.inner_type);
            fprintf(stderr, "?");
            break;
        case TYPE_RESULT: // TASK-026: Result Type Implementation
            fprintf(stderr, "Result<");
            print_type_name(type->result_type.ok_type);
            fprintf(stderr, ", ");
            print_type_name(type->result_type.err_type);
            fprintf(stderr, ">");
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
    
    // Add built-in functions
    const char* stdlib_funcs[] = {
        "print", "print_float", "print_str", "print_bool", "print_hex", "print_bin", "println", "print_debug", "input", "input_float", "input_line", "printf_wyn", "sin_approx", "cos_approx", "pi_const", "e_const",
        "str_len", "str_eq", "str_concat", "str_upper", "str_lower", "str_contains", "str_starts_with", "str_ends_with", "str_trim",
        "str_replace", "str_split", "str_join", "int_to_str", "str_to_int", "str_repeat", "str_reverse", "str_parse_int", "str_parse_float", "str_free",
        "abs_val", "min", "max", "pow_int", "clamp", "sign", "gcd", "lcm", "is_even", "is_odd",
        "sqrt_int", "ceil_int", "floor_int", "round_int", "abs_float",
        "swap", "clamp_float", "lerp", "map_range",
        "bit_set", "bit_clear", "bit_toggle", "bit_check", "bit_count",
        "arr_sum", "arr_max", "arr_min", "arr_contains", "arr_find", "arr_reverse", "arr_sort", "arr_count", "arr_fill", "arr_all", "arr_join", "arr_map_double", "arr_map_square", "arr_filter_positive", "arr_filter_even", "arr_filter_greater_than_3", "arr_reduce_sum", "arr_reduce_product",
        "file_read", "file_write", "file_exists", "file_size", "file_delete", "file_append", "file_copy", "last_error_get",
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
        "wyn_array_map", "wyn_array_filter", "wyn_array_reduce", "wyn_array_find",
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
        "wyn_crypto_random_bytes", "wyn_crypto_random_hex", "wyn_crypto_xor_cipher"
    };
    
    for (int i = 0; i < 217; i++) {
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
    if (!symbol || symbol->type->kind != TYPE_FUNCTION) return symbol;
    
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
            
            // Allow bool operations
            if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_OR) {
                expr->expr_type = builtin_bool;
                return builtin_bool;
            }
            
            // Comparison operators return bool
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ) {
                expr->expr_type = builtin_bool;
                return builtin_bool;
            }
            
            // Allow string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS && 
                left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
                expr->expr_type = builtin_string;
                return builtin_string;
            }
            
            if (left->kind != right->kind) {
                fprintf(stderr, "Error at line %d: Type mismatch in binary expression\n", 
                        expr->binary.op.line);
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
                for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                    if (expr->call.callee->token.start[i] == ':' && expr->call.callee->token.start[i+1] == ':') {
                        is_qualified = true;
                        break;
                    }
                }
                
                if (is_qualified && !best_match) {
                    // Module-qualified function - assume it's valid
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
                    fprintf(stderr, "Error: No matching overload found for function '%s' with %d arguments\n",
                            func_name, expr->call.arg_count);
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
                        fprintf(stderr, "Error: Variadic function expects at least %d arguments, got %d\n",
                                callee_type->fn_type.param_count, expr->call.arg_count);
                        had_error = true;
                    }
                } else if (expr->call.arg_count != callee_type->fn_type.param_count) {
                    fprintf(stderr, "Error: Parameter count mismatch - function expects %d arguments, got %d\n",
                            callee_type->fn_type.param_count, expr->call.arg_count);
                    had_error = true;
                }
                
                // Check type compatibility for each argument (only for non-variadic params)
                int params_to_check = callee_type->fn_type.param_count;
                for (int i = 0; i < expr->call.arg_count && i < params_to_check; i++) {
                    Type* expected_type = callee_type->fn_type.param_types[i];
                    Type* actual_type = check_expr(expr->call.args[i], scope);
                    
                    if (!wyn_is_type_compatible(expected_type, actual_type)) {
                        fprintf(stderr, "Error: Type mismatch for argument %d - expected ", i + 1);
                        print_type_name(expected_type);
                        fprintf(stderr, ", got ");
                        print_type_name(actual_type);
                        fprintf(stderr, "\n");
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
            
            // Use method signature table for type inference (Phase 1)
            const char* receiver_type = get_receiver_type_string(object_type);
            if (receiver_type) {
                Token method = expr->method_call.method;
                char method_name[256];
                int len = method.length < 255 ? method.length : 255;
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
                        expr->expr_type = builtin_array;
                        return builtin_array;
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
                
                expr->expr_type = builtin_array;
                return builtin_array;
            }
            
            expr->expr_type = builtin_array;
            return builtin_array;
        }
        case EXPR_HASHMAP_LITERAL: {
            // v1.2.3: {} creates a hashmap
            expr->expr_type = builtin_int;  // Placeholder - hashmap type
            return builtin_int;
        }
        case EXPR_HASHSET_LITERAL: {
            // v1.2.3: () creates a hashset
            expr->expr_type = builtin_int;  // Placeholder - hashset type
            return builtin_int;
        }
        case EXPR_INDEX: {
            Type* array_type = check_expr(expr->index.array, scope);
            Type* idx_type = check_expr(expr->index.index, scope);
            
            // Allow string indices for maps, int indices for arrays
            if (array_type && array_type->kind == TYPE_MAP) {
                // Map indexing - allow string keys
                if (idx_type && idx_type->kind != TYPE_STRING) {
                    fprintf(stderr, "Error: Map index must be string\n");
                    return NULL;
                }
                return builtin_int; // Map value type (simplified)
            } else {
                // Array indexing - require int indices
                if (idx_type && idx_type->kind != TYPE_INT) {
                    fprintf(stderr, "Error: Array index must be int\n");
                    return NULL;
                }
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
            if (expr->if_expr.then_expr) {
                check_expr(expr->if_expr.then_expr, scope);
            }
            if (expr->if_expr.else_expr) {
                check_expr(expr->if_expr.else_expr, scope);
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
            (void)check_expr(expr->field_access.object, scope);  // Validate object
            
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
            // TASK-026: Ok(value) expression - creates Result type
            if (!expr->option.value) {
                fprintf(stderr, "Error: Ok() requires a value\n");
                had_error = true;
                return NULL;
            }
            
            Type* value_type = check_expr(expr->option.value, scope);
            if (!value_type) return NULL;
            
            // Create Result<T, String> type (default error type is string)
            Type* result_type = make_result_type(value_type, builtin_string);
            expr->expr_type = result_type;
            return result_type;
        }
        case EXPR_ERR: {
            // TASK-026: Err(error) expression - creates Result type
            if (!expr->option.value) {
                fprintf(stderr, "Error: Err() requires an error value\n");
                had_error = true;
                return NULL;
            }
            
            Type* error_type = check_expr(expr->option.value, scope);
            if (!error_type) return NULL;
            
            // Create Result<void, E> type (default success type is void for errors)
            Type* result_type = make_result_type(builtin_void, error_type);
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
            // T2.5.1: Some(value) expression - creates optional type
            if (!expr->option.value) {
                fprintf(stderr, "Error: Some() requires a value\n");
                had_error = true;
                return NULL;
            }
            
            Type* inner_type = check_expr(expr->option.value, scope);
            if (!inner_type) return NULL;
            
            // Create optional type containing the inner type
            Type* optional_type = make_type(TYPE_OPTIONAL);
            optional_type->optional_type.inner_type = inner_type;
            expr->expr_type = optional_type;
            return optional_type;
        }
        case EXPR_NONE: {
            // T2.5.1: None expression - creates empty optional type
            // For now, create a generic optional type - in a full implementation,
            // this would be inferred from context
            Type* optional_type = make_type(TYPE_OPTIONAL);
            optional_type->optional_type.inner_type = builtin_void; // Generic None
            expr->expr_type = optional_type;
            return optional_type;
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
                if (arm->pattern.type == TOKEN_UNDERSCORE) {
                    has_wildcard = true;
                }
                
                // Type-check the result expression
                Type* arm_type = check_expr(arm->result, scope);
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
            
            // Check exhaustiveness for enum types
            if (match_value_type->kind == TYPE_ENUM && !has_wildcard) {
                // For enum types, check if all variants are covered
                bool* covered = calloc(match_value_type->enum_type.variant_count, sizeof(bool));
                
                for (int i = 0; i < expr->match.arm_count; i++) {
                    MatchArm* arm = &expr->match.arms[i];
                    
                    // Check if this pattern matches an enum variant
                    if (arm->pattern.type == TOKEN_IDENT) {
                        // Look for enum variant pattern like Status::PENDING
                        for (int v = 0; v < match_value_type->enum_type.variant_count; v++) {
                            Token variant = match_value_type->enum_type.variants[v];
                            
                            // Simple string comparison for now
                            if (arm->pattern.length >= variant.length &&
                                memcmp(arm->pattern.start + arm->pattern.length - variant.length,
                                       variant.start, variant.length) == 0) {
                                covered[v] = true;
                                break;
                            }
                        }
                    }
                }
                
                // Check if any variants are missing
                for (int v = 0; v < match_value_type->enum_type.variant_count; v++) {
                    if (!covered[v]) {
                        Token variant = match_value_type->enum_type.variants[v];
                        fprintf(stderr, "Error: non-exhaustive match, missing case: %.*s\n",
                                variant.length, variant.start);
                        had_error = true;
                        free(covered);
                        return NULL;
                    }
                }
                
                free(covered);
            }
            
            expr->expr_type = result_type ? result_type : builtin_void;
            return expr->expr_type;
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
                init_type = check_expr(stmt->var.type, scope);
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
                        // Allow returning Result from any function
                        break;
                    }
                    if (current_function_return_type->kind != return_expr_type->kind) {
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
                add_symbol(scope, stmt->for_stmt.loop_var, builtin_int, false);
            }
            
            check_stmt(stmt->for_stmt.body, scope);
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
                }
            }
            break;
        case STMT_TYPE_ALIAS:
            // Register type alias in global scope
            add_symbol(global_scope, stmt->type_alias.name, builtin_int, false);
            break;
        case STMT_IMPORT:
            // Register module namespace in scope
            add_symbol(scope, stmt->import.module, builtin_int, false);
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
                
                // Type-check the case body
                if (match_case->body) {
                    check_stmt(match_case->body, scope);
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
                            if (match_case->pattern && match_case->pattern->type == PATTERN_IDENT) {
                                Token pattern_name = match_case->pattern->ident.name;
                                
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
                                if (match_case->pattern && match_case->pattern->type == PATTERN_IDENT) {
                                    Token pattern_name = match_case->pattern->ident.name;
                                    
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
    // Pass 0: Register all struct types and enums first (so functions can reference them)
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
            }
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
        }
    }
    
    // First pass: process imports and register functions with their signatures
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            // Load and process imported module
            ImportStmt* import = &prog->stmts[i]->import;
            char module_path[256];
            snprintf(module_path, 256, "%.*s.wyn", import->module.length, import->module.start);
            
            // For now, just register some common module functions
            // In a full implementation, we'd parse the module file
            if (import->module.length == 4 && memcmp(import->module.start, "math", 4) == 0) {
                // Register the module itself as a namespace
                add_symbol(global_scope, import->module, builtin_int, false); // Module type (simplified)
                
                // Register math module functions with both qualified and unqualified names
                Token add_name = {TOKEN_IDENT, "add", 3, 0};
                Token multiply_name = {TOKEN_IDENT, "multiply", 8, 0};
                Token pi_name = {TOKEN_IDENT, "PI", 2, 0};
                
                Token math_add = {TOKEN_IDENT, "math.add", 8, 0};
                Token math_multiply = {TOKEN_IDENT, "math.multiply", 13, 0};
                Token math_pi = {TOKEN_IDENT, "math.PI", 7, 0};
                
                Type* fn_type = make_type(TYPE_FUNCTION);
                fn_type->fn_type.param_count = 2;
                fn_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
                fn_type->fn_type.param_types[0] = builtin_int;
                fn_type->fn_type.param_types[1] = builtin_int;
                fn_type->fn_type.return_type = builtin_int;
                
                // Register both qualified and unqualified names
                add_symbol(global_scope, add_name, fn_type, false);
                add_symbol(global_scope, multiply_name, fn_type, false);
                add_symbol(global_scope, pi_name, builtin_float, false);
                add_symbol(global_scope, math_add, fn_type, false);
                add_symbol(global_scope, math_multiply, fn_type, false);
                add_symbol(global_scope, math_pi, builtin_float, false);
            }
        } else if (prog->stmts[i]->type == STMT_FN) {
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
                        param_type = builtin_array;
                    }
                }
                fn_type->fn_type.param_types[j] = param_type;
            }
            
            // Determine return type from function signature or infer from body
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
                    // Check if it's a user-defined type (struct or enum)
                    Symbol* type_symbol = find_symbol(global_scope, type_name);
                    if (type_symbol && type_symbol->type) {
                        fn_type->fn_type.return_type = type_symbol->type;
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
            add_function_overload(global_scope, function_name, fn_type, false);
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
                    if (fn->param_types[j] && fn->param_types[j]->type == EXPR_IDENT) {
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
            EnumStmt* enum_decl = &prog->stmts[i]->enum_decl;
            
            // Register enum type
            add_symbol(global_scope, enum_decl->name, builtin_int, false);
            
            // Register each enum member with qualified name (EnumName.MEMBER)
            for (int j = 0; j < enum_decl->variant_count; j++) {
                char qualified_member[128];
                snprintf(qualified_member, 128, "%.*s.%.*s",
                        enum_decl->name.length, enum_decl->name.start,
                        enum_decl->variants[j].length, enum_decl->variants[j].start);
                
                Token qualified_token = {TOKEN_IDENT, strdup(qualified_member), (int)strlen(qualified_member), 0};
                add_symbol(global_scope, qualified_token, builtin_int, false);
            }
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
            if (fn->return_type && fn->return_type->type == EXPR_IDENT) {
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
                        param_type = builtin_array;
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
            
            check_stmt(fn->body, &local_scope);
            current_function_return_type = NULL; // Reset after function
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
        case TYPE_ARRAY:
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_FUNCTION:
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
