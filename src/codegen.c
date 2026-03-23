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
FILE* codegen_get_output(void) { return out; }

// Centralized name collision check: C keywords + runtime function names
// Returns true if 'name' would collide with a C keyword or wyn_runtime.h symbol
int is_c_name_collision(const char* name) {
    // C keywords and standard library
    static const char* reserved[] = {
        "double","float","int","char","void","return","if","else","while","for",
        "switch","case","break","continue","struct","union","enum","typedef",
        "static","extern","register","volatile","const","signed","unsigned",
        "short","long","auto","default","do","goto","sizeof",
        "div","abs","exit","free","malloc","printf","puts","remove","rename",
        "signal","time","clock","rand","log","exp","sqrt",
        // wyn_runtime.h functions that collide with user names
        "swap","clamp","clamp_float","lerp","map_range","sign",
        "gcd","lcm","pow_int","abs_val","abs_float","sqrt_int","ceil_int",
        "floor_int","round_int",
        "input","input_float","input_line",
        "print","print_float","print_str","print_bool","print_hex","print_bin",
        "println","print_debug","print_int","print_value","print_error",
        "print_int_no_nl","print_float_no_nl","print_str_no_nl","print_bool_no_nl",
        "print_array","print_array_no_nl","print_args_impl","printf_wyn",
        "panic","todo","assert_eq","assert_true","assert_false",
        "range","range_has_next","range_next","len",
        "array_new","array_push","array_push_str","array_push_int","array_push_array",
        "array_pop","array_len","array_clear","array_contains","array_contains_str",
        "array_index_of","array_index_of_str","array_remove_at","array_remove_str",
        "array_remove_value","array_insert","array_insert_at","array_reverse",
        "array_reverse_copy","array_sort","array_sort_copy","array_sort_str",
        "array_unique_int","array_sum","array_min","array_max","array_average",
        "array_first","array_last","array_is_empty","array_count","array_each",
        "array_every","array_concat","array_take","array_skip","array_flat_map",
        "array_get_nested_int","array_get_nested3_int","array_length_dyn",
        "array_new_int","array_filter","array_map","array_reduce","array_from_values",
        "arr_sum","arr_max","arr_min","arr_contains","arr_find","arr_reverse",
        "arr_sort","arr_count","arr_fill","arr_all","arr_join",
        "arr_map_double","arr_map_square","arr_filter_positive","arr_filter_even",
        "arr_filter_greater_than_3","arr_reduce_sum","arr_reduce_product",
        "str_len","str_eq","str_contains","str_starts_with","str_ends_with",
        "str_count","str_contains_substr","str_free","str_index_of",
        "str_parse_int_failed","str_parse_float","str_to_float",
        "string_len","string_length","string_contains","string_starts_with",
        "string_ends_with","string_equals","string_index_of","string_last_index_of",
        "string_count","string_is_empty","string_is_alpha","string_is_digit",
        "string_is_alnum","string_is_numeric","string_is_whitespace",
        "string_chars","string_lines","string_words","string_split",
        "string_to_bytes","string_format",
        "char_at","char_to_int","char_from_int","char_to_upper","char_to_lower",
        "char_is_alpha","char_is_numeric","char_is_alphanumeric",
        "char_is_uppercase","char_is_lowercase","char_is_whitespace",
        "is_numeric","is_even","is_odd","split_get","split_count",
        "sin_approx","cos_approx","pi_const","e_const",
        "int_abs","int_sign","int_clamp","int_min","int_max","int_pow",
        "int_is_even","int_is_odd","int_is_positive","int_is_negative","int_is_zero",
        "int_times","int_to_float","int_array_len","int_array_push",
        "float_abs","float_sign","float_clamp","float_min","float_max","float_pow",
        "float_sqrt","float_floor","float_ceil","float_round","float_round_to",
        "float_sin","float_cos","float_tan","float_asin","float_acos","float_atan",
        "float_exp","float_log","float_log2","float_log10",
        "float_is_nan","float_is_infinite","float_is_finite",
        "float_is_positive","float_is_negative",
        "bool_and","bool_or","bool_not","bool_xor","bool_to_int",
        "bit_set","bit_clear","bit_toggle","bit_check","bit_count",
        "file_exists","file_size","file_file_size","file_delete","file_copy",
        "file_move","file_write","file_list_dir","file_mkdir","file_rmdir",
        "file_is_file","file_is_dir","file_create_dir","file_create_dir_all",
        "file_remove_dir_all","file_modified_time",
        "random_int","random_range","random_float","random_bool",
        "random_choice_int","random_seed_auto","seed_random",
        "sleep_ms","exit_program","setenv_var",
        "hashmap_new","hashmap_insert","hashmap_get","hashmap_has","hashmap_remove",
        "hashmap_free","hashmap_set","hashmap_clear","hashmap_count","hashmap_len",
        "hashmap_keys","hashmap_keys_string","hashmap_values",
        "hashmap_insert_int","hashmap_insert_float","hashmap_insert_string","hashmap_insert_bool",
        "hashmap_get_int","hashmap_get_float","hashmap_get_string","hashmap_get_bool",
        "hashset_new","hashset_add","hashset_contains","hashset_remove","hashset_free",
        "set_clear","set_is_subset","set_is_superset","set_is_disjoint",
        "json_parse","json_get_string","json_get_int","json_free","json_new",
        "json_set_int","json_set_string","json_stringify",
        "hash_string","regex_match",
        "map_get","map_set","map_has","map_remove","map_len","map_is_empty",
        "map_clear","map_merge","map_get_or_default",
        "http_set_header",
        "none","none_int","none_float","none_string","none_bool",
        "some","some_int","some_float","some_string","some_bool",
        "ok_int","ok_float","ok_string","ok_bool","ok_void",
        "err_int","err_float","err_string","err_bool",
        "unwrap_int","unwrap_float","unwrap_string","unwrap_bool",
        "unwrap_result_int","unwrap_result_float","unwrap_result_string","unwrap_result_bool",
        "unwrap_error_int","unwrap_error_float","unwrap_error_string","unwrap_error_bool",
        "validate_buffer","validate_memory_range","validate_string","validate_string_bounds",
        NULL
    };
    for (int i = 0; reserved[i]; i++) {
        if (strcmp(name, reserved[i]) == 0) return 1;
    }
    return 0;
}

// Current block context for liveness analysis
static Stmt** current_block_stmts = NULL;
static int current_block_count = 0;
static int current_stmt_idx = 0;

// Escape analysis: skip wyn_strdup for temporaries that don't escape
static bool codegen_skip_strdup = false;

// Spawn array: emit WynIntArray instead of WynArray for current init
static bool codegen_emit_int_array = false;

// Module emission tracking (reset per compilation)
static bool modules_emitted_this_compilation = false;

// Current module being emitted (for prefixing internal calls)
static const char* current_module_prefix = NULL;

// Identifier scope tracking for proper identifier resolution
__attribute__((unused)) static IdentScope* current_ident_scope = NULL;
__attribute__((unused)) static IdentScope* module_ident_scope = NULL;

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

// Track user-defined function names that collide with runtime
static char* user_collision_fns[512];
static int user_collision_fn_count = 0;

void register_user_collision(const char* name) {
    if (user_collision_fn_count < 512)
        user_collision_fns[user_collision_fn_count++] = strdup(name);
}

int is_user_collision(const char* name) {
    for (int i = 0; i < user_collision_fn_count; i++)
        if (strcmp(user_collision_fns[i], name) == 0) return 1;
    return 0;
}

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
static bool current_param_mut[64];
static int current_param_count = 0;
char current_param_types[64][64];

// Trait name tracking for vtable dispatch
static char trait_names[64][64];
static int trait_name_count = 0;
static void register_trait_name(const char* name, int len) {
    if (trait_name_count < 64) {
        snprintf(trait_names[trait_name_count], 64, "%.*s", len, name);
        trait_name_count++;
    }
}
static int is_trait_type(const char* name, int len) {
    for (int i = 0; i < trait_name_count; i++) {
        if ((int)strlen(trait_names[i]) == len && memcmp(trait_names[i], name, len) == 0) return 1;
    }
    return 0;
}

// Trait impl tracking: which struct implements which trait
static struct { char struct_name[64]; char trait_name[64]; } trait_impls[128];
static int trait_impl_count = 0;
static void register_trait_impl(const char* sname, int slen, const char* tname, int tlen) {
    if (trait_impl_count < 128) {
        snprintf(trait_impls[trait_impl_count].struct_name, 64, "%.*s", slen, sname);
        snprintf(trait_impls[trait_impl_count].trait_name, 64, "%.*s", tlen, tname);
        trait_impl_count++;
    }
}
static const char* find_trait_for_struct(const char* sname) {
    for (int i = 0; i < trait_impl_count; i++) {
        if (strcmp(trait_impls[i].struct_name, sname) == 0) return trait_impls[i].trait_name;
    }
    return NULL;
}

// Function trait param tracking: fn_name -> param_index -> trait_name
static struct { char fn_name[64]; int param_idx; char trait_name[64]; } fn_trait_params[128];
static int fn_trait_param_count = 0;
static void register_fn_trait_param(const char* fn, const char* trait, int idx) {
    if (fn_trait_param_count < 128) {
        snprintf(fn_trait_params[fn_trait_param_count].fn_name, 64, "%s", fn);
        snprintf(fn_trait_params[fn_trait_param_count].trait_name, 64, "%s", trait);
        fn_trait_params[fn_trait_param_count].param_idx = idx;
        fn_trait_param_count++;
    }
}
static const char* get_fn_trait_param(const char* fn, int idx) {
    for (int i = 0; i < fn_trait_param_count; i++) {
        if (strcmp(fn_trait_params[i].fn_name, fn) == 0 && fn_trait_params[i].param_idx == idx)
            return fn_trait_params[i].trait_name;
    }
    return NULL;
}

// Local variable tracking for current function
static char* current_function_locals[256];
static int current_local_count = 0;

// Track current function return type for Ok/Err/Some/None resolution
// Values: "ResultInt", "ResultString", "OptionInt", "OptionString", or NULL
static const char* current_fn_return_kind = NULL;

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

__attribute__((unused)) static void register_parameter(const char* name) {
    if (current_param_count < 64) {
        current_param_mut[current_param_count] = false;
        current_param_types[current_param_count][0] = 0;
        current_function_params[current_param_count++] = strdup(name);
    }
}

static void register_parameter_mut(const char* name, bool is_mut) {
    if (current_param_count < 64) {
        current_param_mut[current_param_count] = is_mut;
        current_param_types[current_param_count][0] = 0;
        current_function_params[current_param_count++] = strdup(name);
    }
}

static void register_parameter_typed(const char* name, const char* type_name, bool is_mut) {
    if (current_param_count < 64) {
        current_param_mut[current_param_count] = is_mut;
        if (type_name) snprintf(current_param_types[current_param_count], 64, "%s", type_name);
        else current_param_types[current_param_count][0] = 0;
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

static bool is_mut_parameter(const char* name) {
    for (int i = 0; i < current_param_count; i++) {
        if (strcmp(current_function_params[i], name) == 0) {
            return current_param_mut[i];
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

// Track which local variables are arrays (for method dispatch)
static char* array_var_names[256];
static int array_var_count = 0;
static void register_array_var(const char* name) {
    if (array_var_count < 256) array_var_names[array_var_count++] = strdup(name);
}
int is_known_array_var(const char* name) {
    for (int i = 0; i < array_var_count; i++) {
        if (strcmp(array_var_names[i], name) == 0) return 1;
    }
    return 0;
}

// String content array tracking — arrays whose elements are strings
static char* str_array_var_names[64];
static int str_array_var_count = 0;
void register_str_array_var(const char* name) {
    for (int i = 0; i < str_array_var_count; i++)
        if (strcmp(str_array_var_names[i], name) == 0) return;
    if (str_array_var_count < 64) str_array_var_names[str_array_var_count++] = strdup(name);
}
int is_str_array_var(const char* name) {
    for (int i = 0; i < str_array_var_count; i++)
        if (strcmp(str_array_var_names[i], name) == 0) return 1;
    return 0;
}

// String variable tracking for RC release on reassignment
static char* string_var_names[256];
static int string_var_count = 0;
static int string_var_scope_depth = 0;

// Scope stack: tracks string_var_count at each scope entry for block-scoped release
#define SCOPE_STACK_MAX 64
static int scope_var_count_stack[SCOPE_STACK_MAX];
static int scope_stack_top = 0;

void push_string_scope(void) {
    if (scope_stack_top < SCOPE_STACK_MAX)
        scope_var_count_stack[scope_stack_top++] = string_var_count;
}
void pop_string_scope_and_release(void) {
    if (scope_stack_top <= 0) return;
    int saved = scope_var_count_stack[--scope_stack_top];
    extern FILE* codegen_get_output(void);
    FILE* out = codegen_get_output();
    if (out) {
        for (int i = saved; i < string_var_count; i++)
            fprintf(out, "wyn_rc_release(%s); ", string_var_names[i]);
    }
    // Restore count — inner-scope vars are no longer tracked
    string_var_count = saved;
}
// Emit releases for current block's vars (for continue/break)
void emit_block_string_releases(void) {
    if (scope_stack_top <= 0) return;
    int saved = scope_var_count_stack[scope_stack_top - 1];
    extern FILE* codegen_get_output(void);
    FILE* out = codegen_get_output();
    if (out) {
        for (int i = saved; i < string_var_count; i++)
            fprintf(out, "wyn_rc_release(%s); ", string_var_names[i]);
    }
}
void register_string_var(const char* name) {
    // Always register for type detection (used by + operator)
    for (int i = 0; i < string_var_count; i++)
        if (strcmp(string_var_names[i], name) == 0) return;
    if (string_var_count < 256) string_var_names[string_var_count++] = strdup(name);
}
// Only top-level string vars are released at scope exit
static char* string_var_releasable[256];
static int string_var_releasable_count = 0;
void register_releasable_string_var(const char* name) {
    if (string_var_scope_depth > 0) return;
    for (int i = 0; i < string_var_releasable_count; i++)
        if (strcmp(string_var_releasable[i], name) == 0) return;
    if (string_var_releasable_count < 256) string_var_releasable[string_var_releasable_count++] = strdup(name);
}
int is_string_var(const char* name) {
    for (int i = 0; i < string_var_count; i++)
        if (strcmp(string_var_names[i], name) == 0) return 1;
    return 0;
}
void unregister_string_var(const char* name) {
    for (int i = 0; i < string_var_count; i++) {
        if (strcmp(string_var_names[i], name) == 0) {
            free(string_var_names[i]);
            string_var_names[i] = string_var_names[--string_var_count];
            break;
        }
    }
    for (int i = 0; i < string_var_releasable_count; i++) {
        if (strcmp(string_var_releasable[i], name) == 0) {
            free(string_var_releasable[i]);
            string_var_releasable[i] = string_var_releasable[--string_var_releasable_count];
            return;
        }
    }
}
void reset_string_vars(void) { string_var_count = 0; string_var_releasable_count = 0; string_var_scope_depth = -1; }
void emit_string_releases(const char* except_var) {
    extern FILE* codegen_get_output(void);
    FILE* out = codegen_get_output();
    if (!out) return;
    for (int i = 0; i < string_var_releasable_count; i++) {
        if (except_var && strcmp(string_var_releasable[i], except_var) == 0) continue;
        fprintf(out, "wyn_rc_release(%s); ", string_var_releasable[i]);
    }
}
int get_string_var_count(void) { return string_var_releasable_count; }

// Liveness: check if a variable name appears in an expression
static int expr_references_var(Expr* e, const char* name) {
    if (!e) return 0;
    switch (e->type) {
        case EXPR_IDENT: {
            int nl = strlen(name);
            return (e->token.length == nl && memcmp(e->token.start, name, nl) == 0);
        }
        case EXPR_BINARY: return expr_references_var(e->binary.left, name) || expr_references_var(e->binary.right, name);
        case EXPR_UNARY: return expr_references_var(e->unary.operand, name);
        case EXPR_CALL:
            if (expr_references_var(e->call.callee, name)) return 1;
            for (int i = 0; i < e->call.arg_count; i++)
                if (expr_references_var(e->call.args[i], name)) return 1;
            return 0;
        case EXPR_METHOD_CALL:
            if (expr_references_var(e->method_call.object, name)) return 1;
            for (int i = 0; i < e->method_call.arg_count; i++)
                if (expr_references_var(e->method_call.args[i], name)) return 1;
            return 0;
        case EXPR_ASSIGN: return expr_references_var(e->assign.value, name);
        case EXPR_AWAIT: return expr_references_var(e->await.expr, name);
        case EXPR_SPAWN: return expr_references_var(e->spawn.call, name);
        case EXPR_INDEX:
            return expr_references_var(e->index.array, name) || expr_references_var(e->index.index, name);
        default: return 0;
    }
}

// Check if variable is referenced in remaining statements of a block
static int stmt_references_var(Stmt* s, const char* name);
static int block_references_var_from(Stmt** stmts, int count, int from_idx, const char* name) {
    for (int i = from_idx; i < count; i++)
        if (stmt_references_var(stmts[i], name)) return 1;
    return 0;
}

static int stmt_references_var(Stmt* s, const char* name) {
    if (!s) return 0;
    switch (s->type) {
        case STMT_EXPR: return expr_references_var(s->expr, name);
        case STMT_VAR: return expr_references_var(s->var.init, name);
        case STMT_RETURN: return expr_references_var(s->ret.value, name);
        case STMT_IF:
            if (expr_references_var(s->if_stmt.condition, name)) return 1;
            if (stmt_references_var(s->if_stmt.then_branch, name)) return 1;
            if (stmt_references_var(s->if_stmt.else_branch, name)) return 1;
            return 0;
        case STMT_WHILE:
            if (expr_references_var(s->while_stmt.condition, name)) return 1;
            return stmt_references_var(s->while_stmt.body, name);
        case STMT_FOR:
            if (expr_references_var(s->for_stmt.condition, name)) return 1;
            return stmt_references_var(s->for_stmt.body, name);
        case STMT_BLOCK:
            return block_references_var_from(s->block.stmts, s->block.count, 0, name);
        default: return 0;
    }
}

// Check if var is used after stmt at index `after_idx` in a block
int var_is_live_after(Stmt** stmts, int count, int after_idx, const char* name) {
    return block_references_var_from(stmts, count, after_idx + 1, name);
}

// Check if a function body contains yield points or external calls
static int expr_has_yield(Expr* e) {
    if (!e) return 0;
    if (e->type == EXPR_AWAIT) return 1;
    switch (e->type) {
        case EXPR_BINARY: return expr_has_yield(e->binary.left) || expr_has_yield(e->binary.right);
        case EXPR_UNARY: return expr_has_yield(e->unary.operand);
        case EXPR_CALL:
            // Any function call makes it non-inlineable (conservative)
            return 1;
        case EXPR_METHOD_CALL:
            // Any method call makes it non-inlineable
            return 1;
        case EXPR_ASSIGN: return expr_has_yield(e->assign.value);
        default: return 0;
    }
}
static int stmt_has_yield(Stmt* s) {
    if (!s) return 0;
    switch (s->type) {
        case STMT_YIELD: return 1;
        case STMT_EXPR: return expr_has_yield(s->expr);
        case STMT_VAR: return expr_has_yield(s->var.init);
        case STMT_RETURN: return expr_has_yield(s->ret.value);
        case STMT_IF:
            return expr_has_yield(s->if_stmt.condition) ||
                   stmt_has_yield(s->if_stmt.then_branch) ||
                   stmt_has_yield(s->if_stmt.else_branch);
        case STMT_WHILE: return expr_has_yield(s->while_stmt.condition) || stmt_has_yield(s->while_stmt.body);
        case STMT_FOR: return expr_has_yield(s->for_stmt.condition) || stmt_has_yield(s->for_stmt.body);
        case STMT_BLOCK:
            for (int i = 0; i < s->block.count; i++)
                if (stmt_has_yield(s->block.stmts[i])) return 1;
            return 0;
        default: return 0;
    }
}
// L3: Check if a function statement contains yield (is a generator)
static int _has_yield_stmt(Stmt* s) {
    if (!s) return 0;
    if (s->type == STMT_YIELD) return 1;
    if (s->type == STMT_BLOCK) { for (int i = 0; i < s->block.count; i++) if (_has_yield_stmt(s->block.stmts[i])) return 1; }
    if (s->type == STMT_FOR && _has_yield_stmt(s->for_stmt.body)) return 1;
    if (s->type == STMT_WHILE && _has_yield_stmt(s->while_stmt.body)) return 1;
    if (s->type == STMT_IF && (_has_yield_stmt(s->if_stmt.then_branch) || _has_yield_stmt(s->if_stmt.else_branch))) return 1;
    return 0;
}
int fn_is_generator(Stmt* fn_stmt) {
    if (!fn_stmt || fn_stmt->type != STMT_FN) return 0;
    return _has_yield_stmt(fn_stmt->fn.body);
}
int function_can_inline(const char* name) {
    extern Program* current_program;
    if (!current_program) return 0;
    for (int i = 0; i < current_program->count; i++) {
        if (current_program->stmts[i]->type == STMT_FN) {
            FnStmt* fn = &current_program->stmts[i]->fn;
            if ((int)strlen(name) == fn->name.length && memcmp(name, fn->name.start, fn->name.length) == 0) {
                return !stmt_has_yield(fn->body);
            }
        }
    }
    return 0; // unknown function — don't inline
}

static char* sb_var_names[64];
static int sb_var_count = 0;
static void register_sb_var(const char* name) {
    if (sb_var_count < 64) { sb_var_names[sb_var_count++] = strdup(name); }
}

static char* float_var_names[256];
static int float_var_count = 0;
void register_float_var(const char* name) {
    if (float_var_count < 256) float_var_names[float_var_count++] = strdup(name);
}
int is_known_float_var(const char* name) {
    for (int i = 0; i < float_var_count; i++) {
        if (strcmp(float_var_names[i], name) == 0) return 1;
    }
    return 0;
}
int is_known_sb_var(const char* name) {
    for (int i = 0; i < sb_var_count; i++) {
        if (strcmp(sb_var_names[i], name) == 0) return 1;
    }
    return 0;
}

// Variable shadowing: track declared names and return shadow suffix
static struct { char name[64]; int count; } shadow_vars[512];
static int shadow_var_count = 0;

// Returns shadow count for a variable name (0 = first declaration)
int get_shadow_suffix(const char* name) {
    for (int i = 0; i < shadow_var_count; i++) {
        if (strcmp(shadow_vars[i].name, name) == 0) {
            return ++shadow_vars[i].count;
        }
    }
    if (shadow_var_count < 512) {
        strncpy(shadow_vars[shadow_var_count].name, name, 63);
        shadow_vars[shadow_var_count].name[63] = 0;
        shadow_vars[shadow_var_count].count = 0;
        shadow_var_count++;
    }
    return 0;
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
    bool is_closure; // true if this lambda is returned (uses WynClosure)
} LambdaFunction;

static LambdaFunction lambda_functions[256];
static int lambda_count = 0;
static int lambda_id_counter = 0;
static int lambda_ref_counter = 0;
static bool in_return_lambda = false;
Program* current_program = NULL;

// Look up a struct method's return type string ("float", "string", "int", etc.)
const char* lookup_struct_method_return_type(const char* struct_name, const char* method_name) {
    if (!current_program) return NULL;
    for (int i = 0; i < current_program->count; i++) {
        // Check struct methods
        if (current_program->stmts[i]->type == STMT_STRUCT) {
            StructStmt* s = &current_program->stmts[i]->struct_decl;
            if (s->name.length == (int)strlen(struct_name) &&
                memcmp(s->name.start, struct_name, s->name.length) == 0) {
                for (int j = 0; j < s->method_count; j++) {
                    FnStmt* m = s->methods[j];
                    if (m->name.length == (int)strlen(method_name) &&
                        memcmp(m->name.start, method_name, m->name.length) == 0) {
                        if (m->return_type && m->return_type->type == EXPR_IDENT) {
                            static char buf[64];
                            int len = m->return_type->token.length < 63 ? m->return_type->token.length : 63;
                            memcpy(buf, m->return_type->token.start, len);
                            buf[len] = 0;
                            return buf;
                        }
                    }
                }
            }
        }
        // Check impl blocks
        if (current_program->stmts[i]->type == STMT_IMPL) {
            Token impl_name = current_program->stmts[i]->impl.type_name;
            if (impl_name.length == (int)strlen(struct_name) &&
                memcmp(impl_name.start, struct_name, impl_name.length) == 0) {
                for (int j = 0; j < current_program->stmts[i]->impl.method_count; j++) {
                    FnStmt* ms = current_program->stmts[i]->impl.methods[j];
                    if (ms->name.length == (int)strlen(method_name) &&
                        memcmp(ms->name.start, method_name, ms->name.length) == 0) {
                        if (ms->return_type && ms->return_type->type == EXPR_IDENT) {
                            static char buf2[64];
                            int len = ms->return_type->token.length < 63 ? ms->return_type->token.length : 63;
                            memcpy(buf2, ms->return_type->token.start, len);
                            buf2[len] = 0;
                            return buf2;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

// Track lambda variable names and their captures for call site injection
typedef struct {
    char var_name[64];
    char name[64];      // same as var_name, for lookup
    int name_len;
    char captured_vars[16][64];
    int capture_count;
    bool is_closure;    // true if this var holds a WynClosure
} LambdaVarInfo;

static LambdaVarInfo lambda_var_info[256];
static int lambda_var_count = 0;

// Spawn wrapper collection
typedef struct {
    char func_name[256];
    int arg_count;
    int spawn_id;
    int returns_void;
    int returns_string;
    char return_type[64];
    int can_inline;     // 1 if function has no yield points (no await/channel)
} SpawnWrapper;

static SpawnWrapper spawn_wrappers[256];
static int spawn_wrapper_count = 0;
static int spawn_id_counter = 0;

// Spawn array tracking — arrays that hold Future pointers use WynIntArray
static char spawn_array_vars[64][256];
static int spawn_array_count = 0;
void register_spawn_array(const char* name) {
    for (int i = 0; i < spawn_array_count; i++)
        if (strcmp(spawn_array_vars[i], name) == 0) return;
    if (spawn_array_count < 64)
        strcpy(spawn_array_vars[spawn_array_count++], name);
}
int is_spawn_array(const char* name) {
    for (int i = 0; i < spawn_array_count; i++)
        if (strcmp(spawn_array_vars[i], name) == 0) return 1;
    return 0;
}

// String future tracking — variables that hold futures from string-returning spawns
static char string_future_vars[64][256];
static int string_future_count = 0;
void register_string_future(const char* name) {
    for (int i = 0; i < string_future_count; i++)
        if (strcmp(string_future_vars[i], name) == 0) return;
    if (string_future_count < 64)
        strcpy(string_future_vars[string_future_count++], name);
}

// String spawn array tracking — arrays that hold futures from string-returning spawns
static char string_spawn_array_vars[64][256];
static int string_spawn_array_count = 0;
void register_string_spawn_array(const char* name) {
    for (int i = 0; i < string_spawn_array_count; i++)
        if (strcmp(string_spawn_array_vars[i], name) == 0) return;
    if (string_spawn_array_count < 64)
        strcpy(string_spawn_array_vars[string_spawn_array_count++], name);
}
int is_string_spawn_array(const char* name) {
    for (int i = 0; i < string_spawn_array_count; i++)
        if (strcmp(string_spawn_array_vars[i], name) == 0) return 1;
    return 0;
}
int is_string_future(const char* name) {
    for (int i = 0; i < string_future_count; i++)
        if (strcmp(string_future_vars[i], name) == 0) return 1;
    return 0;
}

// Struct future tracking — variables that hold futures from struct-returning spawns
static char struct_future_vars[64][256];
static char struct_future_types[64][64];
static int struct_future_count = 0;
void register_struct_future(const char* name, const char* type_name) {
    for (int i = 0; i < struct_future_count; i++)
        if (strcmp(struct_future_vars[i], name) == 0) { strncpy(struct_future_types[i], type_name, 63); return; }
    if (struct_future_count < 64) {
        strcpy(struct_future_vars[struct_future_count], name);
        strncpy(struct_future_types[struct_future_count], type_name, 63);
        struct_future_count++;
    }
}
const char* get_struct_future_type(const char* name) {
    for (int i = 0; i < struct_future_count; i++)
        if (strcmp(struct_future_vars[i], name) == 0) return struct_future_types[i];
    return NULL;
}

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
        // Initialize arrays to NULL
        for (int i = 0; i < 256; i++) {
            scopes[scope_depth].vars[i] = NULL;
            scopes[scope_depth].types[i] = NULL;
        }
        for (int i = 0; i < 64; i++) {
            scopes[scope_depth].string_objects[i] = NULL;
        }
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
            
            // Safety check: skip if var_name is NULL
            if (!var_name) {
                continue;
            }
            
            if (var_type && strcmp(var_type, "WynArray") == 0) {
                // For WynArray, free the internal data array if it exists
                emit("    if(%s.data) free(%s.data);\n", var_name, var_name);
            } else if (var_type && (strcmp(var_type, "char*") == 0 || strcmp(var_type, "const char*") == 0)) {
                // For string pointers, only free if they're not string literals
                emit("    /* String cleanup handled by ARC */\n");
            } else if (var_type) {
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
    // Safety check: ensure scope_depth is valid
    if (scope_depth == 0) {
        fprintf(stderr, "WARNING: track_var_with_type called with scope_depth=0\n");
        return;
    }
    if (scope_depth > 32) {
        fprintf(stderr, "ERROR: scope_depth=%d exceeds maximum (32)\n", scope_depth);
        return;
    }
    
    int scope_idx = scope_depth - 1;
    if (scopes[scope_idx].count >= 256) {
        fprintf(stderr, "WARNING: scope %d has %d vars (max 256)\n", scope_idx, scopes[scope_idx].count);
        return;
    }
    
    char* var = malloc(len + 1);
    if (!var) {
        fprintf(stderr, "ERROR: malloc failed for var name\n");
        return;
    }
    strncpy(var, name, len);
    var[len] = 0;
    scopes[scope_idx].vars[scopes[scope_idx].count] = var;
    
    // Store the type for proper cleanup
    if (type) {
        int type_len = strlen(type);
        char* var_type = malloc(type_len + 1);
        if (!var_type) {
            fprintf(stderr, "ERROR: malloc failed for var type\n");
            free(var);
            return;
        }
        strcpy(var_type, type);
        scopes[scope_idx].types[scopes[scope_idx].count] = var_type;
    } else {
        scopes[scope_idx].types[scopes[scope_idx].count] = NULL;
    }
    
    scopes[scope_idx].count++;
}

__attribute__((unused)) static void track_string_var(const char* name, int len) {
    track_var_with_type(name, len, "char*");
}

__attribute__((unused)) static void track_string_object(WynObject* obj) {
    if (scope_depth > 0 && scopes[scope_depth - 1].string_count < 256) {
        scopes[scope_depth - 1].string_objects[scopes[scope_depth - 1].string_count++] = obj;
    }
}

static void emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fflush(out);  // Flush after every emit to prevent buffer truncation
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

static const char* c_type_from_expr(Expr* type_expr) {
    if (type_expr && type_expr->type == EXPR_IDENT) {
        Token t = type_expr->token;
        if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return "int";
        if (t.length == 6 && memcmp(t.start, "string", 6) == 0) return "const char*";
        if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) return "int";
        if (t.length == 5 && memcmp(t.start, "float", 5) == 0) return "double";
    }
    return "long long";
}


// Expression code generation
#include "codegen_expr.c"

// Async function context tracking
static bool in_async_function = false;

// codegen_c_header - emit runtime include
// Use slim header for release builds (40% faster execution)
static bool use_slim_runtime = false;
void codegen_set_slim_runtime(bool slim) { use_slim_runtime = slim; }
void codegen_c_header() {
    if (use_slim_runtime) {
        emit("#include \"wyn_runtime_slim.h\"\n\n");
    } else {
        emit("#include \"wyn_runtime.h\"\n");
        emit("#ifdef __TINYC__\n#define __auto_type long long\n#endif\n\n");
    }
}

// Statement code generation (includes emit_function_with_prefix)
#include "codegen_stmt.c"

// Lambda scanning and generation
#include "codegen_lambda.c"

// Program-level code generation and match statements
#include "codegen_program.c"

int get_current_shadow(const char* name) {
    for (int i = 0; i < shadow_var_count; i++) {
        if (strcmp(shadow_vars[i].name, name) == 0) return shadow_vars[i].count;
    }
    return 0;
}

// Defer stack
static Expr* defer_stack[64];
static int defer_count = 0;
void push_defer(Expr* e) { if (defer_count < 64) defer_stack[defer_count++] = e; }
int get_defer_count() { return defer_count; }
Expr* get_defer(int i) { return defer_stack[i]; }
void reset_defers() { defer_count = 0; }

// Enum name tracking for constructor detection
static char* enum_type_names[64];
static int enum_type_count = 0;
void register_enum_type(const char* name) {
    if (enum_type_count < 64) enum_type_names[enum_type_count++] = strdup(name);
}
int is_enum_type(const char* name) {
    for (int i = 0; i < enum_type_count; i++) {
        if (strcmp(enum_type_names[i], name) == 0) return 1;
    }
    return 0;
}

// Data enum tracking (enums with variant data)
static char* data_enum_names[64];
static int data_enum_count = 0;
void register_data_enum_type(const char* name) {
    if (data_enum_count < 64) data_enum_names[data_enum_count++] = strdup(name);
}
int is_data_enum_type(const char* name) {
    for (int i = 0; i < data_enum_count; i++) {
        if (strcmp(data_enum_names[i], name) == 0) return 1;
    }
    return 0;
}

// Track variables that hold data-carrying enum types
static struct { char var_name[64]; char enum_name[64]; } enum_var_map[128];
static int enum_var_count = 0;

// Track variables that hold struct types
static struct { char var_name[64]; char struct_name[64]; } struct_var_map[128];
static int struct_var_count = 0;
void register_struct_var(const char* var, const char* struct_type) {
    if (struct_var_count < 128) {
        strncpy(struct_var_map[struct_var_count].var_name, var, 63);
        strncpy(struct_var_map[struct_var_count].struct_name, struct_type, 63);
        struct_var_count++;
    }
}
const char* get_struct_var_type(const char* var) {
    for (int i = struct_var_count - 1; i >= 0; i--) {
        if (strcmp(struct_var_map[i].var_name, var) == 0) return struct_var_map[i].struct_name;
    }
    return NULL;
}
int is_known_struct(const char* name) {
    if (!current_program) return 0;
    for (int i = 0; i < current_program->count; i++) {
        if (current_program->stmts[i]->type == STMT_STRUCT) {
            StructStmt* s = &current_program->stmts[i]->struct_decl;
            if (s->name.length == (int)strlen(name) && memcmp(s->name.start, name, s->name.length) == 0) return 1;
        }
    }
    return 0;
}

// Track enum variant data types: "EnumName.VariantName" -> C type
static struct { char key[128]; char c_type[64]; } enum_variant_types[256];
static int enum_variant_type_count = 0;
void register_enum_variant_type(const char* enum_name, const char* variant_name, const char* c_type) {
    if (enum_variant_type_count < 256) {
        snprintf(enum_variant_types[enum_variant_type_count].key, 128, "%s.%s", enum_name, variant_name);
        strncpy(enum_variant_types[enum_variant_type_count].c_type, c_type, 63);
        enum_variant_type_count++;
    }
}
const char* get_enum_variant_c_type(const char* enum_name, const char* variant_name) {
    char key[128];
    snprintf(key, 128, "%s.%s", enum_name, variant_name);
    for (int i = 0; i < enum_variant_type_count; i++) {
        if (strcmp(enum_variant_types[i].key, key) == 0) return enum_variant_types[i].c_type;
    }
    return "long long"; // default
}

const char* find_enum_for_variant(const char* variant_name) {
    // Search "EnumName.VariantName" keys for matching variant
    
    for (int i = 0; i < enum_variant_type_count; i++) {
        char* dot = strchr(enum_variant_types[i].key, '.');
        if (dot && strcmp(dot + 1, variant_name) == 0) {
            static char _found_enum[128];
            int elen = dot - enum_variant_types[i].key;
            memcpy(_found_enum, enum_variant_types[i].key, elen);
            _found_enum[elen] = '\0';
            return _found_enum;
        }
    }
    // Also check data_enum_names with enum_type_names
    // Try prefixing with each known data enum
    for (int i = 0; i < data_enum_count; i++) {
        char tag_name[128];
        snprintf(tag_name, 128, "%s_%s_TAG", data_enum_names[i], variant_name);
        // We can't easily check if this tag exists, but return the first data enum
        // that has this variant registered
        char key[128];
        snprintf(key, 128, "%s.%s", data_enum_names[i], variant_name);
        for (int j = 0; j < enum_variant_type_count; j++) {
            if (strcmp(enum_variant_types[j].key, key) == 0) return data_enum_names[i];
        }
    }
    return NULL;
}

void register_enum_var(const char* var, const char* enum_type) {
    if (enum_var_count < 128) {
        strncpy(enum_var_map[enum_var_count].var_name, var, 63);
        strncpy(enum_var_map[enum_var_count].enum_name, enum_type, 63);
        enum_var_count++;
    }
}
const char* get_enum_var_type(const char* var) {
    for (int i = enum_var_count - 1; i >= 0; i--) {
        if (strcmp(enum_var_map[i].var_name, var) == 0) return enum_var_map[i].enum_name;
    }
    return NULL;
}

// Function default parameter registry
static struct { char name[128]; Expr** defaults; int param_count; char return_type[32]; char param_names[16][64]; } fn_defaults[256];
static int fn_defaults_count = 0;

void register_fn_defaults(const char* name, Expr** defaults, int param_count) {
    // Update existing entry if present
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0) {
            fn_defaults[i].defaults = defaults;
            fn_defaults[i].param_count = param_count;
            return;
        }
    }
    if (fn_defaults_count < 256) {
        strncpy(fn_defaults[fn_defaults_count].name, name, 127);
        fn_defaults[fn_defaults_count].defaults = defaults;
        fn_defaults[fn_defaults_count].param_count = param_count;
        fn_defaults[fn_defaults_count].return_type[0] = '\0';
        fn_defaults_count++;
    }
}

void register_fn_param_names(const char* name, Token* params, int count) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0) {
            for (int j = 0; j < count && j < 16; j++) {
                int len = params[j].length < 63 ? params[j].length : 63;
                memcpy(fn_defaults[i].param_names[j], params[j].start, len);
                fn_defaults[i].param_names[j][len] = '\0';
            }
            return;
        }
    }
}

int get_fn_param_index(const char* fn_name, const char* param_name) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, fn_name) == 0) {
            for (int j = 0; j < fn_defaults[i].param_count; j++) {
                if (strcmp(fn_defaults[i].param_names[j], param_name) == 0) return j;
            }
        }
    }
    return -1;
}

void register_fn_return_type(const char* name, const char* ret_type) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0) {
            strncpy(fn_defaults[i].return_type, ret_type, 31);
            return;
        }
    }
    // Function not yet registered — add it
    if (fn_defaults_count < 256) {
        strncpy(fn_defaults[fn_defaults_count].name, name, 127);
        fn_defaults[fn_defaults_count].defaults = NULL;
        fn_defaults[fn_defaults_count].param_count = 0;
        strncpy(fn_defaults[fn_defaults_count].return_type, ret_type, 31);
        fn_defaults_count++;
    }
}

const char* get_function_return_type(const char* name) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0 && fn_defaults[i].return_type[0])
            return fn_defaults[i].return_type;
    }
    return NULL;
}

Expr* get_fn_default(const char* name, int param_index) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0 && param_index < fn_defaults[i].param_count) {
            return fn_defaults[i].defaults[param_index];
        }
    }
    return NULL;
}

int get_fn_param_count(const char* name) {
    for (int i = 0; i < fn_defaults_count; i++) {
        if (strcmp(fn_defaults[i].name, name) == 0) return fn_defaults[i].param_count;
    }
    return -1;
}
