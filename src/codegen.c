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

static char* sb_var_names[64];
static int sb_var_count = 0;
static void register_sb_var(const char* name) {
    if (sb_var_count < 64) { sb_var_names[sb_var_count++] = strdup(name);  }
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
static Program* current_program = NULL;

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
} SpawnWrapper;

static SpawnWrapper spawn_wrappers[256];
static int spawn_wrapper_count = 0;
static int spawn_id_counter = 0;

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
        emit("#include \"wyn_runtime.h\"\n\n");
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

// Track variables that hold data-carrying enum types
static struct { char var_name[64]; char enum_name[64]; } enum_var_map[128];
static int enum_var_count = 0;
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
static struct { char name[128]; Expr** defaults; int param_count; } fn_defaults[256];
static int fn_defaults_count = 0;

void register_fn_defaults(const char* name, Expr** defaults, int param_count) {
    if (fn_defaults_count < 256) {
        strncpy(fn_defaults[fn_defaults_count].name, name, 127);
        fn_defaults[fn_defaults_count].defaults = defaults;
        fn_defaults[fn_defaults_count].param_count = param_count;
        fn_defaults_count++;
    }
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
