#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "common.h"
#include "growable.h"

// Source context for error messages
static const char* checker_source = NULL;
static const char* checker_filename = NULL;
void set_checker_source(const char* src, const char* fname) {
    checker_source = src; checker_filename = fname;
    extern void error_set_source(const char*, const char*);
    error_set_source(src, fname);
}
const char* get_checker_source(void) { return checker_source; }
const char* get_checker_filename(void) { return checker_filename; }

static void show_source_line(int line) {
    if (!checker_source || line < 1) return;
    const char* p = checker_source;
    int cur = 1;
    while (*p && cur < line) { if (*p == '\n') cur++; p++; }
    if (cur == line) {
        const char* end = p;
        while (*end && *end != '\n') end++;
        fprintf(stderr, "  \033[2m%4d |\033[0m %.*s\n", line, (int)(end - p), p);
        fprintf(stderr, "       \033[31m");
        for (int i = 0; i < (int)(end - p); i++) fprintf(stderr, "^");
        fprintf(stderr, "\033[0m\n");
    }
}
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
static int lambda_param_is_string(const char* pname, Expr* e, SymbolTable* scope);
static int lambda_param_is_float(const char* pname, Expr* e, SymbolTable* scope);
static int lambda_param_is_bool(const char* pname, Expr* e, SymbolTable* scope);
void analyze_expr_captures(Expr* expr, LambdaExpr* lambda, SymbolTable* scope);

// S3: element type of the receiver array while checking a map/filter lambda
// argument - unannotated lambda params default to this instead of int.
static Type* lambda_ctx_param_seed = NULL;

static SymbolTable* global_scope = NULL;
static Program* current_program = NULL;  // For looking up struct definitions
static Type* builtin_int = NULL;
static Type* builtin_int_opt = NULL;  // int? — used by Task.try_recv's return type

// W9 namespaced imports: names brought in by a WHOLE-module `import m` (as
// opposed to selective `import { foo } from m`). After a whole-module import,
// the bare name must NOT be callable - `m.foo()` is the required form. We record
// each such name here so a flat call site can emit a clear "did you mean m.foo()?"
// error. Selective imports are never recorded (their bare names stay valid).
typedef struct { char name[128]; char module[128]; } WholeModuleFn;
static WholeModuleFn* whole_module_fns = NULL;
static int whole_module_fn_count = 0;
static int whole_module_fn_cap = 0;

static void register_whole_module_fn(const char* module, const char* fn) {
    // De-dupe: if a later selective import (or a local definition) also provides
    // the name, we keep the first module recorded for the hint; the flat-call
    // guard separately checks that no real local/selective symbol shadows it.
    for (int i = 0; i < whole_module_fn_count; i++) {
        if (strcmp(whole_module_fns[i].name, fn) == 0) return;
    }
    WYN_ENSURE_CAP(whole_module_fns, whole_module_fn_count, whole_module_fn_cap);
    strncpy(whole_module_fns[whole_module_fn_count].name, fn, 127);
    whole_module_fns[whole_module_fn_count].name[127] = '\0';
    strncpy(whole_module_fns[whole_module_fn_count].module, module, 127);
    whole_module_fns[whole_module_fn_count].module[127] = '\0';
    whole_module_fn_count++;
}

// Defined after check_program's helpers (needs Program/Stmt layout); forward
// declared here for the flat-call guard inside check_expr.
static bool flat_callable_in_program(Program* prog, const char* name, int name_len);

// Returns the owning module name if `fn` was imported ONLY via a whole-module
// import (and thus must be qualified), or NULL otherwise.
static const char* whole_module_fn_owner(const char* fn) {
    for (int i = 0; i < whole_module_fn_count; i++) {
        if (strcmp(whole_module_fns[i].name, fn) == 0) return whole_module_fns[i].module;
    }
    return NULL;
}
static Type* builtin_float = NULL;
static Type* builtin_string = NULL;
static Type* builtin_bool = NULL;
static Type* builtin_void = NULL;
static Type* builtin_array = NULL;
static Type* builtin_ptr = NULL;  // opaque C pointer (void*) for FFI

// Map an `extern fn` C type expression (e.g. `int`, `float`, `bool`, `string`,
// `void`, or a pointer-ish type) to the Wyn builtin used for type-checking calls.
// NULL (no `-> T`) maps to void. Unknown types default to int (treated as an
// opaque machine word - the C prototype in codegen mirrors this).
// True if t is the FFI opaque pointer type (`ptr` -> TYPE_STRUCT named "void*").
static bool is_ptr_type(Type* t) {
    return t && t->kind == TYPE_STRUCT && t->struct_type.name.length == 5 &&
           memcmp(t->struct_type.name.start, "void*", 5) == 0;
}

static StructStmt* find_struct_definition(Token struct_name);
Type* make_type(TypeKind kind);
static Type* extern_map_type(Expr* type_expr) {
    if (!type_expr) return builtin_void;
    if (type_expr->type == EXPR_IDENT) {
        Token t = type_expr->token;
        if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return builtin_int;
        if (t.length == 5 && memcmp(t.start, "float", 5) == 0) return builtin_float;
        if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) return builtin_bool;
        if (t.length == 6 && memcmp(t.start, "string", 6) == 0) return builtin_string;
        // `char*` / `cstr`: a raw C string, distinct from Wyn's UTF-8 `string`.
        // Typed as string on the Wyn side (both are char*), but codegen emits a
        // plain `char*` (not the owned/const-managed string path).
        if (t.length == 4 && memcmp(t.start, "cstr", 4) == 0) return builtin_string;
        if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return builtin_void;
        if (t.length == 3 && memcmp(t.start, "ptr", 3) == 0) return builtin_ptr;
        // A user struct passed/returned by value across FFI: resolve to its
        // struct Type so calls type-check (codegen emits the C struct by value).
        StructStmt* sd = find_struct_definition(t);
        if (sd) {
            Type* st = make_type(TYPE_STRUCT);
            st->struct_type.name = t;
            return st;
        }
    }
    // `char*` written as a pointer type expression (EXPR_UNARY '*' on char, if the
    // parser produces one) - treat like cstr. Fallback: opaque machine word.
    return builtin_int;
}
static bool had_error = false;
static Type* current_function_return_type = NULL;
static Type* current_self_type = NULL; // receiver type for extension methods

// Module visibility tracking
static char current_module_name[256] = "";

// Module collision tracking
typedef struct {
    char short_name[128];
    char full_path[256];
    int line_number;
} ImportedModule;

static ImportedModule* imported_modules = NULL;
static int imported_modules_count = 0;
static int imported_modules_cap = 0;

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
    WYN_ENSURE_CAP(imported_modules, imported_modules_count, imported_modules_cap);
    memset(&imported_modules[imported_modules_count], 0, sizeof(ImportedModule));
    strncpy(imported_modules[imported_modules_count].short_name, short_name, 127);
    strncpy(imported_modules[imported_modules_count].full_path, full_path, 255);
    imported_modules[imported_modules_count].line_number = line;
    imported_modules_count++;
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

// --- pub visibility enforcement (2026-07) ---------------------------------
// `pub` is real: a module function without `pub` (or `export`) cannot be
// called from outside its module. Visibility is answered straight from the
// parsed module AST in the module registry - no side registry to populate,
// so it cannot go stale or miss the normal import path (the old
// FunctionVisibility table was only filled while checking module bodies and
// never consulted for dot calls, so everything slipped through).

// Find a loaded module's AST by its registry name, or by short name
// (last path segment, e.g. "utils" for "lib/utils").
static Program* find_module_ast(const char* name) {
    extern Program* get_module(const char* name);
    extern int get_module_count(void);
    extern void* get_module_entry_at(int index);
    Program* m = get_module(name);
    if (m) return m;
    int mc = get_module_count();
    for (int i = 0; i < mc; i++) {
        typedef struct { char* name; Program* ast; } ME;
        ME* e = (ME*)get_module_entry_at(i);
        if (!e) continue;
        const char* slash = strrchr(e->name, '/');
        const char* short_name = slash ? slash + 1 : e->name;
        if (strcmp(short_name, name) == 0) return e->ast;
    }
    return NULL;
}

typedef enum {
    VIS_MODULE_UNKNOWN,  // module not in registry (builtin/C package) - no check
    VIS_FN_UNKNOWN,      // module found but no such fn - other paths report it
    VIS_PRIVATE,
    VIS_PUBLIC
} FnVisibility;

static FnVisibility module_fn_visibility(const char* module_name, const char* fn_name) {
    Program* m = find_module_ast(module_name);
    if (!m) return VIS_MODULE_UNKNOWN;
    bool found = false;
    size_t fn_len = strlen(fn_name);
    for (int i = 0; i < m->count; i++) {
        Stmt* s = m->stmts[i];
        if (!s) continue;
        bool exported = false;
        if (s->type == STMT_EXPORT && s->export.stmt) {
            exported = true;  // `export fn` exports, same as `pub fn`
            s = s->export.stmt;
        }
        if (s->type != STMT_FN) continue;
        if ((size_t)s->fn.name.length == fn_len &&
            memcmp(s->fn.name.start, fn_name, fn_len) == 0) {
            found = true;
            // Overloads: if any definition is exported, the name is callable.
            if (exported || s->fn.is_public) return VIS_PUBLIC;
        }
    }
    return found ? VIS_PRIVATE : VIS_FN_UNKNOWN;
}

// Is the checker currently inside `target` (module calling its own fns)?
static bool checking_same_module(const char* target) {
    if (current_module_name[0] == '\0') return false;
    if (strcmp(current_module_name, target) == 0) return true;
    // Registry names are full paths ("lib/utils"); calls use short names.
    const char* slash = strrchr(current_module_name, '/');
    const char* cur_short = slash ? slash + 1 : current_module_name;
    if (strcmp(cur_short, target) == 0) return true;
    const char* tslash = strrchr(target, '/');
    if (tslash && strcmp(tslash + 1, cur_short) == 0) return true;
    return false;
}

static void private_fn_error(int line, const char* fn_name, const char* module_name) {
    fprintf(stderr, "\nError at line %d: function '%s' in module '%s' is private\n",
            line, fn_name, module_name);
    if (current_module_name[0] == '\0') show_source_line(line);
    // File hint: prefer the registry's full path ("lib/utils" for a call
    // through the short name "utils") so the Help points at the real file.
    const char* file_hint = module_name;
    {
        extern int get_module_count(void);
        extern void* get_module_entry_at(int index);
        extern Program* get_module(const char* name);
        if (!get_module(module_name)) {
            int mc = get_module_count();
            for (int i = 0; i < mc; i++) {
                typedef struct { char* name; Program* ast; } ME;
                ME* e = (ME*)get_module_entry_at(i);
                if (!e) continue;
                const char* slash = strrchr(e->name, '/');
                if (slash && strcmp(slash + 1, module_name) == 0) { file_hint = e->name; break; }
            }
        }
    }
    fprintf(stderr, "  \033[34mHelp:\033[0m Add 'pub' to 'fn %s' in %s.wyn to export it.\n",
            fn_name, file_hint);
    had_error = true;
}

static bool check_function_visibility(const char* module, const char* func) {
    // Calls from inside the same module are always allowed
    if (checking_same_module(module)) {
        return true;
    }
    // Unknown modules and unknown fns stay permissive - builtin namespaces
    // and C packages are resolved elsewhere.
    return module_fn_visibility(module, func) != VIS_PRIVATE;
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
    if (kind == TYPE_FUNCTION) t->fn_type.min_param_count = -1;
    return t;
}

// A container constructor (HashMap::new / HashSet::new) shares ONE builtin
// return-type node across every call site. Its value/element type is inferred
// later from the first `.set()`, so each call must yield a FRESH map/set node -
// otherwise every map in a program aliases one value_type and only the first
// inference wins (a float map read a bool map's values as 0.0). `call_expr`'s
// per-node expr_type is reused across re-checks so an inferred value_type
// survives a second walk. Non-container return types pass through unchanged.
static Type* freshen_container_ret(Expr* call_expr, Type* ret) {
    if (!ret || (ret->kind != TYPE_MAP && ret->kind != TYPE_SET)) return ret;
    if (call_expr && call_expr->expr_type &&
        call_expr->expr_type->kind == ret->kind && call_expr->expr_type != ret) {
        return call_expr->expr_type;
    }
    Type* fresh = make_type(ret->kind);
    if (ret->kind == TYPE_MAP) {
        fresh->map_type.key_type = ret->map_type.key_type;
        fresh->map_type.value_type = ret->map_type.value_type;
    }
    if (call_expr) call_expr->expr_type = fresh;
    return fresh;
}

// Open-map read-modify-write inference. An empty `{}` map leaves its value
// type OPEN (NULL) so the first STORE fixes it - but `m[k] = m[k] + 1` reads
// m[k] BEFORE the store, and the open-map read's historical string default
// then fixed the map to string, rejecting the int store in the other branch
// (the book's word-count pattern). In a read-modify-write the read and the
// stored result share the map's ONE value type, so the read cannot decide it -
// the OTHER operand of the arithmetic can. Walk the RHS: when one side of an
// arithmetic binary op is an index-read of this same open map, the value type
// is the other side's type. Returns NULL when the pattern doesn't apply
// (then the normal first-store rule decides, unchanged).
static bool expr_is_read_of_map(Expr* e, Type* map_t, SymbolTable* scope) {
    return e && e->type == EXPR_INDEX &&
           check_expr(e->index.array, scope) == map_t;
}
static Type* infer_open_map_rmw(Expr* v, Type* map_t, SymbolTable* scope) {
    if (!v || v->type != EXPR_BINARY) return NULL;
    WynTokenType op = v->binary.op.type;
    if (op != TOKEN_PLUS && op != TOKEN_MINUS && op != TOKEN_STAR &&
        op != TOKEN_SLASH && op != TOKEN_PERCENT) return NULL;
    Expr* l = v->binary.left;
    Expr* r = v->binary.right;
    if (expr_is_read_of_map(l, map_t, scope)) {
        Type* t = infer_open_map_rmw(r, map_t, scope);
        return t ? t : check_expr(r, scope);
    }
    if (expr_is_read_of_map(r, map_t, scope)) {
        Type* t = infer_open_map_rmw(l, map_t, scope);
        return t ? t : check_expr(l, scope);
    }
    // The read may sit deeper: m[k] * 2 + 1.
    Type* t = infer_open_map_rmw(l, map_t, scope);
    return t ? t : infer_open_map_rmw(r, map_t, scope);
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

// Cycle detection for by-value struct fields: returns true when struct `from`
// can reach a field of struct type `target`, following plain (`B`) and
// Optional-wrapped (`B?`) struct fields transitively. Arrays and maps add heap
// indirection, so they are deliberately NOT followed (`kids: [Node]` is fine).
// `visited` guards against unrelated cycles so the walk stays linear.
static bool struct_field_reaches(Token from, Token target,
                                 Token* visited, int* visited_count) {
    for (int i = 0; i < *visited_count; i++) {
        if (visited[i].length == from.length &&
            memcmp(visited[i].start, from.start, from.length) == 0) return false;
    }
    if (*visited_count >= 64) return false;
    visited[(*visited_count)++] = from;

    StructStmt* s = find_struct_definition(from);
    if (!s) return false;
    for (int i = 0; i < s->field_count; i++) {
        Expr* ft = s->field_types[i];
        if (ft && ft->type == EXPR_OPTIONAL_TYPE) ft = ft->optional_type.inner_type;
        if (!ft || ft->type != EXPR_IDENT) continue;
        if (ft->token.length == target.length &&
            memcmp(ft->token.start, target.start, target.length) == 0) return true;
        if (struct_field_reaches(ft->token, target, visited, visited_count)) return true;
    }
    return false;
}

// Field-wise struct equality (`a == b` on struct values): a struct is
// comparable when every field is int/float/bool/string, an enum, or another
// comparable struct. Arrays/maps/functions/optionals make it non-comparable -
// those get a check-time error instead of the C-level ICE `==` used to hit.
// On failure, the offending field's name is written to *bad_field.
static bool struct_fields_comparable(Token struct_name, Token* bad_field, int depth) {
    if (depth > 8) return false;  // recursive fields are rejected elsewhere; belt and braces
    StructStmt* s = find_struct_definition(struct_name);
    if (!s) return false;
    for (int i = 0; i < s->field_count; i++) {
        Expr* ft = s->field_types[i];
        bool ok = false;
        if (ft && ft->type == EXPR_IDENT) {
            Token t = ft->token;
            if ((t.length == 3 && memcmp(t.start, "int", 3) == 0) ||
                (t.length == 5 && memcmp(t.start, "float", 5) == 0) ||
                (t.length == 4 && memcmp(t.start, "bool", 4) == 0) ||
                (t.length == 6 && memcmp(t.start, "string", 6) == 0)) {
                ok = true;
            } else if (find_struct_definition(t)) {
                if (!struct_fields_comparable(t, bad_field, depth + 1)) return false;
                ok = true;
            } else if (current_program) {
                // Enum fields compare as ints in C.
                for (int j = 0; j < current_program->count && !ok; j++) {
                    Stmt* es = current_program->stmts[j];
                    if (es->type == STMT_EXPORT && es->export.stmt) es = es->export.stmt;
                    if (es->type == STMT_ENUM && es->enum_decl.name.length == t.length &&
                        memcmp(es->enum_decl.name.start, t.start, t.length) == 0) ok = true;
                }
            }
        }
        if (!ok) {
            if (bad_field) *bad_field = s->fields[i];
            return false;
        }
    }
    return true;
}

// True if `field` is a declared field of the struct named `struct_name`. Used to
// avoid flagging `p.field()` (a function-typed field call) as a missing method.
static bool is_field_of_struct(Token struct_name, Token field) {
    StructStmt* s = find_struct_definition(struct_name);
    if (!s) return false;
    for (int i = 0; i < s->field_count; i++) {
        if (s->fields[i].length == field.length &&
            memcmp(s->fields[i].start, field.start, field.length) == 0) return true;
    }
    return false;
}

// True if the struct named `struct_name` declares a method `name` - either in
// its body (`struct S { fn name(self) ... }`, held in methods[]) or via an `impl`
// block / extension method registered as `S_name` in global scope. Struct-body
// methods are NOT added to the symbol table, so both sources must be consulted.
static bool struct_has_method(SymbolTable* global, Token struct_name, Token name) {
    StructStmt* s = find_struct_definition(struct_name);
    if (s) {
        for (int i = 0; i < s->method_count; i++) {
            if (s->methods[i]->name.length == name.length &&
                memcmp(s->methods[i]->name.start, name.start, name.length) == 0) return true;
        }
    }
    char ext[256];
    snprintf(ext, sizeof(ext), "%.*s_%.*s",
             struct_name.length, struct_name.start, name.length, name.start);
    Token t = {TOKEN_IDENT, ext, (int)strlen(ext), 0};
    Symbol* sym = find_symbol(global, t);
    return sym && sym->type && sym->type->kind == TYPE_FUNCTION;
}

static EnumStmt* find_enum_definition(Token enum_name) {
    if (!current_program) return NULL;
    
    for (int i = 0; i < current_program->count; i++) {
        Stmt* stmt = current_program->stmts[i];
        // Unwrap export
        if (stmt->type == STMT_EXPORT && stmt->export.stmt && stmt->export.stmt->type == STMT_ENUM) {
            stmt = stmt->export.stmt;
        }
        if (stmt->type == STMT_ENUM) {
            Token name = stmt->enum_decl.name;
            if (name.length == enum_name.length &&
                memcmp(name.start, enum_name.start, name.length) == 0) {
                return &stmt->enum_decl;
            }
        }
    }
    // Also search all loaded modules
    extern int get_module_count(void);
    extern Program* get_module_at(int index);
    int mc = get_module_count();
    for (int m = 0; m < mc; m++) {
        Program* mod = get_module_at(m);
        if (!mod) continue;
        for (int i = 0; i < mod->count; i++) {
            Stmt* stmt = mod->stmts[i];
            if (stmt->type == STMT_EXPORT && stmt->export.stmt && stmt->export.stmt->type == STMT_ENUM) {
                stmt = stmt->export.stmt;
            }
            if (stmt->type == STMT_ENUM) {
                Token name = stmt->enum_decl.name;
                if (name.length == enum_name.length &&
                    memcmp(name.start, enum_name.start, name.length) == 0) {
                    return &stmt->enum_decl;
                }
            }
        }
    }
    return NULL;
}

// True when `enum_name` names a DATA-carrying enum (at least one variant has a
// payload, e.g. `Circle(float)`). The distinction matters wherever an enum is
// lowered: a PLAIN enum is an `int` in C (so its Option family is `OptionInt`),
// while a data-carrying enum is a C *struct* and needs the struct treatment.
static bool enum_name_is_data_enum(Token enum_name) {
    EnumStmt* e = find_enum_definition(enum_name);
    if (!e || !e->variant_type_counts) return false;
    for (int v = 0; v < e->variant_count; v++)
        if (e->variant_type_counts[v] > 0) return true;
    return false;
}

// Find the enum that declares a variant named `variant` (searching the current
// program and loaded modules), returning its EnumStmt and, via *out_vi, the
// variant index. Used to resolve a BARE (unqualified) enum constructor call
// like `Circle(5)` -> the `Shape` enum's `Circle` variant, mirroring the
// qualified `Shape.Circle(5)` path. Returns the FIRST match: when two enums
// share a variant name the bare form is inherently ambiguous, so callers should
// treat the qualified form as the disambiguator (documented in
// tests/regression/bare_enum_constructor.wyn). Returns NULL if no enum has it.
static EnumStmt* find_enum_for_bare_variant(Token variant, int* out_vi) {
    if (!current_program) return NULL;
    Program* progs[64]; int np = 0;
    progs[np++] = current_program;
    extern int get_module_count(void);
    extern Program* get_module_at(int index);
    int mc = get_module_count();
    for (int m = 0; m < mc && np < 64; m++) {
        Program* mod = get_module_at(m);
        if (mod) progs[np++] = mod;
    }
    for (int p = 0; p < np; p++) {
        Program* prog = progs[p];
        for (int i = 0; i < prog->count; i++) {
            Stmt* stmt = prog->stmts[i];
            if (stmt->type == STMT_EXPORT && stmt->export.stmt &&
                stmt->export.stmt->type == STMT_ENUM)
                stmt = stmt->export.stmt;
            if (stmt->type != STMT_ENUM) continue;
            for (int vi = 0; vi < stmt->enum_decl.variant_count; vi++) {
                Token v = stmt->enum_decl.variants[vi];
                if (v.length == variant.length &&
                    memcmp(v.start, variant.start, v.length) == 0) {
                    if (out_vi) *out_vi = vi;
                    return &stmt->enum_decl;
                }
            }
        }
    }
    return NULL;
}

// Does enum `from` reach enum `target` through a chain of enum-typed variant
// payloads? Used to detect a MUTUALLY-recursive enum cycle
// (enum A{AtoB(B)} enum B{BtoA(A)}), which codegen cannot yet represent: the
// tagged-union stores payloads by value, so a cycle is infinite-size, and the
// constructors take payloads by value (an incomplete sibling type). Rejected at
// check with a clear message instead of leaking a raw C error. `visited` guards
// against infinite recursion in the walk itself.
static bool enum_payload_reaches(Token from, Token target, Token* visited, int* nvisited) {
    for (int i = 0; i < *nvisited; i++)
        if (visited[i].length == from.length &&
            memcmp(visited[i].start, from.start, from.length) == 0) return false;
    if (*nvisited < 64) visited[(*nvisited)++] = from;
    EnumStmt* e = find_enum_definition(from);
    if (!e) return false;
    for (int v = 0; v < e->variant_count; v++) {
        for (int f = 0; f < e->variant_type_counts[v]; f++) {
            Expr* ft = e->variant_types[v] ? e->variant_types[v][f] : NULL;
            if (!ft || ft->type != EXPR_IDENT) continue;
            // A payload that IS the target enum closes the cycle.
            if (ft->token.length == target.length &&
                memcmp(ft->token.start, target.start, target.length) == 0) return true;
            // Otherwise, if it names another enum, recurse.
            if (find_enum_definition(ft->token) &&
                enum_payload_reaches(ft->token, target, visited, nvisited)) return true;
        }
    }
    return false;
}

// Resolve an array-element type annotation Expr* to a Type*. Handles builtin
// names, user struct/enum names, AND nested array annotations (`[[float]]`),
// which the inline STMT_VAR resolver did not: it only looked at EXPR_IDENT, so
// the inner element_type of a [[float]] was left NULL and every fg[i][j] read
// truncated through array_get_nested_int (G4). Returns NULL when the annotation
// names nothing recognizable (caller leaves element_type unset, as before).
static Type* resolve_array_elem_annotation(Expr* elem_type_expr) {
    if (!elem_type_expr) return NULL;
    if (elem_type_expr->type == EXPR_ARRAY) {
        // Nested array: `[[T]]`. Build an inner TYPE_ARRAY and recurse for its
        // element type so the leaf (int/float/bool/...) is preserved.
        Type* inner = make_type(TYPE_ARRAY);
        if (elem_type_expr->array.count > 0) {
            inner->array_type.element_type =
                resolve_array_elem_annotation(elem_type_expr->array.elements[0]);
        }
        return inner;
    }
    // Nested map annotation: `{string: {string: int}}` / `HashMap<K, V>` in a
    // value position. The parser desugars `{K: V}` to an EXPR_CALL with a
    // HashMap callee, so both spellings land here and recurse for K and V.
    if (elem_type_expr->type == EXPR_CALL &&
        elem_type_expr->call.callee &&
        elem_type_expr->call.callee->type == EXPR_IDENT &&
        elem_type_expr->call.callee->token.length == 7 &&
        memcmp(elem_type_expr->call.callee->token.start, "HashMap", 7) == 0) {
        Type* inner = make_type(TYPE_MAP);
        if (elem_type_expr->call.arg_count >= 1)
            inner->map_type.key_type = resolve_array_elem_annotation(elem_type_expr->call.args[0]);
        if (elem_type_expr->call.arg_count >= 2)
            inner->map_type.value_type = resolve_array_elem_annotation(elem_type_expr->call.args[1]);
        return inner;
    }
    if (elem_type_expr->type != EXPR_IDENT) return NULL;
    Token n = elem_type_expr->token;
    StructStmt* struct_def = find_struct_definition(n);
    if (struct_def) {
        Type* t = make_type(TYPE_STRUCT);
        t->struct_type.name = n;
        return t;
    }
    EnumStmt* enum_def = find_enum_definition(n);
    if (enum_def) {
        Type* t = make_type(TYPE_ENUM);
        t->name = n;
        t->enum_type.variants = enum_def->variants;
        t->enum_type.variant_count = enum_def->variant_count;
        return t;
    }
    if (n.length == 3 && memcmp(n.start, "int", 3) == 0) return builtin_int;
    if (n.length == 6 && memcmp(n.start, "string", 6) == 0) return builtin_string;
    if (n.length == 5 && memcmp(n.start, "float", 5) == 0) return builtin_float;
    if (n.length == 4 && memcmp(n.start, "bool", 4) == 0) return builtin_bool;
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
                } else if (type_name.length == 4 && memcmp(type_name.start, "char", 4) == 0) {
                    return builtin_int;  // char is int in Wyn
                } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                    return builtin_string;
                } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                    return builtin_bool;
                } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                    return builtin_float;
                } else if (type_name.length == 3 && memcmp(type_name.start, "ptr", 3) == 0) {
                    // The FFI pointer family, ahead of the user-struct fallback
                    // below for the same reason as everywhere else: these are
                    // BUILTINS. `ptr` is itself TYPE_STRUCT (named "void*"), so a
                    // `ptr` field happened to survive the fallback; `cstr` did
                    // not - it resolved to a struct named "cstr", and passing the
                    // field to an `extern fn atoi(s: cstr)` was rejected with
                    // "Expected string, got struct".
                    return builtin_ptr;
                } else if (type_name.length == 4 && memcmp(type_name.start, "cstr", 4) == 0) {
                    return builtin_string;
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
                // Array field `f: [T]`. Route through the one element-type
                // authority (resolve_array_elem_annotation) instead of a partial
                // inline copy: the old copy only knew `int` and `string` and
                // turned everything else - including `float` and `bool` - into a
                // TYPE_STRUCT, so every read of `s.f[i]` emitted
                // array_get_struct (segfault) and every write/push was rejected
                // with "Cannot store float in array of struct".
                Type* array_type = make_type(TYPE_ARRAY);
                if (field_type_expr->array.count > 0) {
                    Expr* elem_type_expr = field_type_expr->array.elements[0];
                    Type* elem = resolve_array_elem_annotation(elem_type_expr);
                    if (!elem && elem_type_expr->type == EXPR_IDENT) {
                        // Unrecognized bare name (e.g. a struct declared in an
                        // imported module that find_struct_definition can't see):
                        // keep the historical by-name struct fallback.
                        elem = make_type(TYPE_STRUCT);
                        elem->struct_type.name = elem_type_expr->token;
                    }
                    array_type->array_type.element_type = elem;
                }
                return array_type;
            } else if ((field_type_expr->type == EXPR_OPTIONAL_TYPE) ||
                       (field_type_expr->type == EXPR_CALL &&
                        field_type_expr->call.callee &&
                        field_type_expr->call.callee->type == EXPR_IDENT &&
                        field_type_expr->call.callee->token.length == 6 &&
                        memcmp(field_type_expr->call.callee->token.start, "Option", 6) == 0 &&
                        field_type_expr->call.arg_count == 1)) {
                // Optional field `f: T?` (EXPR_OPTIONAL_TYPE) or the generic form
                // `f: Option<T>` (EXPR_CALL) - resolve to the Option<T> family type
                // so field access (`x.f`) and match on it lower correctly (was
                // falling through to NULL → default int, breaking match on the field).
                Expr* inner = (field_type_expr->type == EXPR_OPTIONAL_TYPE)
                    ? field_type_expr->optional_type.inner_type
                    : field_type_expr->call.args[0];
                const char* fam = NULL;
                if (inner && inner->type == EXPR_IDENT) {
                    Token t = inner->token;
                    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) fam = "OptionInt";
                    else if (t.length == 6 && memcmp(t.start, "string", 6) == 0) fam = "OptionString";
                    else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) fam = "OptionFloat";
                    else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) fam = "OptionBool";
                    else {
                        // A PLAIN enum payload's family is OptionInt (an enum is an int
                        // in C) — there is no `Option<Enum>` family symbol to find, so
                        // building the name `OptionCode` made the lookup below MISS and
                        // return NULL, which the caller reads as "struct 'H' has no
                        // field 'tag'". A data-carrying enum is a C struct, so it keeps
                        // the by-name path below.
                        Symbol* _es = find_symbol(global_scope, t);
                        if (_es && _es->type && _es->type->kind == TYPE_ENUM &&
                            !enum_name_is_data_enum(t)) {
                            fam = "OptionInt";
                        } else {
                            // Struct?: reuse the registered Option<Struct> family symbol.
                            char _stn[96]; token_to_cstr(_stn, sizeof(_stn), t);
                            static char _famb[128]; snprintf(_famb, sizeof(_famb), "Option%s", _stn);
                            fam = _famb;
                        }
                    }
                }
                if (fam) {
                    Token fam_tok = {TOKEN_IDENT, (char*)fam, (int)strlen(fam), 0};
                    Symbol* fsym = find_symbol(global_scope, fam_tok);
                    if (fsym && fsym->type) return fsym->type;
                }
                return NULL;
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
    
    // Allow enum <-> int (enums are represented as ints)
    if ((expected->kind == TYPE_ENUM && actual->kind == TYPE_INT) ||
        (expected->kind == TYPE_INT && actual->kind == TYPE_ENUM)) {
        return true;
    }

    // Allow bool <-> int, because THIS COMPILER MAKES THEM THE SAME THING.
    //
    // A comparison is typed int, not bool - see the `expr_type = builtin_int`
    // at the end of the comparison branch in EXPR_BINARY, and the note on the
    // and/or branch above it: the lambda and predicate runtime ABI
    // (long long (*fn)(...)) depends on that choice. The consequence was that
    // passing a comparison straight to a `bool` parameter was REJECTED -
    //
    //     fn wrap(ok: bool) -> bool { return ok }
    //     wrap(f(x) == 1)          // "Expected: bool  Got: int"
    //
    // - while the identical value hoisted into a local first was accepted,
    // because a `var` declaration special-cases the op to declare bool. So the
    // same expression was legal or illegal depending on whether it passed
    // through a variable, which is not a rule anyone can learn. Found writing
    // ordinary Wyn: a one-line `return changed(sel_all(s) == 1)` wrapper in
    // WynCanvas's selection module.
    //
    // Fixing it here rather than by retyping comparisons keeps the ABI note
    // above true and cannot change any generated code: this function only
    // decides whether a call is ACCEPTED. The reverse direction (an int-typed
    // argument to a bool parameter and vice versa) is admitted for the same
    // reason the enum and channel rules above are - they are one representation
    // with two spellings, and `if 1 { }` has always been legal.
    if ((expected->kind == TYPE_BOOL && actual->kind == TYPE_INT) ||
        (expected->kind == TYPE_INT && actual->kind == TYPE_BOOL)) {
        return true;
    }

    // FFI `ptr` interop: an opaque C pointer (`void*`) is freely convertible at
    // the C boundary with a Wyn `string` (both are char* - passing a string to a
    // `const char*` param), with the null idiom `0` (int), and with a raw machine
    // word. Accept these so `extern fn`s taking/returning `ptr` are callable with
    // string literals and null without a cast. (is_ptr_type: TYPE_STRUCT "void*".)
    if (is_ptr_type(expected) &&
        (actual->kind == TYPE_STRING || actual->kind == TYPE_INT || is_ptr_type(actual))) {
        return true;
    }
    if (is_ptr_type(actual) &&
        (expected->kind == TYPE_STRING || expected->kind == TYPE_INT)) {
        return true;
    }

    // Allow channel <-> int (channels are int handles, so they can pass
    // through int-typed function params - e.g. fn produce(ch: int, ...)).
    if ((expected->kind == TYPE_CHANNEL && actual->kind == TYPE_INT) ||
        (expected->kind == TYPE_INT && actual->kind == TYPE_CHANNEL)) {
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
    
    // Check parameter count (allow fewer args if defaults exist)
    int min_params = func_type->fn_type.min_param_count;
    if (min_params < 0) min_params = func_type->fn_type.param_count;
    if (arg_count < min_params || arg_count > func_type->fn_type.param_count) {
        return VALIDATION_PARAM_COUNT_MISMATCH;
    }
    
    // Check type compatibility for each provided parameter
    for (int i = 0; i < arg_count; i++) {
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

// FNV-1a hash for token bytes
static inline uint32_t sym_hash(const char* s, int len) {
    if (!s || len <= 0) return 0;
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= (unsigned char)s[i];
        h *= 16777619u;
    }
    return h;
}

static void sym_table_rehash(SymbolTable* scope) {
    int new_cap = scope->hash_capacity == 0 ? 16 : scope->hash_capacity * 2;
    int* new_indices = malloc(new_cap * sizeof(int));
    for (int i = 0; i < new_cap; i++) new_indices[i] = -1;
    int mask = new_cap - 1;
    for (int i = 0; i < scope->count; i++) {
        if (!scope->symbols[i].name.start || scope->symbols[i].name.length <= 0) continue;
        uint32_t h = sym_hash(scope->symbols[i].name.start, scope->symbols[i].name.length);
        int slot = h & mask;
        while (new_indices[slot] != -1) slot = (slot + 1) & mask;
        new_indices[slot] = i;
    }
    free(scope->hash_indices);
    scope->hash_indices = new_indices;
    scope->hash_capacity = new_cap;
}

void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable) {
    if (scope->count >= scope->capacity) {
        int new_cap = scope->capacity == 0 ? 8 : scope->capacity * 2;
        scope->symbols = realloc(scope->symbols, new_cap * sizeof(Symbol));
        scope->capacity = new_cap;
    }
    int idx = scope->count;
    scope->symbols[idx].name = name;
    scope->symbols[idx].type = type;
    scope->symbols[idx].is_mutable = is_mutable;
    scope->symbols[idx].is_used = false;
    scope->symbols[idx].next_overload = NULL;
    scope->symbols[idx].mangled_name = NULL;
    scope->count++;

    // Rehash if load factor > 0.7
    if (scope->hash_capacity == 0 || scope->count * 10 > scope->hash_capacity * 7) {
        sym_table_rehash(scope);
    } else if (name.start && name.length > 0) {
        int mask = scope->hash_capacity - 1;
        uint32_t h = sym_hash(name.start, name.length);
        int slot = h & mask;
        while (scope->hash_indices[slot] != -1) slot = (slot + 1) & mask;
        scope->hash_indices[slot] = idx;
    }
}

static void mark_used(SymbolTable* scope, Token name) {
    Symbol* sym = find_symbol(scope, name);
    if (sym) sym->is_used = true;
    // A re-declared name occupies a SECOND slot (add_symbol always appends;
    // nothing dedupes), but find_symbol returns just one of them - so marking
    // only that one left the other permanently unused and produced a false
    // "unused variable" warning for a variable that is plainly read on the very
    // next line:
    //
    //     var line_start = 0            // outer
    //     while ... {
    //         var line_start = i + 1    // inner, shadows
    //         print("${line_start}")    // READS it - yet it was reported unused
    //     }
    //
    // Since these warnings are per-function and this is a diagnostic rather than
    // a semantic decision, mark EVERY same-named slot. The failure direction
    // matters: a missed warning costs nothing, while a false one on correct code
    // trains people to ignore all warnings. Measured across sample-apps: 5 such
    // warnings on committed, working code (netstat-lite x3, dockermon, and this
    // shape), of which 4 were false.
    // Walk the whole enclosing chain, not just this scope: the READ usually
    // happens in a child scope (inside a for/if body) while the declarations sit
    // in the function scope, so marking only the innermost table left every
    // outer duplicate false. Measured on sysadmin/netstat-lite, which declares
    // `line_start` four times in one function and reads all four: slot 3 was
    // marked used, slots 6/11/14 were not, giving three false warnings.
    if (!name.start || name.length <= 0) return;
    for (SymbolTable* t = scope; t; t = t->parent) {
        for (int i = 0; i < t->count; i++) {
            Symbol* s = &t->symbols[i];
            if (s->name.length == name.length && s->name.start &&
                memcmp(s->name.start, name.start, name.length) == 0) {
                s->is_used = true;
            }
        }
    }
}

// K7: does this call target a genuinely non-callable value - i.e. a LOCAL
// variable holding a non-function (`var x = 5; x(3)`)? We must NOT reject calls
// to global names: the stdlib registers many callable builtins as `int`
// PLACEHOLDERS (sleep_ms, await_any, clamp, sign, input, ...) that codegen
// resolves by name, and a user function can also shadow such a placeholder as a
// separate global symbol entry. So we only diagnose when the name resolves to a
// non-function in a LOCAL scope (strictly below global_scope) and no function
// of that name shadows it in the local chain. Returns the offending local
// symbol, or NULL if the target is callable / global / a real function.
static bool token_name_eq(Token a, Token b) {
    return a.length == b.length && a.length > 0 &&
           memcmp(a.start, b.start, a.length) == 0;
}
static Symbol* find_local_noncallable(SymbolTable* scope, Token name) {
    for (SymbolTable* s = scope; s && s != global_scope; s = s->parent) {
        for (int i = 0; i < s->count; i++) {
            Symbol* sym = &s->symbols[i];
            if (!token_name_eq(sym->name, name)) continue;
            // Found the name in a local scope. If any overload here is a
            // function (e.g. a lambda-typed local), it's callable.
            for (Symbol* o = sym; o; o = o->next_overload) {
                if (o->type && o->type->kind == TYPE_FUNCTION) return NULL;
            }
            if (sym->type && (sym->type->kind == TYPE_INT ||
                              sym->type->kind == TYPE_FLOAT ||
                              sym->type->kind == TYPE_BOOL ||
                              sym->type->kind == TYPE_STRING ||
                              sym->type->kind == TYPE_ARRAY ||
                              sym->type->kind == TYPE_MAP)) {
                return sym;
            }
            return NULL; // some other kind - don't touch
        }
    }
    return NULL;
}

// Lazily register a monomorphic Option<Struct> family for a user-struct payload
// (e.g. `User` -> `OptionUser`): the fake struct type symbol + the 6 member
// functions (_Some/_None/_is_some/_is_none/_unwrap/_unwrap_or), mirroring the
// builtin OptionInt/OptionString families. Idempotent. Also records the struct in
// codegen's registry so the concrete C family is emitted after the struct typedef.
// Returns the Option<Struct> Type (a TYPE_STRUCT named "Option<Struct>").
//
// A DATA-CARRYING ENUM takes this path too. It lowers to a C struct (a tagged union),
// so it needs the same monomorphic family a user struct gets -- but its checker Type is
// TYPE_ENUM, not TYPE_STRUCT, and the `kind != TYPE_STRUCT` guard used to reject it. Every
// caller then fell back to OptionInt while codegen (which asks wyn_option_family, the
// authority) correctly produced OptionShape, so `fn pick() -> Shape?` emitted
// `OptionInt __match_opt_1 = pick(1);` against a function returning OptionShape and the C
// compile failed. Accepting it HERE fixes all five call sites at once rather than adding the
// same enum test to each -- the shape of the earlier wyn_option_family() consolidation.
//
// A PLAIN enum must still be rejected: it is an `int` in C, so its family is the builtin
// OptionInt, and callers rely on NULL to mean exactly that.
static Type* register_option_struct_family(Type* struct_type) {
    if (!struct_type) return NULL;
    bool is_data_enum = struct_type->kind == TYPE_ENUM &&
                        struct_type->name.length > 0 &&
                        enum_name_is_data_enum(struct_type->name);
    if (!is_data_enum &&
        (struct_type->kind != TYPE_STRUCT || struct_type->struct_type.name.length == 0))
        return NULL;
    char sname[96];
    token_to_cstr(sname, sizeof(sname),
                  is_data_enum ? struct_type->name : struct_type->struct_type.name);
    // Family type name "Option<Struct>" - persistent storage for the Token.
    char* fam = malloc(strlen(sname) + 7);
    sprintf(fam, "Option%s", sname);
    Token fam_tok = {TOKEN_IDENT, fam, (int)strlen(fam), 0};

    // Idempotent: if already registered, return the existing symbol's type.
    Symbol* existing = find_symbol(global_scope, fam_tok);
    if (existing) { free(fam); return existing->type; }

    Type* opt_type = make_type(TYPE_STRUCT);
    opt_type->struct_type.name = fam_tok;
    add_symbol(global_scope, fam_tok, opt_type, false);

    // Helper to register a member fn: name suffix, param types, return type.
    #define REG_OPT_FN(suffix, npar, p0, p1, ret) do {                      \
        Type* ft = make_type(TYPE_FUNCTION);                                \
        ft->fn_type.param_count = (npar);                                   \
        ft->fn_type.param_types = (npar) ? malloc(sizeof(Type*) * (npar)) : NULL; \
        if ((npar) >= 1) ft->fn_type.param_types[0] = (p0);                 \
        if ((npar) >= 2) ft->fn_type.param_types[1] = (p1);                 \
        ft->fn_type.return_type = (ret);                                    \
        char* fn = malloc(strlen(fam) + strlen(suffix) + 2);                \
        sprintf(fn, "%s%s", fam, suffix);                                   \
        Token ftok = {TOKEN_IDENT, fn, (int)strlen(fn), 0};                 \
        add_symbol(global_scope, ftok, ft, false);                          \
    } while (0)

    REG_OPT_FN("_Some", 1, struct_type, NULL, opt_type);
    REG_OPT_FN("_None", 0, NULL, NULL, opt_type);
    REG_OPT_FN("_is_some", 1, opt_type, NULL, builtin_bool);
    REG_OPT_FN("_is_none", 1, opt_type, NULL, builtin_bool);
    REG_OPT_FN("_unwrap", 1, opt_type, NULL, struct_type);
    REG_OPT_FN("_unwrap_or", 2, opt_type, struct_type, struct_type);
    #undef REG_OPT_FN

    extern void register_option_struct(const char*);
    register_option_struct(sname);
    return opt_type;
}

// The Wyn type-name of an error payload, used for the family-name suffix. Returns
// a static string: "string"/"int"/"float"/"bool" for scalars, the struct name for
// a struct error. NULL if the type is unusable.
static const char* result_err_type_name(Type* err_type) {
    if (!err_type) return "string";
    switch (err_type->kind) {
        case TYPE_STRING: return "string";
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_BOOL:   return "bool";
        case TYPE_STRUCT: {
            if (err_type->struct_type.name.length == 0) return NULL;
            static char b[96]; token_to_cstr(b, sizeof(b), err_type->struct_type.name);
            return b;
        }
        case TYPE_ENUM: {
            // A plain enum lowers to a C typedef of its own name, so it is a valid
            // err payload exactly like a struct. Returning NULL here (the old
            // behavior) made the checker fall back to the `string` family while
            // codegen still named `Result<Ok>_<Enum>` — the two disagreed and the
            // build failed with "unknown method 'ResultP.Code_Err'".
            if (err_type->name.length == 0) return NULL;
            static char b[96]; token_to_cstr(b, sizeof(b), err_type->name);
            return b;
        }
        default: return NULL;
    }
}

// The payload type bound by a `Result` match arm. `fam_type` is the family's fake
// struct type (named "Result<OkTag>" for a string err, "Result<OkTag>_<ErrTag>"
// otherwise); `want_ok` selects the Ok arm's payload over the Err arm's.
//
// Without this, a Result arm binding defaulted to `int`: the checker then accepted
// nonsense like `e.nope` on a struct error, and codegen could not dispatch
// `p.msg.len()` or `c.method()` — it emitted an EMPTY receiver, surfacing as
// "error: expected expression" or "Unknown method 'len' (no type info)".
// Returns NULL when `fam_type` is not a Result family or the payload is unresolvable,
// so callers keep their existing default.
static Type* result_arm_payload_type(Type* fam_type, bool want_ok) {
    if (!fam_type || fam_type->kind != TYPE_STRUCT) return NULL;
    Token fam = fam_type->struct_type.name;
    if (fam.length <= 6 || memcmp(fam.start, "Result", 6) != 0) return NULL;
    char buf[192];
    int len = fam.length - 6;
    if (len <= 0 || len >= (int)sizeof(buf)) return NULL;
    memcpy(buf, fam.start + 6, len); buf[len] = '\0';
    // Split "<OkTag>_<ErrTag>" at the FIRST '_'. No '_' means the bare,
    // backward-compatible family name, whose error payload is a `string`.
    char* us = strchr(buf, '_');
    const char* ok_tag = buf;
    const char* err_tag = NULL;
    if (us) { *us = '\0'; err_tag = us + 1; }
    const char* want = want_ok ? ok_tag : err_tag;
    if (!want_ok && !err_tag) return builtin_string;
    if (!want || !*want) return NULL;
    if (strcmp(want, "String") == 0) return builtin_string;
    if (strcmp(want, "Int") == 0)    return builtin_int;
    if (strcmp(want, "Float") == 0)  return builtin_float;
    if (strcmp(want, "Bool") == 0)   return builtin_bool;
    if (strcmp(want, "string") == 0) return builtin_string;
    if (strcmp(want, "int") == 0)    return builtin_int;
    if (strcmp(want, "float") == 0)  return builtin_float;
    if (strcmp(want, "bool") == 0)   return builtin_bool;
    // A user struct or enum payload.
    Token pn = {TOKEN_IDENT, want, (int)strlen(want), 0};
    Symbol* ps = find_symbol(global_scope, pn);
    if (ps && ps->type && (ps->type->kind == TYPE_STRUCT || ps->type->kind == TYPE_ENUM))
        return ps->type;
    return NULL;
}

// Lazily register a monomorphic Result<Ok, Err> family for a user-struct ok-payload
// (e.g. `Point`): the fake struct type symbol + the 7 member functions
// (_Ok/_Err/_is_ok/_is_err/_unwrap/_unwrap_err/_unwrap_or), mirroring the builtin
// ResultInt/ResultString families and the Option<Struct> analogue above.
//
// Generalized from #181 (which pinned the error to `string`): `err_type` may now be
// a string (the common case, keeps the bare `Result<Ok>` family name for backward
// compatibility), a scalar (int/float/bool), or another user struct. The family is
// keyed by BOTH types — a non-string error appends `_<ErrTag>` to the name
// (`ResultPoint_Fail`, `ResultPoint_int`) so `Result<Point,string>` and
// `Result<Point,Fail>` are distinct and never collide. `err_type == NULL` means the
// string default. Idempotent. Also records the family in codegen's registry so the
// concrete C family is emitted after the struct typedefs.
// Returns the Result Type (a TYPE_STRUCT named after the family).
static Type* register_result_struct_family_e(Type* struct_type, Type* err_type) {
    if (!struct_type) return NULL;
    // The ok payload is either a user struct (#181/#182) or a primitive. A primitive
    // ok is spelled with the builtin's own tag ("int" -> "Int"), so a `string` error
    // resolves to the existing builtin family (ResultInt/...) via the find_symbol
    // check below, while a NON-string error gets its own `_<ErrTag>` family instead
    // of collapsing onto the builtin (whose err_value is hardcoded `const char*`).
    char sname[96];
    const char* ok_tag = NULL;
    switch (struct_type->kind) {
        case TYPE_INT:    ok_tag = "Int";    break;
        case TYPE_STRING: ok_tag = "String"; break;
        case TYPE_FLOAT:  ok_tag = "Float";  break;
        case TYPE_BOOL:   ok_tag = "Bool";   break;
        case TYPE_STRUCT:
            if (struct_type->struct_type.name.length == 0) return NULL;
            token_to_cstr(sname, sizeof(sname), struct_type->struct_type.name);
            ok_tag = sname;
            break;
        default: return NULL;
    }
    // Wyn-level ok type name handed to codegen (it derives the C type + tag itself).
    const char* ok_wyn = (struct_type->kind == TYPE_INT)    ? "int"
                       : (struct_type->kind == TYPE_STRING) ? "string"
                       : (struct_type->kind == TYPE_FLOAT)  ? "float"
                       : (struct_type->kind == TYPE_BOOL)   ? "bool"
                       : sname;
    const char* ename = result_err_type_name(err_type);
    if (!ename) ename = "string";              // unusable err falls back to string
    Type* eff_err = err_type ? err_type : builtin_string;
    if (eff_err == builtin_string || eff_err->kind == TYPE_STRING) ename = "string";

    // Family name: bare "Result<Ok>" for a string error (backward compatible),
    // "Result<Ok>_<ErrTag>" otherwise.
    char fambuf[192];
    if (strcmp(ename, "string") == 0) snprintf(fambuf, sizeof(fambuf), "Result%s", ok_tag);
    else                              snprintf(fambuf, sizeof(fambuf), "Result%s_%s", ok_tag, ename);
    char* fam = strdup(fambuf);
    Token fam_tok = {TOKEN_IDENT, fam, (int)strlen(fam), 0};

    Symbol* existing = find_symbol(global_scope, fam_tok);
    if (existing) { free(fam); return existing->type; }

    Type* res_type = make_type(TYPE_STRUCT);
    res_type->struct_type.name = fam_tok;
    add_symbol(global_scope, fam_tok, res_type, false);

    #define REG_RES_FN(suffix, npar, p0, p1, ret) do {                      \
        Type* ft = make_type(TYPE_FUNCTION);                                \
        ft->fn_type.param_count = (npar);                                   \
        ft->fn_type.param_types = (npar) ? malloc(sizeof(Type*) * (npar)) : NULL; \
        if ((npar) >= 1) ft->fn_type.param_types[0] = (p0);                 \
        if ((npar) >= 2) ft->fn_type.param_types[1] = (p1);                 \
        ft->fn_type.return_type = (ret);                                    \
        char* fn = malloc(strlen(fam) + strlen(suffix) + 2);                \
        sprintf(fn, "%s%s", fam, suffix);                                   \
        Token ftok = {TOKEN_IDENT, fn, (int)strlen(fn), 0};                 \
        add_symbol(global_scope, ftok, ft, false);                          \
    } while (0)

    REG_RES_FN("_Ok", 1, struct_type, NULL, res_type);
    REG_RES_FN("_Err", 1, eff_err, NULL, res_type);
    REG_RES_FN("_is_ok", 1, res_type, NULL, builtin_bool);
    REG_RES_FN("_is_err", 1, res_type, NULL, builtin_bool);
    REG_RES_FN("_unwrap", 1, res_type, NULL, struct_type);
    REG_RES_FN("_unwrap_err", 1, res_type, NULL, eff_err);
    REG_RES_FN("_unwrap_or", 2, res_type, struct_type, struct_type);
    #undef REG_RES_FN

    extern const char* register_result_family_for_types(const char*, const char*);
    register_result_family_for_types(ok_wyn, ename);
    return res_type;
}
// Backward-compatible shim: string-error family.
static Type* register_result_struct_family(Type* struct_type) {
    return register_result_struct_family_e(struct_type, NULL);
}

// reg_fn - register one builtin function signature in the global scope.
//
// Every builtin was hand-rolled as an 11-line block:
//
//     {
//         Type* ft = make_type(TYPE_FUNCTION);
//         ft->fn_type.param_count = 4;
//         ft->fn_type.param_types = malloc(sizeof(Type*) * 4);
//         ft->fn_type.param_types[0] = builtin_int;
//         ...
//         Token tok = {TOKEN_IDENT, "Http_respond", 12, 0};
//         add_symbol(global_scope, tok, ft, false);
//     }
//
// 126 of them, ~1400 lines of the 1707-line init_checker. Besides the bulk, the
// hand-written form carries a hazard the helper removes: the Token length is a
// HAND-COUNTED literal (`"Http_respond", 12`). Miscount it and the symbol is
// registered under a truncated or overlong name, so the builtin silently does not
// resolve - a bug you cannot see by reading the line it is on. strlen() cannot be
// miscounted.
//
// Variadic in the param types, terminated by the count, so a signature reads as one
// line that looks like the Wyn declaration it mirrors.
static void reg_fn(const char* name, Type* ret, int npar, ...) {
    Type* ft = make_type(TYPE_FUNCTION);
    ft->fn_type.param_count = npar;
    ft->fn_type.param_types = npar ? malloc(sizeof(Type*) * npar) : NULL;
    va_list ap;
    va_start(ap, npar);
    for (int i = 0; i < npar; i++) ft->fn_type.param_types[i] = va_arg(ap, Type*);
    va_end(ap);
    ft->fn_type.return_type = ret;
    Token tok = {TOKEN_IDENT, name, (int)strlen(name), 0};
    add_symbol(global_scope, tok, ft, false);
}

// init_checker() lives in checker_builtins.c - see the header comment there.
#include "checker_builtins.c"


Symbol* find_symbol(SymbolTable* scope, Token name) {
    if (name.length <= 0 || !name.start) {
        if (scope->parent) return find_symbol(scope->parent, name);
        return NULL;
    }
    if (scope->hash_capacity > 0 && scope->hash_indices) {
        int mask = scope->hash_capacity - 1;
        uint32_t h = sym_hash(name.start, name.length);
        int slot = h & mask;
        while (scope->hash_indices[slot] != -1) {
            int idx = scope->hash_indices[slot];
            if (scope->symbols[idx].name.length == name.length &&
                memcmp(scope->symbols[idx].name.start, name.start, name.length) == 0) {
                return &scope->symbols[idx];
            }
            slot = (slot + 1) & mask;
        }
    } else {
        for (int i = 0; i < scope->count; i++) {
            if (scope->symbols[i].name.length == name.length &&
                memcmp(scope->symbols[i].name.start, name.start, name.length) == 0) {
                return &scope->symbols[i];
            }
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
            token_to_cstr(func_name, sizeof(func_name), name);
            type_error_mismatch("unique function signature", "duplicate signature", func_name, name.line, 0);
            return;
        }
        
        // Add to overload chain
        Symbol* current = existing;
        while (current->next_overload) {
            if (signatures_match(current->next_overload->type, type)) {
                char func_name[256];
                token_to_cstr(func_name, sizeof(func_name), name);
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

    Symbol* best_match = NULL;
    int best_score = -1;

    // Check all overloads. The head symbol may be a NON-function (e.g. a stdlib
    // placeholder registered as `int` for a name a user later defines a real
    // function under, like `clamp`/`sign`/`input`) - a real function overload
    // can still live further down the chain, so we must scan past a non-function
    // head rather than bailing on it. If NO function is found, fall back to
    // returning the head so callers can diagnose a non-callable value (K7).
    Symbol* current = symbol;
    while (current) {
        if (current->type && current->type->kind == TYPE_FUNCTION) {
            int score = calculate_match_score(current->type, arg_types, arg_count);
            if (score > best_score) {
                best_score = score;
                best_match = current;
            }
        }
        current = current->next_overload;
    }

    return best_match ? best_match : symbol;
}

// T1.5.3: Calculate how well a function signature matches the arguments
static int calculate_match_score(Type* fn_type, Type** arg_types, int arg_count) {
    if (fn_type->kind != TYPE_FUNCTION) return -1;
    
    FunctionType* func = &fn_type->fn_type;
    
    // For variadic functions, allow arg_count >= param_count
    if (func->is_variadic) {
        if (arg_count < func->param_count) return -1;
    } else {
        int min_p = func->min_param_count;
        if (min_p < 0) min_p = func->param_count;  // -1 means unset
        if (arg_count < min_p || arg_count > func->param_count) return -1;
    }
    
    int score = 0;
    // Only check types for the declared parameters
    for (int i = 0; i < func->param_count && i < arg_count; i++) {
        if (!func->param_types || !func->param_types[i] || !arg_types[i]) {
            score += 1; // Unknown type - allow
        } else if (types_equal(func->param_types[i], arg_types[i])) {
            score += 10;  // Exact match
        } else if (can_convert_type(arg_types[i], func->param_types[i])) {
            score += 5;   // Convertible match
        } else if (func->is_variadic) {
            score += 1;   // Variadic builtins accept any type
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
    
    // Allow enum <-> int (enums are represented as ints)
    if ((from->kind == TYPE_ENUM && to->kind == TYPE_INT) ||
        (from->kind == TYPE_INT && to->kind == TYPE_ENUM)) return true;
    
    return false;
}

// A statically-known channel capacity, if the expression is a constant integer
// literal (optionally negated). Returns 1 and writes *out on success, else 0.
// Only literals are folded on purpose: anything computed is a runtime concern
// and stays the runtime's job (Task_channel still rejects capacity < 1).
static int channel_const_capacity(const Expr* cap, long long* out) {
    if (!cap) return 0;
    int negate = 0;
    if (cap->type == EXPR_UNARY && cap->unary.op.type == TOKEN_MINUS && cap->unary.operand) {
        negate = 1;
        cap = cap->unary.operand;
    }
    if (cap->type != EXPR_INT) return 0;
    char buf[80]; int bi = 0; int base = 10; int start = 0;
    const char* p = cap->token.start;
    int n = cap->token.length;
    if (n >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; start = 2; }
    else if (n >= 2 && p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; start = 2; }
    else if (n >= 2 && p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) { base = 8; start = 2; }
    for (int i = start; i < n && bi < (int)sizeof(buf) - 1; i++)
        if (p[i] != '_') buf[bi++] = p[i];
    buf[bi] = '\0';
    if (bi == 0) return 0;
    errno = 0;
    char* endp = NULL;
    unsigned long long v = strtoull(buf, &endp, base);
    if (errno != 0 || (endp && *endp) || v > 9223372036854775807ULL) return 0;
    *out = negate ? -(long long)v : (long long)v;
    return 1;
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
        case EXPR_INT: {
            // K8: reject integer literals that don't fit in Wyn's 64-bit int.
            // Without this, an oversized literal (9999999999999999999999) passed
            // check and hit an opaque codegen ICE with no diagnostic. Normalize
            // out underscores and any 0x/0b/0o prefix, then parse in the right
            // base with strtoull and flag ERANGE. Negative literals are a
            // separate unary-minus node, so the magnitude here is non-negative.
            {
                const char* p = expr->token.start;
                int n = expr->token.length;
                char buf[80]; int bi = 0; int base = 10;
                int start = 0;
                if (n >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; start = 2; }
                else if (n >= 2 && p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; start = 2; }
                else if (n >= 2 && p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) { base = 8; start = 2; }
                for (int i = start; i < n && bi < (int)sizeof(buf) - 1; i++) {
                    if (p[i] != '_') buf[bi++] = p[i];
                }
                buf[bi] = '\0';
                if (bi > 0) {
                    errno = 0;
                    char* endp = NULL;
                    unsigned long long v = strtoull(buf, &endp, base);
                    if (errno == ERANGE || v > 9223372036854775807ULL) {
                        fprintf(stderr, "Error at line %d: integer literal '%.*s' is too large for a 64-bit int (max 9223372036854775807)\n",
                                expr->token.line, n, p);
                        had_error = true;
                    }
                }
            }
            expr->expr_type = builtin_int;
            return builtin_int;
        }
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
            
            // Check for built-in Option/Result constants.
            //
            // A DECLARED VARIABLE WINS. This test used to run before the scope lookup
            // below, so any local spelled `none` was forced to int no matter what it
            // held: `none = f()` where f returns string, then `g(none)` against a
            // `string` parameter, reported "Expected: string, Got: int" - pointing at
            // the call rather than at the name, which is why it reads as a mystery.
            // `none` is not a reserved word (the parser accepts it as an identifier and
            // every other name tested - nil, null, empty, result - is unaffected), so
            // shadowing it is legal and must behave like shadowing anything else.
            // Found while building VisualWyn, which worked around it by renaming.
            if (expr->token.length == 4 && memcmp(expr->token.start, "none", 4) == 0 &&
                !find_symbol(scope, expr->token)) {
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
            
            // T2.5.1: Handle built-in type names - UNLESS a real variable of that
            // name is in scope. A user var may be named after a builtin type
            // (int/float/string/str/bool/char/void); codegen emits it with a
            // wynfn_ prefix, so the checker must resolve it as that variable (count
            // its uses, flow its real type), not swallow it as a type literal.
            if (!find_symbol(scope, expr->token)) {
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
            if (expr->token.length == 4 && memcmp(expr->token.start, "char", 4) == 0) {
                expr->expr_type = builtin_int;  // char is int in Wyn
                return builtin_int;
            }
            if (expr->token.length == 4 && memcmp(expr->token.start, "void", 4) == 0) {
                expr->expr_type = builtin_void;
                return builtin_void;
            }
            // The FFI pointer family. Missing here, a type annotation naming
            // `ptr`/`cstr` in a position that is checked as an EXPRESSION - a
            // STRUCT FIELD is the one that bites - fell through to the
            // undefined-variable branch and was rejected outright, with a
            // "Did you mean: Ptr?" pointing at the unrelated Ptr namespace:
            //   struct Row { buf: ptr }  ->  Error: Undefined variable 'ptr'
            // #235 fixed the same omission in the four ladders behind a module
            // pub fn signature; field types were not in that fix's scope.
            if (expr->token.length == 3 && memcmp(expr->token.start, "ptr", 3) == 0) {
                expr->expr_type = builtin_ptr;
                return builtin_ptr;
            }
            if (expr->token.length == 4 && memcmp(expr->token.start, "cstr", 4) == 0) {
                expr->expr_type = builtin_string;
                return builtin_string;
            }
            }

            Symbol* sym = find_symbol(scope, expr->token);
            if (sym) sym->is_used = true;
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
                    char name_buf[256]; token_to_cstr(name_buf, sizeof(name_buf), expr->token);
                    if (resolve_parser_module_alias(name_buf) != NULL) {
                        is_qualified = true;
                    }
                }
                
                if (is_qualified) {
                    // Module-qualified name - assume it's valid
                    expr->expr_type = builtin_int;  // Default type
                    return builtin_int;
                }
                
                // Suppress "Undefined variable" for function-like names (will be caught by function checker)
                // Heuristic: if name starts with lowercase and is short, it's likely a function call
                bool _suppress_var_err = false;
                {
                    char _vn[256]; token_to_cstr(_vn, sizeof(_vn), expr->token);
                    // Check if any function with this name exists (even with wrong args)
                    SymbolTable* _s = scope;
                    while (_s && !_suppress_var_err) {
                        for (int _si = 0; _si < _s->count; _si++) {
                            if (_s->symbols[_si].type && _s->symbols[_si].type->kind == TYPE_FUNCTION &&
                                _s->symbols[_si].name.length == expr->token.length &&
                                memcmp(_s->symbols[_si].name.start, expr->token.start, expr->token.length) == 0) {
                                _suppress_var_err = true; break;
                            }
                        }
                        _s = _s->parent;
                    }
                }
                // Foreign-value table: True/False/null/undefined/self etc. get
                // a targeted fix instead of "Undefined variable 'True' - did
                // you mean Time?" (edit-distance suggestions were harmful here).
                if (!_suppress_var_err) {
                    static const struct { const char* kw; const char* fix; } _fkv[] = {
                        {"True",      "booleans are lowercase: true"},
                        {"False",     "booleans are lowercase: false"},
                        {"None",      "Wyn optionals use lowercase 'none' (or Some(x))"},
                        {"null",      "Wyn has no null - use Option types (none / Some(x))"},
                        {"undefined", "Wyn has no undefined - use Option types (none / Some(x))"},
                        {"self",      "'self' only exists inside struct methods"},
                        {"console",   "print with: print(\"...\")"},
                        {"len",       "call it as a function: len(x), or use x.len()"},
                        {NULL, NULL}
                    };
                    for (int _fi = 0; _fkv[_fi].kw; _fi++) {
                        size_t _kl = strlen(_fkv[_fi].kw);
                        if ((size_t)expr->token.length == _kl &&
                            memcmp(expr->token.start, _fkv[_fi].kw, _kl) == 0) {
                            fprintf(stderr, "\nError at line %d: '%.*s' is not Wyn syntax - %s\n",
                                    expr->token.line, expr->token.length, expr->token.start,
                                    _fkv[_fi].fix);
                            show_source_line(expr->token.line);
                            had_error = true;
                            expr->expr_type = builtin_int;
                            return builtin_int;
                        }
                    }
                }
                if (!_suppress_var_err) {
                fprintf(stderr, "\nError at line %d: Undefined variable '%.*s'\n",
                        expr->token.line, expr->token.length, expr->token.start);
                show_source_line(expr->token.line);
                
                // Find similar variable name using Levenshtein distance
                char var_name[256];
                token_to_cstr(var_name, sizeof(var_name), expr->token);
                
                extern int levenshtein_distance(const char*, const char*);
                char best_name[256] = "";
                int best_dist = 999;
                for (int i = 0; i < scope->count; i++) {
                    char sym_name[256];
                    token_to_cstr(sym_name, sizeof(sym_name), scope->symbols[i].name);
                    int d = levenshtein_distance(var_name, sym_name);
                    if (d < best_dist && d <= 2) { best_dist = d; strncpy(best_name, sym_name, 255); }
                }
                // Also check global scope
                if (!best_name[0] && global_scope) {
                    for (int i = 0; i < global_scope->count; i++) {
                        char sym_name[256];
                        token_to_cstr(sym_name, sizeof(sym_name), global_scope->symbols[i].name);
                        int d = levenshtein_distance(var_name, sym_name);
                        if (d < best_dist && d <= 2) { best_dist = d; strncpy(best_name, sym_name, 255); }
                    }
                }
                if (best_name[0]) {
                    fprintf(stderr, "  \033[33mDid you mean:\033[0m %s?\n", best_name);
                }
                
                type_error_undefined_variable(var_name, expr->token.line, 0);
                
                had_error = true;
                }
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
            
            // Membership `x in c` / `x not in c` - always yields bool. The right
            // side must be a container (array/map/set/string); the element type
            // isn't constrained here (runtime dispatch handles it).
            if (expr->binary.op.type == TOKEN_IN) {
                if (right->kind != TYPE_ARRAY && right->kind != TYPE_MAP &&
                    right->kind != TYPE_SET && right->kind != TYPE_STRING) {
                    fprintf(stderr, "Error at line %d: 'in' requires an array, map, set, or string on the right\n",
                            expr->binary.op.line);
                    had_error = true;
                }
                expr->expr_type = builtin_bool;
                return builtin_bool;
            }
            
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
                // Return int (which works as bool in C). NOTE: the lambda and
                // predicate runtime ABI (long long (*fn)(...)) depends on this;
                // var decls storing an and/or result special-case the op to
                // declare bool (see codegen_stmt.c STMT_VAR EXPR_BINARY).
                expr->expr_type = builtin_int;
                return builtin_int;
            }
            
            // Comparison operators return bool
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ) {
                // Comparing an Option to None/none doesn't lower to C == (the
                // Option families are structs) - it used to either ICE at the C
                // level (`r == None`, struct == struct) or give an unhelpful
                // "Cannot compare different types" (`r == none`). Reject with
                // the actual fix. (Python devs write `x == None` on day one.)
                if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ) {
                    bool left_opt = left->kind == TYPE_STRUCT &&
                        left->struct_type.name.length >= 6 &&
                        memcmp(left->struct_type.name.start, "Option", 6) == 0;
                    bool right_is_none_ident =
                        (expr->binary.right->type == EXPR_IDENT &&
                         ((expr->binary.right->token.length == 4 &&
                           (memcmp(expr->binary.right->token.start, "none", 4) == 0 ||
                            memcmp(expr->binary.right->token.start, "None", 4) == 0)))) ||
                        expr->binary.right->type == EXPR_NONE;
                    if (left_opt && right_is_none_ident) {
                        const char* m = expr->binary.op.type == TOKEN_EQEQ ? "is_none" : "is_some";
                        fprintf(stderr, "Error at line %d: Options are not compared with %s none - use .%s()\n",
                                expr->binary.op.line,
                                expr->binary.op.type == TOKEN_EQEQ ? "==" : "!=", m);
                        had_error = true;
                        return NULL;
                    }
                    // Option == Option (both struct Option families) also can't
                    // lower to C ==; same targeted rejection.
                    bool right_opt = right->kind == TYPE_STRUCT &&
                        right->struct_type.name.length >= 6 &&
                        memcmp(right->struct_type.name.start, "Option", 6) == 0;
                    if (left_opt && right_opt) {
                        fprintf(stderr, "Error at line %d: Options cannot be compared directly - match on them, or compare .unwrap_or(...) values\n",
                                expr->binary.op.line);
                        had_error = true;
                        return NULL;
                    }
                    // struct == struct: lowered to a generated field-wise
                    // __wyn_eq_<Name> helper. Gate it here - different struct
                    // types can't compare, and structs with non-comparable
                    // fields (arrays/maps/fns) get a real error instead of
                    // the C-level ICE raw `==` used to produce.
                    bool left_ustruct = left->kind == TYPE_STRUCT && !left_opt &&
                        !is_ptr_type(left) && left->struct_type.name.length > 0 &&
                        find_struct_definition(left->struct_type.name);
                    bool right_opt2 = right->kind == TYPE_STRUCT &&
                        right->struct_type.name.length >= 6 &&
                        memcmp(right->struct_type.name.start, "Option", 6) == 0;
                    bool right_ustruct = right->kind == TYPE_STRUCT && !right_opt2 &&
                        !is_ptr_type(right) && right->struct_type.name.length > 0 &&
                        find_struct_definition(right->struct_type.name);
                    if (left_ustruct || right_ustruct) {
                        if (!left_ustruct || !right_ustruct ||
                            left->struct_type.name.length != right->struct_type.name.length ||
                            memcmp(left->struct_type.name.start, right->struct_type.name.start,
                                   left->struct_type.name.length) != 0) {
                            fprintf(stderr, "Error at line %d: Cannot compare different types\n",
                                    expr->binary.op.line);
                            had_error = true;
                            return NULL;
                        }
                        Token bad = {0};
                        if (!struct_fields_comparable(left->struct_type.name, &bad, 0)) {
                            fprintf(stderr, "Error at line %d: == is not defined for struct '%.*s' (field '%.*s' is not comparable) - compare fields explicitly\n",
                                    expr->binary.op.line,
                                    left->struct_type.name.length, left->struct_type.name.start,
                                    bad.length, bad.start);
                            had_error = true;
                            return NULL;
                        }
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    }
                }
                // Ordering on structs has no meaning (and raw C < on struct
                // values doesn't compile) - reject at check time.
                if ((left->kind == TYPE_STRUCT || right->kind == TYPE_STRUCT) &&
                    !is_ptr_type(left) && !is_ptr_type(right) &&
                    (expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                     expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ)) {
                    bool l_user = left->kind == TYPE_STRUCT && left->struct_type.name.length > 0 &&
                                  find_struct_definition(left->struct_type.name);
                    bool r_user = right->kind == TYPE_STRUCT && right->struct_type.name.length > 0 &&
                                  find_struct_definition(right->struct_type.name);
                    if (l_user || r_user) {
                        fprintf(stderr, "Error at line %d: structs cannot be ordered with %.*s - compare fields explicitly\n",
                                expr->binary.op.line, expr->binary.op.length, expr->binary.op.start);
                        had_error = true;
                        return NULL;
                    }
                }
                // Allow comparing compatible types
                // Int, bool, and enum are all compatible for comparison
                // Only operands in the SAME comparable family may compare.
                // A blanket "either side is a string/function is OK" used to
                // let `int == string` type-check and then SIGSEGV at runtime
                // (codegen emits strcmp((char*)5, ...)). Require:
                //   - numeric family: int/float/bool/enum interoperate (C
                //     numeric ==; int<->float promotion, bool/enum are ints)
                //   - string == string only
                //   - ptr == ptr, or ptr == int (the C NULL idiom `if p == 0`)
                //   - generic type params (T): unknown, stay permissive so
                //     `a == b` inside a generic fn still checks
                // struct == struct and struct ordering are handled above.
                bool left_num  = left->kind == TYPE_INT || left->kind == TYPE_FLOAT ||
                                 left->kind == TYPE_BOOL || left->kind == TYPE_ENUM;
                bool right_num = right->kind == TYPE_INT || right->kind == TYPE_FLOAT ||
                                 right->kind == TYPE_BOOL || right->kind == TYPE_ENUM;
                bool types_compatible =
                    (left_num && right_num) ||
                    (left->kind == TYPE_STRING && right->kind == TYPE_STRING) ||
                    // FFI opaque pointer (`ptr`, a TYPE_STRUCT named "void*"):
                    // comparable to another ptr and to the int literal 0.
                    (is_ptr_type(left) && is_ptr_type(right)) ||
                    (is_ptr_type(left) && right->kind == TYPE_INT) ||
                    (is_ptr_type(right) && left->kind == TYPE_INT) ||
                    // Generic type params can't be resolved statically.
                    (left->kind == TYPE_GENERIC) || (right->kind == TYPE_GENERIC);

                if (!types_compatible) {
                    char lt[256], rt[256];
                    snprintf(lt, sizeof(lt), "%s", type_to_string(left));
                    snprintf(rt, sizeof(rt), "%s", type_to_string(right));
                    fprintf(stderr, "Error at line %d: Cannot compare %s with %s - operands are not the same comparable type\n",
                            expr->binary.op.line, lt, rt);
                    had_error = true;
                    return NULL;
                }
                expr->expr_type = builtin_int;  // Return int (works as bool; see and/or note above)
                return builtin_int;
            }
            
            // Bit shifts: int-only (C semantics; no float/string shifting)
            if (expr->binary.op.type == TOKEN_LSHIFT || expr->binary.op.type == TOKEN_RSHIFT) {
                if (left->kind != TYPE_INT || right->kind != TYPE_INT) {
                    fprintf(stderr, "Error at line %d: '%.*s' requires int operands\n",
                            expr->binary.op.line, expr->binary.op.length, expr->binary.op.start);
                    had_error = true;
                    return NULL;
                }
                expr->expr_type = builtin_int;
                return builtin_int;
            }

            // String repeat: "ha" * 3 → string
            if (expr->binary.op.type == TOKEN_STAR && left->kind == TYPE_STRING && right->kind == TYPE_INT) {
                expr->expr_type = builtin_string;
                return builtin_string;
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
                // Auto-promote int to float in mixed arithmetic
                if ((left->kind == TYPE_FLOAT && right->kind == TYPE_INT) ||
                    (left->kind == TYPE_INT && right->kind == TYPE_FLOAT)) {
                    expr->expr_type = builtin_float;
                    return builtin_float;
                }
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
            // Check for named arguments
            bool _has_named_args = false;
            if (expr->call.arg_names) {
                for (int i = 0; i < expr->call.arg_count; i++)
                    if (expr->call.arg_names[i].length > 0) { _has_named_args = true; break; }
            }
            // Check for enum constructor calls: EnumName::Variant(args)
            //
            // The parser folds `Shape::Circle` into ONE EXPR_IDENT whose text is
            // "Shape::Circle", so this never reached the EXPR_FIELD_ACCESS path below that
            // handles the equivalent `Shape.Circle(...)`. The name IS registered, but as a
            // value symbol of the enum type rather than as a function, so a CALL through it
            // fell all the way to the module-qualified-call handling and came back
            // `builtin_int`. The expression then had no enum type for anything downstream to
            // see:
            //
            //     a = Shape::Circle(1.0)      // a: int
            //     xs = [a]                    // array_push_int(&arr, a)  -> C error:
            //                                 //   passing 'Shape' to parameter of type
            //                                 //   'long long'
            //
            // It only SURFACED in an array literal, because that is where codegen consults
            // the element's expr_type; `a` alone, and `take(Shape::Circle(1.0))`, both worked,
            // which is why this looked like "arrays of data enums are broken". The dot form
            // was fine throughout -- the two spellings simply disagreed.
            if (expr->call.callee->type == EXPR_IDENT) {
                Token qn = expr->call.callee->token;
                int sep = -1;
                for (int i = 0; i + 1 < qn.length; i++)
                    if (qn.start[i] == ':' && qn.start[i+1] == ':') { sep = i; break; }
                if (sep > 0) {
                    // Resolve the enum half; the C constructor is EnumName_VariantName.
                    Token ename = {TOKEN_IDENT, qn.start, sep, qn.line};
                    Symbol* esym = find_symbol(scope, ename);
                    if (esym && esym->type && esym->type->kind == TYPE_ENUM) {
                        char cname[256];
                        snprintf(cname, sizeof(cname), "%.*s_%.*s", sep, qn.start,
                                 qn.length - sep - 2, qn.start + sep + 2);
                        Token ctok = {TOKEN_IDENT, cname, (int)strlen(cname), qn.line};
                        Symbol* csym = find_symbol(scope, ctok);
                        if (csym && csym->type && csym->type->kind == TYPE_FUNCTION) {
                            for (int i = 0; i < expr->call.arg_count; i++)
                                check_expr(expr->call.args[i], scope);
                            expr->expr_type = esym->type;
                            return esym->type;
                        }
                    }
                }
            }
            // Check for enum constructor calls: EnumName.Variant(args)
            if (expr->call.callee->type == EXPR_FIELD_ACCESS &&
                expr->call.callee->field_access.object->type == EXPR_IDENT) {
                Token obj = expr->call.callee->field_access.object->token;
                Token field = expr->call.callee->field_access.field;
                // Look up as EnumName_Variant constructor
                char ctor_name[256];
                snprintf(ctor_name, 256, "%.*s_%.*s", obj.length, obj.start, field.length, field.start);
                Token ctor_tok = {TOKEN_IDENT, ctor_name, (int)strlen(ctor_name), obj.line};
                Symbol* ctor_sym = find_symbol(scope, ctor_tok);
                if (ctor_sym && ctor_sym->type && ctor_sym->type->kind == TYPE_FUNCTION) {
                    // It's an enum constructor - check args and return enum type
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        check_expr(expr->call.args[i], scope);
                    }
                    // Return the enum type
                    Symbol* enum_sym = find_symbol(scope, obj);
                    if (enum_sym && enum_sym->type && enum_sym->type->kind == TYPE_ENUM) {
                        expr->expr_type = enum_sym->type;
                        return enum_sym->type;
                    }
                    expr->expr_type = ctor_sym->type->fn_type.return_type;
                    return ctor_sym->type->fn_type.return_type;
                }
            }
            // BARE (unqualified) enum constructor call: `Circle(5)` where Circle
            // is a variant of some enum. The variant name is registered as a
            // value symbol (TYPE_ENUM) but not as a function, so without this it
            // falls through to the int fallback and codegen emits an undeclared
            // `Circle(` call. Type it as the enum, mirroring the qualified
            // `Shape.Circle(5)` path above and the bare match-arm resolution.
            // Rule: a real user FUNCTION of the same name wins (do not shadow
            // it); the enum constructor stays reachable via the qualified form.
            if (expr->call.callee->type == EXPR_IDENT) {
                Token bare = expr->call.callee->token;
                Symbol* fn_sym = find_symbol(scope, bare);
                bool is_real_fn = false;
                for (Symbol* s = fn_sym; s; s = s->next_overload) {
                    if (s->type && s->type->kind == TYPE_FUNCTION) { is_real_fn = true; break; }
                }
                if (!is_real_fn) {
                    int bvi = -1;
                    EnumStmt* ed = find_enum_for_bare_variant(bare, &bvi);
                    // Only intercept payload-carrying variants: a bare no-payload
                    // variant is a value, not a call, and any `V()` on it is a
                    // genuine error we leave to the normal path.
                    if (ed && bvi >= 0 && ed->variant_type_counts &&
                        ed->variant_type_counts[bvi] > 0) {
                        for (int i = 0; i < expr->call.arg_count; i++)
                            check_expr(expr->call.args[i], scope);
                        Type* et = make_type(TYPE_ENUM);
                        et->name = ed->name;
                        et->enum_type.variants = ed->variants;
                        et->enum_type.variant_count = ed->variant_count;
                        expr->expr_type = et;
                        return et;
                    }
                }
            }
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
                } else if (strcmp(name_buf, "await_all") == 0) {
                    // await_all(futures) : [T] where T is the futures' common
                    // result type. EXPR_SPAWN types a future as its result T,
                    // so the argument array's element type IS T. The blanket
                    // global registration (return [int]) made every result int:
                    // string results passed `wyn check` and then died at codegen
                    // ("Unknown method 'len' for type 'int'"); float/struct
                    // results miscompiled at the C level. Mixed-type future
                    // arrays are already rejected by the array-literal
                    // consistency check / the push type check on the argument.
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'await_all' expects 1 argument (an array of futures), got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_int;
                    }
                    Type* aa_arg = check_expr(expr->call.args[0], scope);
                    Type* aa_elem = (aa_arg && aa_arg->kind == TYPE_ARRAY)
                        ? aa_arg->array_type.element_type : NULL;
                    Type* aa_ret = make_type(TYPE_ARRAY);
                    aa_ret->array_type.element_type = aa_elem ? aa_elem : builtin_int;
                    expr->expr_type = aa_ret;
                    return aa_ret;
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
                } else if (strcmp(name_buf, "array_push") == 0 || strcmp(name_buf, "array_push_str") == 0) {
                    // array_push(arr, val) - infer element type from val
                    if (expr->call.arg_count >= 2) {
                        Type* arr_type = check_expr(expr->call.args[0], scope);
                        Type* val_type = check_expr(expr->call.args[1], scope);
                        // Update array element type - but only if consistent
                        if (arr_type && arr_type->kind == TYPE_ARRAY && val_type) {
                            Type* existing = arr_type->array_type.element_type;
                            if (!existing) {
                                arr_type->array_type.element_type = val_type;
                            } else if (existing->kind != val_type->kind) {
                                fprintf(stderr, "Error at line %d: Cannot push %s into array of %s\n",
                                    func_name.line,
                                    val_type->kind == TYPE_STRING ? "string" : "int",
                                    existing->kind == TYPE_STRING ? "string" : "int");
                                fprintf(stderr, "  \033[34mHelp:\033[0m Use separate arrays for different types\n");
                                had_error = true;
                            }
                        }
                        if (expr->call.args[0]->type == EXPR_IDENT) {
                            Symbol* sym = find_symbol(scope, expr->call.args[0]->token);
                            if (sym && sym->type && sym->type->kind == TYPE_ARRAY && val_type) {
                                Type* existing = sym->type->array_type.element_type;
                                if (!existing) {
                                    sym->type->array_type.element_type = val_type;
                                } else if (existing->kind != val_type->kind) {
                                    sym->type->array_type.element_type = NULL;
                                }
                            }
                        }
                    }
                    expr->expr_type = builtin_void;
                    return builtin_void;
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
                } else if (strcmp(name_buf, "assert_eq") == 0) {
                    if (expr->call.arg_count != 2) {
                        fprintf(stderr, "Error at line %d: 'assert_eq' expects 2 arguments, got %d\n",
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
                } else if (strcmp(name_buf, "str") == 0 && !find_symbol(scope, func_name)) {
                    // str(x) - uniform to-string (Python str), week-one P5.
                    // Same silent-forgiveness family as len(): the function form
                    // aliases .to_string(). Skipped when a user fn named str exists.
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'str' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_string;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if ((strcmp(name_buf, "int") == 0 || strcmp(name_buf, "float") == 0) &&
                           !find_symbol(scope, func_name)) {
                    // int(x) / float(x) - numeric conversion builtins (Python).
                    // int("42")/int(3.9) truncates; float("1.5")/float(2) widens.
                    Type* conv_ret = name_buf[0] == 'i' ? builtin_int : builtin_float;
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: '%s' expects 1 argument, got %d\n",
                                func_name.line, name_buf, expr->call.arg_count);
                        had_error = true;
                        return conv_ret;
                    }
                    check_expr(expr->call.args[0], scope);
                    expr->expr_type = conv_ret;
                    return conv_ret;
                } else if (strcmp(name_buf, "sorted") == 0 && !find_symbol(scope, func_name)) {
                    // sorted(xs) - non-mutating sort (Python sorted), the
                    // function spelling of xs.sorted(). Returns the array type.
                    if (expr->call.arg_count != 1) {
                        fprintf(stderr, "Error at line %d: 'sorted' expects 1 argument, got %d\n",
                                func_name.line, expr->call.arg_count);
                        had_error = true;
                        return builtin_array;
                    }
                    Type* arr_t = check_expr(expr->call.args[0], scope);
                    Type* ret_t = (arr_t && arr_t->kind == TYPE_ARRAY) ? arr_t : builtin_array;
                    expr->expr_type = ret_t;
                    return ret_t;
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
                
                
                // W9: reject a FLAT call to a function that is only available via
                // a whole-module `import m` - it must be qualified `m.foo()`.
                // Selective imports and locally-defined functions stay flat (see
                // flat_callable_in_program). Only enforced in the top-level
                // program (current_module_name empty); a module's own internal
                // calls to its siblings remain flat.
                if (current_module_name[0] == '\0') {
                    const char* owner = whole_module_fn_owner(name_buf);
                    if (owner &&
                        !flat_callable_in_program(current_program, func_name.start, func_name.length)) {
                        fprintf(stderr, "\nError at line %d: '%.*s' must be called as '%s.%.*s(...)'\n",
                                func_name.line, func_name.length, func_name.start,
                                owner, func_name.length, func_name.start);
                        show_source_line(func_name.line);
                        fprintf(stderr, "  \033[34mHelp:\033[0m after `import %s`, use the module-qualified form `%s.%.*s(...)`.\n",
                                owner, owner, func_name.length, func_name.start);
                        fprintf(stderr, "        Or import the name explicitly: `import { %.*s } from %s`.\n",
                                func_name.length, func_name.start, owner);
                        had_error = true;
                        for (int i = 0; i < expr->call.arg_count; i++) check_expr(expr->call.args[i], scope);
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    }
                }

                // T1.5.3: Function overloading support with T1.5.4: Parameter validation
                // Collect argument types for overload resolution
                Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                for (int i = 0; i < expr->call.arg_count; i++) {
                    arg_types[i] = check_expr(expr->call.args[i], scope);
                }
                
                // Find best matching overload
                Symbol* best_match = NULL;
                if (_has_named_args) {
                    // Named args: just look up by name, skip type matching
                    best_match = find_symbol(scope, expr->call.callee->token);
                } else {
                    best_match = find_function_overload(scope, expr->call.callee->token, arg_types, expr->call.arg_count);
                }
                
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
                        private_fn_error(expr->call.callee->token.line, qual_func, qual_module);
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
                    // Stdlib namespaces register their functions under the
                    // `Module_func` underscore name (e.g. Color_green, String_…).
                    // Resolve the qualified `Module::func` callee to that symbol and
                    // use its REAL return type - otherwise every namespaced stdlib
                    // call defaulted to int, so a string-returning one (Color::green)
                    // was concatenated/printed as a raw pointer value. (2026-07)
                    {
                        char _uname[264];
                        snprintf(_uname, sizeof(_uname), "%s_%s", qual_module, qual_func);
                        Token _utok = {TOKEN_IDENT, _uname, (int)strlen(_uname), expr->call.callee->token.line};
                        Symbol* _usym = find_symbol(scope, _utok);
                        if (_usym && _usym->type && _usym->type->kind == TYPE_FUNCTION &&
                            _usym->type->fn_type.return_type) {
                            Type* _r = freshen_container_ret(expr, _usym->type->fn_type.return_type);
                            expr->expr_type = _r;
                            free(arg_types);
                            return _r;
                        }
                    }
                    expr->expr_type = builtin_int;  // Default return type
                    free(arg_types);
                    return builtin_int;
                }
                
                if (best_match && best_match->type->kind == TYPE_FUNCTION) {
                    // Mark the callee as used (critical for lambda variables stored
                    // in locals - without this, `var f = (n) => n+1; f(5)` warns
                    // "unused variable 'f'" because this path returns early before
                    // the fallback check_expr(callee) which would mark_used).
                    mark_used(scope, expr->call.callee->token);
                    // T1.5.4: Validate the function call with detailed parameter checking
                    // Skip validation for named arguments (codegen handles reordering)
                    ValidationResult validation = VALIDATION_SUCCESS;
                    if (!_has_named_args && !best_match->type->fn_type.is_variadic)
                        validation = wyn_validate_function_call(best_match, expr->call.args, expr->call.arg_count, scope);
                    
                    if (validation != VALIDATION_SUCCESS) {
                        char func_name[256];
                        token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                        int _err_line = expr->call.callee->token.line;
                        // Emit with the same source-location + caret path as every
                        // other diagnostic (was a bare "validation failed" line with
                        // no `--> file:line`, unlike the rest of the checker).
                        if (validation == VALIDATION_PARAM_COUNT_MISMATCH) {
                            int _expected = best_match->type->fn_type.param_count;
                            type_error_wrong_arg_count(func_name, _expected,
                                                       expr->call.arg_count, _err_line, 0);
                        } else if (validation == VALIDATION_TYPE_MISMATCH) {
                            // Find the first argument whose type doesn't match and
                            // point the caret at its line.
                            int _pc = best_match->type->fn_type.param_count;
                            for (int _ai = 0; _ai < expr->call.arg_count && _ai < _pc; _ai++) {
                                Type* _exp = best_match->type->fn_type.param_types[_ai];
                                Type* _act = expr->call.args[_ai]->expr_type;
                                if (!_act) _act = check_expr(expr->call.args[_ai], scope);
                                if (!wyn_is_type_compatible(_exp, _act)) {
                                    char _ctx[128];
                                    snprintf(_ctx, sizeof(_ctx), "argument %d of '%s'", _ai + 1, func_name);
                                    type_error_mismatch(type_to_string(_exp), type_to_string(_act),
                                                        _ctx, expr->call.args[_ai]->token.line, 0);
                                    break;
                                }
                            }
                        } else {
                            fprintf(stderr, "Error: Function call validation failed for '%s': %s\n",
                                    func_name, wyn_validation_error_message(validation));
                        }
                        had_error = true;
                    }
                    
                    // Store the selected overload for code generation - but ONLY
                    // for real (global) functions. A fn-typed local/param (S3:
                    // `f: fn(float) -> float`) also resolves here, and its Symbol
                    // lives in a stack-allocated scope that is gone by codegen -
                    // dereferencing it read a garbage mangled_name and crashed.
                    {
                        Symbol* _g = find_symbol(global_scope, expr->call.callee->token);
                        while (_g && _g != best_match) _g = _g->next_overload;
                        if (_g == best_match && _g != NULL) {
                            expr->call.selected_overload = (void*)best_match;
                        }
                    }
                    {
                        Type* _r = freshen_container_ret(expr, best_match->type->fn_type.return_type);
                        expr->expr_type = _r;
                        // S3: record the callee's own function type too - codegen
                        // uses it to pick the closure-call ABI (float vs int).
                        expr->call.callee->expr_type = best_match->type;
                        free(arg_types);
                        return _r;
                    }
                } else if (find_local_noncallable(scope, expr->call.callee->token)) {
                    // K7: the callee is a LOCAL variable holding a non-function
                    // value (e.g. `var x = 5; x(3)`). Without this gate the call
                    // silently fell through to the int fallback and then hit an
                    // opaque codegen ICE. Reject at check time. We deliberately
                    // scope this to LOCALS: the stdlib registers many callable
                    // builtins as global `int` placeholders (sleep_ms, await_any,
                    // clamp, sign, input) that codegen resolves by name, and a user
                    // function may shadow such a placeholder as a separate global
                    // symbol - neither is a K7 error.
                    char nc_name[256];
                    token_to_cstr(nc_name, sizeof(nc_name), expr->call.callee->token);
                    fprintf(stderr, "Error at line %d: '%s' is not a function and cannot be called\n",
                            expr->call.callee->token.line, nc_name);
                    had_error = true;
                    mark_used(scope, expr->call.callee->token);
                    free(arg_types);
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (!best_match) {
                    char func_name[256];
                    token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                    
                    // Check if function exists but with wrong arg count
                    Symbol* _existing = find_symbol(scope, expr->call.callee->token);
                    if (_existing && _existing->type && _existing->type->kind == TYPE_FUNCTION) {
                        // Function exists (possibly a lambda variable) - mark it used
                        mark_used(scope, expr->call.callee->token);
                    } else {
                    // Search scope for similar names (typo detection)
                    int min_dist = 999;
                    char closest[256] = {0};
                    SymbolTable* s = scope;
                    while (s) {
                        for (int si = 0; si < s->count; si++) {
                            char sn[256]; int sl = token_to_cstr(sn, sizeof(sn), s->symbols[si].name);
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
                }
                
                free(arg_types);
            }
            
            // Fallback to original logic for non-identifier callees with T1.5.4 validation
            Type* callee_type = check_expr(expr->call.callee, scope);
            
            // T1.5.4: Enhanced parameter validation for function types
            // Skip strict validation when named arguments are used (codegen handles reordering)
            if (!_has_named_args && callee_type && callee_type->kind == TYPE_FUNCTION) {
                // For variadic functions, allow any number of arguments >= param_count
                if (callee_type->fn_type.is_variadic) {
                    if (expr->call.arg_count < callee_type->fn_type.param_count) {
                        char func_name[256];
                        token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                        type_error_wrong_arg_count(func_name, callee_type->fn_type.param_count, 
                                                  expr->call.arg_count, expr->call.callee->token.line, 0);
                        had_error = true;
                    }
                } else {
                    int min_p = callee_type->fn_type.min_param_count;
                    if (min_p < 0) min_p = callee_type->fn_type.param_count;
                    if (expr->call.arg_count < min_p ||
                        expr->call.arg_count > callee_type->fn_type.param_count) {
                        char func_name[256];
                        token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                        type_error_wrong_arg_count(func_name, callee_type->fn_type.param_count, 
                                                  expr->call.arg_count, expr->call.callee->token.line, 0);
                        had_error = true;
                    }
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
                Type* ret = freshen_container_ret(expr, callee_type->fn_type.return_type);
                expr->expr_type = ret;
                return ret;
            }
            expr->expr_type = builtin_int;
            return builtin_int;
        }
        case EXPR_METHOD_CALL: {
            // Channel methods: ch.send(v) / ch.recv() / ch.close(). Channels
            // carry ONE element type, inferred from the first send (mirroring
            // array push inference; stored in the union's array_type slot).
            // Later sends must match - the runtime moves payloads through a
            // single word, so an unchecked string send used to come back as a
            // pointer number and floats silently truncated. recv() returns
            // the element type so codegen can marshal it back correctly.
            {
                Type* obj_t = check_expr(expr->method_call.object, scope);
                if (obj_t && obj_t->kind == TYPE_CHANNEL) {
                    Token method = expr->method_call.method;
                    Type* arg_t = NULL;
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        Type* t = check_expr(expr->method_call.args[i], scope);
                        if (i == 0) arg_t = t;
                    }
                    if (method.length == 4 && memcmp(method.start, "send", 4) == 0 && arg_t) {
                        Type* elem = obj_t->array_type.element_type;
                        if (!elem) {
                            obj_t->array_type.element_type = arg_t;
                        } else if (elem->kind != arg_t->kind &&
                                   !(elem->kind == TYPE_FLOAT && arg_t->kind == TYPE_INT)) {
                            fprintf(stderr, "Error at line %d: Cannot send %s through a channel of %s\n",
                                    method.line, type_to_string(arg_t), type_to_string(elem));
                            fprintf(stderr, "  \033[34mHelp:\033[0m A channel carries one type - its first send decides which\n");
                            had_error = true;
                            return NULL;
                        }
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    }
                    if ((method.length == 4 && memcmp(method.start, "recv", 4) == 0) ||
                        (method.length == 8 && memcmp(method.start, "try_recv", 8) == 0)) {
                        Type* elem = obj_t->array_type.element_type;
                        expr->expr_type = elem ? elem : builtin_int;
                        return expr->expr_type;
                    }
                    expr->expr_type = builtin_int; // close() etc.
                    return builtin_int;
                }
            }
            // Enum constructor with payload written as a method call, e.g.
            // `Shape.Circle(5.0)`. The parser produces EXPR_METHOD_CALL here
            // (object = enum name, method = variant). Type it as the enum so
            // downstream (var decls, match) sees TYPE_ENUM instead of int.
            if (expr->method_call.object->type == EXPR_IDENT) {
                EnumStmt* enum_def = find_enum_definition(expr->method_call.object->token);
                if (enum_def) {
                    Token variant = expr->method_call.method;
                    for (int vi = 0; vi < enum_def->variant_count; vi++) {
                        if (enum_def->variants[vi].length == variant.length &&
                            memcmp(enum_def->variants[vi].start, variant.start, variant.length) == 0) {
                            for (int ai = 0; ai < expr->method_call.arg_count; ai++) {
                                check_expr(expr->method_call.args[ai], scope);
                            }
                            Type* et = make_type(TYPE_ENUM);
                            et->name = expr->method_call.object->token;
                            et->enum_type.variants = enum_def->variants;
                            et->enum_type.variant_count = enum_def->variant_count;
                            expr->expr_type = et;
                            return et;
                        }
                    }
                }
            }

            Type* object_type = check_expr(expr->method_call.object, scope);
            // S3 pre-seed: a map/filter lambda's parameter IS the element type.
            // Seed it before body checking so field access on struct elements
            // ((p) => p.x) and bool/float bodies resolve with real types instead
            // of the int default that S2 patched after the fact.
            // The week-one key-fn methods (sort_by/max_by/min_by/group_by) take
            // the same element-typed single-param lambda, so they share the seed.
            if (object_type && object_type->kind == TYPE_ARRAY &&
                object_type->array_type.element_type &&
                expr->method_call.arg_count == 1 &&
                expr->method_call.args[0]->type == EXPR_LAMBDA &&
                expr->method_call.args[0]->lambda.param_count == 1 &&
                ((expr->method_call.method.length == 3 &&
                  memcmp(expr->method_call.method.start, "map", 3) == 0) ||
                 (expr->method_call.method.length == 6 &&
                  memcmp(expr->method_call.method.start, "filter", 6) == 0) ||
                 (expr->method_call.method.length == 7 &&
                  memcmp(expr->method_call.method.start, "sort_by", 7) == 0) ||
                 (expr->method_call.method.length == 6 &&
                  memcmp(expr->method_call.method.start, "max_by", 6) == 0) ||
                 (expr->method_call.method.length == 6 &&
                  memcmp(expr->method_call.method.start, "min_by", 6) == 0) ||
                 (expr->method_call.method.length == 8 &&
                  memcmp(expr->method_call.method.start, "group_by", 8) == 0))) {
                lambda_ctx_param_seed = object_type->array_type.element_type;
            }
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                check_expr(expr->method_call.args[i], scope);
            }
            lambda_ctx_param_seed = NULL;

            Token method = expr->method_call.method;
            char method_name[256]; token_to_cstr(method_name, sizeof(method_name), method);

            // `"fmt".format(a, b, ...)` - validate the format string at CHECK
            // time. `.format()` used to be a silent no-op: codegen called a
            // runtime that only understood `{}` and copied everything else
            // through, so `"Hello %s".format("World")` printed "Hello %s" at
            // exit 0 with the argument discarded (the worst failure mode).
            //
            // CHOSEN SEMANTICS: brace-style `{}`, matching Wyn's own `"${x}"`
            // interpolation (already brace-based) and the only form the runtime
            // ever implemented. printf-style specs are rejected with a clear
            // error rather than substituted, because codegen stringifies every
            // argument before the call - the runtime cannot honor `%d`/`%.2f`
            // against a value whose C type it no longer knows, and pretending
            // to would reintroduce silent wrong output. `{{`/`}}` escape braces.
            //
            // Only checkable when the receiver is a literal string; a computed
            // format string still runs, substituting `{}` left to right.
            if (strcmp(method_name, "format") == 0 && object_type &&
                object_type->kind == TYPE_STRING &&
                expr->method_call.object->type == EXPR_STRING) {
                Token ft = expr->method_call.object->token;
                bool ml = (ft.length >= 6 && ft.start[0] == '"' &&
                           ft.start[1] == '"' && ft.start[2] == '"');
                int lo = ml ? 3 : 1, hi = ft.length - (ml ? 3 : 1);
                int holes = 0; char bad_spec[8]; bad_spec[0] = 0;
                for (int i = lo; i < hi; i++) {
                    char c = ft.start[i];
                    if (c == '\\' && i + 1 < hi) { i++; continue; }
                    if (c == '{' && i + 1 < hi && ft.start[i+1] == '{') { i++; continue; }
                    if (c == '}' && i + 1 < hi && ft.start[i+1] == '}') { i++; continue; }
                    if (c == '{' && i + 1 < hi && ft.start[i+1] == '}') { holes++; i++; continue; }
                    // A printf conversion: '%' followed by flags/width/precision
                    // then a conversion letter. '%%' is a literal percent.
                    // NOTE: the space flag is deliberately NOT recognized, so
                    // prose like "{}% done" (a real percent sign followed by a
                    // word) is not mistaken for "% d".
                    if (c == '%' && i + 1 < hi) {
                        if (ft.start[i+1] == '%') { i++; continue; }
                        int j = i + 1, w = 0;
                        while (j < hi && (strchr("-+#0123456789.", ft.start[j]) != NULL)) j++;
                        if (j < hi && strchr("diouxXeEfgGcsp", ft.start[j]) != NULL) {
                            for (int k = i; k <= j && w < (int)sizeof(bad_spec) - 1; k++)
                                bad_spec[w++] = ft.start[k];
                            bad_spec[w] = 0;
                            break;
                        }
                    }
                }
                if (bad_spec[0]) {
                    fprintf(stderr, "Error at line %d: .format() uses {} placeholders, not printf specs like '%s'\n",
                            method.line, bad_spec);
                    fprintf(stderr, "  \033[34mHelp:\033[0m Write \"...{}...\".format(x), or interpolate directly: \"...${x}...\"\n");
                    had_error = true;
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
                if (holes != expr->method_call.arg_count) {
                    fprintf(stderr, "Error at line %d: .format() has %d {} placeholder%s but %d argument%s\n",
                            method.line, holes, holes == 1 ? "" : "s",
                            expr->method_call.arg_count,
                            expr->method_call.arg_count == 1 ? "" : "s");
                    fprintf(stderr, "  \033[34mHelp:\033[0m Every {} consumes one argument, left to right (use {{ for a literal brace)\n");
                    had_error = true;
                    expr->expr_type = builtin_string;
                    return builtin_string;
                }
            }

            // Infer an untyped map's value type from the first `.set(k, v)` /
            // `.insert(k, v)`, mirroring the index-assign path (`m[k] = v`).
            // Without this, a `HashMap::new()` map defaulted its value type to
            // string, so `m.set("a", 42)` then `m.get("a")` decoded 42 through
            // hashmap_get_string and returned garbage (FLOWY silent-wrong bug).
            // A conflicting later store (int then string) is rejected below by
            // the store-type check.
            if (object_type && object_type->kind == TYPE_MAP &&
                !object_type->map_type.value_type &&
                expr->method_call.arg_count == 2 &&
                ((strcmp(method_name, "set") == 0) || (strcmp(method_name, "insert") == 0))) {
                Type* vt = expr->method_call.args[1]->expr_type;
                if (!vt) vt = check_expr(expr->method_call.args[1], scope);
                if (vt) object_type->map_type.value_type = vt;
            }

            // S2 context-propagation: when .map()/.filter() is called on a
            // [string] or [float] array and the lambda arg has int-defaulted
            // params, override them to the element type. Handles identity
            // lambdas ((s) => s) and float bodies whose arithmetic gave no
            // string/float evidence at inference time.
            if (object_type && object_type->kind == TYPE_ARRAY &&
                object_type->array_type.element_type &&
                (object_type->array_type.element_type->kind == TYPE_STRING ||
                 object_type->array_type.element_type->kind == TYPE_FLOAT) &&
                expr->method_call.arg_count == 1 &&
                expr->method_call.args[0]->type == EXPR_LAMBDA &&
                ((method.length == 3 && memcmp(method.start, "map", 3) == 0) ||
                 (method.length == 6 && memcmp(method.start, "filter", 6) == 0))) {
                Type* elem_t = object_type->array_type.element_type;
                Expr* lam = expr->method_call.args[0];
                if (lam->expr_type && lam->expr_type->kind == TYPE_FUNCTION) {
                    for (int pi = 0; pi < lam->expr_type->fn_type.param_count; pi++) {
                        if (lam->expr_type->fn_type.param_types[pi] &&
                            lam->expr_type->fn_type.param_types[pi]->kind == TYPE_INT) {
                            lam->expr_type->fn_type.param_types[pi] = elem_t;
                        }
                    }
                    // map return type: an int-typed return usually just means
                    // "no evidence" - identity bodies and float arithmetic
                    // both produce the element type.
                    if (method.length == 3 && memcmp(method.start, "map", 3) == 0 &&
                        lam->expr_type->fn_type.return_type &&
                        lam->expr_type->fn_type.return_type->kind == TYPE_INT) {
                        if (elem_t->kind == TYPE_STRING &&
                            lam->lambda.body && lam->lambda.body->type == EXPR_IDENT) {
                            lam->expr_type->fn_type.return_type = builtin_string;
                        } else if (elem_t->kind == TYPE_FLOAT) {
                            // float-in, arithmetic body: result is float unless
                            // the body is an explicit int producer (.len() etc.)
                            if (!(lam->lambda.body && lam->lambda.body->type == EXPR_METHOD_CALL))
                                lam->expr_type->fn_type.return_type = builtin_float;
                        }
                    }
                }
            }

            // K10: .filter()'s predicate must return bool. An int predicate was
            // accepted (C truthiness by accident) and a string predicate silently
            // no-oped (returned the whole array). The return type is only checked
            // after the S2 propagation above (which never touches filter's bool
            // return). Only enforce when the lambda's return type is known and is
            // a concrete non-bool scalar - unknown/void stays permissive.
            if (object_type && object_type->kind == TYPE_ARRAY &&
                method.length == 6 && memcmp(method.start, "filter", 6) == 0 &&
                expr->method_call.arg_count == 1 &&
                expr->method_call.args[0]->type == EXPR_LAMBDA) {
                Expr* lam = expr->method_call.args[0];
                Type* rt = (lam->expr_type && lam->expr_type->kind == TYPE_FUNCTION)
                               ? lam->expr_type->fn_type.return_type : NULL;
                if (rt && (rt->kind == TYPE_STRING || rt->kind == TYPE_FLOAT)) {
                    fprintf(stderr, "Error at line %d: .filter() predicate must return bool, got %s\n",
                            method.line, type_to_string(rt));
                    had_error = true;
                }
            }

            // S2 context-propagation for .reduce(): the lambda takes TWO params
            // (accumulator and element) and returns the same type. On a [float],
            // the checker defaulted both to int → codegen emitted long long params/
            // return → ABI mismatch with wyn_array_reduce_float's `double(*)(double,
            // double)`. Also set the whole method-call's expr_type so STMT_VAR picks
            // `double` for the result variable (G2). Also applies to sum/min/max/
            // average which are reduce variants.
            if (object_type && object_type->kind == TYPE_ARRAY &&
                object_type->array_type.element_type &&
                object_type->array_type.element_type->kind == TYPE_FLOAT &&
                (method.length == 6 && memcmp(method.start, "reduce", 6) == 0)) {
                Type* elem_t = object_type->array_type.element_type;
                // Find the lambda argument (may be swapped by forgiveness)
                for (int ai = 0; ai < expr->method_call.arg_count; ai++) {
                    Expr* lam = expr->method_call.args[ai];
                    if (lam->type != EXPR_LAMBDA) continue;
                    if (lam->expr_type && lam->expr_type->kind == TYPE_FUNCTION) {
                        for (int pi = 0; pi < lam->expr_type->fn_type.param_count; pi++) {
                            if (lam->expr_type->fn_type.param_types[pi] &&
                                lam->expr_type->fn_type.param_types[pi]->kind == TYPE_INT) {
                                lam->expr_type->fn_type.param_types[pi] = elem_t;
                            }
                        }
                        if (lam->expr_type->fn_type.return_type &&
                            lam->expr_type->fn_type.return_type->kind == TYPE_INT) {
                            lam->expr_type->fn_type.return_type = elem_t;
                        }
                    }
                }
                // The method call itself returns the element type.
                expr->expr_type = elem_t;
            }

            // Check for namespace method calls: File.read() -> File_read
            // Only for known namespaces, not regular variables
            if (expr->method_call.object->type == EXPR_IDENT) {
                char obj_name[256];
                token_to_cstr(obj_name, sizeof(obj_name), expr->method_call.object->token);
                // W9: resolve a module alias (`import m as mm` → mm.foo() means
                // m.foo()) to the real module name before namespace lookup - but
                // NOT if a real local/param shadows the alias name.
                extern const char* resolve_parser_module_alias(const char* name);
                if (!find_symbol(scope, expr->method_call.object->token)) {
                    const char* aliased = resolve_parser_module_alias(obj_name);
                    if (aliased) { strncpy(obj_name, aliased, sizeof(obj_name)-1); obj_name[sizeof(obj_name)-1] = '\0'; }
                }
                // Only treat as namespace if it's a known builtin module
                extern bool is_builtin_module(const char* name);
                extern bool is_module_loaded(const char* name);
                // Check builtin modules and loaded user modules (including short names)
                bool is_known_module = is_builtin_module(obj_name) || is_module_loaded(obj_name);
                // Also check if it's a short name of a loaded module (e.g., "utils" for "lib/utils")
                if (!is_known_module) {
                    extern int get_module_count(void);
                    extern void* get_module_entry_at(int index);
                    int mc = get_module_count();
                    for (int mi = 0; mi < mc; mi++) {
                        typedef struct { char* name; void* ast; } ME;
                        ME* mod = (ME*)get_module_entry_at(mi);
                        // Check if obj_name matches the last segment of the module path
                        char* slash = strrchr(mod->name, '/');
                        const char* short_name = slash ? slash + 1 : mod->name;
                        if (strcmp(short_name, obj_name) == 0) { is_known_module = true; break; }
                    }
                }
                if (is_known_module) {
                    // Task.select has no bare form - only the arity-suffixed
                    // Task.select_2 / Task.select_3 exist (codegen lowers
                    // Task.<m> to Task_<m>, so `Task.select` becomes a call to
                    // the undeclared C fn Task_select and only died at C-compile
                    // with a leaked `undeclared function 'Task_select'`). Reject
                    // it here with a did-you-mean naming the real functions.
                    // Narrow by design: only this known arity-suffixed family,
                    // so no previously-valid namespace call is affected.
                    if (strcmp(obj_name, "Task") == 0 && strcmp(method_name, "select") == 0) {
                        fprintf(stderr, "Error at line %d: unknown function 'Task.select' - did you mean 'Task.select_2' or 'Task.select_3'?\n", method.line);
                        show_source_line(method.line);
                        fprintf(stderr, "  \033[34mHelp:\033[0m Task.select takes a fixed number of channels: use Task.select_2(a, b) for 2 or Task.select_3(a, b, c) for 3.\n");
                        had_error = true;
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    }
                    // pub enforcement: a dot call into a user module (`m.f()`)
                    // must target a `pub fn` (or `export fn`). Builtin
                    // namespaces and C packages are not in the module registry
                    // and stay permissive. An import registers the module name
                    // itself as an int-typed placeholder symbol, so a REAL
                    // variable shadowing the module name is recognized by its
                    // non-int object type and skips the namespace rule.
                    if ((!object_type || object_type->kind == TYPE_INT) &&
                        !checking_same_module(obj_name) &&
                        module_fn_visibility(obj_name, method_name) == VIS_PRIVATE) {
                        private_fn_error(method.line, method_name, obj_name);
                        expr->expr_type = builtin_int;
                        return builtin_int;
                    }
                    char ns_method[256];
                    snprintf(ns_method, sizeof(ns_method), "%s_%s", obj_name, method_name);
                    Token ns_tok = {TOKEN_IDENT, ns_method, (int)strlen(ns_method), 0};
                    Symbol* ns_sym = find_symbol(global_scope, ns_tok);
                    // Also try :: form (System::args)
                    if (!ns_sym) {
                        char ns_method2[256];
                        snprintf(ns_method2, sizeof(ns_method2), "%s::%s", obj_name, method_name);
                        Token ns_tok2 = {TOKEN_IDENT, ns_method2, (int)strlen(ns_method2), 0};
                        ns_sym = find_symbol(global_scope, ns_tok2);
                    }
                    if (ns_sym && ns_sym->type && ns_sym->type->kind == TYPE_FUNCTION) {
                        // ARITY, before the return type is adopted.
                        //
                        // This is the only place a dotted module call - `m.foo(...)`,
                        // which the parser gives us as a METHOD_CALL - meets its real
                        // signature. Without a check here the call was never
                        // validated at all: `m.needs_arg()` with the argument missing
                        // passed `wyn check` clean and then failed as a raw C compiler
                        // error ("too few arguments to function call") pointing at
                        // generated code the programmer never wrote. The `::` spelling
                        // of the identical call WAS checked (it resolves as an
                        // EXPR_CALL and reaches the T1.5.4 validation), so the two
                        // syntaxes disagreed - and the dot form is the one every
                        // program and every doc uses.
                        //
                        // Deliberately narrow, because this path also carries builtin
                        // namespaces (Time.now_millis, System.args) whose registered
                        // types are not all faithful:
                        //   - variadic and overloaded callees are skipped entirely;
                        //     an overload set resolves by argument type, so "no
                        //     overload takes this many" is a different diagnostic and
                        //     reporting the first overload's arity would mislead.
                        //   - min_param_count carries defaulted parameters, so a call
                        //     that omits an argument WITH a default stays legal.
                        // A call that is fine today therefore stays fine; only a call
                        // that could not have compiled is now reported here instead of
                        // by the C compiler.
                        if (!ns_sym->type->fn_type.is_variadic &&
                            ns_sym->next_overload == NULL) {
                            int _pc = ns_sym->type->fn_type.param_count;
                            int _min = ns_sym->type->fn_type.min_param_count;
                            if (_min < 0) _min = _pc;
                            if (expr->method_call.arg_count < _min ||
                                expr->method_call.arg_count > _pc) {
                                char _fq[256];
                                snprintf(_fq, sizeof(_fq), "%s.%s", obj_name, method_name);
                                type_error_wrong_arg_count(_fq, _pc,
                                                           expr->method_call.arg_count,
                                                           method.line, 0);
                                had_error = true;
                            }
                        }
                        Type* ret = ns_sym->type->fn_type.return_type;
                        if (ret) {
                            expr->expr_type = ret;
                            return ret;
                        }
                    }
                    // Fallback: check module function return type table
                    extern const char* lookup_module_fn_return_type(const char*);
                    const char* rt = lookup_module_fn_return_type(ns_method);
                    if (rt) {
                        if (strcmp(rt, "string") == 0) { expr->expr_type = builtin_string; return builtin_string; }
                        if (strcmp(rt, "bool") == 0) { expr->expr_type = builtin_bool; return builtin_bool; }
                        if (strcmp(rt, "float") == 0) { expr->expr_type = builtin_float; return builtin_float; }
                        if (strcmp(rt, "array") == 0) { expr->expr_type = builtin_array; return builtin_array; }
                        expr->expr_type = builtin_int; return builtin_int;
                    }
                    // Still nothing: ask the module's OWN AST. Neither of the two
                    // lookups above can answer for a module loaded from the package
                    // cache - the symbol may not be registered under either spelling,
                    // and lookup_module_fn_return_type is a hardcoded table of stdlib
                    // builtins (types.c), so a package's functions are absent from it.
                    //
                    // Falling through here defaulted to int, so a `pub fn ... -> string`
                    // in a git-fetched package produced
                    // "Cannot compare int with string" on a perfectly correct
                    // comparison - which is how this was found: WynCanvas importing
                    // the `gui` package as a real dependency rather than a symlink.
                    {
                        extern const char* get_module_fn_builtin_return(const char*, const char*);
                        const char* mr = get_module_fn_builtin_return(obj_name, method_name);
                        if (mr) {
                            if (strcmp(mr, "string") == 0) { expr->expr_type = builtin_string; return builtin_string; }
                            if (strcmp(mr, "bool") == 0)   { expr->expr_type = builtin_bool;   return builtin_bool; }
                            if (strcmp(mr, "float") == 0)  { expr->expr_type = builtin_float;  return builtin_float; }
                            if (strcmp(mr, "int") == 0)    { expr->expr_type = builtin_int;    return builtin_int; }
                            // A `-> [T]` return. Without this the array fell through to
                            // the int default, so `xs = m.many()` then `xs.len()` failed
                            // with "Unknown method 'len' for type 'int'" - and only in
                            // codegen, since `wyn check` reported no errors.
                            // "array" or "array:<element>" - see the element-type note in
                            // get_module_fn_builtin_return.
                            if (strncmp(mr, "array", 5) == 0) { expr->expr_type = builtin_array; return builtin_array; }
                        }
                    }
                }
            }
            
            // Handle string methods
            if (object_type && object_type->kind == TYPE_STRING) {
                if (strcmp(method_name, "contains") == 0) {
                    expr->expr_type = builtin_bool;
                    return builtin_bool;
                } else if (strcmp(method_name, "upper") == 0 || strcmp(method_name, "lower") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "len") == 0) {
                    // NOTE: "length" was accepted here as an alias but codegen only
                    // supports len - it compiled clean and then failed in C. One
                    // name, checked and generated the same way.
                    expr->expr_type = builtin_int;
                    return builtin_int;
                } else if (strcmp(method_name, "starts_with") == 0 || strcmp(method_name, "ends_with") == 0) {
                    expr->expr_type = builtin_bool;
                    return builtin_bool;
                } else if (strcmp(method_name, "is_empty") == 0) {
                    expr->expr_type = builtin_bool;
                    return builtin_bool;
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
                } else if (strcmp(method_name, "reverse") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "slice") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "char_at") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "pad_left") == 0 || strcmp(method_name, "pad_right") == 0) {
                    expr->expr_type = builtin_string;
                    return builtin_string;
                } else if (strcmp(method_name, "capitalize") == 0 || strcmp(method_name, "title") == 0) {
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
            
            // map.get(k) returns the map's value type - same source-of-truth
            // approach as index m[k] (see EXPR_INDEX). Without this the getter,
            // var-decl type, and comparisons all defaulted to string and
            // miscompiled (garbage for int/float/bool; strcmp on an int crashed).
            // Two-arg map.get(k, default) is Python's dict.get: the default is
            // returned when the key is missing, so it must match the value
            // type. Before this it compiled and returned garbage (the default
            // was silently DROPPED and a missing key decoded as the wrong
            // type) - FLOWY_DESIGN F1, the top silent-wrong bug.
            if (object_type && object_type->kind == TYPE_MAP &&
                ((method.length == 3 && memcmp(method.start, "get", 3) == 0) ||
                 (method.length == 6 && memcmp(method.start, "get_or", 6) == 0))) {
                if (expr->method_call.arg_count > 2) {
                    fprintf(stderr, "Error at line %d: map.get takes a key and an optional default, got %d arguments\n",
                            method.line, expr->method_call.arg_count);
                    had_error = true;
                    return NULL;
                }
                Type* vt = object_type->map_type.value_type;
                if (expr->method_call.arg_count == 2) {
                    Type* def_t = expr->method_call.args[1]->expr_type;
                    if (!def_t) def_t = check_expr(expr->method_call.args[1], scope);
                    if (vt && def_t && vt->kind != def_t->kind &&
                        !((vt->kind == TYPE_INT || vt->kind == TYPE_FLOAT) &&
                          (def_t->kind == TYPE_INT || def_t->kind == TYPE_FLOAT))) {
                        fprintf(stderr, "Error at line %d: map.get default is %s but the map's values are %s\n",
                                method.line, type_to_string(def_t), type_to_string(vt));
                        fprintf(stderr, "  \033[34mHelp:\033[0m The default is returned when the key is missing - it must match the value type\n");
                        had_error = true;
                        return NULL;
                    }
                    // Untyped map (HashMap.new()): the default tells us the
                    // value type - better than the historical string guess.
                    if (!vt && def_t) vt = def_t;
                }
                // Default to string when the value type is unknown (e.g.
                // HashMap.new() maps set no value_type): string is the historical
                // default and the non-crashing choice. Only a literal {k: v} whose
                // value type we inferred narrows it to int/float/bool.
                if (!vt) vt = builtin_string;
                expr->expr_type = vt;
                return vt;
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
                // Float-array reductions return float, not the table's int
                // default (codegen dispatches array_sum_float etc.). Without
                // this the result was typed int and downstream var decls /
                // prints truncated even after the runtime computed correctly.
                if (object_type->array_type.element_type &&
                    object_type->array_type.element_type->kind == TYPE_FLOAT &&
                    ((method.length == 3 && (memcmp(method.start, "sum", 3) == 0 ||
                                             memcmp(method.start, "min", 3) == 0 ||
                                             memcmp(method.start, "max", 3) == 0 ||
                                             memcmp(method.start, "pop", 3) == 0)) ||
                     (method.length == 5 && memcmp(method.start, "first", 5) == 0) ||
                     (method.length == 4 && memcmp(method.start, "last", 4) == 0))) {
                    expr->expr_type = builtin_float;
                    return builtin_float;
                }
                // array.push(val) - update element type from pushed value
                if (method.length == 4 && memcmp(method.start, "push", 4) == 0 && expr->method_call.arg_count >= 1) {
                    Type* val_type = check_expr(expr->method_call.args[0], scope);
                    // Gate (not yet implemented): array of closures/functions.
                    // A function-typed element is stored as `long long` and the
                    // later `arr[i](x)` call leaks raw C. Reject cleanly until
                    // arrays of closures are implemented.
                    if ((val_type && val_type->kind == TYPE_FUNCTION) ||
                        (expr->method_call.args[0] && expr->method_call.args[0]->type == EXPR_LAMBDA)) {
                        fprintf(stderr, "Error at line %d: arrays of functions/closures are not yet supported\n",
                                method.line);
                        fprintf(stderr, "  \033[34mHelp:\033[0m call the closure directly, or dispatch via a match/enum instead\n");
                        had_error = true;
                        return NULL;
                    }
                    if (val_type && !object_type->array_type.element_type) {
                        object_type->array_type.element_type = val_type;
                        // Also update the symbol's type
                        if (expr->method_call.object->type == EXPR_IDENT) {
                            Symbol* sym = find_symbol(scope, expr->method_call.object->token);
                            if (sym && sym->type && sym->type->kind == TYPE_ARRAY) {
                                sym->type->array_type.element_type = val_type;
                            }
                        }
                    } else if (val_type && object_type->array_type.element_type &&
                               object_type->array_type.element_type->kind != val_type->kind &&
                               // int<->float pushes are coerced at codegen; a
                               // string/struct into a numeric array is not.
                               !((val_type->kind == TYPE_INT || val_type->kind == TYPE_FLOAT) &&
                                 (object_type->array_type.element_type->kind == TYPE_INT ||
                                  object_type->array_type.element_type->kind == TYPE_FLOAT))) {
                        // The function-form array_push(arr, v) has had this
                        // check for ages; the METHOD form silently accepted a
                        // mismatched push and corrupted memory at runtime
                        // (string pointer stored in an int array).
                        fprintf(stderr, "Error at line %d: Cannot push %s into array of %s\n",
                                method.line,
                                type_to_string(val_type),
                                type_to_string(object_type->array_type.element_type));
                        fprintf(stderr, "  \033[34mHelp:\033[0m Use separate arrays for different types\n");
                        had_error = true;
                        return NULL;
                    }
                    expr->expr_type = builtin_void;
                    return builtin_void;
                }
                // In-place mutators that also YIELD the array so they chain and
                // work in expression position: `xs.sort()`, `xs.reverse()`,
                // `xs.sort().reverse()`, `print(xs.sort())`. Typing them as the
                // receiver array (not int/void) is what lets print route to
                // print_array instead of to_string(long long). (2026-07)
                if ((method.length == 4 && memcmp(method.start, "sort", 4) == 0) ||
                    (method.length == 7 && memcmp(method.start, "reverse", 7) == 0)) {
                    expr->expr_type = object_type;
                    return object_type;
                }
                // Week-one stdlib batch (P5): key-fn methods.
                // xs.sort_by(key) yields the receiver array type (sorted copy);
                // xs.sorted() is the non-mutating sibling of .sort().
                if (((method.length == 7 && memcmp(method.start, "sort_by", 7) == 0) &&
                     expr->method_call.arg_count == 1) ||
                    ((method.length == 6 && memcmp(method.start, "sorted", 6) == 0) &&
                     expr->method_call.arg_count == 0)) {
                    expr->expr_type = object_type;
                    return object_type;
                }
                // xs.max_by(f) / xs.min_by(f) yield an ELEMENT (Kotlin maxBy).
                if ((method.length == 6 &&
                     (memcmp(method.start, "max_by", 6) == 0 ||
                      memcmp(method.start, "min_by", 6) == 0)) &&
                    expr->method_call.arg_count == 1) {
                    Type* elem = object_type->array_type.element_type;
                    if (!elem) elem = builtin_int;
                    expr->expr_type = elem;
                    return elem;
                }
                // xs.flatten(): [[T]] -> [T] (one level).
                if ((method.length == 7 && memcmp(method.start, "flatten", 7) == 0) &&
                    expr->method_call.arg_count == 0) {
                    Type* elem = object_type->array_type.element_type;
                    Type* flat = make_type(TYPE_ARRAY);
                    flat->array_type.element_type =
                        (elem && elem->kind == TYPE_ARRAY) ? elem->array_type.element_type : elem;
                    expr->expr_type = flat;
                    return flat;
                }
                // xs.group_by(f) yields a map from the key-fn's return type to
                // arrays of elements (Kotlin groupBy). Keys must be strings for
                // now (the hashmap is string-keyed); the codegen stringifies
                // int keys, so the checker accepts string- or int-keyed fns.
                if ((method.length == 8 && memcmp(method.start, "group_by", 8) == 0) &&
                    expr->method_call.arg_count == 1) {
                    Type* val_arr = make_type(TYPE_ARRAY);
                    val_arr->array_type.element_type = object_type->array_type.element_type;
                    Type* map_t = make_type(TYPE_MAP);
                    map_t->map_type.key_type = builtin_string;
                    map_t->map_type.value_type = val_arr;
                    expr->expr_type = map_t;
                    return map_t;
                }
            }

            // K4: `.unwrap_or(default)` on an Option/Result - the default is the
            // fallback VALUE, so it must match the wrapped type. Before this the
            // method-call path never checked the arg, so `int?.unwrap_or("x")`
            // emitted `OptionInt_unwrap_or(g(), "x")` - a string pointer passed
            // where a `long long` is expected. It compiled; on the None/Err path
            // the pointer was printed as an integer (silent garbage).
            if (object_type && expr->method_call.arg_count == 1 &&
                method.length == 9 && memcmp(method.start, "unwrap_or", 9) == 0) {
                // Wrapped value type, from either the structured Option/Result
                // type or the monomorphic struct family name (OptionInt/…).
                Type* wrapped = NULL;
                if (object_type->kind == TYPE_OPTIONAL) wrapped = object_type->optional_type.inner_type;
                else if (object_type->kind == TYPE_RESULT) wrapped = object_type->result_type.ok_type;
                else if (object_type->kind == TYPE_STRUCT && object_type->struct_type.name.length > 0) {
                    Token n = object_type->struct_type.name;
                    const char* s = n.start; int len = n.length;
                    #define _SUF(fam, lit) (len == (int)(strlen(fam)+strlen(lit)) && \
                        memcmp(s, fam, strlen(fam)) == 0 && \
                        memcmp(s + strlen(fam), lit, strlen(lit)) == 0)
                    if (_SUF("Option","Int") || _SUF("Result","Int")) wrapped = builtin_int;
                    else if (_SUF("Option","String") || _SUF("Result","String")) wrapped = builtin_string;
                    else if (_SUF("Option","Float") || _SUF("Result","Float")) wrapped = builtin_float;
                    else if (_SUF("Option","Bool") || _SUF("Result","Bool")) wrapped = builtin_bool;
                    #undef _SUF
                }
                if (wrapped) {
                    Type* def_t = expr->method_call.args[0]->expr_type;
                    if (!def_t) def_t = check_expr(expr->method_call.args[0], scope);
                    // Only enforce for SCALAR mismatches (both sides scalar) - the
                    // same conservative rule K2 uses for struct-init; int<->float
                    // is coerced by wyn_is_type_compatible. Struct/array/unknown
                    // wrapped types stay permissive.
                    bool wrapped_scalar = (wrapped->kind == TYPE_INT || wrapped->kind == TYPE_FLOAT ||
                                           wrapped->kind == TYPE_BOOL || wrapped->kind == TYPE_STRING);
                    bool def_scalar = def_t && (def_t->kind == TYPE_INT || def_t->kind == TYPE_FLOAT ||
                                                def_t->kind == TYPE_BOOL || def_t->kind == TYPE_STRING);
                    if (wrapped_scalar && def_scalar && !wyn_is_type_compatible(wrapped, def_t)) {
                        fprintf(stderr, "Error at line %d: unwrap_or default is %s but the value is %s\n",
                                method.line, type_to_string(def_t), type_to_string(wrapped));
                        fprintf(stderr, "  \033[34mHelp:\033[0m The default is returned when empty - it must match the wrapped type\n");
                        had_error = true;
                        expr->expr_type = wrapped;
                        return wrapped;
                    }
                }
            }

            // Use method signature table for type inference (Phase 1)
            const char* receiver_type = get_receiver_type_string(object_type);
            if (receiver_type) {
                token_to_cstr(method_name, sizeof(method_name), method);

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
                        // Check if this is a method that returns string array
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
                        // HashMap.keys() and .values() return [string]
                        if (object_type && object_type->kind == TYPE_MAP) {
                            if (strcmp(method_name, "keys") == 0 ||
                                strcmp(method_name, "values") == 0) {
                                Type* string_array = make_type(TYPE_ARRAY);
                                string_array->array_type.element_type = builtin_string;
                                expr->expr_type = string_array;
                                return string_array;
                            }
                        }
                        // S2/S3: .map()/.filter() on a typed array - the result's
                        // element type follows the LAMBDA's return type for map
                        // (a str->int lambda produces [int]; typing it [string]
                        // made downstream indexing read ints as char* and
                        // crash); filter always preserves the element type.
                        if (object_type && object_type->kind == TYPE_ARRAY &&
                            object_type->array_type.element_type &&
                            (strcmp(method_name, "map") == 0 || strcmp(method_name, "filter") == 0)) {
                            Type* elem = object_type->array_type.element_type;
                            if (strcmp(method_name, "map") == 0 &&
                                expr->method_call.arg_count == 1 &&
                                expr->method_call.args[0]->expr_type &&
                                expr->method_call.args[0]->expr_type->kind == TYPE_FUNCTION &&
                                expr->method_call.args[0]->expr_type->fn_type.return_type &&
                                expr->method_call.args[0]->expr_type->fn_type.return_type->kind != elem->kind) {
                                elem = expr->method_call.args[0]->expr_type->fn_type.return_type;
                            }
                            Type* res_arr = make_type(TYPE_ARRAY);
                            res_arr->array_type.element_type = elem;
                            expr->expr_type = res_arr;
                            return res_arr;
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
            
            // Look up user-defined / struct-body methods: Type_method in the
            // symbol table (both `impl` extension methods and methods declared in
            // the struct body register under this name).
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

                // Struct-BODY methods (`struct S { fn m(self) -> T {...} }`) are
                // not in the symbol table, so resolve their declared return type
                // straight from the struct definition. Without this, a method
                // call on any non-variable receiver (struct literal `B{v:0}
                // .add(5)`, nested call result, ...) fell through and typed as
                // int, breaking the documented builder idiom - variable-rooted
                // chains only worked via codegen-side registries.
                {
                    StructStmt* sdef = find_struct_definition(type_name);
                    if (sdef) {
                        for (int mi = 0; mi < sdef->method_count; mi++) {
                            FnStmt* m = sdef->methods[mi];
                            if (m->name.length != method.length ||
                                memcmp(m->name.start, method.start, method.length) != 0)
                                continue;
                            Expr* rt = m->return_type;
                            if (rt && rt->type == EXPR_IDENT) {
                                Token tn = rt->token;
                                if (tn.length == 3 && memcmp(tn.start, "int", 3) == 0) {
                                    expr->expr_type = builtin_int; return builtin_int;
                                }
                                if (tn.length == 6 && memcmp(tn.start, "string", 6) == 0) {
                                    expr->expr_type = builtin_string; return builtin_string;
                                }
                                if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) {
                                    expr->expr_type = builtin_float; return builtin_float;
                                }
                                if (tn.length == 4 && memcmp(tn.start, "bool", 4) == 0) {
                                    expr->expr_type = builtin_bool; return builtin_bool;
                                }
                                Symbol* ts = find_symbol(global_scope, tn);
                                if (ts && ts->type &&
                                    (ts->type->kind == TYPE_STRUCT || ts->type->kind == TYPE_ENUM)) {
                                    expr->expr_type = ts->type; return ts->type;
                                }
                            }
                            // Declared method with no/unresolvable return type:
                            // keep the historical int fallback.
                            expr->expr_type = builtin_int;
                            return builtin_int;
                        }
                    }
                }

                // The method didn't resolve to any known method on this struct.
                // If the receiver is a USER-DEFINED struct (has a real definition
                // in this program), that's an error the checker should catch -
                // otherwise it slips through to codegen and becomes a confusing
                // "Unknown method (no type info)" C-compile failure. Restricted to
                // real struct definitions so built-in/monomorphic struct families
                // (OptionInt, ResultString, void*, …) keep their lenient behavior.
                char sname[128]; token_to_cstr(sname, sizeof(sname), type_name);
                if (find_struct_definition(type_name) &&
                    !struct_has_method(global_scope, type_name, method) &&
                    !is_field_of_struct(type_name, method)) {
                    fprintf(stderr, "\nError at line %d: struct '%s' has no method '%.*s'\n",
                            method.line, sname, (int)method.length, method.start);
                    show_source_line(method.line);
                    fprintf(stderr, "  \033[34mHelp:\033[0m define it with `fn %.*s(self) { ... }` inside `struct %s { ... }` (or an `impl %s` block).\n",
                            (int)method.length, method.start, sname, sname);
                    had_error = true;
                    expr->expr_type = builtin_int;
                    return builtin_int;
                }
            }

            // Unknown method on a STRING or ARRAY receiver: reject at check time.
            // Everything the language supports on these two receivers returned
            // earlier (the explicit chain above or the method_signatures table);
            // reaching here means codegen would fail with a bare
            // "Unknown method '...'" C-compile error or emit garbage. Restricted
            // to string/array so structs, maps, options, FFI handles and other
            // receivers keep their existing (lenient) paths.
            if (object_type &&
                (object_type->kind == TYPE_STRING || object_type->kind == TYPE_ARRAY)) {
                const char* recv = object_type->kind == TYPE_STRING ? "string" : "array";
                extern const char* suggest_method_name(const char*, const char*);
                const char* near = suggest_method_name(recv, method_name);
                fprintf(stderr, "\nError at line %d: %s has no method '%s'\n",
                        method.line, recv, method_name);
                show_source_line(method.line);
                if (near)
                    fprintf(stderr, "  \033[33mDid you mean:\033[0m .%s()?\n", near);
                had_error = true;
                expr->expr_type = builtin_int;
                return builtin_int;
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
        case EXPR_LIST_COMP: {
            // List comprehension: [body for x in iter (if cond)].
            // Result is an array whose element type is the type of `body`,
            // evaluated with the loop variable bound in a child scope.
            SymbolTable comp_scope = {0};
            comp_scope.parent = scope;

            // Determine the loop variable's type from the iterable.
            Type* loop_var_type = builtin_int; // range yields ints
            if (expr->list_comp.iter_start) {
                Type* iter_type = check_expr(expr->list_comp.iter_start, &comp_scope);
                if (expr->list_comp.iter_end) {
                    // Range `start..end` / `start..=end` - element is int.
                    check_expr(expr->list_comp.iter_end, &comp_scope);
                    loop_var_type = builtin_int;
                } else if (iter_type && iter_type->kind == TYPE_ARRAY &&
                           iter_type->array_type.element_type) {
                    // Iterating an array - element type is the array's element.
                    loop_var_type = iter_type->array_type.element_type;
                }
            }
            add_symbol(&comp_scope, expr->list_comp.var_name, loop_var_type, false);

            if (expr->list_comp.condition) {
                check_expr(expr->list_comp.condition, &comp_scope);
            }

            Type* element_type = expr->list_comp.body
                ? check_expr(expr->list_comp.body, &comp_scope)
                : builtin_int;

            Type* array_type = make_type(TYPE_ARRAY);
            array_type->array_type.element_type = element_type ? element_type : builtin_int;
            expr->expr_type = array_type;
            return array_type;
        }
        case EXPR_HASHMAP_LITERAL: {
            // v1.3.0: {} creates a hashmap. Infer the value type from the first
            // value so index m[k] carries the right element type (int/string/
            // float/bool) - a literal has homogeneous value types. Without this
            // every map value typed as int and non-int maps miscompiled.
            Type* map_type = make_type(TYPE_MAP);
            map_type->map_type.key_type = builtin_string;
            // elements are [key0, value0, key1, value1, ...]
            if (expr->array.count >= 2) {
                Type* vt = check_expr(expr->array.elements[1], scope);
                map_type->map_type.value_type = vt ? vt : builtin_int;
                // still type-check the remaining entries
                for (int _i = 0; _i < expr->array.count; _i++)
                    if (_i != 1) check_expr(expr->array.elements[_i], scope);
            } else {
                // Empty `{}`: leave the value type OPEN so the first `m[k] = v`
                // store fixes it (see EXPR_INDEX_ASSIGN). Defaulting to int here
                // rejected struct/array values a later store would supply.
                map_type->map_type.value_type = NULL;
            }
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
                    had_error = true;
                    return NULL;
                }
                // Return the map's value type so downstream codegen (getter
                // selection, var-decl type, comparisons, print) all agree.
                // Default to string when unknown (HashMap.new() maps) - the
                // historical, non-crashing default; literals narrow it.
                Type* vt = array_type->map_type.value_type;
                if (!vt) vt = builtin_string;
                expr->expr_type = vt;
                return vt;
            } else {
                // Array indexing - require int indices
                if (idx_type && idx_type->kind != TYPE_INT) {
                    fprintf(stderr, "Error: Array index must be int\n");
                    had_error = true;
                    return NULL;
                }

                // AUTHORITATIVE: if the array has a known element type, that's the
                // result type. This must come BEFORE the name/source heuristics
                // below - otherwise an int/float array named args/files/names/parts/
                // entries was force-typed `string` purely by its name, miscompiling
                // `var x = parts[1]` (read through array_get_str → crash/garbage).
                if (array_type && array_type->kind == TYPE_ARRAY &&
                    array_type->array_type.element_type) {
                    expr->expr_type = array_type->array_type.element_type;
                    return array_type->array_type.element_type;
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
            if (!sym->is_mutable) {
                fprintf(stderr, "\033[31m\033[1mError:\033[0m Cannot assign to constant '%.*s'\n",
                        expr->assign.name.length, expr->assign.name.start);
                fprintf(stderr, "  \033[34mHelp:\033[0m Use 'var' instead of 'const' if you need to reassign\n");
                had_error = true;
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
                if (sym_inner->kind != val_inner->kind &&
                    sym_inner->kind != TYPE_STRUCT) {
                    fprintf(stderr, "\033[31m\033[1mError:\033[0m Type mismatch in assignment to '%.*s' (line %d)\n",
                            expr->assign.name.length, expr->assign.name.start, expr->assign.name.line);
                    fprintf(stderr, "  \033[1mExpected:\033[0m %s\n", type_to_string(sym_inner));
                    fprintf(stderr, "  \033[1mGot:\033[0m      %s\n", type_to_string(val_inner));
                    had_error = true;
                    return NULL;
                }
            }
            
            return sym->type;
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
            // Walk interpolation expressions to mark variables as used
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.expressions[i]) {
                    check_expr(expr->string_interp.expressions[i], scope);
                }
            }
            return builtin_string;
        case EXPR_AWAIT:
            if (expr->await.expr) {
                Type* inner = check_expr(expr->await.expr, scope);
                // Propagate the inner type for codegen
                if (expr->await.expr && expr->await.expr->expr_type) {
                    expr->expr_type = expr->await.expr->expr_type;
                    return expr->await.expr->expr_type;
                }
                if (inner) { expr->expr_type = inner; return inner; }
            }
            return builtin_int;
        case EXPR_SPAWN:
            if (expr->spawn.call) {
                Type* call_type = check_expr(expr->spawn.call, scope);
                // The spawn returns a future wrapping the call's return type
                // Store the inner type for await to use
                if (call_type) { expr->expr_type = call_type; return call_type; }
            }
            return builtin_int;
        case EXPR_CHANNEL: {
            if (expr->channel.capacity) check_expr(expr->channel.capacity, scope);
            // A statically-known capacity < 1 can never work: the runtime
            // rejects it (`size < capacity` is never true, so every send would
            // block forever) and panics. A constant that is known-bad at
            // compile time must not wait until runtime to say so - report it
            // here, where the user still has the source in front of them.
            // Bare `channel()` lowers to Task_channel(0), so it is the same bug
            // with the capacity left implicit.
            long long cap = 0;
            int cap_known = expr->channel.capacity
                                ? channel_const_capacity(expr->channel.capacity, &cap)
                                : 1;  // no argument == capacity 0
            if (cap_known && cap < 1) {
                fprintf(stderr,
                        "Error at line %d: channel capacity must be >= 1 (got %lld) - "
                        "unbuffered channels are not currently supported\n",
                        expr->token.line, cap);
                show_source_line(expr->token.line);
                fprintf(stderr, "  \033[34mHelp:\033[0m use \033[1mchannel(1)\033[0m for the "
                                "smallest buffered channel. A capacity-0 (rendezvous) channel "
                                "would block every send forever.\n");
                had_error = true;
            }
            Type* ch = make_type(TYPE_CHANNEL);
            expr->expr_type = ch;
            return ch;
        }
        case EXPR_RANGE:
            return builtin_int; // Range type
        case EXPR_LAMBDA: {
            // TASK-040: Lambda expression type checking and capture analysis
            
            // Create new scope for lambda parameters
            SymbolTable lambda_scope = {0};
            lambda_scope.parent = scope;
            
            // S1 lambda rework: infer each param's type from body usage (string if it
            // is concatenated with a string or is a string-method receiver), else int.
            // This lets string-parameter lambdas type-check instead of failing with a
            // bare "Type mismatch".
            // S3: an explicit annotation ((x: float) => ..., |p: Point| ...) wins
            // over inference and supports float/bool/struct params directly.
            // A map/filter context seed (receiver's element type) beats the int
            // default for unannotated params - consumed once so nested lambdas
            // inside this body do not inherit it.
            Type* ctx_seed = lambda_ctx_param_seed;
            lambda_ctx_param_seed = NULL;
            Type* param_inferred[16];
            for (int i = 0; i < expr->lambda.param_count && i < 16; i++) {
                Expr* ann = expr->lambda.param_types ? expr->lambda.param_types[i] : NULL;
                if (ann && ann->type == EXPR_IDENT) {
                    Token tn = ann->token;
                    if (tn.length == 3 && memcmp(tn.start, "int", 3) == 0) {
                        param_inferred[i] = builtin_int;
                    } else if ((tn.length == 6 && memcmp(tn.start, "string", 6) == 0) ||
                               (tn.length == 3 && memcmp(tn.start, "str", 3) == 0)) {
                        param_inferred[i] = builtin_string;
                    } else if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) {
                        param_inferred[i] = builtin_float;
                    } else if (tn.length == 4 && memcmp(tn.start, "bool", 4) == 0) {
                        param_inferred[i] = builtin_bool;
                    } else {
                        // User struct/enum annotation: resolve via the global scope,
                        // falling back to a struct type keyed by the name.
                        Symbol* ts = find_symbol(global_scope, tn);
                        if (ts && ts->type) {
                            param_inferred[i] = ts->type;
                        } else if (find_struct_definition(tn)) {
                            Type* st = make_type(TYPE_STRUCT);
                            st->struct_type.name = tn;
                            param_inferred[i] = st;
                        } else {
                            param_inferred[i] = builtin_int;
                        }
                    }
                    continue;
                }
                char pn[64]; int pl = expr->lambda.params[i].length < 63 ? expr->lambda.params[i].length : 63;
                memcpy(pn, expr->lambda.params[i].start, pl); pn[pl] = '\0';
                if (expr->lambda.body && lambda_param_is_string(pn, expr->lambda.body, scope)) {
                    param_inferred[i] = builtin_string;
                } else if (ctx_seed) {
                    param_inferred[i] = ctx_seed;
                } else if (expr->lambda.body && lambda_param_is_float(pn, expr->lambda.body, scope)) {
                    param_inferred[i] = builtin_float;   // (x) => x * 2.5
                } else if (expr->lambda.body && lambda_param_is_bool(pn, expr->lambda.body, scope)) {
                    param_inferred[i] = builtin_bool;    // (b) => not b
                } else {
                    param_inferred[i] = builtin_int;
                }
            }
            // Add parameters to lambda scope with their inferred types
            for (int i = 0; i < expr->lambda.param_count; i++) {
                Type* pt = (i < 16) ? param_inferred[i] : builtin_int;
                add_symbol(&lambda_scope, expr->lambda.params[i], pt, false);
            }
            
            // Check lambda body statements (multiline lambda)
            for (int i = 0; i < expr->lambda.body_stmt_count; i++) {
                Stmt* s = expr->lambda.body_stmts[i];
                if (s && s->type == STMT_VAR) {
                    Type* init_type = s->var.init ? check_expr(s->var.init, &lambda_scope) : builtin_int;
                    add_symbol(&lambda_scope, s->var.name, init_type ? init_type : builtin_int, false);
                } else if (s && s->type == STMT_EXPR) {
                    check_expr(s->expr, &lambda_scope);
                }
            }
            
            // Check lambda body in the new scope
            Type* body_type = builtin_void;
            if (expr->lambda.body) {
                body_type = check_expr(expr->lambda.body, &lambda_scope);
                if (!body_type) {
                    had_error = true;
                    return NULL;
                }
            }
            
            // Perform capture analysis - find free variables in lambda body
            if (expr->lambda.body) {
                analyze_lambda_captures(&expr->lambda, expr->lambda.body, scope);
            }
            // Also analyze captures in body statements
            for (int ci = 0; ci < expr->lambda.body_stmt_count; ci++) {
                Stmt* cs = expr->lambda.body_stmts[ci];
                if (cs && cs->type == STMT_EXPR && cs->expr) {
                    analyze_lambda_captures(&expr->lambda, cs->expr, scope);
                }
            }
            
            // Create function type for lambda
            Type* lambda_type = make_type(TYPE_FUNCTION);
            lambda_type->fn_type.param_count = expr->lambda.param_count;
            lambda_type->fn_type.param_types = malloc(sizeof(Type*) * expr->lambda.param_count);
            
            // Use the inferred param types (S1).
            for (int i = 0; i < expr->lambda.param_count; i++) {
                lambda_type->fn_type.param_types[i] = (i < 16) ? param_inferred[i] : builtin_int;
            }

            lambda_type->fn_type.return_type = body_type;
            expr->expr_type = lambda_type;

            return lambda_type;
        }
        case EXPR_MAP: {
            // Create a proper map type
            Type* map_type = make_type(TYPE_MAP);
            map_type->map_type.key_type = builtin_string;   // For now, assume string keys

            // Check all keys and values
            for (int i = 0; i < expr->map.count; i++) {
                check_expr(expr->map.keys[i], scope);
                check_expr(expr->map.values[i], scope);
            }
            // Infer value type from the first literal value; leave it NULL for an
            // empty `{}` so the first `m[k] = v` store fixes the value type (see
            // EXPR_INDEX_ASSIGN). Defaulting empty maps to int rejected struct/
            // array values that a later store would supply.
            if (expr->map.count > 0) {
                Type* vt = check_expr(expr->map.values[0], scope);
                map_type->map_type.value_type = vt ? vt : builtin_int;
            } else {
                map_type->map_type.value_type = NULL;
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
        case EXPR_OPT_CHAIN: {
            // `opt?.field`: opt is an Option<Struct>. Result is Option<FieldType>
            // (None if opt is None, else Some(opt.value.field)).
            Type* obj_type = check_expr(expr->opt_chain.object, scope);
            if (!obj_type || obj_type->kind != TYPE_STRUCT ||
                obj_type->struct_type.name.length <= 6 ||
                memcmp(obj_type->struct_type.name.start, "Option", 6) != 0) {
                fprintf(stderr, "Error at line %d: '?.' requires an optional value on the left\n",
                        expr->opt_chain.field.line);
                had_error = true;
                return builtin_int;
            }
            // Resolve the payload struct type (family "OptionUser" -> struct "User").
            Token pl = obj_type->struct_type.name;
            Token payload_name = {TOKEN_IDENT, pl.start + 6, pl.length - 6, 0};
            StructStmt* struct_def = find_struct_definition(payload_name);
            if (!struct_def) {
                fprintf(stderr, "Error at line %d: '?.' left operand is not an optional struct\n",
                        expr->opt_chain.field.line);
                had_error = true;
                return builtin_int;
            }
            Type* field_type = get_struct_field_type(struct_def, expr->opt_chain.field);
            if (!field_type) {
                fprintf(stderr, "Error at line %d: field '%.*s' not found on '%.*s'\n",
                        expr->opt_chain.field.line, expr->opt_chain.field.length,
                        expr->opt_chain.field.start, payload_name.length, payload_name.start);
                had_error = true;
                return builtin_int;
            }
            // Result is Option<field_type>: reuse the family machinery. A data-carrying enum
            // field is a C struct, so it needs the monomorphic family too.
            Type* result;
            if (field_type->kind == TYPE_STRUCT ||
                (field_type->kind == TYPE_ENUM && enum_name_is_data_enum(field_type->name))) {
                result = register_option_struct_family(field_type);
            } else {
                const char* fam = "OptionInt";
                if (field_type->kind == TYPE_STRING) fam = "OptionString";
                else if (field_type->kind == TYPE_FLOAT) fam = "OptionFloat";
                else if (field_type->kind == TYPE_BOOL) fam = "OptionBool";
                Token ft = {TOKEN_IDENT, (char*)fam, (int)strlen(fam), 0};
                Symbol* sym = find_symbol(global_scope, ft);
                result = sym ? sym->type : make_type(TYPE_OPTIONAL);
            }
            expr->expr_type = result;
            return result;
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
                    // For data enums, return the enum type (it's a constructor)
                    if (object_type && object_type->kind == TYPE_ENUM) {
                        expr->expr_type = object_type;
                        return object_type;
                    }
                    return builtin_int; // Simple enum values are integers
                }
            }
            
            // Check if this is struct field access (struct_var.field)
            if (object_type && object_type->kind == TYPE_STRUCT) {
                Token struct_name = object_type->struct_type.name;
                Token field_name = expr->field_access.field;

                // Monomorphic generic-struct instance (e.g. Box_string): its name
                // is synthetic and not in the source, so find_struct_definition
                // fails. The struct Type carries resolved field types - consult
                // them directly so `b.val` gets the concrete field type.
                if (object_type->struct_type.field_count > 0 &&
                    object_type->struct_type.field_types &&
                    object_type->struct_type.field_names &&
                    !find_struct_definition(struct_name)) {
                    for (int fi = 0; fi < object_type->struct_type.field_count; fi++) {
                        Token fn = object_type->struct_type.field_names[fi];
                        if (fn.length == field_name.length &&
                            memcmp(fn.start, field_name.start, field_name.length) == 0) {
                            expr->expr_type = object_type->struct_type.field_types[fi];
                            return object_type->struct_type.field_types[fi];
                        }
                    }
                }

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
                    // Genuinely-absent field on a KNOWN user struct. A typo like
                    // `u.namee` used to pass check and then leak a raw C error
                    // ("no member named 'namee'") at build. Mirror the method
                    // validation path and reject at check. Guard: only fire when
                    // the name is neither a field nor a method (bare `u.method`
                    // references stay valid), so method calls and builtin
                    // receivers are untouched.
                    if (!struct_has_method(global_scope, struct_name, field_name)) {
                        char sname[128]; token_to_cstr(sname, sizeof(sname), struct_name);
                        fprintf(stderr, "\nError at line %d: struct '%s' has no field '%.*s'\n",
                                field_name.line, sname,
                                (int)field_name.length, field_name.start);
                        show_source_line(field_name.line);
                        had_error = true;
                        expr->expr_type = builtin_int;
                        return builtin_int;
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
            Type* obj_t = check_expr(expr->index_assign.object, scope);
            check_expr(expr->index_assign.index, scope);
            // OPEN map (empty {} literal, value type not yet fixed) written with
            // a read-modify-write of itself (m[k] = m[k] + 1): the read cannot
            // decide the value type (its string default used to poison the map),
            // so infer it from the arithmetic's OTHER operand BEFORE checking the
            // RHS - then the inner m[k] reads type as the inferred type and the
            // store below sees consistent types. See infer_open_map_rmw.
            if (obj_t && obj_t->kind == TYPE_MAP && !obj_t->map_type.value_type) {
                Type* rmw = infer_open_map_rmw(expr->index_assign.value, obj_t, scope);
                if (rmw && (rmw->kind == TYPE_INT || rmw->kind == TYPE_FLOAT ||
                            rmw->kind == TYPE_BOOL || rmw->kind == TYPE_STRING)) {
                    obj_t->map_type.value_type = rmw;
                }
            }
            Type* val_t = check_expr(expr->index_assign.value, scope);
            // Enforce element/value type on stores. Codegen picks the insert
            // fn from the VALUE's type but reads decode with the CONTAINER's
            // declared type - a mismatched store passed check and read back
            // as garbage (string stored, int-decoded → 0 or a pointer).
            // int<->float stays permissive (codegen coerces numerics).
            if (obj_t && val_t) {
                Type* slot = NULL;
                const char* container = NULL;
                if (obj_t->kind == TYPE_MAP) { slot = obj_t->map_type.value_type; container = "map with values of"; }
                else if (obj_t->kind == TYPE_ARRAY) { slot = obj_t->array_type.element_type; container = "array of"; }
                if (slot && slot->kind != val_t->kind &&
                    !((val_t->kind == TYPE_INT || val_t->kind == TYPE_FLOAT) &&
                      (slot->kind == TYPE_INT || slot->kind == TYPE_FLOAT))) {
                    fprintf(stderr, "Error at line %d: Cannot store %s in %s %s\n",
                            expr->index_assign.object->token.line,
                            type_to_string(val_t), container, type_to_string(slot));
                    fprintf(stderr, "  \033[34mHelp:\033[0m Values in a collection must share one type\n");
                    had_error = true;
                    return NULL;
                }
                // First store into an untyped map slot sets the value type.
                if (obj_t->kind == TYPE_MAP && !obj_t->map_type.value_type) {
                    obj_t->map_type.value_type = val_t;
                }
            }
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
                // Check field types
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    check_expr(expr->struct_init.field_values[i], scope);
                }
                
                // Infer one type argument PER type parameter (in declaration
                // order), not just the first field. A multi-param struct
                // `Pair<A,B> { first:A, second:B }` needs BOTH A and B
                // substituted; the old "first field only" heuristic left B
                // unsubstituted -> C error "unknown type name 'B'". For each
                // type param, find the field whose DECLARED type is that param
                // and take that field value's inferred type.
                StructStmt* _gdef = find_struct_definition(struct_name);
                int _nparams = _gdef ? _gdef->type_param_count : 1;
                if (_nparams < 1) _nparams = 1;
                Type** type_args = malloc(sizeof(Type*) * _nparams);
                int type_arg_count = 0;

                if (_gdef && _gdef->type_param_count > 0) {
                    for (int tp = 0; tp < _gdef->type_param_count; tp++) {
                        Type* resolved = NULL;
                        // Find a declared field whose type expr names this param.
                        for (int fi = 0; fi < _gdef->field_count && !resolved; fi++) {
                            Expr* fte = _gdef->field_types[fi];
                            if (!fte || fte->type != EXPR_IDENT) continue;
                            if (fte->token.length != _gdef->type_params[tp].length ||
                                memcmp(fte->token.start, _gdef->type_params[tp].start,
                                       fte->token.length) != 0) continue;
                            // Match this declared field to the supplied init field
                            // by name, then use that value's inferred type.
                            Token dfn = _gdef->fields[fi];
                            for (int ii = 0; ii < expr->struct_init.field_count; ii++) {
                                Token ifn = expr->struct_init.field_names[ii];
                                if (ifn.length == dfn.length &&
                                    memcmp(ifn.start, dfn.start, dfn.length) == 0) {
                                    resolved = check_expr(expr->struct_init.field_values[ii], scope);
                                    break;
                                }
                            }
                        }
                        type_args[type_arg_count++] = resolved ? resolved : builtin_int;
                    }
                } else {
                    // No decl available: fall back to the first field's type.
                    for (int i = 0; i < expr->struct_init.field_count; i++) {
                        Type* field_type = check_expr(expr->struct_init.field_values[i], scope);
                        if (field_type && type_arg_count == 0) {
                            type_args[type_arg_count++] = field_type;
                        }
                    }
                }
                
                // Generate monomorphic struct name (e.g. Box_int, Box_string) and
                // register the instantiation so codegen emits a distinct struct
                // per concrete type. Without this, two instantiations (Box<int>
                // and Box<string>) collapsed onto one heuristic C type and the
                // second silently miscompiled (garbage pointer). Store the name on
                // the init expr so struct_init / var-decl codegen use it.
                char monomorphic_name[256];
                wyn_generate_monomorphic_struct_name(struct_name, type_args, type_arg_count,
                                                     monomorphic_name, sizeof(monomorphic_name));
                extern void wyn_register_generic_struct_instantiation(Token, Type**, int);
                if (type_arg_count > 0) {
                    wyn_register_generic_struct_instantiation(struct_name, type_args, type_arg_count);
                    expr->struct_init.monomorphic_name = strdup(monomorphic_name);
                }

                // Create struct type. Name it with the MONOMORPHIC name so field
                // access and var typing resolve against the concrete instance.
                Type* struct_type = make_type(TYPE_STRUCT);
                if (type_arg_count > 0) {
                    Token mono_tok = { TOKEN_IDENT, expr->struct_init.monomorphic_name,
                                       (int)strlen(expr->struct_init.monomorphic_name), struct_name.line };
                    struct_type->struct_type.name = mono_tok;
                    // Resolve and cache each field's concrete Type on the struct
                    // Type so EXPR_FIELD_ACCESS on this monomorphic instance sees
                    // real field types (the monomorphic name isn't in the source,
                    // so find_struct_definition can't). A field typed as the
                    // generic param T takes the (single) inferred type arg;
                    // concrete fields keep their declared type.
                    StructStmt* gdef = find_struct_definition(struct_name);
                    if (gdef && gdef->field_count > 0) {
                        struct_type->struct_type.field_count = gdef->field_count;
                        struct_type->struct_type.field_names = malloc(sizeof(Token) * gdef->field_count);
                        struct_type->struct_type.field_types = malloc(sizeof(Type*) * gdef->field_count);
                        for (int fi = 0; fi < gdef->field_count; fi++) {
                            struct_type->struct_type.field_names[fi] = gdef->fields[fi];
                            Expr* fte = gdef->field_types[fi];
                            Type* resolved = NULL;
                            if (fte && fte->type == EXPR_IDENT) {
                                int tp_idx = -1;
                                for (int tp = 0; tp < gdef->type_param_count; tp++) {
                                    if (gdef->type_params[tp].length == fte->token.length &&
                                        memcmp(gdef->type_params[tp].start, fte->token.start, fte->token.length) == 0) {
                                        tp_idx = tp; break;
                                    }
                                }
                                // Use the type arg for THIS param's index (not
                                // always [0]) so multi-param structs resolve each
                                // field to its own concrete type.
                                resolved = (tp_idx >= 0 && tp_idx < type_arg_count)
                                           ? type_args[tp_idx] : extern_map_type(fte);
                            }
                            struct_type->struct_type.field_types[fi] = resolved ? resolved : builtin_int;
                        }
                    }
                } else {
                    struct_type->struct_type.name = struct_name;
                }
                expr->expr_type = struct_type;

                free(type_args);
                return struct_type;
            } else {
                // Regular struct initialization
                StructStmt* sdef = find_struct_definition(struct_name);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    Type* provided = check_expr(expr->struct_init.field_values[i], scope);
                    // K5/K2: validate each provided field against the definition -
                    // an unknown field name used to reach codegen and ICE ("no
                    // member named"), and a wrong-typed field was stored and read
                    // back as garbage. Only enforce when we can see the struct's
                    // definition (skips FFI/opaque types).
                    if (sdef && expr->struct_init.field_names) {
                        Token fname = expr->struct_init.field_names[i];
                        if (fname.length > 0) {
                            bool found = false;
                            for (int k = 0; k < sdef->field_count; k++) {
                                if (token_name_eq(sdef->fields[k], fname)) { found = true; break; }
                            }
                            if (!found) {
                                fprintf(stderr, "Error at line %d: struct '%.*s' has no field '%.*s'\n",
                                        struct_name.line, struct_name.length, struct_name.start,
                                        fname.length, fname.start);
                                had_error = true;
                            } else {
                                // K2: only type-check scalar fields. get_struct_field_type
                                // reliably resolves int/float/bool/string; for enum
                                // (fabricated as a bare struct here), struct, array and map
                                // field types it is not authoritative, so checking those
                                // over-rejects legit values (e.g. an enum-typed field
                                // initialized with an enum value). Name/presence checks
                                // (K5/K11) remain reliable for every field kind.
                                Type* expected = get_struct_field_type(sdef, fname);
                                bool expected_is_scalar = expected &&
                                    (expected->kind == TYPE_INT || expected->kind == TYPE_FLOAT ||
                                     expected->kind == TYPE_BOOL || expected->kind == TYPE_STRING);
                                bool provided_is_scalar = provided &&
                                    (provided->kind == TYPE_INT || provided->kind == TYPE_FLOAT ||
                                     provided->kind == TYPE_BOOL || provided->kind == TYPE_STRING);
                                if (expected_is_scalar && provided_is_scalar &&
                                    !wyn_is_type_compatible(expected, provided)) {
                                    fprintf(stderr, "Error at line %d: field '%.*s' of struct '%.*s' expects %s, got %s\n",
                                            struct_name.line, fname.length, fname.start,
                                            struct_name.length, struct_name.start,
                                            type_to_string(expected), type_to_string(provided));
                                    had_error = true;
                                }
                            }
                        }
                    }
                }
                // K11: every declared field must be initialized. A missing field
                // silently read back 0/null with no diagnostic.
                if (sdef && expr->struct_init.field_names) {
                    for (int k = 0; k < sdef->field_count; k++) {
                        bool present = false;
                        for (int i = 0; i < expr->struct_init.field_count; i++) {
                            if (token_name_eq(expr->struct_init.field_names[i], sdef->fields[k])) {
                                present = true; break;
                            }
                        }
                        if (!present) {
                            fprintf(stderr, "Error at line %d: struct '%.*s' is missing field '%.*s'\n",
                                    struct_name.line, struct_name.length, struct_name.start,
                                    sdef->fields[k].length, sdef->fields[k].start);
                            had_error = true;
                        }
                    }
                }
                // Create struct type with name
                Type* struct_type = make_type(TYPE_STRUCT);
                struct_type->struct_type.name = struct_name;  // Set name in struct_type union
                expr->expr_type = struct_type;
                return struct_type;
            }
        }
        case EXPR_OPTIONAL_TYPE: {
            // T2.5.1: Optional Type Implementation
            Type* inner_type = check_expr(expr->optional_type.inner_type, scope);
            if (!inner_type) return NULL;
            
            // Map int? → OptionInt, string? → OptionString (concrete struct types)
            if (inner_type->kind == TYPE_INT) {
                Token oi_name = {TOKEN_IDENT, "OptionInt", 9, 0};
                Symbol* sym = find_symbol(scope, oi_name);
                if (sym) { expr->expr_type = sym->type; return sym->type; }
            } else if (inner_type->kind == TYPE_STRING) {
                Token os_name = {TOKEN_IDENT, "OptionString", 12, 0};
                Symbol* sym = find_symbol(scope, os_name);
                if (sym) { expr->expr_type = sym->type; return sym->type; }
            } else if (inner_type->kind == TYPE_FLOAT) {
                Token of_name = {TOKEN_IDENT, "OptionFloat", 11, 0};
                Symbol* sym = find_symbol(scope, of_name);
                if (sym) { expr->expr_type = sym->type; return sym->type; }
            } else if (inner_type->kind == TYPE_BOOL) {
                Token ob_name = {TOKEN_IDENT, "OptionBool", 10, 0};
                Symbol* sym = find_symbol(scope, ob_name);
                if (sym) { expr->expr_type = sym->type; return sym->type; }
            } else if ((inner_type->kind == TYPE_STRUCT && inner_type->struct_type.name.length > 0) ||
                       (inner_type->kind == TYPE_ENUM && enum_name_is_data_enum(inner_type->name))) {
                // `Struct?` -> the monomorphic Option<Struct> family (lazily made). A
                // data-carrying enum lowers to a C struct, so `Shape?` gets OptionShape the
                // same way; the plain-enum branch below still collapses to OptionInt.
                Type* opt_type = register_option_struct_family(inner_type);
                if (opt_type) { expr->expr_type = opt_type; return opt_type; }
            } else if (inner_type->kind == TYPE_ENUM &&
                       !enum_name_is_data_enum(inner_type->name)) {
                // `Enum?` -> OptionInt: a PLAIN enum is an int in C, and OptionInt is
                // what `Some(Code::X)`/`None` actually produce. Falling through to the
                // generic TYPE_OPTIONAL below made a `c: Code?` PARAMETER reject its own
                // argument ("Expected: optional (Option<T>) / Got: struct (struct)").
                // A data-carrying enum is a C struct, so it keeps the generic fallback.
                Token oi_name = {TOKEN_IDENT, "OptionInt", 9, 0};
                Symbol* sym = find_symbol(scope, oi_name);
                if (sym) { expr->expr_type = sym->type; return sym->type; }
            }

            // Fallback: generic optional type
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
            // A user-struct ok-payload gets its own monomorphic family
            // Result<Struct> (e.g. `Ok(Point{...})` -> ResultPoint), registered
            // on demand - mirroring Some(Struct) -> OptionStruct.
            if (value_type->kind == TYPE_STRUCT && value_type->struct_type.name.length > 0) {
                Type* res_type = register_result_struct_family(value_type);
                if (res_type) { expr->expr_type = res_type; return res_type; }
            }
            // Resolve to concrete ResultInt or ResultString
            Token concrete_name;
            if (value_type == builtin_string) {
                concrete_name = (Token){TOKEN_IDENT, "ResultString", 12, 0};
            } else if (value_type == builtin_float) {
                concrete_name = (Token){TOKEN_IDENT, "ResultFloat", 11, 0};
            } else if (value_type == builtin_bool) {
                concrete_name = (Token){TOKEN_IDENT, "ResultBool", 10, 0};
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
            // ? operator for error propagation
            if (!expr->try_expr.value) {
                fprintf(stderr, "Error: ? operator requires an expression\n");
                had_error = true;
                return NULL;
            }
            
            Type* value_type = check_expr(expr->try_expr.value, scope);
            
            // Accept TYPE_RESULT or ResultInt/ResultString by name
            if (value_type && is_result_type(value_type)) {
                expr->expr_type = value_type->result_type.ok_type;
                return value_type->result_type.ok_type;
            }
            
            // Also accept the monomorphic Result struct families by name. Each
            // unwraps to its own ok-type - returning int for all of them (the old
            // behavior) mistyped ResultString/Float/Bool downstream.
            if (value_type && value_type->kind == TYPE_STRUCT) {
                Token name = value_type->struct_type.name;
                if (name.length == 9 && memcmp(name.start, "ResultInt", 9) == 0) {
                    expr->expr_type = builtin_int;   return builtin_int;
                }
                if (name.length == 12 && memcmp(name.start, "ResultString", 12) == 0) {
                    expr->expr_type = builtin_string; return builtin_string;
                }
                if (name.length == 11 && memcmp(name.start, "ResultFloat", 11) == 0) {
                    expr->expr_type = builtin_float; return builtin_float;
                }
                if (name.length == 10 && memcmp(name.start, "ResultBool", 10) == 0) {
                    expr->expr_type = builtin_bool;  return builtin_bool;
                }
                // Monomorphic Result<Struct,E> (ResultPoint / ResultPoint_Fail): `?`
                // unwraps to the underlying ok struct. The ok type is recovered from
                // the family's registered `<fam>_unwrap` function (its return type is
                // the ok struct) - robust to the "_<ErrTag>" suffix that a struct/
                // scalar error appends to the family name.
                if (name.length > 6 && memcmp(name.start, "Result", 6) == 0) {
                    char unw[256]; int fl = name.length < 200 ? name.length : 200;
                    memcpy(unw, name.start, fl); memcpy(unw + fl, "_unwrap", 7); unw[fl + 7] = '\0';
                    Token utok = {TOKEN_IDENT, unw, fl + 7, name.line};
                    Symbol* us = find_symbol(global_scope, utok);
                    if (us && us->type && us->type->kind == TYPE_FUNCTION &&
                        us->type->fn_type.return_type) {
                        expr->expr_type = us->type->fn_type.return_type;
                        return us->type->fn_type.return_type;
                    }
                    // Fallback: bare "Result<Struct>" (string err) -> strip prefix.
                    Token sname = {TOKEN_IDENT, name.start + 6, name.length - 6, name.line};
                    Symbol* st = find_symbol(global_scope, sname);
                    if (st && st->type && st->type->kind == TYPE_STRUCT) {
                        expr->expr_type = st->type; return st->type;
                    }
                }
            }

            // K6: `?` is only sound on a Result value - codegen unwraps a Result
            // struct. Applying it to an Option, scalar, or plain struct used to be
            // accepted here (forced to int) and then ICE'd in the C compiler
            // ("initializing 'ResultInt' with an expression of incompatible type").
            // Reject it with a clear message. (`?.` optional chaining is a distinct
            // construct - TOKEN_QUESTION_DOT - and is unaffected.)
            fprintf(stderr, "Error at line %d: the '?' operator can only be applied to a Result value, but got ",
                    expr->token.line);
            print_type_name(value_type);
            fprintf(stderr, "\n");
            had_error = true;
            expr->expr_type = builtin_int;
            return builtin_int;
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
            // A user-struct payload gets its own monomorphic family Option<Struct>
            // (e.g. `Some(User{...})` -> OptionUser), registered on demand. A DATA-carrying
            // enum payload (`Some(Shape::Circle(1.5))` -> OptionShape) takes the same path,
            // since it is a C struct; a PLAIN enum falls through to OptionInt below.
            if ((inner_type->kind == TYPE_STRUCT && inner_type->struct_type.name.length > 0) ||
                (inner_type->kind == TYPE_ENUM && enum_name_is_data_enum(inner_type->name))) {
                Type* opt_type = register_option_struct_family(inner_type);
                if (opt_type) { expr->expr_type = opt_type; return opt_type; }
            }
            Token concrete_name;
            if (inner_type == builtin_string) {
                concrete_name = (Token){TOKEN_IDENT, "OptionString", 12, 0};
            } else if (inner_type == builtin_float) {
                concrete_name = (Token){TOKEN_IDENT, "OptionFloat", 11, 0};
            } else if (inner_type == builtin_bool) {
                concrete_name = (Token){TOKEN_IDENT, "OptionBool", 10, 0};
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
        case EXPR_TERNARY: {
            // cond ? a : b - type the branches and return their common type so a
            // ternary bound to a var gets the right C type (e.g. a string ternary
            // was declared `long long`, storing the char* as an int -> garbage).
            check_expr(expr->ternary.condition, scope);
            Type* t_then = check_expr(expr->ternary.then_expr, scope);
            Type* t_else = check_expr(expr->ternary.else_expr, scope);
            Type* result = t_then ? t_then : t_else;
            if (!result) result = builtin_int;
            expr->expr_type = result;
            return result;
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
                            if (match_value_type->kind == TYPE_STRUCT &&
                                match_value_type->struct_type.field_names) {
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
                        // Enum variant with data destructuring
                        if (pat->option.inner_count > 1) {
                            // Multi-arg: Rect(w, h)
                            for (int pi = 0; pi < pat->option.inner_count; pi++) {
                                if (pat->option.inners[pi] && pat->option.inners[pi]->type == PATTERN_IDENT) {
                                    add_symbol(&arm_scope, pat->option.inners[pi]->ident.name, builtin_int, false);
                                }
                            }
                        } else if (pat->option.inner->type == PATTERN_IDENT) {
                            // Look up variant data type from enum definition
                            Type* bound_type = builtin_int;
                            // Option<T> Some(x): bind x to the payload type.
                            // Primitive families (OptionString/OptionInt/
                            // OptionFloat/OptionBool) bind their scalar; struct
                            // families (e.g. "OptionUser" -> struct "User") bind
                            // the payload struct type. Without the primitive
                            // arm, `Some(v)` on a `string?`/`float?` mis-typed v
                            // as int, so a later `v == "..."` false-rejected
                            // (and used to silently rely on the unsound blanket
                            // string-compare rule).
                            if (match_value_type && match_value_type->kind == TYPE_STRUCT &&
                                match_value_type->struct_type.name.length > 6 &&
                                memcmp(match_value_type->struct_type.name.start, "Option", 6) == 0 &&
                                pat->option.is_some) {
                                Token pl = match_value_type->struct_type.name;
                                int plen = pl.length - 6;
                                const char* pstart = pl.start + 6;
                                if (plen == 6 && memcmp(pstart, "String", 6) == 0) bound_type = builtin_string;
                                else if (plen == 3 && memcmp(pstart, "Int", 3) == 0) bound_type = builtin_int;
                                else if (plen == 5 && memcmp(pstart, "Float", 5) == 0) bound_type = builtin_float;
                                else if (plen == 4 && memcmp(pstart, "Bool", 4) == 0) bound_type = builtin_bool;
                                else {
                                    Token payload_name = {TOKEN_IDENT, pl.start + 6, plen, 0};
                                    Symbol* ps = find_symbol(global_scope, payload_name);
                                    if (ps && ps->type && ps->type->kind == TYPE_STRUCT) bound_type = ps->type;
                                }
                            }
                            if (match_value_type && match_value_type->kind == TYPE_ENUM) {
                                Token variant_name = pat->option.variant_name;
                                EnumStmt* enum_def = find_enum_definition(match_value_type->name);
                                if (enum_def) {
                                    for (int vi = 0; vi < enum_def->variant_count; vi++) {
                                        if (enum_def->variants[vi].length == variant_name.length &&
                                            memcmp(enum_def->variants[vi].start, variant_name.start, variant_name.length) == 0) {
                                            if (enum_def->variant_type_counts[vi] == 1) {
                                                Expr* te = enum_def->variant_types[vi][0];
                                                if (te && te->type == EXPR_IDENT) {
                                                    Token tt = te->token;
                                                    if (tt.length == 6 && memcmp(tt.start, "string", 6) == 0) bound_type = builtin_string;
                                                    else if (tt.length == 5 && memcmp(tt.start, "float", 5) == 0) bound_type = builtin_float;
                                                    else if (tt.length == 3 && memcmp(tt.start, "int", 3) == 0) bound_type = builtin_int;
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                            add_symbol(&arm_scope, pat->option.inner->ident.name, bound_type, false);
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
                    // Allow mismatched types - codegen will handle it
                    // Common case: enum destructuring returns different representations of same type
                }
            }
            
            // K3: a match EXPRESSION on a scalar (int/string) without a wildcard
            // is never exhaustive - you cannot enumerate every int or string - so
            // codegen has no arm for the fall-through and yields uninitialized
            // memory (silent 0 / (null)) or an ICE. Require a `_ =>` arm. This
            // also fences three cases that mis-resolve their scalar type here: an
            // int-literal match, a `none`-inferred Option (types as int → ICE on
            // the string path), and an enum whose name collides with a builtin
            // namespace (Color/Math/Time/File → not seen as TYPE_ENUM). In every
            // one the fix a user wants is the same: cover the rest with `_ =>`.
            // (Bool is exempt: `true`/`false` arms can be exhaustive.)
            if ((match_value_type->kind == TYPE_INT || match_value_type->kind == TYPE_STRING) &&
                !has_wildcard) {
                // A plain (dataless) enum whose name collides with a builtin
                // namespace mis-resolves to TYPE_INT right here. If the arms are
                // enum-variant patterns (Color.Red) naming a real enum and they
                // cover every variant, the match IS exhaustive - don't flag it.
                // Genuine int/string literals, bare Some/None, and partially
                // covered enums fall through and still require a `_ =>` arm.
                EnumStmt* ed = NULL;
                for (int ai = 0; ai < expr->match.arm_count && !ed; ai++) {
                    Pattern* pat = expr->match.arms[ai].pattern;
                    if (pat && pat->type == PATTERN_GUARD) pat = pat->guard.pattern;
                    if (pat && pat->type == PATTERN_OPTION && pat->option.enum_name.length > 0)
                        ed = find_enum_definition(pat->option.enum_name);
                }
                bool enum_exhaustive = false;
                if (ed) {
                    enum_exhaustive = true;
                    for (int vi = 0; vi < ed->variant_count && enum_exhaustive; vi++) {
                        Token ev = ed->variants[vi];
                        bool covered = false;
                        for (int ai = 0; ai < expr->match.arm_count && !covered; ai++) {
                            Pattern* pat = expr->match.arms[ai].pattern;
                            if (pat && pat->type == PATTERN_GUARD) continue; // guard may not cover
                            Token vn = {0}; int have = 0;
                            if (pat && pat->type == PATTERN_OPTION && pat->option.variant_name.length > 0) { vn = pat->option.variant_name; have = 1; }
                            else if (pat && pat->type == PATTERN_IDENT) { vn = pat->ident.name; have = 1; }
                            if (have && vn.length == ev.length && memcmp(vn.start, ev.start, ev.length) == 0)
                                covered = true;
                        }
                        if (!covered) enum_exhaustive = false;
                    }
                }
                if (!enum_exhaustive) {
                    fprintf(stderr, "Error at line %d: non-exhaustive match - a match on %s must end with a wildcard '_ =>' arm\n",
                            expr->match.value->token.line,
                            match_value_type->kind == TYPE_STRING ? "a string" : "an int");
                    had_error = true;
                }
            }

            // Exhaustiveness for match expressions checked in STMT_MATCH handler
            // Exhaustiveness: a match expression on an enum without a wildcard
            // must cover every variant. Otherwise codegen emits no default arm and
            // an unmatched value reads uninitialized memory (was silent garbage).
            if (match_value_type->kind == TYPE_ENUM && !has_wildcard) {
                EnumStmt* enum_def = find_enum_definition(match_value_type->name);
                if (enum_def) {
                    for (int vi = 0; vi < enum_def->variant_count; vi++) {
                        Token ev = enum_def->variants[vi];
                        bool covered = false;
                        for (int ai = 0; ai < expr->match.arm_count && !covered; ai++) {
                            Pattern* pat = expr->match.arms[ai].pattern;
                            if (pat && pat->type == PATTERN_GUARD) continue; // guard may not cover
                            Token vn = {0}; int have = 0;
                            if (pat && pat->type == PATTERN_OPTION && pat->option.variant_name.length > 0) { vn = pat->option.variant_name; have = 1; }
                            else if (pat && pat->type == PATTERN_IDENT) { vn = pat->ident.name; have = 1; }
                            if (have && vn.length == ev.length && memcmp(vn.start, ev.start, ev.length) == 0)
                                covered = true;
                        }
                        if (!covered) {
                            fprintf(stderr, "Error at line %d: non-exhaustive match - variant '%.*s' is not handled (add it or a wildcard '_ =>' arm)\n",
                                    expr->match.value->token.line, ev.length, ev.start);
                            had_error = true;
                        }
                    }
                }
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

// Does this statement guarantee that control leaves the function (return/yield)
// on every path? Used so a function whose body ends in an exhaustive if/else
// (each branch returning) isn't wrongly flagged "may not return a value".
static bool stmt_guarantees_return(Stmt* s) {
    if (!s) return false;
    switch (s->type) {
        case STMT_RETURN: return true;
        case STMT_BLOCK:
            for (int i = 0; i < s->block.count; i++)
                if (stmt_guarantees_return(s->block.stmts[i])) return true;
            return false;
        case STMT_IF:
            // Exhaustive only with an else where BOTH branches return.
            return s->if_stmt.else_branch &&
                   stmt_guarantees_return(s->if_stmt.then_branch) &&
                   stmt_guarantees_return(s->if_stmt.else_branch);
        case STMT_MATCH:
            // A match expression/stmt returns iff every arm's body returns.
            // (Conservative: only when there is at least one arm.)
            if (s->match_stmt.case_count == 0) return false;
            for (int i = 0; i < s->match_stmt.case_count; i++)
                if (!stmt_guarantees_return(s->match_stmt.cases[i].body)) return false;
            return true;
        default: return false;
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
                        // Get element type from array type annotation. Handles
                        // builtins, structs/enums, AND nested arrays ([[float]])
                        // via the recursive resolver, so the leaf element type is
                        // preserved for typed accessors.
                        array_type->array_type.element_type =
                            resolve_array_elem_annotation(stmt->var.type->array.elements[0]);
                    }
                    init_type = array_type;
                } else if (stmt->var.type->type == EXPR_CALL && 
                           stmt->var.type->call.callee && 
                           stmt->var.type->call.callee->type == EXPR_IDENT) {
                    // Handle generic type annotations: Array<T>, HashMap<K,V>, etc.
                    Token type_name = stmt->var.type->call.callee->token;
                    if ((type_name.length == 5 && memcmp(type_name.start, "Array", 5) == 0) ||
                        (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0)) {
                        init_type = make_type(TYPE_ARRAY);
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                        init_type = make_type(TYPE_MAP);
                        // Carry the ANNOTATED key/value types into the map type.
                        // Without this the annotation erased them, so
                        // `m: {string: int} = {"a": 1}` (and the older
                        // `m: HashMap<string, int> = ...`) built the map with
                        // hashmap_insert_int but READ it with
                        // hashmap_index_string -> m["a"] printed "" instead of 1.
                        // Only the inferred form `m = {"a": 1}` was correct.
                        // resolve_array_elem_annotation resolves a leaf type
                        // expression (int/string/float/bool/struct/enum/nested
                        // [T]) - exactly what a type argument is here.
                        if (stmt->var.type->call.arg_count >= 1)
                            init_type->map_type.key_type =
                                resolve_array_elem_annotation(stmt->var.type->call.args[0]);
                        if (stmt->var.type->call.arg_count >= 2)
                            init_type->map_type.value_type =
                                resolve_array_elem_annotation(stmt->var.type->call.args[1]);
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                        init_type = make_type(TYPE_SET);
                    } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                        init_type = make_type(TYPE_OPTIONAL);
                    } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                        init_type = make_type(TYPE_RESULT);
                    } else if (find_struct_definition(type_name)) {
                        // Generic STRUCT annotation: `b: Box<int> = Box{val: 7}`.
                        // This fell through to check_expr, which read the
                        // EXPR_CALL as a *call* to Box and typed the annotation
                        // as the type argument (int) - so a correct program was
                        // rejected with a spurious "Type mismatch / Expected:
                        // int, Got: struct". Struct generics monomorphize from
                        // the field values, so the annotation's type arguments
                        // add no information: the annotated type IS the struct.
                        init_type = make_type(TYPE_STRUCT);
                        init_type->struct_type.name = type_name;
                    } else {
                        init_type = check_expr(stmt->var.type, scope);
                    }
                    // Still check init expression
                    if (stmt->var.init) {
                        check_expr(stmt->var.init, scope);
                    }
                } else {
                    init_type = check_expr(stmt->var.type, scope);
                }
                
                // IMPORTANT: Still check the init expression to resolve method calls
                // and propagate type information, even though we have an explicit type
                if (stmt->var.init) {
                    Type* actual_type = check_expr(stmt->var.init, scope);
                    // Task 1.1: Check type mismatch between declared and actual type
                    if (init_type && actual_type && !types_equal(init_type, actual_type)) {
                        // Allow optional types to accept struct values (OptionInt_Some returns struct)
                        if (init_type->kind == TYPE_OPTIONAL && actual_type->kind == TYPE_STRUCT) {
                            // Compatible - OptionInt is a struct
                        } else {
                            char expected_str[128], actual_str[128];
                            snprintf(expected_str, sizeof(expected_str), "%s", type_to_string(init_type));
                            snprintf(actual_str, sizeof(actual_str), "%s", type_to_string(actual_type));
                            type_error_mismatch(expected_str, actual_str, "variable declaration",
                                stmt->var.name.line, 0);
                            had_error = true;
                        }
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
        case STMT_EXPR: {
            // Bare assignment: `x = expr` where `x` is not yet declared is a
            // declaration (Python-style), coexisting with `var`/`const`. Rewrite
            // the statement in place into a STMT_VAR so all existing declaration
            // type-inference and codegen apply. An assignment to an *existing*
            // variable stays a normal assignment (checked below).
            if (stmt->expr && stmt->expr->type == EXPR_ASSIGN &&
                !find_symbol(scope, stmt->expr->assign.name)) {
                Expr* init = stmt->expr->assign.value;
                Token name = stmt->expr->assign.name;
                stmt->type = STMT_VAR;
                stmt->var.name = name;
                stmt->var.pattern = NULL;
                stmt->var.type = NULL;         // inferred from init
                stmt->var.init = init;
                stmt->var.is_const = false;    // bare bindings are mutable
                stmt->var.is_mutable = true;
                stmt->var.uses_pattern = false;
                stmt->var.from_bare_assign = true; // codegen hoists nested ones
                check_stmt(stmt, scope);       // re-check as a var declaration
                break;
            }
            check_expr(stmt->expr, scope);
            break;
        }
        case STMT_YIELD: if (stmt->yield_stmt.value) check_expr(stmt->yield_stmt.value, scope); break;
        case STMT_SPAWN:
            // Fire-and-forget `spawn f(...)`: run normal expression checking on
            // the call. Skipping it (the old default: break) left every argument
            // expression UNTYPED - codegen then read array-element args through
            // array_get_int regardless of element type (string elements arrived
            // as NULL, float elements truncated) - and never marked the arg
            // identifiers used, so the arrays were falsely warned "unused".
            if (stmt->spawn.call) check_expr(stmt->spawn.call, scope);
            break;
        case STMT_RETURN:
            if (stmt->ret.value) {
                // `return m[k]` where m is an OPEN map (empty {} literal whose
                // value type no store has fixed yet): the declared return type
                // IS the expected value type, so let it fill the map instead of
                // the open-read string default (which mis-reported "Return type
                // mismatch. Expected int, got string" - the book's cache
                // pattern). Scalars only; the first-store rule stays in charge
                // of everything else.
                if (current_function_return_type && stmt->ret.value->type == EXPR_INDEX &&
                    (current_function_return_type->kind == TYPE_INT ||
                     current_function_return_type->kind == TYPE_FLOAT ||
                     current_function_return_type->kind == TYPE_BOOL ||
                     current_function_return_type->kind == TYPE_STRING)) {
                    Type* cont_t = check_expr(stmt->ret.value->index.array, scope);
                    if (cont_t && cont_t->kind == TYPE_MAP && !cont_t->map_type.value_type) {
                        cont_t->map_type.value_type = current_function_return_type;
                    }
                }
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
                    // Coerce bare values into Optional: `return x` in an `fn -> T?`
                    // auto-wraps in Some, and `return none` auto-wraps in None().
                    // Without this, users must write `return Some(x)` / `return None()`
                    // which violates the "obvious and forgiving" design goal.
                    // The return type of `fn -> int?` is resolved to the monomorphic
                    // Option struct (OptionInt/OptionString/OptionFloat/OptionBool),
                    // which has kind TYPE_STRUCT - detect it by name prefix.
                    if (current_function_return_type->kind == TYPE_STRUCT &&
                        current_function_return_type->struct_type.name.length >= 6 &&
                        memcmp(current_function_return_type->struct_type.name.start, "Option", 6) == 0) {
                        // `none` is typed as int by the identifier resolver; accept it.
                        if (stmt->ret.value->type == EXPR_IDENT &&
                            stmt->ret.value->token.length == 4 &&
                            memcmp(stmt->ret.value->token.start, "none", 4) == 0) {
                            break;
                        }
                        // Any other value: coerce into Some(value) - the inner type
                        // should match the Optional's wrapped type, but codegen already
                        // handles the wrapping; here we just don't reject it.
                        break;
                    }
                    // Allow int/bool interchangeability (comparisons return int but work as bool)
                    bool types_match = (current_function_return_type->kind == return_expr_type->kind) ||
                        // FFI `ptr` (TYPE_STRUCT "void*") and a machine word are
                        // interchangeable here for the same reason
                        // wyn_is_type_compatible allows it at a call boundary: `0` is
                        // the null idiom, and an unresolved `-> ptr` annotation still
                        // defaults to int. Without this, `pub fn f() -> ptr { return
                        // extern_returning_ptr() }` was rejected with
                        // "Expected int, got void*" on correct code.
                        (is_ptr_type(current_function_return_type) && return_expr_type->kind == TYPE_INT) ||
                        (is_ptr_type(return_expr_type) && current_function_return_type->kind == TYPE_INT) ||
                        (current_function_return_type->kind == TYPE_BOOL && return_expr_type->kind == TYPE_INT) ||
                        (current_function_return_type->kind == TYPE_INT && return_expr_type->kind == TYPE_BOOL) ||
                        (current_function_return_type->kind == TYPE_ENUM && return_expr_type->kind == TYPE_ENUM) ||
                        (current_function_return_type->kind == TYPE_ENUM && return_expr_type->kind == TYPE_INT) ||
                        (current_function_return_type->kind == TYPE_INT && return_expr_type->kind == TYPE_ENUM);
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
                // 10.7: Warn about unreachable code after return/break/continue
                if (i < stmt->block.count - 1) {
                    StmtType t = stmt->block.stmts[i]->type;
                    if (t == STMT_RETURN || t == STMT_BREAK || t == STMT_CONTINUE) {
                        const char* kw = t == STMT_RETURN ? "return" : t == STMT_BREAK ? "break" : "continue";
                        fprintf(stderr, "\033[33mWarning:\033[0m unreachable code after %s\n", kw);
                        break;
                    }
                }
            }
            break;
        case STMT_PARALLEL:
            // Check body statements in the SAME scope so bindings (including
            // parallel's spawn-bound vars) escape to the enclosing scope, as
            // the codegen emits them there.
            if (stmt->type == STMT_PARALLEL && stmt->block.timeout)
                check_expr(stmt->block.timeout, scope);
            for (int i = 0; i < stmt->block.count; i++) {
                check_stmt(stmt->block.stmts[i], scope);
            }
            break;
        case STMT_SELECT:
            for (int i = 0; i < stmt->select_stmt.arm_count; i++) {
                if (stmt->select_stmt.channels[i])
                    check_expr(stmt->select_stmt.channels[i], scope);
                // Each arm binds the received value (int) in its own scope.
                SymbolTable arm_scope = {0};
                arm_scope.parent = scope;
                add_symbol(&arm_scope, stmt->select_stmt.bind_names[i], builtin_int, false);
                if (stmt->select_stmt.bodies[i])
                    check_stmt(stmt->select_stmt.bodies[i], &arm_scope);
            }
            break;
        case STMT_IF:
            // Warn about assignment in condition (= vs ==)
            if (stmt->if_stmt.condition && stmt->if_stmt.condition->type == EXPR_ASSIGN) {
                fprintf(stderr, "\033[33mWarning\033[0m at line %d: Assignment in if condition - did you mean '=='?\n",
                        stmt->if_stmt.condition->token.line);
                show_source_line(stmt->if_stmt.condition->token.line);
            }
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
            // The loop variable (and init/index vars) live in a CHILD scope:
            // the emitted C declares them inside the for statement, so they do
            // not exist after the loop. Checking them into the enclosing scope
            // made a post-loop bare assignment (`for i in 0..3 {...}; i = 0`)
            // look like an assignment to an existing var - which then referenced
            // an undeclared C identifier and ICE'd at build. With a child scope
            // the name is free again after the loop and the bare assignment is
            // rewritten to a fresh declaration, matching the C scoping.
            SymbolTable for_scope = {0};
            for_scope.parent = scope;
            for_scope.capacity = 8;
            for_scope.symbols = calloc(8, sizeof(Symbol));
            for_scope.count = 0;

            if (stmt->for_stmt.init) {
                check_stmt(stmt->for_stmt.init, &for_scope);
            }
            check_expr(stmt->for_stmt.condition, &for_scope);
            check_expr(stmt->for_stmt.increment, &for_scope);

            // For array iteration, add the loop variable to scope
            if (stmt->for_stmt.array_expr) {
                // Determine element type from array expression
                Type* array_type = check_expr(stmt->for_stmt.array_expr, &for_scope);
                Type* elem_type = builtin_int; // default
                if (array_type && array_type->kind == TYPE_ARRAY && array_type->array_type.element_type) {
                    elem_type = array_type->array_type.element_type;
                }
                // Map iteration: `for k in m` binds k to the KEY (string);
                // `for k, v in m` binds k=key(string), v=value(map's value type).
                // Without this the value var typed as int, so `v == "..."` on a
                // string-valued map false-rejected after the comparison-soundness
                // fix (it used to lean on the now-removed blanket string rule).
                if (array_type && array_type->kind == TYPE_MAP) {
                    if (stmt->for_stmt.has_index) {
                        // key in index_var (string), value in loop_var.
                        Type* vt = array_type->map_type.value_type;
                        if (!vt) vt = builtin_string;
                        add_symbol(&for_scope, stmt->for_stmt.index_var, builtin_string, false);
                        add_symbol(&for_scope, stmt->for_stmt.loop_var, vt, false);
                    } else {
                        // single var is the key (string)
                        add_symbol(&for_scope, stmt->for_stmt.loop_var, builtin_string, false);
                    }
                } else {
                    // A STRING is not iterable. Codegen's for-in fallthrough assigns
                    // the iterable to a `WynArray` unconditionally, so `for c in s`
                    // emitted invalid C and died as a bare "internal codegen error"
                    // AFTER passing `wyn check` - the v1.21 soundness rule is that a
                    // program which checks must build.
                    //
                    // Rejected here rather than given a meaning: iterating a string
                    // could reasonably yield characters, bytes, or grapheme clusters,
                    // and silently picking one would be a language decision smuggled
                    // in as a bug fix. `.split("")` already spells "by character"
                    // explicitly, and splitting on a separator is what the reported
                    // case actually wanted (`for f in File::walk_dir(d).split("\n")`).
                    if (array_type && array_type->kind == TYPE_STRING) {
                        fprintf(stderr,
                                "\nError at line %d: cannot iterate a string directly\n",
                                stmt->for_stmt.loop_var.line);
                        show_source_line(stmt->for_stmt.loop_var.line);
                        fprintf(stderr,
                                "  \033[34mHelp:\033[0m be explicit about the unit:\n"
                                "    for %.*s in s.split(\"\") { ... }      // one character at a time\n"
                                "    for %.*s in s.split(\"\\n\") { ... }    // one line at a time\n"
                                "    for i in 0..s.len() { ... }         // by index\n",
                                stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start,
                                stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                        had_error = true;
                    }
                    add_symbol(&for_scope, stmt->for_stmt.loop_var, elem_type, false);
                    // Add index variable for indexed iteration: for i, v in arr
                    if (stmt->for_stmt.has_index) {
                        add_symbol(&for_scope, stmt->for_stmt.index_var, builtin_int, false);
                    }
                }
            }

            check_stmt(stmt->for_stmt.body, &for_scope);
            free(for_scope.symbols);
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
                        } else {
                            // User struct/enum param type.
                            Symbol* ts = find_symbol(global_scope, type_name);
                            if (ts && ts->type) param_type = ts->type;
                        }
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // `b: Struct?` / `b: int?` - resolve to the Option family.
                        Type* ot = check_expr(fn->param_types[j], &fn_scope);
                        if (ot) param_type = ot;
                    } else if (fn->param_types[j]->type == EXPR_CALL &&
                               fn->param_types[j]->call.callee &&
                               fn->param_types[j]->call.callee->type == EXPR_IDENT &&
                               fn->param_types[j]->call.callee->token.length == 7 &&
                               memcmp(fn->param_types[j]->call.callee->token.start, "HashMap", 7) == 0) {
                        // Map parameter `m: {string: int}` / `HashMap<K, V>` -
                        // see the matching branch in the signature pass.
                        Type* mt = make_type(TYPE_MAP);
                        if (fn->param_types[j]->call.arg_count >= 1)
                            mt->map_type.key_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[0]);
                        if (fn->param_types[j]->call.arg_count >= 2)
                            mt->map_type.value_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[1]);
                        param_type = mt;
                    }
                }

                add_symbol(&fn_scope, fn->params[j], param_type, true);
            }

            // Check function body with parameters in scope
            if (fn->body) {
                check_stmt(fn->body, &fn_scope);
            }
            
            // Warn about unused variables (for nested functions)
            for (int j = fn->param_count; j < fn_scope.count; j++) {
                Symbol* s = &fn_scope.symbols[j];
                // Skip params and _ prefixed
                if (j < fn->param_count) continue;
                if (s->name.start[0] == '_') continue;
                if (!s->is_used && s->name.length > 0) {
                    fprintf(stderr, "\033[33mWarning:\033[0m unused variable '%.*s' (line %d)\n",
                        s->name.length, s->name.start, s->name.line);
                }
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
            
            add_symbol(scope, const_stmt->name, const_type, false);
            break;
        }
        case STMT_EXPORT:
            // Check the exported statement
            check_stmt(stmt->export.stmt, scope);
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

            // Reject self-referential fields (`next: Node` or `next: Node?` inside
            // `struct Node`). Wyn structs are by-value with no pointer indirection,
            // so a recursive field is infinite-size; letting it through here used to
            // surface later as a bare "internal codegen error" at build time.
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                Expr* ft = stmt->struct_decl.field_types[i];
                if (ft && ft->type == EXPR_OPTIONAL_TYPE) ft = ft->optional_type.inner_type;
                if (!ft || ft->type != EXPR_IDENT) continue;
                if (ft->token.length == stmt->struct_decl.name.length &&
                    memcmp(ft->token.start, stmt->struct_decl.name.start,
                           ft->token.length) == 0) {
                    fprintf(stderr,
                            "Error at line %d: struct '%.*s' cannot contain a field of its own type"
                            " (structs are by-value, so a recursive field would be infinite-size).\n"
                            "  Workaround: store an index into an array of nodes instead of the node itself.\n",
                            ft->token.line,
                            (int)stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                    had_error = true;
                } else if (find_struct_definition(ft->token)) {
                    // Mutually-recursive cycle (A has a B field, B has an A field,
                    // possibly through more hops). Same infinite-size problem as
                    // direct self-reference; without this it dies in the generated
                    // C ("unknown type name") or as a codegen ICE.
                    Token visited[64];
                    int visited_count = 0;
                    if (struct_field_reaches(ft->token, stmt->struct_decl.name,
                                             visited, &visited_count)) {
                        fprintf(stderr,
                                "Error at line %d: struct '%.*s' field '%.*s' creates a recursive"
                                " struct cycle: '%.*s' contains '%.*s' back"
                                " (structs are by-value, so a recursive field would be infinite-size).\n"
                                "  Workaround: store an index into an array of nodes instead of the node itself.\n",
                                ft->token.line,
                                (int)stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                                (int)stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start,
                                (int)ft->token.length, ft->token.start,
                                (int)stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                        had_error = true;
                    }
                }
            }

            // Function-typed struct fields (`on_click: fn() -> void`) ARE supported:
            // the field is emitted as WynClosure and `b.on_click()` is lowered
            // through {fn, env} (see the EXPR_FN_TYPE branch in codegen_stmt.c and
            // the EXPR_FIELD_ACCESS callee branch in codegen_expr.c). This used to
            // be a hard check-time error because codegen typed the field
            // `long long` and lowered the call as a method call
            // (`Button_on_click(...)`, a symbol that does not exist).
            //
            // Kept as a comment rather than deleted because the OPTIONAL form
            // (`on_click: fn() -> void ?`) is still NOT wired up - Option<closure>
            // has no family type - and this is where a future gate for it belongs.

            // Gate (not yet implemented): a struct field whose declared type is a
            // HashMap/HashSet (or bare map/set) container. Codegen silently types
            // such a field as `long long` (not WynHashMap*/WynHashSet*), so the
            // field is unusable - `s.m["k"]` / `s.m.add()` later report "struct
            // has no field 'm'" and the program fails to build. A silently-
            // mistyped field is worse than a clear error, so reject at check with
            // a "not yet supported" message until container-valued fields land.
            // Array-typed fields (`vals: [int]`) DO work and are NOT gated here.
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                Expr* ft = stmt->struct_decl.field_types[i];
                if (ft && ft->type == EXPR_OPTIONAL_TYPE) ft = ft->optional_type.inner_type;
                int is_container = 0;
                const char* cname = NULL;
                if (ft && ft->type == EXPR_CALL && ft->call.callee &&
                    ft->call.callee->type == EXPR_IDENT) {
                    Token c = ft->call.callee->token;
                    if ((c.length == 7 && memcmp(c.start, "HashMap", 7) == 0)) { is_container = 1; cname = "HashMap"; }
                    else if ((c.length == 7 && memcmp(c.start, "HashSet", 7) == 0)) { is_container = 1; cname = "HashSet"; }
                    else if ((c.length == 3 && memcmp(c.start, "Map", 3) == 0)) { is_container = 1; cname = "Map"; }
                    else if ((c.length == 3 && memcmp(c.start, "Set", 3) == 0)) { is_container = 1; cname = "Set"; }
                }
                if (is_container) {
                    fprintf(stderr,
                        "Error at line %d: struct '%.*s' field '%.*s' has a %s type,"
                        " which is not yet supported as a struct field.\n"
                        "  Workaround: keep the %s in a separate variable, or store"
                        " its entries in an array field.\n",
                        stmt->struct_decl.name.line,
                        (int)stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                        (int)stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start,
                        cname, cname);
                    had_error = true;
                }
            }

            // Type-check struct-body methods (`struct S { fn m(self, ...) {...} }`).
            // Without this, method bodies were never checked, so field accesses on
            // `self` and typed params never got an expr_type - and codegen then
            // mis-lowered e.g. `self.name + suffix` (string+string) as a raw C `+`
            // instead of wyn_string_concat_safe. Bind `self` to the struct type and
            // each param to its declared type, exactly like impl-block methods.
            {
                Symbol* self_sym = find_symbol(global_scope, stmt->struct_decl.name);
                Type* self_type = (self_sym && self_sym->type && self_sym->type->kind == TYPE_STRUCT)
                    ? self_sym->type : NULL;
                for (int mi = 0; mi < stmt->struct_decl.method_count; mi++) {
                    FnStmt* method = stmt->struct_decl.methods[mi];
                    if (!method->body) continue;
                    SymbolTable method_scope = {0};
                    method_scope.parent = scope;
                    for (int j = 0; j < method->param_count; j++) {
                        Type* param_type = builtin_int;
                        if (j == 0 && method->params[0].length == 4 &&
                            memcmp(method->params[0].start, "self", 4) == 0) {
                            if (self_type) param_type = self_type;
                        } else if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                            Token tn = method->param_types[j]->token;
                            if (tn.length == 3 && memcmp(tn.start, "int", 3) == 0) param_type = builtin_int;
                            else if (tn.length == 6 && memcmp(tn.start, "string", 6) == 0) param_type = builtin_string;
                            else if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) param_type = builtin_float;
                            else if (tn.length == 4 && memcmp(tn.start, "bool", 4) == 0) param_type = builtin_bool;
                            else {
                                Symbol* ts = find_symbol(global_scope, tn);
                                if (ts && ts->type && ts->type->kind == TYPE_STRUCT) param_type = ts->type;
                            }
                        }
                        add_symbol(&method_scope, method->params[j], param_type,
                                   method->param_mutable ? method->param_mutable[j] : false);
                    }
                    // Bind `self` and check the body for its type-inference side
                    // effects (so field/param accesses get an expr_type). Leave
                    // return-type mismatch checking OFF here (NULL): a self-method
                    // call like `return self.other()` isn't yet return-typed by the
                    // checker, so enforcing it would false-positive. Return-type
                    // validation for methods can come with a fuller method-typing
                    // pass later.
                    Type* saved_ret = current_function_return_type;
                    Type* saved_self = current_self_type;
                    current_self_type = self_type;
                    current_function_return_type = NULL;
                    check_stmt(method->body, &method_scope);
                    current_function_return_type = saved_ret;
                    current_self_type = saved_self;
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
            // Register trait as a type (for dynamic dispatch parameters)
            {
                Type* trait_type = make_type(TYPE_STRUCT);
                trait_type->struct_type.name = stmt->trait_decl.name;
                add_symbol(scope, stmt->trait_decl.name, trait_type, false);
            }
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
                
                // Register trait method as TraitName_method for type resolution
                {
                    Type* fn_type = make_type(TYPE_FUNCTION);
                    fn_type->fn_type.param_count = method->param_count;
                    fn_type->fn_type.param_types = malloc(sizeof(Type*) * 4);
                    for (int j = 0; j < method->param_count && j < 4; j++)
                        fn_type->fn_type.param_types[j] = builtin_int;
                    fn_type->fn_type.return_type = builtin_int;
                    if (method->return_type && method->return_type->type == EXPR_IDENT) {
                        Token rt = method->return_type->token;
                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0)
                            fn_type->fn_type.return_type = builtin_string;
                        else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0)
                            fn_type->fn_type.return_type = builtin_float;
                        else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0)
                            fn_type->fn_type.return_type = builtin_bool;
                    }
                    char qname[128];
                    snprintf(qname, 128, "%.*s_%.*s",
                        stmt->trait_decl.name.length, stmt->trait_decl.name.start,
                        method->name.length, method->name.start);
                    Token qt = {TOKEN_IDENT, strdup(qname), (int)strlen(qname), 0};
                    add_function_overload(scope, qt, fn_type, false);
                }
            }
            break;
        case STMT_ENUM:
            // Create proper enum type
            {
                // Generic enums are NOT yet monomorphized (generic structs are).
                // Emitting one would put the literal type parameter into the C
                // (`T Some_value;`, `Opt_Some(T value)`) and fail to build, and a
                // single C enum type cannot hold two different payload types for
                // two instantiations. Reject cleanly at check time rather than
                // leak a raw C error. (Generic STRUCTS are supported - use a
                // struct wrapper, or a concrete enum, until generic-enum
                // monomorphization lands.)
                if (stmt->enum_decl.type_param_count > 0) {
                    fprintf(stderr,
                        "Error at line %d: generic enums are not yet supported "
                        "(enum '%.*s' has type parameter%s).\n"
                        "  Generic structs ARE supported; use a concrete enum "
                        "or a generic struct wrapper for now.\n",
                        stmt->enum_decl.name.line,
                        (int)stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                        stmt->enum_decl.type_param_count > 1 ? "s" : "");
                    had_error = true;
                    break;
                }
                // Reject a mutually-recursive enum cycle (enum A{AtoB(B)} enum
                // B{BtoA(A)}). Codegen represents enum payloads by value and only
                // heap-boxes DIRECT self-references, so a cross-enum cycle is an
                // infinite-size / incomplete-type C error. Detect the cycle and
                // report it clearly (a direct self-reference like Tree(Tree,Tree)
                // IS supported via boxing, so only flag cycles through a SIBLING).
                for (int vi = 0; vi < stmt->enum_decl.variant_count; vi++) {
                    for (int fi = 0; fi < stmt->enum_decl.variant_type_counts[vi]; fi++) {
                        Expr* ft = stmt->enum_decl.variant_types[vi] ? stmt->enum_decl.variant_types[vi][fi] : NULL;
                        if (!ft || ft->type != EXPR_IDENT) continue;
                        // Skip a direct self-reference (supported via boxing).
                        if (ft->token.length == stmt->enum_decl.name.length &&
                            memcmp(ft->token.start, stmt->enum_decl.name.start, ft->token.length) == 0) continue;
                        if (find_enum_definition(ft->token)) {
                            Token visited[64]; int nv = 0;
                            if (enum_payload_reaches(ft->token, stmt->enum_decl.name, visited, &nv)) {
                                fprintf(stderr,
                                    "Error at line %d: enum '%.*s' variant '%.*s' forms a mutually-recursive"
                                    " enum cycle through '%.*s', which is not yet supported"
                                    " (enum payloads are stored by value, so a cross-enum cycle is infinite-size).\n"
                                    "  Workaround: box one side by storing an index into an array of nodes,"
                                    " or make the recursion direct (a variant of its OWN enum type is supported).\n",
                                    stmt->enum_decl.name.line,
                                    (int)stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                                    (int)stmt->enum_decl.variants[vi].length, stmt->enum_decl.variants[vi].start,
                                    (int)ft->token.length, ft->token.start);
                                had_error = true;
                            }
                        }
                    }
                }
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
            {
                Type* alias_type = builtin_int;
                Token tn = stmt->type_alias.target;
                if (tn.length == 6 && memcmp(tn.start, "string", 6) == 0) alias_type = builtin_string;
                else if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) alias_type = builtin_float;
                else if (tn.length == 4 && memcmp(tn.start, "bool", 4) == 0) alias_type = builtin_bool;
                add_symbol(global_scope, stmt->type_alias.name, alias_type, false);
            }
            break;
        case STMT_IMPORT:
            // Register module namespace in scope
            add_symbol(scope, stmt->import.module, builtin_int, false);
            // Check for collision
            register_import(stmt->import.module.start, stmt->import.module.line);
            break;
        case STMT_TEST:
            // Check the test body in its OWN child scope, exactly as STMT_FN
            // does for a function body.
            //
            // Each `test` block is lowered to a separate function, so its
            // locals are genuinely independent at runtime. Checking the body
            // in the enclosing scope instead let a `var` declared in one test
            // leak its inferred type into every later test reusing the name:
            //     test "a" { var b = returns_float() ... }
            //     test "b" { var b = returns_int()   ... }
            // the second block resolved `b` to the first block's float symbol
            // and rejected valid code with "Type mismatch ... Expected: int,
            // Got: float". The two names occupy disjoint runtime scopes, so
            // this was a checker-only false positive.
            if (stmt->test_stmt.body) {
                SymbolTable test_scope = {0};
                test_scope.parent = scope;
                test_scope.capacity = 32;
                test_scope.symbols = calloc(32, sizeof(Symbol));
                test_scope.count = 0;
                check_stmt(stmt->test_stmt.body, &test_scope);
                free(test_scope.symbols);
            }
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
                    if (match_case->pattern->option.inner_count > 1) {
                        for (int pi = 0; pi < match_case->pattern->option.inner_count; pi++) {
                            if (match_case->pattern->option.inners[pi] &&
                                match_case->pattern->option.inners[pi]->type == PATTERN_IDENT) {
                                Type* bound_type = calloc(1, sizeof(Type));
                                bound_type->kind = TYPE_INT;
                                add_symbol(&arm_scope, match_case->pattern->option.inners[pi]->ident.name, bound_type, false);
                            }
                        }
                    } else if (match_case->pattern->option.inner && 
                        match_case->pattern->option.inner->type == PATTERN_IDENT) {
                        Token var_name = match_case->pattern->option.inner->ident.name;
                        // Look up the variant's data type from the enum definition
                        Type* bound_type = builtin_int; // default
                        // Option family Some(x): bind x to the PAYLOAD type -
                        // OptionString -> string, OptionOptionInt -> OptionInt
                        // (global-scope lookup), OptionUser -> struct User.
                        // Without this the binder was always int, so a nested
                        // `match inner` miscompiled for non-int payloads.
                        if (match_value_type && match_value_type->kind == TYPE_STRUCT &&
                            match_value_type->struct_type.name.length > 6 &&
                            memcmp(match_value_type->struct_type.name.start, "Option", 6) == 0 &&
                            match_case->pattern->option.is_some) {
                            Token pl = match_value_type->struct_type.name;
                            Token payload = {TOKEN_IDENT, pl.start + 6, pl.length - 6, 0};
                            if (payload.length == 6 && memcmp(payload.start, "String", 6) == 0) bound_type = builtin_string;
                            else if (payload.length == 5 && memcmp(payload.start, "Float", 5) == 0) bound_type = builtin_float;
                            else if (payload.length == 4 && memcmp(payload.start, "Bool", 4) == 0) bound_type = builtin_bool;
                            else if (payload.length == 3 && memcmp(payload.start, "Int", 3) == 0) bound_type = builtin_int;
                            else {
                                Symbol* ps = find_symbol(global_scope, payload);
                                if (ps && ps->type && ps->type->kind == TYPE_STRUCT) bound_type = ps->type;
                            }
                        }
                        // Result family Ok(v)/Err(e): bind the arm variable to the real
                        // payload type (see result_arm_payload_type). Previously only
                        // the Option families were handled here, so every Result arm
                        // binding was typed `int`.
                        {
                            // Select the arm by VARIANT NAME, not by `is_some`: the
                            // parser sets is_some=true for any data-carrying variant
                            // pattern, `Err(e)` included, so is_some cannot tell the
                            // two Result arms apart. Codegen keys off the name too.
                            Token _vn = match_case->pattern->option.variant_name;
                            bool _is_err = (_vn.length == 3 && memcmp(_vn.start, "Err", 3) == 0);
                            Type* _rp = result_arm_payload_type(match_value_type, !_is_err);
                            if (_rp) bound_type = _rp;
                        }
                        if (match_value_type && match_value_type->kind == TYPE_ENUM) {
                            Token variant_name = match_case->pattern->option.variant_name;
                            EnumStmt* enum_def = find_enum_definition(match_value_type->name);
                            if (enum_def) {
                                for (int vi = 0; vi < enum_def->variant_count; vi++) {
                                    if (enum_def->variants[vi].length == variant_name.length &&
                                        memcmp(enum_def->variants[vi].start, variant_name.start, variant_name.length) == 0) {
                                        if (enum_def->variant_type_counts[vi] == 1) {
                                            Expr* te = enum_def->variant_types[vi][0];
                                            if (te && te->type == EXPR_IDENT) {
                                                Token tt = te->token;
                                                if (tt.length == 6 && memcmp(tt.start, "string", 6) == 0) bound_type = builtin_string;
                                                else if (tt.length == 5 && memcmp(tt.start, "float", 5) == 0) bound_type = builtin_float;
                                                else if (tt.length == 3 && memcmp(tt.start, "int", 3) == 0) bound_type = builtin_int;
                                                else if (tt.length == 4 && memcmp(tt.start, "bool", 4) == 0) bound_type = builtin_bool;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
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
                (void)enum_variant_count;
                
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

// Map a function-parameter / return type EXPR_IDENT to a builtin Type*. Used
// when registering an imported module's functions so that qualified `m.foo()`
// calls get the RIGHT return type (previously everything was stubbed to int,
// which miscompiled/segfaulted string/float/bool returns from `m.foo()`).
static Type* imported_type_from_expr(Expr* type_expr) {
    if (!type_expr || type_expr->type != EXPR_IDENT) return builtin_int;
    Token t = type_expr->token;
    if (t.length == 6 && memcmp(t.start, "string", 6) == 0) return builtin_string;
    if (t.length == 5 && memcmp(t.start, "float", 5) == 0) return builtin_float;
    if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) return builtin_bool;
    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return builtin_int;
    if (t.length == 5 && memcmp(t.start, "array", 5) == 0) return builtin_array;
    if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return builtin_void;
    // The FFI pointer family, mapped exactly as extern_map_type does it. Missing
    // here, `pub fn cstr_through(s: cstr) -> cstr` registered as returning int, so
    // handing its result to an `extern fn atoi(s: cstr)` was rejected with
    // "Expected string, got int" - the annotation was right and the call was right.
    if (t.length == 3 && memcmp(t.start, "ptr", 3) == 0) return builtin_ptr;
    if (t.length == 4 && memcmp(t.start, "cstr", 4) == 0) return builtin_string;
    // A STRUCT or ENUM the module returns. The name is resolvable by now: pass -1
    // (check_program) merges module exports and pass 0 registers their types
    // BEFORE the import-registration loop reaches here, so the symbol exists.
    //
    // Defaulting these to builtin_int made `var b = lib.make()` type `b` as int,
    // and every field access on it then degraded to int as well - so
    // `b.items[0]` reported "member reference base type 'long long' is not a
    // structure". Codegen was taught to emit the right C type for the RETURN
    // (get_module_fn_struct_return, #229) but the checker's type stayed wrong,
    // which is why the C declaration looked correct while uses of it did not.
    {
        Symbol* s = find_symbol(global_scope, t);
        if (s && s->type && (s->type->kind == TYPE_STRUCT || s->type->kind == TYPE_ENUM))
            return s->type;
    }
    return builtin_int;  // unresolvable name: int-sized handle, as before
}

// W9: is `name` legitimately flat-callable in `prog` - i.e. defined as a
// top-level function in the program itself, or brought in by a SELECTIVE import
// (`import { name } from m`)? Those keep flat calls valid; a name available only
// via a whole-module `import m` does not (and must be qualified `m.name()`).
static bool flat_callable_in_program(Program* prog, const char* name, int name_len) {
    if (!prog) return false;
    for (int i = 0; i < prog->count; i++) {
        Stmt* s = prog->stmts[i];
        if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
        if (s->type == STMT_FN) {
            if (s->fn.name.length == name_len &&
                memcmp(s->fn.name.start, name, name_len) == 0) return true;
        } else if (s->type == STMT_IMPORT && s->import.item_count > 0) {
            for (int j = 0; j < s->import.item_count; j++) {
                if (s->import.items[j].length == name_len &&
                    memcmp(s->import.items[j].start, name, name_len) == 0) return true;
            }
        }
    }
    return false;
}

// Build a TYPE_FUNCTION for an imported module function from its AST, resolving
// param/return types (not just int stubs).
static Type* imported_fn_type(FnStmt* fn) {
    Type* fn_type = make_type(TYPE_FUNCTION);
    fn_type->fn_type.param_count = fn->param_count;
    // Required-parameter count, counted the same way the local-function path
    // counts it: walk back from the end while the parameter has a default.
    //
    // Without this the field kept make_type's -1 sentinel, so an imported
    // function's DEFAULTED parameters were invisible to any caller-side arity
    // check - `pub fn f(a: int, b: int = 5)` looked like it required both. That
    // was harmless while nothing checked arity on the dotted path; it stops being
    // harmless the moment something does, which is why it is fixed here rather
    // than worked around at the call site.
    {
        int min_params = fn->param_count;
        if (fn->param_defaults) {
            for (int j = fn->param_count - 1; j >= 0; j--) {
                if (fn->param_defaults[j]) min_params = j;
                else break;
            }
        }
        fn_type->fn_type.min_param_count = min_params;
    }
    fn_type->fn_type.param_types = fn->param_count
        ? malloc(sizeof(Type*) * fn->param_count) : NULL;
    for (int k = 0; k < fn->param_count; k++) {
        fn_type->fn_type.param_types[k] =
            imported_type_from_expr(fn->param_types ? fn->param_types[k] : NULL);
    }
    fn_type->fn_type.return_type = imported_type_from_expr(fn->return_type);
    return fn_type;
}

// ===================================================================
// Shared mutable global analysis (data-race soundness gate)
// ===================================================================
//
// A mutable global (`var total = 0` at top level) lowers to a plain C global
// (`long long total = 0;`) and a write lowers to a bare `total = (total + 1);`
// with NO synchronization. When the writing function is reachable from a
// `spawn` / `parallel { }` / `await_all` site, several OS threads (or
// coroutines interleaved at a yield point) execute that unsynchronized
// read-modify-write concurrently. The result is a torn count: the program
// prints a WRONG NUMBER and exits 0.
//
// This is easy to miss because the narrow window usually closes: with no yield
// point inside the loop, each task tends to run to completion and the answer
// looks right on every run. Insert any yield (`Time::sleep(0)`, I/O) and it
// collapses - measured 7947/7953/7957/7963 where 8000 was correct.
//
// POSTURE: reject at check time (an error, not a warning).
//   - The language already HAS the correct tool for this exact job: `Shared`
//     (`src/wyn_runtime.h`, lock-free atomics - Shared.new/get/set/add/sub) and
//     channels. This diagnostic points at them.
//   - It follows the precedent already set for the aggregate case: concurrent
//     structural mutation of a plain array panics at RUNTIME with
//     "use a channel or Shared to coordinate writers"
//     (wyn_concurrent_mutation_panic, src/wyn_runtime.h:576). Scalars had no
//     equivalent guard at all. A compile-time error is strictly better than a
//     runtime panic where we can prove it statically, and this we can.
//   - Rejected making it correct implicitly (atomics/lock on every qualifying
//     global): it would silently tax ordinary single-threaded code, and a
//     seq_cst RMW on a hot counter is a large regression on exactly the
//     push/index loops the release's perf numbers were measured on. It also
//     hides the design problem from the user instead of naming it.
//   - An error, not a warning: a warning preserves the wrong-answer-at-exit-0
//     failure mode, which is the whole defect.
//
// PRECISION (false positives are the main risk - a global written only from
// main, or only before any spawn, must still compile):
//   - Only MUTABLE globals count. `const` / immutable globals are never
//     flagged (read-only sharing is safe).
//   - Only writes from a function REACHABLE from a spawn site count. The
//     reachability set is the transitive closure of the call graph starting at
//     the functions named by `spawn f()` / `parallel { }` bodies / functions
//     whose futures reach `await_all`.
//   - `main` is deliberately NOT in the set (it is the single root thread), so
//     a global initialized or mutated by main - the overwhelmingly common
//     "configure once, then fan out" shape - still compiles.
//   - Reads are never flagged; only writes race destructively here.

#define WYN_MAXSMG 512

typedef struct {
    // Mutable global names (top-level `var`).
    char names[WYN_MAXSMG][128];
    int count;
} SmgGlobals;

typedef struct {
    char names[WYN_MAXSMG][128];
    int count;
} SmgFnSet;

static int smg_set_has(SmgFnSet* s, const char* n) {
    for (int i = 0; i < s->count; i++) if (strcmp(s->names[i], n) == 0) return 1;
    return 0;
}
static int smg_set_add(SmgFnSet* s, const char* n) {
    if (smg_set_has(s, n)) return 0;
    if (s->count >= WYN_MAXSMG) return 0;
    snprintf(s->names[s->count++], 128, "%s", n);
    return 1;  // newly added
}

// --- collect the direct callees / spawn targets of a function body ---------

static void smg_collect_expr(Expr* e, SmgFnSet* calls, SmgFnSet* spawns);
static void smg_collect_stmt(Stmt* s, SmgFnSet* calls, SmgFnSet* spawns);

// Record the callee name of an EXPR_CALL into `set`.
static void smg_record_callee(Expr* call, SmgFnSet* set) {
    if (!call || call->type != EXPR_CALL) return;
    if (call->call.callee->type != EXPR_IDENT) return;
    char n[128]; token_to_cstr(n, sizeof(n), call->call.callee->token);
    smg_set_add(set, n);
}

static void smg_collect_expr(Expr* e, SmgFnSet* calls, SmgFnSet* spawns) {
    if (!e) return;
    switch (e->type) {
        case EXPR_SPAWN:
            // `spawn f(...)` - f and everything it reaches runs concurrently.
            smg_record_callee(e->spawn.call, spawns);
            smg_collect_expr(e->spawn.call, calls, spawns);
            break;
        case EXPR_CALL:
            smg_record_callee(e, calls);
            // `await_all(futures)` / `Task.race(...)`: the futures were created
            // by spawn sites already recorded, so nothing extra to do here.
            smg_collect_expr(e->call.callee, calls, spawns);
            for (int i = 0; i < e->call.arg_count; i++)
                smg_collect_expr(e->call.args[i], calls, spawns);
            break;
        case EXPR_METHOD_CALL:
            smg_collect_expr(e->method_call.object, calls, spawns);
            for (int i = 0; i < e->method_call.arg_count; i++)
                smg_collect_expr(e->method_call.args[i], calls, spawns);
            break;
        case EXPR_BINARY:
            smg_collect_expr(e->binary.left, calls, spawns);
            smg_collect_expr(e->binary.right, calls, spawns); break;
        case EXPR_UNARY: smg_collect_expr(e->unary.operand, calls, spawns); break;
        case EXPR_TRY: smg_collect_expr(e->try_expr.value, calls, spawns); break;
        case EXPR_AWAIT: smg_collect_expr(e->await.expr, calls, spawns); break;
        case EXPR_ASSIGN: smg_collect_expr(e->assign.value, calls, spawns); break;
        case EXPR_INDEX:
            smg_collect_expr(e->index.array, calls, spawns);
            smg_collect_expr(e->index.index, calls, spawns); break;
        case EXPR_INDEX_ASSIGN:
            smg_collect_expr(e->index_assign.object, calls, spawns);
            smg_collect_expr(e->index_assign.index, calls, spawns);
            smg_collect_expr(e->index_assign.value, calls, spawns); break;
        case EXPR_FIELD_ACCESS: smg_collect_expr(e->field_access.object, calls, spawns); break;
        case EXPR_OPT_CHAIN: smg_collect_expr(e->opt_chain.object, calls, spawns); break;
        case EXPR_FIELD_ASSIGN:
            smg_collect_expr(e->field_assign.object, calls, spawns);
            smg_collect_expr(e->field_assign.value, calls, spawns); break;
        case EXPR_ARRAY:
            for (int i = 0; i < e->array.count; i++) smg_collect_expr(e->array.elements[i], calls, spawns);
            break;
        case EXPR_TUPLE:
            for (int i = 0; i < e->tuple.count; i++) smg_collect_expr(e->tuple.elements[i], calls, spawns);
            break;
        case EXPR_MAP:
            for (int i = 0; i < e->map.count; i++) {
                smg_collect_expr(e->map.keys[i], calls, spawns);
                smg_collect_expr(e->map.values[i], calls, spawns);
            }
            break;
        case EXPR_STRUCT_INIT:
            for (int i = 0; i < e->struct_init.field_count; i++)
                smg_collect_expr(e->struct_init.field_values[i], calls, spawns);
            break;
        case EXPR_STRING_INTERP:
            for (int i = 0; i < e->string_interp.count; i++)
                smg_collect_expr(e->string_interp.expressions[i], calls, spawns);
            break;
        case EXPR_TERNARY:
            smg_collect_expr(e->ternary.condition, calls, spawns);
            smg_collect_expr(e->ternary.then_expr, calls, spawns);
            smg_collect_expr(e->ternary.else_expr, calls, spawns); break;
        case EXPR_IF_EXPR:
            smg_collect_expr(e->if_expr.condition, calls, spawns);
            smg_collect_expr(e->if_expr.then_expr, calls, spawns);
            smg_collect_expr(e->if_expr.else_expr, calls, spawns); break;
        case EXPR_RANGE:
            smg_collect_expr(e->range.start, calls, spawns);
            smg_collect_expr(e->range.end, calls, spawns); break;
        case EXPR_SOME: case EXPR_OK: case EXPR_ERR:
            smg_collect_expr(e->option.value, calls, spawns); break;
        case EXPR_LAMBDA: smg_collect_expr(e->lambda.body, calls, spawns); break;
        case EXPR_TUPLE_INDEX: smg_collect_expr(e->tuple_index.tuple, calls, spawns); break;
        case EXPR_MATCH:
            smg_collect_expr(e->match.value, calls, spawns);
            for (int i = 0; i < e->match.arm_count; i++)
                smg_collect_expr(e->match.arms[i].result, calls, spawns);
            break;
        case EXPR_BLOCK:
            for (int i = 0; i < e->block.stmt_count; i++) smg_collect_stmt(e->block.stmts[i], calls, spawns);
            smg_collect_expr(e->block.result, calls, spawns); break;
        case EXPR_LIST_COMP:
            smg_collect_expr(e->list_comp.iter_start, calls, spawns);
            smg_collect_expr(e->list_comp.iter_end, calls, spawns);
            smg_collect_expr(e->list_comp.body, calls, spawns);
            smg_collect_expr(e->list_comp.condition, calls, spawns); break;
        default: break;
    }
}

static void smg_collect_stmt(Stmt* s, SmgFnSet* calls, SmgFnSet* spawns) {
    if (!s) return;
    switch (s->type) {
        case STMT_VAR: smg_collect_expr(s->var.init, calls, spawns); break;
        case STMT_CONST: smg_collect_expr(s->const_stmt.init, calls, spawns); break;
        case STMT_EXPR: case STMT_DEFER: smg_collect_expr(s->expr, calls, spawns); break;
        case STMT_RETURN: smg_collect_expr(s->ret.value, calls, spawns); break;
        case STMT_SPAWN:
            // Bare `spawn f()` statement form.
            smg_record_callee(s->spawn.call, spawns);
            smg_collect_expr(s->spawn.call, calls, spawns);
            break;
        case STMT_PARALLEL:
            // Every call bound inside `parallel { }` runs concurrently, whether
            // or not it is spelled with `spawn` (codegen_stmt.c treats a direct
            // call to a known user fn as an implicit spawn).
            for (int i = 0; i < s->block.count; i++) {
                Stmt* b = s->block.stmts[i];
                if (b && b->type == STMT_VAR && b->var.init && b->var.init->type == EXPR_CALL)
                    smg_record_callee(b->var.init, spawns);
                if (b && b->type == STMT_EXPR && b->expr && b->expr->type == EXPR_CALL)
                    smg_record_callee(b->expr, spawns);
                smg_collect_stmt(b, calls, spawns);
            }
            break;
        case STMT_BLOCK: case STMT_UNSAFE:
            for (int i = 0; i < s->block.count; i++) smg_collect_stmt(s->block.stmts[i], calls, spawns);
            break;
        case STMT_IF:
            smg_collect_expr(s->if_stmt.condition, calls, spawns);
            smg_collect_stmt(s->if_stmt.then_branch, calls, spawns);
            smg_collect_stmt(s->if_stmt.else_branch, calls, spawns); break;
        case STMT_WHILE:
            smg_collect_expr(s->while_stmt.condition, calls, spawns);
            smg_collect_stmt(s->while_stmt.body, calls, spawns); break;
        case STMT_FOR:
            smg_collect_expr(s->for_stmt.array_expr, calls, spawns);
            smg_collect_stmt(s->for_stmt.init, calls, spawns);
            smg_collect_expr(s->for_stmt.condition, calls, spawns);
            smg_collect_expr(s->for_stmt.increment, calls, spawns);
            smg_collect_stmt(s->for_stmt.body, calls, spawns); break;
        case STMT_MATCH:
            smg_collect_expr(s->match_stmt.value, calls, spawns);
            for (int i = 0; i < s->match_stmt.case_count; i++) {
                smg_collect_expr(s->match_stmt.cases[i].guard, calls, spawns);
                smg_collect_stmt(s->match_stmt.cases[i].body, calls, spawns);
            }
            break;
        case STMT_THROW: smg_collect_expr(s->throw_stmt.value, calls, spawns); break;
        case STMT_TRY:
            smg_collect_stmt(s->try_stmt.try_block, calls, spawns);
            for (int i = 0; i < s->try_stmt.catch_count; i++)
                smg_collect_stmt(s->try_stmt.catch_blocks[i], calls, spawns);
            smg_collect_stmt(s->try_stmt.finally_block, calls, spawns); break;
        case STMT_FN: case STMT_ASYNC_FN: smg_collect_stmt(s->fn.body, calls, spawns); break;
        case STMT_TEST: smg_collect_stmt(s->test_stmt.body, calls, spawns); break;
        default: break;
    }
}

// --- find writes to a global inside a function body -----------------------

// Report a write at `line` to global `g`.
static void smg_report(const char* g, int line, const char* fn_name) {
    fprintf(stderr,
        "\033[31m\033[1mError:\033[0m data race: shared mutable global '%s' is written from '%s', "
        "which runs concurrently (line %d)\n", g, fn_name, line);
    show_source_line(line);
    fprintf(stderr,
        "  \033[34mHelp:\033[0m A plain global is not synchronized, so concurrent writes lose\n"
        "        updates and the program prints a wrong answer while exiting 0.\n"
        "        Use an atomic Shared value:\n"
        "          total = Shared.new(0)      // instead of: var total = 0\n"
        "          Shared.add(total, 1)       // instead of: total = total + 1\n"
        "          Shared.get(total)          // to read the final value\n"
        "        Or send results over a channel, or return them from the task and\n"
        "        combine the awaited values in the caller.\n");
    had_error = true;
}

static void smg_check_expr_writes(Expr* e, SmgGlobals* globals, const char* fn_name, SmgFnSet* shadowed);
static void smg_check_stmt_writes(Stmt* s, SmgGlobals* globals, const char* fn_name, SmgFnSet* shadowed);

static int smg_is_flagged_global(SmgGlobals* g, SmgFnSet* shadowed, const char* n) {
    if (smg_set_has(shadowed, n)) return 0;  // a local shadows the global here
    for (int i = 0; i < g->count; i++) if (strcmp(g->names[i], n) == 0) return 1;
    return 0;
}

static void smg_check_expr_writes(Expr* e, SmgGlobals* globals, const char* fn_name, SmgFnSet* shadowed) {
    if (!e) return;
    switch (e->type) {
        case EXPR_ASSIGN: {
            char n[128]; token_to_cstr(n, sizeof(n), e->assign.name);
            if (smg_is_flagged_global(globals, shadowed, n))
                smg_report(n, e->assign.name.line, fn_name);
            smg_check_expr_writes(e->assign.value, globals, fn_name, shadowed);
            break;
        }
        case EXPR_INDEX_ASSIGN:
            smg_check_expr_writes(e->index_assign.object, globals, fn_name, shadowed);
            smg_check_expr_writes(e->index_assign.index, globals, fn_name, shadowed);
            smg_check_expr_writes(e->index_assign.value, globals, fn_name, shadowed);
            break;
        case EXPR_FIELD_ASSIGN:
            smg_check_expr_writes(e->field_assign.object, globals, fn_name, shadowed);
            smg_check_expr_writes(e->field_assign.value, globals, fn_name, shadowed);
            break;
        case EXPR_BINARY:
            smg_check_expr_writes(e->binary.left, globals, fn_name, shadowed);
            smg_check_expr_writes(e->binary.right, globals, fn_name, shadowed); break;
        case EXPR_UNARY: smg_check_expr_writes(e->unary.operand, globals, fn_name, shadowed); break;
        case EXPR_CALL:
            for (int i = 0; i < e->call.arg_count; i++)
                smg_check_expr_writes(e->call.args[i], globals, fn_name, shadowed);
            break;
        case EXPR_METHOD_CALL:
            smg_check_expr_writes(e->method_call.object, globals, fn_name, shadowed);
            for (int i = 0; i < e->method_call.arg_count; i++)
                smg_check_expr_writes(e->method_call.args[i], globals, fn_name, shadowed);
            break;
        case EXPR_TERNARY:
            smg_check_expr_writes(e->ternary.condition, globals, fn_name, shadowed);
            smg_check_expr_writes(e->ternary.then_expr, globals, fn_name, shadowed);
            smg_check_expr_writes(e->ternary.else_expr, globals, fn_name, shadowed); break;
        case EXPR_IF_EXPR:
            smg_check_expr_writes(e->if_expr.condition, globals, fn_name, shadowed);
            smg_check_expr_writes(e->if_expr.then_expr, globals, fn_name, shadowed);
            smg_check_expr_writes(e->if_expr.else_expr, globals, fn_name, shadowed); break;
        case EXPR_LAMBDA: smg_check_expr_writes(e->lambda.body, globals, fn_name, shadowed); break;
        case EXPR_BLOCK:
            for (int i = 0; i < e->block.stmt_count; i++)
                smg_check_stmt_writes(e->block.stmts[i], globals, fn_name, shadowed);
            smg_check_expr_writes(e->block.result, globals, fn_name, shadowed); break;
        case EXPR_MATCH:
            smg_check_expr_writes(e->match.value, globals, fn_name, shadowed);
            for (int i = 0; i < e->match.arm_count; i++)
                smg_check_expr_writes(e->match.arms[i].result, globals, fn_name, shadowed);
            break;
        case EXPR_AWAIT: smg_check_expr_writes(e->await.expr, globals, fn_name, shadowed); break;
        case EXPR_SPAWN: smg_check_expr_writes(e->spawn.call, globals, fn_name, shadowed); break;
        default: break;
    }
}

static void smg_check_stmt_writes(Stmt* s, SmgGlobals* globals, const char* fn_name, SmgFnSet* shadowed) {
    if (!s) return;
    switch (s->type) {
        case STMT_VAR: {
            // A local declaration shadows the global from here on: `var total = 0`
            // inside the function is a different variable.
            char n[128]; token_to_cstr(n, sizeof(n), s->var.name);
            smg_set_add(shadowed, n);
            smg_check_expr_writes(s->var.init, globals, fn_name, shadowed);
            break;
        }
        case STMT_CONST: {
            char n[128]; token_to_cstr(n, sizeof(n), s->const_stmt.name);
            smg_set_add(shadowed, n);
            smg_check_expr_writes(s->const_stmt.init, globals, fn_name, shadowed);
            break;
        }
        case STMT_EXPR: case STMT_DEFER: smg_check_expr_writes(s->expr, globals, fn_name, shadowed); break;
        case STMT_RETURN: smg_check_expr_writes(s->ret.value, globals, fn_name, shadowed); break;
        case STMT_BLOCK: case STMT_UNSAFE: case STMT_PARALLEL:
            for (int i = 0; i < s->block.count; i++)
                smg_check_stmt_writes(s->block.stmts[i], globals, fn_name, shadowed);
            break;
        case STMT_IF:
            smg_check_expr_writes(s->if_stmt.condition, globals, fn_name, shadowed);
            smg_check_stmt_writes(s->if_stmt.then_branch, globals, fn_name, shadowed);
            smg_check_stmt_writes(s->if_stmt.else_branch, globals, fn_name, shadowed); break;
        case STMT_WHILE:
            smg_check_expr_writes(s->while_stmt.condition, globals, fn_name, shadowed);
            smg_check_stmt_writes(s->while_stmt.body, globals, fn_name, shadowed); break;
        case STMT_FOR:
            smg_check_expr_writes(s->for_stmt.array_expr, globals, fn_name, shadowed);
            smg_check_stmt_writes(s->for_stmt.init, globals, fn_name, shadowed);
            smg_check_expr_writes(s->for_stmt.condition, globals, fn_name, shadowed);
            smg_check_expr_writes(s->for_stmt.increment, globals, fn_name, shadowed);
            smg_check_stmt_writes(s->for_stmt.body, globals, fn_name, shadowed); break;
        case STMT_MATCH:
            smg_check_expr_writes(s->match_stmt.value, globals, fn_name, shadowed);
            for (int i = 0; i < s->match_stmt.case_count; i++)
                smg_check_stmt_writes(s->match_stmt.cases[i].body, globals, fn_name, shadowed);
            break;
        case STMT_SPAWN: smg_check_expr_writes(s->spawn.call, globals, fn_name, shadowed); break;
        case STMT_THROW: smg_check_expr_writes(s->throw_stmt.value, globals, fn_name, shadowed); break;
        case STMT_TRY:
            smg_check_stmt_writes(s->try_stmt.try_block, globals, fn_name, shadowed);
            for (int i = 0; i < s->try_stmt.catch_count; i++)
                smg_check_stmt_writes(s->try_stmt.catch_blocks[i], globals, fn_name, shadowed);
            smg_check_stmt_writes(s->try_stmt.finally_block, globals, fn_name, shadowed); break;
        default: break;
    }
}

// Entry point. Runs after the normal checking passes.
static void check_shared_mutable_globals(Program* prog) {
    static SmgGlobals globals;
    globals.count = 0;

    // 1. Collect MUTABLE top-level globals. `const` and immutable bindings are
    //    safe to share, so they are never candidates.
    for (int i = 0; i < prog->count; i++) {
        Stmt* s = prog->stmts[i];
        if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
        if (s->type != STMT_VAR) continue;
        if (!s->var.is_mutable) continue;
        if (globals.count >= WYN_MAXSMG) break;
        token_to_cstr(globals.names[globals.count], 128, s->var.name);
        globals.count++;
    }
    if (globals.count == 0) return;

    // 2. Seed the concurrent set with every spawn / parallel target anywhere in
    //    the program, then close it transitively over the call graph. `main` is
    //    never seeded: it is the single root thread, so a global that main
    //    mutates before fanning out is not racing with anything.
    static SmgFnSet concurrent; concurrent.count = 0;
    for (int i = 0; i < prog->count; i++) {
        static SmgFnSet ignored_calls; ignored_calls.count = 0;
        smg_collect_stmt(prog->stmts[i], &ignored_calls, &concurrent);
    }
    if (concurrent.count == 0) return;

    // Transitive closure: anything a concurrent function calls is concurrent too.
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < prog->count; i++) {
            Stmt* s = prog->stmts[i];
            if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
            if (s->type != STMT_FN && s->type != STMT_ASYNC_FN) continue;
            char fn[128]; token_to_cstr(fn, sizeof(fn), s->fn.name);
            if (!smg_set_has(&concurrent, fn)) continue;
            static SmgFnSet callees; callees.count = 0;
            static SmgFnSet more_spawns; more_spawns.count = 0;
            smg_collect_stmt(s->fn.body, &callees, &more_spawns);
            for (int c = 0; c < callees.count; c++)
                if (smg_set_add(&concurrent, callees.names[c])) changed = 1;
            for (int c = 0; c < more_spawns.count; c++)
                if (smg_set_add(&concurrent, more_spawns.names[c])) changed = 1;
        }
    }

    // 3. Report writes to a candidate global from any concurrent function.
    for (int i = 0; i < prog->count; i++) {
        Stmt* s = prog->stmts[i];
        if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
        if (s->type != STMT_FN && s->type != STMT_ASYNC_FN) continue;
        char fn[128]; token_to_cstr(fn, sizeof(fn), s->fn.name);
        if (!smg_set_has(&concurrent, fn)) continue;
        // Parameters shadow globals of the same name.
        static SmgFnSet shadowed; shadowed.count = 0;
        for (int p = 0; p < s->fn.param_count; p++) {
            char pn[128]; token_to_cstr(pn, sizeof(pn), s->fn.params[p]);
            smg_set_add(&shadowed, pn);
        }
        smg_check_stmt_writes(s->fn.body, &globals, fn, &shadowed);
    }
}

void check_program(Program* prog) {
    // Set global pointer for struct field type lookup
    current_program = prog;
    
    // Pass -1: Process imports first so module types are available in Pass 0
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            ImportStmt* import = &prog->stmts[i]->import;
            char module_name[256];
            token_to_cstr(module_name, sizeof(module_name), import->module);
            extern Program* load_module(const char* name);
            extern void merge_module_exports(Program* module, Program* target, ImportStmt* import);
            Program* module = load_module(module_name);
            if (module) {
                merge_module_exports(module, prog, import);
            }
        }
    }
    
    // Pass 0: Register all struct types, enums, and constants first (so functions can reference them)
    for (int i = 0; i < prog->count; i++) {
        Stmt* pass0_stmt = prog->stmts[i];
        // Unwrap export statements
        if (pass0_stmt->type == STMT_EXPORT && pass0_stmt->export.stmt) {
            pass0_stmt = pass0_stmt->export.stmt;
        }
        if (pass0_stmt->type == STMT_STRUCT) {
            StructStmt* struct_decl = &pass0_stmt->struct_decl;
            Type* struct_type = make_type(TYPE_STRUCT);
            struct_type->struct_type.name = struct_decl->name;
            struct_type->struct_type.field_count = struct_decl->field_count;
            // Populate field_names so consumers that iterate fields (struct
            // pattern destructuring, field access) don't deref a NULL array.
            // field_types is resolved lazily below; names come straight from
            // the declaration. Without this, field_count is set but the arrays
            // are NULL, crashing any field iteration.
            if (struct_decl->field_count > 0) {
                struct_type->struct_type.field_names =
                    malloc(sizeof(Token) * struct_decl->field_count);
                struct_type->struct_type.field_types =
                    malloc(sizeof(Type*) * struct_decl->field_count);
                for (int _fi = 0; _fi < struct_decl->field_count; _fi++) {
                    struct_type->struct_type.field_names[_fi] = struct_decl->fields[_fi];
                    // Best-effort field type; may reference a not-yet-registered
                    // type, in which case default to int (resolved fully when the
                    // struct body is checked).
                    Type* ft = get_struct_field_type(struct_decl, struct_decl->fields[_fi]);
                    struct_type->struct_type.field_types[_fi] = ft ? ft : builtin_int;
                }
            }
            // A user struct SHADOWS a same-named builtin stdlib namespace.
            //
            // init_checker() registers all 42 namespaces (File, Path, Time, Color, Log,
            // Env, Task, ...) as global symbols typed `int`, and it runs before parsing, so
            // it cannot know what the program declares. add_symbol appends without
            // replacing and find_symbol returns the FIRST hash match, so the namespace won
            // and `struct Path` resolved to an int -- which surfaced far away as
            // "Type mismatch at line 1:0 / Expected: enum, Got: string", naming neither the
            // struct nor a real line. Retyping the existing symbol here makes the user's
            // declaration win, which is the only sensible resolution: a struct and a
            // namespace are different kinds of name, and only the struct is in this file.
            {
                Symbol* prior = find_symbol(global_scope, struct_decl->name);
                if (prior && prior->type && prior->type->kind != TYPE_STRUCT) {
                    prior->type = struct_type;
                } else {
                    add_symbol(global_scope, struct_decl->name, struct_type, false);
                }
            }
        } else if (pass0_stmt->type == STMT_ENUM) {
            // Register enum type and variants early so functions can use them
            EnumStmt* enum_decl = &pass0_stmt->enum_decl;
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
            fn_type->fn_type.param_types = malloc(sizeof(Type*) * (ext->param_count > 0 ? ext->param_count : 1));
            for (int j = 0; j < ext->param_count; j++) {
                // Map the declared C type to a Wyn builtin. Exact-length matches
                // avoid the old prefix bug (e.g. "int64" matching "int").
                fn_type->fn_type.param_types[j] = extern_map_type(ext->param_types[j]);
            }
            fn_type->fn_type.return_type = extern_map_type(ext->return_type); // void/NULL -> void
            // A C symbol may be declared once but reach this loop twice (e.g. a
            // C-package binding merged across checker passes, or the same header
            // bound in two imported packages). Re-declaring the identical extern
            // is harmless, so skip it silently rather than firing the "duplicate
            // signature" error that add_function_overload would raise.
            Symbol* _ext_existing = find_symbol(global_scope, ext->name);
            if (!(_ext_existing && _ext_existing->type &&
                  _ext_existing->type->kind == TYPE_FUNCTION &&
                  signatures_match(_ext_existing->type, fn_type))) {
                add_function_overload(global_scope, ext->name, fn_type, false);
            }
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
        } else if (prog->stmts[i]->type == STMT_TRAIT) {
            // Register trait as a type early so functions can use trait params
            Type* trait_type = make_type(TYPE_STRUCT);
            trait_type->struct_type.name = prog->stmts[i]->trait_decl.name;
            add_symbol(global_scope, prog->stmts[i]->trait_decl.name, trait_type, false);
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
        
        // String module
        Token string_mod_tok = {TOKEN_IDENT, "String", 6, 0};
        add_symbol(global_scope, string_mod_tok, builtin_string, false);
        
        // Data module
        Token data_mod_tok = {TOKEN_IDENT, "Data", 4, 0};
        add_symbol(global_scope, data_mod_tok, builtin_int, false);
        
        // Data::save returns void, Data::load returns map
        Token data_save_tok = {TOKEN_IDENT, "Data::save", 10, 0};
        Type* data_save_type = make_type(TYPE_FUNCTION);
        data_save_type->fn_type.param_count = 2;
        data_save_type->fn_type.param_types = malloc(sizeof(Type*) * 2);
        data_save_type->fn_type.param_types[0] = builtin_string;
        data_save_type->fn_type.param_types[1] = make_type(TYPE_MAP);
        data_save_type->fn_type.return_type = builtin_void;
        add_symbol(global_scope, data_save_tok, data_save_type, false);
        
        Token data_load_tok = {TOKEN_IDENT, "Data::load", 10, 0};
        Type* data_load_type = make_type(TYPE_FUNCTION);
        data_load_type->fn_type.param_count = 1;
        data_load_type->fn_type.param_types = malloc(sizeof(Type*));
        data_load_type->fn_type.param_types[0] = builtin_string;
        data_load_type->fn_type.return_type = make_type(TYPE_MAP);
        add_symbol(global_scope, data_load_tok, data_load_type, false);
        
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
            token_to_cstr(module_name, sizeof(module_name), import->module);
            
            // Load module from same directory as current file
            Program* module = load_module(module_name);
            
            if (module) {
                // Check if this is a whole-module import (no items specified)
                if (import->item_count == 0) {
                    // Whole module import - register functions with module prefix
                    char module_name_str[256];
                    token_to_cstr(module_name_str, sizeof(module_name_str), import->module);
                    // W9: if the import is aliased (`import m as mm`), the required
                    // call form - and the flat-call hint - use the alias.
                    char hint_name[256];
                    if (import->alias.start != NULL && import->alias.length > 0) {
                        token_to_cstr(hint_name, sizeof(hint_name), import->alias);
                    } else {
                        strncpy(hint_name, module_name_str, sizeof(hint_name)-1);
                        hint_name[sizeof(hint_name)-1] = '\0';
                    }

                    // Register all exported functions with qualified names.
                    // A module function is exported either as `export fn`
                    // (STMT_EXPORT wrapping STMT_FN) or as `pub fn` (a STMT_FN
                    // with is_public); handle both.
                    for (int j = 0; j < module->count; j++) {
                        Stmt* stmt = module->stmts[j];
                        FnStmt* fn = NULL;
                        if (stmt->type == STMT_EXPORT && stmt->export.stmt && stmt->export.stmt->type == STMT_FN) {
                            fn = &stmt->export.stmt->fn;
                        } else if (stmt->type == STMT_FN && stmt->fn.is_public) {
                            fn = &stmt->fn;
                        }
                        if (fn) {

                            // Create qualified name: module::function
                            char* qualified_name = malloc(strlen(module_name_str) + 2 + fn->name.length + 1);
                            snprintf(qualified_name, strlen(module_name_str) + 2 + fn->name.length + 1, "%s::%.*s", module_name_str, fn->name.length, fn->name.start);
                            
                            Token qualified_token = fn->name;
                            qualified_token.start = qualified_name;
                            qualified_token.length = strlen(qualified_name);

                            // Function type with REAL param/return types so that
                            // `m.foo()` calls type-check correctly (a string/float
                            // return through the qualified path used to be stubbed
                            // to int and miscompiled).
                            Type* fn_type = imported_fn_type(fn);

                            // Register with qualified name
                            Symbol* existing = find_symbol(global_scope, qualified_token);
                            if (!existing) {
                                add_symbol(global_scope, qualified_token, fn_type, false);
                            }

                            // W9: record the bare name as whole-module-only so a
                            // flat `foo()` call errors with a "did you mean
                            // m.foo()?" hint (unless the program itself defines it
                            // or selectively imports it - checked at the call site).
                            {
                                char bare[128];
                                token_to_cstr(bare, sizeof(bare), fn->name);
                                register_whole_module_fn(hint_name, bare);
                            }
                        }
                    }
                } else {
                    // Selective import: `import { f } from m` may only name
                    // pub/export fns. Same rule as qualified calls.
                    char sel_module[256];
                    token_to_cstr(sel_module, sizeof(sel_module), import->module);
                    if (!checking_same_module(sel_module)) {
                        for (int j = 0; j < import->item_count; j++) {
                            char item_name[128];
                            token_to_cstr(item_name, sizeof(item_name), import->items[j]);
                            if (module_fn_visibility(sel_module, item_name) == VIS_PRIVATE) {
                                private_fn_error(import->module.line, item_name, sel_module);
                            }
                        }
                    }
                }

                // Merge exported functions into current program for codegen
                // Symbols are already registered by check_all_modules
                merge_module_exports(module, prog, import);
            } else {
                // Error already printed by module loader - suppress duplicate
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
            // Count required params (without defaults)
            int min_params = fn->param_count;
            if (fn->param_defaults) {
                for (int j = fn->param_count - 1; j >= 0; j--) {
                    if (fn->param_defaults[j]) min_params = j;
                    else break;
                }
            }
            fn_type->fn_type.min_param_count = min_params;
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
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // `b: Struct?`/`b: int?` - Option family, so the signature
                        // validates Some/None arguments (not the bare int default).
                        Type* ot = check_expr(fn->param_types[j], global_scope);
                        if (ot) param_type = ot;
                    } else if (fn->param_types[j]->type == EXPR_CALL &&
                               fn->param_types[j]->call.callee &&
                               fn->param_types[j]->call.callee->type == EXPR_IDENT &&
                               fn->param_types[j]->call.callee->token.length == 7 &&
                               memcmp(fn->param_types[j]->call.callee->token.start, "HashMap", 7) == 0) {
                        // Map parameter: `m: {string: int}` (desugared to
                        // HashMap<string, int>) or the explicit HashMap<K, V>.
                        // Previously BOTH fell through to the `builtin_int`
                        // default, so passing a map to the function reported
                        // "Expected: int, Got: map" and `m[k]` inside the body
                        // reported "Array index must be int".
                        Type* mt = make_type(TYPE_MAP);
                        if (fn->param_types[j]->call.arg_count >= 1)
                            mt->map_type.key_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[0]);
                        if (fn->param_types[j]->call.arg_count >= 2)
                            mt->map_type.value_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[1]);
                        param_type = mt;
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
                            // Carry the declared key/value types (`-> {string: int}`
                            // desugars to HashMap<string, int>), else callers read
                            // the returned map through the wrong typed getter.
                            if (fn->return_type->call.arg_count >= 1)
                                fn_type->fn_type.return_type->map_type.key_type =
                                    resolve_array_elem_annotation(fn->return_type->call.args[0]);
                            if (fn->return_type->call.arg_count >= 2)
                                fn_type->fn_type.return_type->map_type.value_type =
                                    resolve_array_elem_annotation(fn->return_type->call.args[1]);
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            fn_type->fn_type.return_type = make_type(TYPE_SET);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            // Resolve Option<int> -> OptionInt, Option<string> -> OptionString
                            Token concrete = {TOKEN_IDENT, "OptionInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionString", 12, 0};
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionFloat", 11, 0};
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionBool", 10, 0};
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            fn_type->fn_type.return_type = sym ? sym->type : builtin_int;
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            // Resolve Result<int, string> -> ResultInt, Result<string, string> -> ResultString
                            Token concrete = {TOKEN_IDENT, "ResultInt", 9, 0};
                            Type* struct_res = NULL;
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                // Resolve E once, up front: it decides whether a PRIMITIVE
                                // ok payload may use the builtin family at all. A primitive
                                // ok with a non-string E must get its own monomorphic family
                                // — the builtin's err_value is hardcoded `const char*`, so
                                // reusing it silently discards E (uncompilable C for a struct
                                // E, a segfault for a scalar E).
                                Type* err_t = NULL;
                                if (fn->return_type->call.arg_count > 1 &&
                                    fn->return_type->call.args[1]->type == EXPR_IDENT) {
                                    Token en = fn->return_type->call.args[1]->token;
                                    if (en.length == 6 && memcmp(en.start, "string", 6) == 0) err_t = builtin_string;
                                    else if (en.length == 3 && memcmp(en.start, "int", 3) == 0) err_t = builtin_int;
                                    else if (en.length == 5 && memcmp(en.start, "float", 5) == 0) err_t = builtin_float;
                                    else if (en.length == 4 && memcmp(en.start, "bool", 4) == 0) err_t = builtin_bool;
                                    else {
                                        Symbol* es = find_symbol(global_scope, en);
                                        if (es && es->type) err_t = es->type;
                                    }
                                }
                                int err_is_str = (!err_t || err_t->kind == TYPE_STRING);
                                Type* prim_ok = NULL;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0) {
                                    concrete = (Token){TOKEN_IDENT, "ResultString", 12, 0};
                                    prim_ok = builtin_string;
                                } else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0) {
                                    concrete = (Token){TOKEN_IDENT, "ResultFloat", 11, 0};
                                    prim_ok = builtin_float;
                                } else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0) {
                                    concrete = (Token){TOKEN_IDENT, "ResultBool", 10, 0};
                                    prim_ok = builtin_bool;
                                } else if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0) {
                                    prim_ok = builtin_int;   // `concrete` already ResultInt
                                }
                                if (prim_ok) {
                                    // Primitive ok: builtin family for a string E (unchanged),
                                    // own `Result<Tag>_<ErrTag>` family otherwise.
                                    if (!err_is_str)
                                        struct_res = register_result_struct_family_e(prim_ok, err_t);
                                } else {
                                    // `Result<Struct, E>` - resolve the user struct and
                                    // make its monomorphic Result<Struct, E> family the
                                    // signature type (mirrors the `-> Struct?` path). The
                                    // error type E is resolved too (string/scalar/struct)
                                    // so the family carries the real err C type.
                                    Symbol* st = find_symbol(global_scope, inner);
                                    if (st && st->type && st->type->kind == TYPE_STRUCT)
                                        struct_res = register_result_struct_family_e(st->type, err_t);
                                }
                            }
                            if (struct_res) {
                                fn_type->fn_type.return_type = struct_res;
                            } else {
                                Symbol* sym = find_symbol(global_scope, concrete);
                                fn_type->fn_type.return_type = sym ? sym->type : builtin_int;
                            }
                        }
                    }
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    // `-> int?` / `-> string?` sugar → the concrete Option struct,
                    // registered on the fn signature so callers and match on the
                    // result see the Option family (not plain int).
                    Expr* inner = fn->return_type->optional_type.inner_type;
                    Token concrete = {TOKEN_IDENT, "OptionInt", 9, 0};
                    Type* struct_opt = NULL;
                    if (inner && inner->type == EXPR_IDENT) {
                        if (inner->token.length == 6 && memcmp(inner->token.start, "string", 6) == 0)
                            concrete = (Token){TOKEN_IDENT, "OptionString", 12, 0};
                        else if (inner->token.length == 5 && memcmp(inner->token.start, "float", 5) == 0)
                            concrete = (Token){TOKEN_IDENT, "OptionFloat", 11, 0};
                        else if (inner->token.length == 4 && memcmp(inner->token.start, "bool", 4) == 0)
                            concrete = (Token){TOKEN_IDENT, "OptionBool", 10, 0};
                        else if (inner->token.length != 3 || memcmp(inner->token.start, "int", 3) != 0) {
                            // `-> Struct?` - resolve the user struct and make its
                            // monomorphic Option<Struct> family the signature type. A
                            // DATA-carrying enum takes the same path (it is a C struct);
                            // register_option_struct_family() returns NULL for a PLAIN
                            // enum, which correctly falls through to OptionInt below.
                            Symbol* st = find_symbol(global_scope, inner->token);
                            if (st && st->type)
                                struct_opt = register_option_struct_family(st->type);
                        }
                    }
                    if (struct_opt) {
                        fn_type->fn_type.return_type = struct_opt;
                    } else {
                        Symbol* sym = find_symbol(global_scope, concrete);
                        fn_type->fn_type.return_type = sym ? sym->type : builtin_int;
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int], [string] or [S].
                    //
                    // This used to inline a four-way int/string/float/bool chain, so a
                    // STRUCT or ENUM element fell through and left element_type NULL:
                    //
                    //     fn make() -> [S] { ... }
                    //     for s in make() { print(s.name) }    // s was `long long`
                    //
                    // `wyn check` passed and the C compiler then rejected the program
                    // with "member reference base type 'long long' is not a structure
                    // or union" - so returning an array of structs, the natural way to
                    // structure any program with records in it, was unusable. Adding an
                    // explicit `var xs: [S] = make()` worked, because the ANNOTATION
                    // path already used the resolver below; that asymmetry is what made
                    // this read as a rule about annotations rather than a gap here.
                    //
                    // resolve_array_elem_annotation() is the same resolver the
                    // annotated-variable and parameter paths use, and it also handles
                    // nested arrays ([[float]]) and map elements, so those come along
                    // rather than needing their own arms later.
                    Type* array_type = make_type(TYPE_ARRAY);
                    if (fn->return_type->array.count > 0 && fn->return_type->array.elements[0]) {
                        array_type->array_type.element_type =
                            resolve_array_elem_annotation(fn->return_type->array.elements[0]);
                    }
                    fn_type->fn_type.return_type = array_type;
                } else if (fn->return_type->type == EXPR_FN_TYPE) {
                    // S3: `-> fn(T) -> R` - register the function type so a
                    // closure-holding var (s = make_scaler(3.0)) carries its
                    // real signature and the call site uses the right ABI.
                    Type* rt = check_expr(fn->return_type, global_scope);
                    if (rt) fn_type->fn_type.return_type = rt;
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
        Stmt* fn_stmt = prog->stmts[i];
        // Unwrap export
        if (fn_stmt->type == STMT_EXPORT && fn_stmt->export.stmt && fn_stmt->export.stmt->type == STMT_FN) {
            fn_stmt = fn_stmt->export.stmt;
        }
        if (fn_stmt->type == STMT_FN) {
            SymbolTable local_scope = {0};
            local_scope.parent = global_scope;
            local_scope.capacity = 32;
            local_scope.symbols = calloc(32, sizeof(Symbol));
            
            FnStmt* fn = &fn_stmt->fn;
            
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
                            if (fn->return_type->call.arg_count >= 1)
                                current_function_return_type->map_type.key_type =
                                    resolve_array_elem_annotation(fn->return_type->call.args[0]);
                            if (fn->return_type->call.arg_count >= 2)
                                current_function_return_type->map_type.value_type =
                                    resolve_array_elem_annotation(fn->return_type->call.args[1]);
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            current_function_return_type = make_type(TYPE_SET);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            Token concrete = {TOKEN_IDENT, "OptionInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionString", 12, 0};
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionFloat", 11, 0};
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    concrete = (Token){TOKEN_IDENT, "OptionBool", 10, 0};
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            current_function_return_type = sym ? sym->type : make_type(TYPE_OPTIONAL);
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            Token concrete = {TOKEN_IDENT, "ResultInt", 9, 0};
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    concrete = (Token){TOKEN_IDENT, "ResultString", 12, 0};
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    concrete = (Token){TOKEN_IDENT, "ResultFloat", 11, 0};
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    concrete = (Token){TOKEN_IDENT, "ResultBool", 10, 0};
                            }
                            Symbol* sym = find_symbol(global_scope, concrete);
                            current_function_return_type = sym ? sym->type : make_type(TYPE_RESULT);
                        } else if (wyn_is_generic_struct(type_name)) {
                            // A generic fn returning its OWN generic struct, e.g.
                            // `fn wrap<T>(x: T) -> Box<T>`. The body returns a
                            // monomorphic Box_int (TYPE_STRUCT), so make the declared
                            // return a TYPE_STRUCT too - otherwise it defaulted to int
                            // and the return-stmt check false-rejected with
                            // "Return type mismatch. Expected int, got Box_int".
                            current_function_return_type = make_type(TYPE_STRUCT);
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
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    // `-> int?` / `-> string?` sugar → the concrete Option struct,
                    // so returning Some(..)/None() type-checks (via the
                    // TYPE_OPTIONAL/struct allowance below). EXPR_OPTIONAL_TYPE's
                    // own checker already maps int?→OptionInt, string?→OptionString.
                    Type* ot = check_expr(fn->return_type, &local_scope);
                    if (ot) current_function_return_type = ot;
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
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // `b: Struct?` / `b: int?` - resolve to the Option family so
                        // the fn signature (used for call validation) matches Some/None
                        // arguments instead of defaulting to int.
                        Type* ot = check_expr(fn->param_types[j], &local_scope);
                        if (ot) param_type = ot;
                    } else if (fn->param_types[j]->type == EXPR_FN_TYPE) {
                        // S3: `f: fn(float) -> float` - the param was defaulting to
                        // int inside the body, so `return f(v)` in an fn -> float
                        // was rejected with "Return type mismatch ... got int".
                        Type* ft = check_expr(fn->param_types[j], &local_scope);
                        if (ft) param_type = ft;
                    } else if (fn->param_types[j]->type == EXPR_CALL &&
                               fn->param_types[j]->call.callee &&
                               fn->param_types[j]->call.callee->type == EXPR_IDENT &&
                               fn->param_types[j]->call.callee->token.length == 7 &&
                               memcmp(fn->param_types[j]->call.callee->token.start, "HashMap", 7) == 0) {
                        // Map parameter `m: {string: int}` / `HashMap<K, V>`: the
                        // param defaulted to int inside the body, so `m[k]` was
                        // rejected with "Array index must be int".
                        Type* mt = make_type(TYPE_MAP);
                        if (fn->param_types[j]->call.arg_count >= 1)
                            mt->map_type.key_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[0]);
                        if (fn->param_types[j]->call.arg_count >= 2)
                            mt->map_type.value_type =
                                resolve_array_elem_annotation(fn->param_types[j]->call.args[1]);
                        param_type = mt;
                    }
                }

                // T1.5.2: Type check default parameter values
                if (fn->param_defaults && fn->param_defaults[j]) {
                    Type* default_type = check_expr(fn->param_defaults[j], &local_scope);
                    if (default_type && !types_equal(param_type, default_type)) {
                        char param_name[256];
                        token_to_cstr(param_name, sizeof(param_name), fn->params[j]);
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
                    // Also synthesize an AST return-type node so codegen (which
                    // reads fn->return_type) emits the right C signature - this
                    // is what makes `fn f(x: int) => x * 2` work without `-> T`.
                    const char* tn = NULL;
                    switch (inferred_return->kind) {
                        case TYPE_INT:    tn = "int"; break;
                        case TYPE_FLOAT:  tn = "float"; break;
                        case TYPE_BOOL:   tn = "bool"; break;
                        case TYPE_STRING: tn = "string"; break;
                        case TYPE_STRUCT:
                            if (inferred_return->struct_type.name.length > 0) {
                                Expr* rt = calloc(1, sizeof(Expr));
                                rt->type = EXPR_IDENT;
                                rt->token = inferred_return->struct_type.name;
                                rt->expr_type = inferred_return;
                                fn->return_type = rt;
                            }
                            break;
                        default: break;
                    }
                    if (tn) {
                        Expr* rt = calloc(1, sizeof(Expr));
                        rt->type = EXPR_IDENT;
                        Token t; t.type = TOKEN_IDENT; t.start = tn;
                        t.length = (int)strlen(tn); t.line = fn->name.line;
                        rt->token = t;
                        rt->expr_type = inferred_return;
                        fn->return_type = rt;
                    }
                }
            }
            
            // Set self type for extension methods
            if (fn->is_extension) {
                Symbol* recv = find_symbol(global_scope, fn->receiver_type);
                current_self_type = (recv && recv->type) ? recv->type : NULL;
            }
            
            check_stmt(fn->body, &local_scope);
            
            // Error on missing return in non-void functions
            if (current_function_return_type && current_function_return_type->kind != TYPE_VOID &&
                !(fn->name.length == 4 && memcmp(fn->name.start, "main", 4) == 0)) {
                bool has_return = false;
                if (fn->body && fn->body->type == STMT_BLOCK) {
                    for (int j = 0; j < fn->body->block.count; j++) {
                        Stmt* st = fn->body->block.stmts[j];
                        // A top-level return, or any statement that guarantees a
                        // return on all paths (e.g. an exhaustive if/else or a
                        // fully-covered match), satisfies the requirement.
                        if (st && (st->type == STMT_RETURN || stmt_guarantees_return(st)))
                            has_return = true;
                    }
                    // Implicit return: last statement is an expression
                    if (!has_return && fn->body->block.count > 0) {
                        Stmt* last = fn->body->block.stmts[fn->body->block.count - 1];
                        if (last && last->type == STMT_EXPR) has_return = true;
                    }
                }
                if (!has_return) {
                    // Suppress for generators (use yield, not return)
                    bool _has_yield = false;
                    if (fn->body && fn->body->type == STMT_BLOCK) {
                        for (int j = 0; j < fn->body->block.count && !_has_yield; j++) {
                            Stmt* s = fn->body->block.stmts[j];
                            if (!s) continue;
                            if (s->type == STMT_YIELD) _has_yield = true;
                            if (s->type == STMT_FOR && s->for_stmt.body) {
                                Stmt* fb = s->for_stmt.body;
                                if (fb->type == STMT_YIELD) _has_yield = true;
                                if (fb->type == STMT_BLOCK) for (int k = 0; k < fb->block.count; k++)
                                    if (fb->block.stmts[k] && fb->block.stmts[k]->type == STMT_YIELD) _has_yield = true;
                            }
                            if (s->type == STMT_WHILE && s->while_stmt.body) {
                                Stmt* wb = s->while_stmt.body;
                                if (wb->type == STMT_YIELD) _has_yield = true;
                                if (wb->type == STMT_BLOCK) for (int k = 0; k < wb->block.count; k++)
                                    if (wb->block.stmts[k] && wb->block.stmts[k]->type == STMT_YIELD) _has_yield = true;
                            }
                        }
                    }
                    if (!_has_yield) {
                        fprintf(stderr, "\033[31m\033[1mError:\033[0m function '%.*s' may not return a value (line %d)\n",
                            fn->name.length, fn->name.start, fn->name.line);
                        had_error = true;
                    }
                }
            }
            
            current_function_return_type = NULL;
            current_self_type = NULL;
            
            // Warn about unused variables
            for (int j = fn->param_count; j < local_scope.count; j++) {
                Symbol* s = &local_scope.symbols[j];
                if (!s->is_used && s->name.length > 0 && s->name.start[0] != '_') {
                    fprintf(stderr, "\033[33mWarning:\033[0m unused variable '%.*s' (line %d)\n",
                        s->name.length, s->name.start, s->name.line);
                }
            }
            
            free(local_scope.symbols);
        } else {
            check_stmt(prog->stmts[i], global_scope);
        }
    }

    // Data-race soundness gate: a mutable global written from a function that
    // runs concurrently is an unsynchronized read-modify-write, i.e. a silent
    // wrong answer at exit 0. Reject it and point at Shared/channels.
    check_shared_mutable_globals(prog);
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
// S1 (lambda rework): detect whether a lambda parameter named `pname` is used as a
// STRING anywhere in `e` - i.e. it participates in string concat (`+` with a string
// operand) or is the receiver of a string method. Used to infer the param's type so
// string-parameter lambdas type-check (rather than failing with a bare "Type
// mismatch"). Conservative: returns false unless there's positive evidence.
static int expr_is_string_literal_or_typed(Expr* e, SymbolTable* scope) {
    if (!e) return 0;
    if (e->type == EXPR_STRING || e->type == EXPR_STRING_INTERP) return 1;
    if (e->expr_type && e->expr_type->kind == TYPE_STRING) return 1;
    // Inference runs BEFORE the body is checked, so a captured outer variable
    // has no expr_type yet - resolve identifiers through the enclosing scope
    // (a string capture used to give no evidence, so `(s) => s + suffix`
    // inferred s as int and emitted pointer garbage).
    if (e->type == EXPR_IDENT && scope) {
        Symbol* sym = find_symbol(scope, e->token);
        if (sym && sym->type && sym->type->kind == TYPE_STRING) return 1;
    }
    return 0;
}
// S3: same idea for floats - a param that participates in arithmetic or a
// comparison with a float operand is a float ((x) => x * 2.5). Conservative:
// no evidence means "not float", the int default stays.
static int expr_is_float_literal_or_typed(Expr* e, SymbolTable* scope) {
    if (!e) return 0;
    if (e->type == EXPR_FLOAT) return 1;
    if (e->expr_type && e->expr_type->kind == TYPE_FLOAT) return 1;
    if (e->type == EXPR_IDENT && scope) {
        Symbol* sym = find_symbol(scope, e->token);
        if (sym && sym->type && sym->type->kind == TYPE_FLOAT) return 1;
    }
    return 0;
}
static int lambda_param_is_float(const char* pname, Expr* e, SymbolTable* scope) {
    if (!e || !pname) return 0;
    switch (e->type) {
        case EXPR_BINARY: {
            WynTokenType op = e->binary.op.type;
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
                op == TOKEN_SLASH || op == TOKEN_LT || op == TOKEN_GT ||
                op == TOKEN_LTEQ || op == TOKEN_GTEQ ||
                op == TOKEN_EQEQ || op == TOKEN_BANGEQ) {
                Expr* l = e->binary.left; Expr* r = e->binary.right;
                int l_is_p = (l && l->type == EXPR_IDENT && l->token.length == (int)strlen(pname) &&
                              memcmp(l->token.start, pname, l->token.length) == 0);
                int r_is_p = (r && r->type == EXPR_IDENT && r->token.length == (int)strlen(pname) &&
                              memcmp(r->token.start, pname, r->token.length) == 0);
                if (l_is_p && expr_is_float_literal_or_typed(r, scope)) return 1;
                if (r_is_p && expr_is_float_literal_or_typed(l, scope)) return 1;
            }
            return lambda_param_is_float(pname, e->binary.left, scope) ||
                   lambda_param_is_float(pname, e->binary.right, scope);
        }
        case EXPR_CALL:
            for (int i = 0; i < e->call.arg_count; i++)
                if (lambda_param_is_float(pname, e->call.args[i], scope)) return 1;
            return 0;
        case EXPR_METHOD_CALL:
            if (lambda_param_is_float(pname, e->method_call.object, scope)) return 1;
            for (int i = 0; i < e->method_call.arg_count; i++)
                if (lambda_param_is_float(pname, e->method_call.args[i], scope)) return 1;
            return 0;
        case EXPR_IF_EXPR:
            return lambda_param_is_float(pname, e->if_expr.condition, scope) ||
                   lambda_param_is_float(pname, e->if_expr.then_expr, scope) ||
                   lambda_param_is_float(pname, e->if_expr.else_expr, scope);
        case EXPR_UNARY:
            return lambda_param_is_float(pname, e->unary.operand, scope);
        default:
            return 0;
    }
}
// S3: bool evidence - the param is negated with `not`, or compared with a
// bool literal, or is directly a logical (and/or) operand.
static int lambda_param_is_bool(const char* pname, Expr* e, SymbolTable* scope) {
    if (!e || !pname) return 0;
    switch (e->type) {
        case EXPR_UNARY: {
            Expr* o = e->unary.operand;
            if (e->unary.op.type == TOKEN_NOT && o && o->type == EXPR_IDENT &&
                o->token.length == (int)strlen(pname) &&
                memcmp(o->token.start, pname, o->token.length) == 0) return 1;
            return lambda_param_is_bool(pname, o, scope);
        }
        case EXPR_BINARY: {
            WynTokenType op = e->binary.op.type;
            Expr* l = e->binary.left; Expr* r = e->binary.right;
            int l_is_p = (l && l->type == EXPR_IDENT && l->token.length == (int)strlen(pname) &&
                          memcmp(l->token.start, pname, l->token.length) == 0);
            int r_is_p = (r && r->type == EXPR_IDENT && r->token.length == (int)strlen(pname) &&
                          memcmp(r->token.start, pname, r->token.length) == 0);
            if (op == TOKEN_AND || op == TOKEN_OR) {
                if (l_is_p || r_is_p) return 1;
            }
            if (op == TOKEN_EQEQ || op == TOKEN_BANGEQ) {
                if (l_is_p && r && r->type == EXPR_BOOL) return 1;
                if (r_is_p && l && l->type == EXPR_BOOL) return 1;
            }
            return lambda_param_is_bool(pname, l, scope) ||
                   lambda_param_is_bool(pname, r, scope);
        }
        case EXPR_CALL:
            for (int i = 0; i < e->call.arg_count; i++)
                if (lambda_param_is_bool(pname, e->call.args[i], scope)) return 1;
            return 0;
        case EXPR_IF_EXPR:
            return lambda_param_is_bool(pname, e->if_expr.condition, scope) ||
                   lambda_param_is_bool(pname, e->if_expr.then_expr, scope) ||
                   lambda_param_is_bool(pname, e->if_expr.else_expr, scope);
        default:
            return 0;
    }
}
static int lambda_param_is_string(const char* pname, Expr* e, SymbolTable* scope) {
    if (!e || !pname) return 0;
    switch (e->type) {
        case EXPR_BINARY: {
            // `param + <string>` or `<string> + param` → param is a string
            if (e->binary.op.type == TOKEN_PLUS) {
                Expr* l = e->binary.left; Expr* r = e->binary.right;
                int l_is_p = (l && l->type == EXPR_IDENT && l->token.length == (int)strlen(pname) &&
                              memcmp(l->token.start, pname, l->token.length) == 0);
                int r_is_p = (r && r->type == EXPR_IDENT && r->token.length == (int)strlen(pname) &&
                              memcmp(r->token.start, pname, r->token.length) == 0);
                if (l_is_p && expr_is_string_literal_or_typed(r, scope)) return 1;
                if (r_is_p && expr_is_string_literal_or_typed(l, scope)) return 1;
            }
            return lambda_param_is_string(pname, e->binary.left, scope) ||
                   lambda_param_is_string(pname, e->binary.right, scope);
        }
        case EXPR_METHOD_CALL: {
            // `param.<string-method>(...)` → param is a string. Recognize the common
            // string methods (the ones the mini-emitter/string runtime expose).
            Expr* obj = e->method_call.object;
            if (obj && obj->type == EXPR_IDENT && obj->token.length == (int)strlen(pname) &&
                memcmp(obj->token.start, pname, obj->token.length) == 0) {
                Token m = e->method_call.method;
                const char* sm[] = {"to_upper","to_lower","upper","lower","trim","replace",
                    "repeat","substring","split","contains","starts_with","ends_with",
                    "len","chars","index_of","pad_left","pad_right","capitalize", NULL};
                for (int i = 0; sm[i]; i++)
                    if ((int)strlen(sm[i]) == m.length && memcmp(sm[i], m.start, m.length) == 0)
                        return 1;
            }
            if (lambda_param_is_string(pname, e->method_call.object, scope)) return 1;
            for (int i = 0; i < e->method_call.arg_count; i++)
                if (lambda_param_is_string(pname, e->method_call.args[i], scope)) return 1;
            return 0;
        }
        case EXPR_CALL:
            for (int i = 0; i < e->call.arg_count; i++)
                if (lambda_param_is_string(pname, e->call.args[i], scope)) return 1;
            return 0;
        case EXPR_IF_EXPR:
            return lambda_param_is_string(pname, e->if_expr.condition, scope) ||
                   lambda_param_is_string(pname, e->if_expr.then_expr, scope) ||
                   lambda_param_is_string(pname, e->if_expr.else_expr, scope);
        case EXPR_UNARY:
            return lambda_param_is_string(pname, e->unary.operand, scope);
        default:
            return 0;
    }
}

void analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope) {
    if (!lambda || !body || !scope) return;
    
    // Simple capture analysis - find free variables in lambda body
    // This is a simplified implementation that captures identifiers not in parameters
    
    // Initialize capture arrays
    lambda->captured_vars = malloc(sizeof(Token) * 8);
    lambda->capture_by_move = malloc(sizeof(bool) * 8);
    lambda->captured_types = malloc(sizeof(Type*) * 8);
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
                    // Record the capture's real type so codegen can emit the
                    // right capture-cell C type (a string capture in a
                    // long long cell produced pointer-address output).
                    Symbol* cap_sym = find_symbol(scope, expr->token);
                    lambda->captured_types[lambda->captured_count] =
                        cap_sym ? cap_sym->type : NULL;
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
        case EXPR_STRING_INTERP:
            // "${n}" segments inside a lambda body capture outer vars too -
            // skipping them left the capture untyped (and unemitted).
            for (int i = 0; i < expr->string_interp.count; i++) {
                analyze_expr_captures(expr->string_interp.expressions[i], lambda, scope);
            }
            break;
        case EXPR_METHOD_CALL:
            analyze_expr_captures(expr->method_call.object, lambda, scope);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                analyze_expr_captures(expr->method_call.args[i], lambda, scope);
            }
            break;
        // Add more cases as needed for other expression types
        default:
            break;
    }
}
