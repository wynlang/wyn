// LLVM backend for Wyn — emits LLVM IR instead of C
// Used by: wyn build --release --llvm
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"

// === Module state ===
static LLVMContextRef ctx;
static LLVMModuleRef mod;
static LLVMBuilderRef builder;

// Struct type registry
#define MAX_STRUCTS 64
static struct {
    char name[64];
    LLVMTypeRef type;
    Token* fields;
    int field_count;
} structs[MAX_STRUCTS];
static int struct_count = 0;

static int find_struct(const char* name) {
    for (int i = 0; i < struct_count; i++)
        if (strcmp(structs[i].name, name) == 0) return i;
    return -1;
}
static int find_field_index(int si, const char* field, int flen) {
    for (int i = 0; i < structs[si].field_count; i++)
        if (structs[si].fields[i].length == flen &&
            memcmp(structs[si].fields[i].start, field, flen) == 0) return i;
    return -1;
}

// Enum registry
#define MAX_ENUMS 64
static struct { char name[64]; Token* variants; int variant_count; } enums[MAX_ENUMS];
static int enum_count = 0;

static int find_enum_variant(const char* enum_name, const char* variant, int vlen) {
    for (int i = 0; i < enum_count; i++) {
        if (strcmp(enums[i].name, enum_name) != 0) continue;
        for (int j = 0; j < enums[i].variant_count; j++)
            if (enums[i].variants[j].length == vlen &&
                memcmp(enums[i].variants[j].start, variant, vlen) == 0) return j;
    }
    return -1;
}

static LLVMTypeRef i64_type(void) { return LLVMInt64TypeInContext(ctx); }
static LLVMTypeRef i1_type(void) { return LLVMInt1TypeInContext(ctx); }
static LLVMTypeRef i8ptr_type(void) { return LLVMPointerTypeInContext(ctx, 0); }
static LLVMValueRef i64_const(long long v) { return LLVMConstInt(i64_type(), v, 1); }

#define MAX_VARS 256
#define VAR_INT 0
#define VAR_PTR 1
#define VAR_STRUCT 2
static struct { char name[64]; LLVMValueRef alloca; int kind; LLVMTypeRef type; } vars[MAX_VARS];
static int var_count = 0;

static LLVMValueRef lookup_var(const char* name) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0)
            return LLVMBuildLoad2(builder, vars[i].type, vars[i].alloca, name);
    return NULL;
}
static LLVMValueRef get_var_alloca(const char* name) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return vars[i].alloca;
    return NULL;
}
static int get_var_kind(const char* name) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return vars[i].kind;
    return VAR_INT;
}
static void create_var(const char* name, LLVMValueRef val, int kind) {
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(fn);
    LLVMBuilderRef tmp = LLVMCreateBuilderInContext(ctx);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);
    if (first) LLVMPositionBuilderBefore(tmp, first);
    else LLVMPositionBuilderAtEnd(tmp, entry);
    LLVMTypeRef lt = LLVMTypeOf(val);  // Use actual value type
    LLVMValueRef a = LLVMBuildAlloca(tmp, lt, name);
    LLVMDisposeBuilder(tmp);
    snprintf(vars[var_count].name, 64, "%s", name);
    vars[var_count].alloca = a;
    vars[var_count].kind = kind;
    vars[var_count].type = lt;
    var_count++;
    LLVMBuildStore(builder, val, a);
}
static void set_var(const char* name, LLVMValueRef val) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) {
            LLVMBuildStore(builder, val, vars[i].alloca);
            return;
        }
    int kind = (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) ? VAR_PTR : VAR_INT;
    // Check if it's a struct type
    if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind) kind = VAR_STRUCT;
    create_var(name, val, kind);
}

static long long parse_int_token(Token t) {
    char buf[64]; int len = t.length < 63 ? t.length : 63;
    memcpy(buf, t.start, len); buf[len] = 0;
    return strtoll(buf, NULL, 0);
}

static LLVMValueRef to_i64(LLVMValueRef v) {
    if (LLVMTypeOf(v) == i1_type()) return LLVMBuildZExt(builder, v, i64_type(), "ext");
    if (LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(v)) < 64)
        return LLVMBuildSExt(builder, v, i64_type(), "sext");
    if (LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind)
        return LLVMBuildPtrToInt(builder, v, i64_type(), "ptoi");
    return v;
}
static int is_ptr_val(LLVMValueRef v) {
    return LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind;
}
static LLVMValueRef to_bool(LLVMValueRef v) {
    if (LLVMTypeOf(v) != i1_type()) {
        LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(v), 0, 0);
        return LLVMBuildICmp(builder, LLVMIntNE, v, zero, "tobool");
    }
    return v;
}


// Declare external C runtime functions
static void declare_runtime(void) {
    LLVMTypeRef i8p = i8ptr_type();
    LLVMTypeRef i64 = i64_type();
    LLVMTypeRef vd = LLVMVoidTypeInContext(ctx);

    // printf(fmt, ...) -> int
    LLVMTypeRef printf_args[] = {i8p};
    LLVMAddFunction(mod, "printf", LLVMFunctionType(LLVMInt32TypeInContext(ctx), printf_args, 1, 1));

    // puts(s) -> int
    LLVMTypeRef puts_args[] = {i8p};
    LLVMAddFunction(mod, "puts", LLVMFunctionType(LLVMInt32TypeInContext(ctx), puts_args, 1, 0));

    // sprintf(buf, fmt, ...) -> int
    LLVMTypeRef sprintf_args[] = {i8p, i8p};
    LLVMAddFunction(mod, "sprintf", LLVMFunctionType(LLVMInt32TypeInContext(ctx), sprintf_args, 2, 1));

    // snprintf(buf, n, fmt, ...) -> int
    LLVMTypeRef snprintf_args[] = {i8p, i64, i8p};
    LLVMAddFunction(mod, "snprintf", LLVMFunctionType(LLVMInt32TypeInContext(ctx), snprintf_args, 3, 1));

    // wyn_strdup(s) -> char*
    LLVMTypeRef strdup_args[] = {i8p};
    LLVMAddFunction(mod, "wyn_strdup", LLVMFunctionType(i8p, strdup_args, 1, 0));

    // wyn_spawn_fast(func, arg) -> void
    LLVMTypeRef spawn_args[] = {i8p, i8p};
    LLVMAddFunction(mod, "wyn_spawn_fast", LLVMFunctionType(vd, spawn_args, 2, 0));

    // wyn_spawn_async(func, arg) -> Future*
    LLVMAddFunction(mod, "wyn_spawn_async", LLVMFunctionType(i8p, spawn_args, 2, 0));

    // wyn_spawn_wait() -> void
    LLVMAddFunction(mod, "wyn_spawn_wait", LLVMFunctionType(vd, NULL, 0, 0));

    // string_upper(s) -> char*
    LLVMAddFunction(mod, "string_upper", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMAddFunction(mod, "string_lower", LLVMFunctionType(i8p, strdup_args, 1, 0));

    // string_contains(s, sub) -> int
    LLVMTypeRef str2_args[] = {i8p, i8p};
    LLVMAddFunction(mod, "string_contains", LLVMFunctionType(LLVMInt32TypeInContext(ctx), str2_args, 2, 0));

    // string_replace(s, old, new) -> char*
    LLVMTypeRef str3_args[] = {i8p, i8p, i8p};
    LLVMAddFunction(mod, "string_replace", LLVMFunctionType(i8p, str3_args, 3, 0));

    // strlen(s) -> i64
    LLVMAddFunction(mod, "strlen", LLVMFunctionType(i64, strdup_args, 1, 0));

    // Result/Option types and functions
    LLVMTypeRef result_fields[] = {LLVMInt32TypeInContext(ctx), i64};  // tag + value/ptr
    LLVMTypeRef result_type = LLVMStructCreateNamed(ctx, "ResultInt");
    LLVMStructSetBody(result_type, result_fields, 2, 0);
    LLVMTypeRef option_fields[] = {LLVMInt32TypeInContext(ctx), LLVMInt32TypeInContext(ctx)};
    LLVMTypeRef option_type = LLVMStructCreateNamed(ctx, "OptionInt");
    LLVMStructSetBody(option_type, option_fields, 2, 0);

    LLVMTypeRef ri_int[] = {i64};
    LLVMAddFunction(mod, "ResultInt_Ok", LLVMFunctionType(result_type, ri_int, 1, 0));
    LLVMTypeRef ri_str[] = {i8p};
    LLVMAddFunction(mod, "ResultInt_Err", LLVMFunctionType(result_type, ri_str, 1, 0));
    LLVMTypeRef ri_r[] = {result_type};
    LLVMAddFunction(mod, "ResultInt_is_ok", LLVMFunctionType(LLVMInt32TypeInContext(ctx), ri_r, 1, 0));
    LLVMAddFunction(mod, "ResultInt_is_err", LLVMFunctionType(LLVMInt32TypeInContext(ctx), ri_r, 1, 0));
    LLVMAddFunction(mod, "ResultInt_unwrap", LLVMFunctionType(i64, ri_r, 1, 0));
    LLVMAddFunction(mod, "ResultInt_unwrap_err", LLVMFunctionType(i8p, ri_r, 1, 0));

    LLVMTypeRef oi_int[] = {i64};
    LLVMAddFunction(mod, "OptionInt_Some", LLVMFunctionType(option_type, oi_int, 1, 0));
    LLVMAddFunction(mod, "OptionInt_None", LLVMFunctionType(option_type, NULL, 0, 0));
    LLVMTypeRef oi_r[] = {option_type};
    LLVMAddFunction(mod, "OptionInt_is_some", LLVMFunctionType(LLVMInt32TypeInContext(ctx), oi_r, 1, 0));
    LLVMAddFunction(mod, "OptionInt_is_none", LLVMFunctionType(LLVMInt32TypeInContext(ctx), oi_r, 1, 0));
    LLVMAddFunction(mod, "OptionInt_unwrap", LLVMFunctionType(i64, oi_r, 1, 0));
    LLVMTypeRef oi_or[] = {option_type, i64};
    LLVMAddFunction(mod, "OptionInt_unwrap_or", LLVMFunctionType(i64, oi_or, 2, 0));
    LLVMTypeRef ri_or[] = {result_type, i64};
    LLVMAddFunction(mod, "ResultInt_unwrap_or", LLVMFunctionType(i64, ri_or, 2, 0));

    // ResultString variant
    LLVMTypeRef result_str_type = LLVMStructCreateNamed(ctx, "ResultString");
    LLVMTypeRef rs_fields[] = {LLVMInt32TypeInContext(ctx), i8p};
    LLVMStructSetBody(result_str_type, rs_fields, 2, 0);
    LLVMTypeRef rs_ok[] = {i8p};
    LLVMAddFunction(mod, "ResultString_Ok", LLVMFunctionType(result_str_type, rs_ok, 1, 0));
    LLVMAddFunction(mod, "ResultString_Err", LLVMFunctionType(result_str_type, rs_ok, 1, 0));
    LLVMTypeRef rs_r[] = {result_str_type};
    LLVMAddFunction(mod, "ResultString_is_ok", LLVMFunctionType(LLVMInt32TypeInContext(ctx), rs_r, 1, 0));
    LLVMAddFunction(mod, "ResultString_is_err", LLVMFunctionType(LLVMInt32TypeInContext(ctx), rs_r, 1, 0));
    LLVMAddFunction(mod, "ResultString_unwrap", LLVMFunctionType(i8p, rs_r, 1, 0));

    // WynArray support
    // WynValue = { i32 type, [4 x i8] padding, i64 data } = 16 bytes (matches C layout)
    LLVMTypeRef wyn_value_fields[] = {LLVMInt32TypeInContext(ctx), LLVMArrayType2(LLVMInt8TypeInContext(ctx), 4), i64};
    LLVMTypeRef wyn_value_type = LLVMStructCreateNamed(ctx, "WynValue");
    LLVMStructSetBody(wyn_value_type, wyn_value_fields, 3, 0);
    // WynArray = { WynValue* data, i32 count, i32 capacity }
    LLVMTypeRef wyn_array_fields[] = {i8p, LLVMInt32TypeInContext(ctx), LLVMInt32TypeInContext(ctx)};
    LLVMTypeRef wyn_array_type = LLVMStructCreateNamed(ctx, "WynArray");
    LLVMStructSetBody(wyn_array_type, wyn_array_fields, 3, 0);

    LLVMTypeRef wa_args[] = {wyn_array_type};
    LLVMAddFunction(mod, "array_new", LLVMFunctionType(wyn_array_type, NULL, 0, 0));
    LLVMAddFunction(mod, "array_len", LLVMFunctionType(LLVMInt32TypeInContext(ctx), wa_args, 1, 0));
    LLVMTypeRef wa_ptr_args[] = {i8p};  // WynArray*
    LLVMTypeRef wa_push_int[] = {i8p, i64};  // WynArray*, i64
    LLVMAddFunction(mod, "array_push_int", LLVMFunctionType(vd, wa_push_int, 2, 0));
    LLVMAddFunction(mod, "array_push_str", LLVMFunctionType(vd, (LLVMTypeRef[]){i8p, i8p}, 2, 0));
    LLVMAddFunction(mod, "array_push", LLVMFunctionType(vd, wa_push_int, 2, 0));
    LLVMAddFunction(mod, "array_pop_int", LLVMFunctionType(i64, wa_ptr_args, 1, 0));

    // Namespaced API functions
    LLVMAddFunction(mod, "File_read", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMAddFunction(mod, "File_write", LLVMFunctionType(vd, (LLVMTypeRef[]){i8p, i8p}, 2, 0));
    LLVMAddFunction(mod, "File_exists", LLVMFunctionType(LLVMInt32TypeInContext(ctx), strdup_args, 1, 0));
    LLVMAddFunction(mod, "File_delete", LLVMFunctionType(vd, strdup_args, 1, 0));
    LLVMAddFunction(mod, "Http_get", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMAddFunction(mod, "Http_post", LLVMFunctionType(i8p, str2_args, 2, 0));
    LLVMAddFunction(mod, "split_get", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0));
    LLVMAddFunction(mod, "DateTime_now", LLVMFunctionType(i64, NULL, 0, 0));
    LLVMAddFunction(mod, "System_exec", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMAddFunction(mod, "Math_abs", LLVMFunctionType(i64, ri_int, 1, 0));
    LLVMAddFunction(mod, "Math_sqrt", LLVMFunctionType(i64, ri_int, 1, 0));
    LLVMAddFunction(mod, "to_int", LLVMFunctionType(i64, strdup_args, 1, 0));

    // hashmap functions
    LLVMAddFunction(mod, "hashmap_new", LLVMFunctionType(i8p, NULL, 0, 0));
    LLVMTypeRef hm_insert_int[] = {i8p, i8p, i64};
    LLVMAddFunction(mod, "hashmap_insert_int", LLVMFunctionType(vd, hm_insert_int, 3, 0));
    LLVMTypeRef hm_get_int[] = {i8p, i8p};
    LLVMAddFunction(mod, "hashmap_get_int", LLVMFunctionType(i64, hm_get_int, 2, 0));
    LLVMTypeRef hm_insert_str[] = {i8p, i8p, i8p};
    LLVMAddFunction(mod, "hashmap_insert_string", LLVMFunctionType(vd, hm_insert_str, 3, 0));
    LLVMAddFunction(mod, "hashmap_get_string", LLVMFunctionType(i8p, hm_get_int, 2, 0));

    // file functions
    LLVMAddFunction(mod, "file_read", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMTypeRef file_write_args[] = {i8p, i8p};
    LLVMAddFunction(mod, "file_write", LLVMFunctionType(vd, file_write_args, 2, 0));
    LLVMAddFunction(mod, "file_exists", LLVMFunctionType(LLVMInt32TypeInContext(ctx), strdup_args, 1, 0));
    LLVMAddFunction(mod, "file_delete", LLVMFunctionType(vd, strdup_args, 1, 0));

    // string extras
    LLVMAddFunction(mod, "string_concat", LLVMFunctionType(i8p, str2_args, 2, 0));
    LLVMAddFunction(mod, "string_substring", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i64, i64}, 3, 0));
    LLVMAddFunction(mod, "string_split_get", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0));
    LLVMAddFunction(mod, "string_trim", LLVMFunctionType(i8p, strdup_args, 1, 0));
    LLVMAddFunction(mod, "string_starts_with", LLVMFunctionType(LLVMInt32TypeInContext(ctx), str2_args, 2, 0));
    LLVMAddFunction(mod, "string_ends_with", LLVMFunctionType(LLVMInt32TypeInContext(ctx), str2_args, 2, 0));
    LLVMAddFunction(mod, "string_index_of", LLVMFunctionType(i64, str2_args, 2, 0));
}

// Helper: emit a global string constant
static LLVMValueRef emit_string(const char* str, int len) {
    LLVMValueRef gs = LLVMBuildGlobalStringPtr(builder, str, "str");
    return gs;
}

// Forward declarations
static LLVMValueRef llvm_expr(Expr* e);
static void llvm_stmt(Stmt* s);

static LLVMValueRef llvm_expr(Expr* e) {
    if (!e) return i64_const(0);
    switch (e->type) {
    case EXPR_INT:
        return i64_const(parse_int_token(e->token));
    case EXPR_BOOL:
        return i64_const(e->token.length == 4 && memcmp(e->token.start, "true", 4) == 0 ? 1 : 0);
    case EXPR_STRING: {
        // Strip quotes from token
        int start = 1, end = e->token.length - 1;
        if (e->token.length >= 6 && e->token.start[0] == '"' && e->token.start[1] == '"') { start = 3; end = e->token.length - 3; }
        int len = end - start; if (len < 0) len = 0; if (len > 4094) len = 4094;
        char buf[4096]; memcpy(buf, e->token.start + start, len); buf[len] = 0;
        return emit_string(buf, len);
    }
    case EXPR_STRING_INTERP: {
        // Build format string and args for snprintf
        // Allocate a stack buffer, snprintf into it, then wyn_strdup
        char fmt[1024]; int fpos = 0;
        LLVMValueRef interp_args[32]; int iarg = 0;

        for (int i = 0; i < e->string_interp.count; i++) {
            if (e->string_interp.parts[i]) {
                const char* p = e->string_interp.parts[i];
                while (*p && fpos < 1020) {
                    if (*p == '%') { fmt[fpos++] = '%'; fmt[fpos++] = '%'; }
                    else fmt[fpos++] = *p;
                    p++;
                }
            } else if (e->string_interp.expressions[i]) {
                // Integer expression -> %lld
                fmt[fpos++] = '%'; fmt[fpos++] = 'l'; fmt[fpos++] = 'l'; fmt[fpos++] = 'd';
                interp_args[iarg++] = to_i64(llvm_expr(e->string_interp.expressions[i]));
            }
        }
        fmt[fpos] = 0;

        // snprintf into stack buffer, then wyn_strdup
        LLVMValueRef buf_alloca = LLVMBuildArrayAlloca(builder, LLVMInt8TypeInContext(ctx),
            LLVMConstInt(LLVMInt32TypeInContext(ctx), 512, 0), "buf");
        LLVMValueRef fmt_str = LLVMBuildGlobalStringPtr(builder, fmt, "fmt");

        // Build snprintf call: snprintf(buf, 512, fmt, args...)
        LLVMValueRef snprintf_fn = LLVMGetNamedFunction(mod, "snprintf");
        LLVMValueRef call_args[35];
        call_args[0] = buf_alloca;
        call_args[1] = i64_const(512);
        call_args[2] = fmt_str;
        for (int i = 0; i < iarg; i++) call_args[3 + i] = interp_args[i];

        LLVMTypeRef snprintf_type = LLVMGlobalGetValueType(snprintf_fn);
        LLVMBuildCall2(builder, snprintf_type, snprintf_fn, call_args, 3 + iarg, "");

        // wyn_strdup(buf)
        LLVMValueRef strdup_fn = LLVMGetNamedFunction(mod, "wyn_strdup");
        LLVMTypeRef strdup_type = LLVMGlobalGetValueType(strdup_fn);
        LLVMValueRef strdup_args[] = {buf_alloca};
        return LLVMBuildCall2(builder, strdup_type, strdup_fn, strdup_args, 1, "str");
    }
    case EXPR_IDENT: {
        char n[256]; snprintf(n, sizeof(n), "%.*s", (int)e->token.length, e->token.start);
        LLVMValueRef v = lookup_var(n);
        if (v) return v;
        LLVMValueRef fn = LLVMGetNamedFunction(mod, n);
        if (fn) return fn;
        fprintf(stderr, "LLVM: undefined '%s'\n", n);
        return i64_const(0);
    }
    case EXPR_BINARY: {
        LLVMValueRef l = llvm_expr(e->binary.left);
        LLVMValueRef r = llvm_expr(e->binary.right);
        const char* op = e->binary.op.start; int ol = e->binary.op.length;
        // String concatenation: if either side is a pointer and op is +
        if (ol==1 && op[0]=='+' && (is_ptr_val(l) || is_ptr_val(r))) {
            LLVMValueRef concat_fn = LLVMGetNamedFunction(mod, "string_concat");
            if (concat_fn) {
                // Convert int to string if needed
                if (!is_ptr_val(l)) {
                    LLVMValueRef buf = LLVMBuildArrayAlloca(builder, LLVMInt8TypeInContext(ctx), LLVMConstInt(LLVMInt32TypeInContext(ctx), 32, 0), "b");
                    LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld", "f");
                    LLVMValueRef sf = LLVMGetNamedFunction(mod, "snprintf");
                    LLVMValueRef sa[] = {buf, i64_const(32), fmt, to_i64(l)};
                    LLVMBuildCall2(builder, LLVMGlobalGetValueType(sf), sf, sa, 4, "");
                    l = buf;
                }
                if (!is_ptr_val(r)) {
                    LLVMValueRef buf = LLVMBuildArrayAlloca(builder, LLVMInt8TypeInContext(ctx), LLVMConstInt(LLVMInt32TypeInContext(ctx), 32, 0), "b");
                    LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld", "f");
                    LLVMValueRef sf = LLVMGetNamedFunction(mod, "snprintf");
                    LLVMValueRef sa[] = {buf, i64_const(32), fmt, to_i64(r)};
                    LLVMBuildCall2(builder, LLVMGlobalGetValueType(sf), sf, sa, 4, "");
                    r = buf;
                }
                LLVMValueRef args[] = {l, r};
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(concat_fn), concat_fn, args, 2, "concat");
            }
        }
        l = to_i64(l); r = to_i64(r);
        if (ol==1 && op[0]=='+') return LLVMBuildAdd(builder, l, r, "add");
        if (ol==1 && op[0]=='-') return LLVMBuildSub(builder, l, r, "sub");
        if (ol==1 && op[0]=='*') return LLVMBuildMul(builder, l, r, "mul");
        if (ol==1 && op[0]=='/') return LLVMBuildSDiv(builder, l, r, "div");
        if (ol==1 && op[0]=='%') return LLVMBuildSRem(builder, l, r, "mod");
        if (ol==2 && op[0]=='<' && op[1]=='=') return LLVMBuildICmp(builder, LLVMIntSLE, l, r, "le");
        if (ol==2 && op[0]=='>' && op[1]=='=') return LLVMBuildICmp(builder, LLVMIntSGE, l, r, "ge");
        if (ol==2 && op[0]=='=' && op[1]=='=') return LLVMBuildICmp(builder, LLVMIntEQ, l, r, "eq");
        if (ol==2 && op[0]=='!' && op[1]=='=') return LLVMBuildICmp(builder, LLVMIntNE, l, r, "ne");
        if (ol==1 && op[0]=='<') return LLVMBuildICmp(builder, LLVMIntSLT, l, r, "lt");
        if (ol==1 && op[0]=='>') return LLVMBuildICmp(builder, LLVMIntSGT, l, r, "gt");
        fprintf(stderr, "LLVM: unknown op '%.*s'\n", ol, op);
        return l;
    }
    case EXPR_CALL: {
        char fn_name[256];
        if (e->call.callee->type == EXPR_IDENT)
            snprintf(fn_name, sizeof(fn_name), "%.*s", (int)e->call.callee->token.length, e->call.callee->token.start);
        else { strcpy(fn_name, "__unknown"); }

        // Handle println/print as printf calls
        if (strcmp(fn_name, "println") == 0 && e->call.arg_count == 1) {
            LLVMValueRef arg = llvm_expr(e->call.args[0]);
            LLVMValueRef printf_fn = LLVMGetNamedFunction(mod, "printf");
            LLVMTypeRef printf_type = LLVMGlobalGetValueType(printf_fn);
            if (is_ptr_val(arg)) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%s\n", "fmt");
                LLVMValueRef args[] = {fmt, arg};
                return LLVMBuildCall2(builder, printf_type, printf_fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld\n", "fmt");
                LLVMValueRef args[] = {fmt, to_i64(arg)};
                return LLVMBuildCall2(builder, printf_type, printf_fn, args, 2, "");
            }
        }
        if (strcmp(fn_name, "print") == 0 && e->call.arg_count == 1) {
            LLVMValueRef arg = llvm_expr(e->call.args[0]);
            LLVMValueRef printf_fn = LLVMGetNamedFunction(mod, "printf");
            LLVMTypeRef printf_type = LLVMGlobalGetValueType(printf_fn);
            if (is_ptr_val(arg)) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%s", "fmt");
                LLVMValueRef args[] = {fmt, arg};
                return LLVMBuildCall2(builder, printf_type, printf_fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld", "fmt");
                LLVMValueRef args[] = {fmt, to_i64(arg)};
                return LLVMBuildCall2(builder, printf_type, printf_fn, args, 2, "");
            }
        }

        LLVMValueRef fn = LLVMGetNamedFunction(mod, fn_name);
        if (!fn) {
            // Try as function pointer variable (indirect call)
            LLVMValueRef fn_ptr = lookup_var(fn_name);
            if (fn_ptr && is_ptr_val(fn_ptr)) {
                int argc = e->call.arg_count; if (argc > 16) argc = 16;
                LLVMValueRef args[16];
                LLVMTypeRef param_types[16];
                for (int i = 0; i < argc; i++) {
                    args[i] = to_i64(llvm_expr(e->call.args[i]));
                    param_types[i] = i64_type();
                }
                LLVMTypeRef fn_type = LLVMFunctionType(i64_type(), param_types, argc, 0);
                return LLVMBuildCall2(builder, fn_type, fn_ptr, args, argc, "icall");
            }
            fprintf(stderr, "LLVM: undefined fn '%s'\n", fn_name);
            return i64_const(0);
        }
        int argc = e->call.arg_count; if (argc > 16) argc = 16;
        LLVMValueRef args[16];
        for (int i = 0; i < argc; i++) {
            LLVMValueRef a = llvm_expr(e->call.args[i]);
            // Check if param expects ptr (function pointer or struct)
            LLVMTypeRef param_type = LLVMTypeOf(LLVMGetParam(fn, i));
            if (LLVMGetTypeKind(param_type) == LLVMPointerTypeKind) {
                if (!is_ptr_val(a)) a = LLVMBuildIntToPtr(builder, a, i8ptr_type(), "toptr");
            } else {
                a = to_i64(a);
            }
            args[i] = a;
        }
        LLVMTypeRef fn_ret = LLVMGetReturnType(LLVMGlobalGetValueType(fn));
        const char* call_name = (LLVMGetTypeKind(fn_ret) == LLVMVoidTypeKind) ? "" : "call";
        return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, argc, call_name);
    }
    case EXPR_SPAWN: {
        // spawn func(arg) -> Future*
        if (e->spawn.call && e->spawn.call->type == EXPR_CALL) {
            Expr* call = e->spawn.call;
            char fn_name[256];
            if (call->call.callee->type == EXPR_IDENT)
                snprintf(fn_name, sizeof(fn_name), "%.*s", (int)call->call.callee->token.length, call->call.callee->token.start);
            else strcpy(fn_name, "__unknown");

            LLVMValueRef target_fn = LLVMGetNamedFunction(mod, fn_name);
            if (!target_fn) { fprintf(stderr, "LLVM: spawn: undefined fn '%s'\n", fn_name); return i64_const(0); }

            // Create wrapper if needed
            char wrap_name[300]; snprintf(wrap_name, sizeof(wrap_name), "__spawn_async_wrap_%s", fn_name);
            LLVMValueRef wrapper = LLVMGetNamedFunction(mod, wrap_name);
            if (!wrapper) {
                LLVMTypeRef wp[] = {i8ptr_type()};
                LLVMTypeRef wt = LLVMFunctionType(i8ptr_type(), wp, 1, 0);
                wrapper = LLVMAddFunction(mod, wrap_name, wt);
                LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(builder);
                LLVMBasicBlockRef we = LLVMAppendBasicBlockInContext(ctx, wrapper, "entry");
                LLVMPositionBuilderAtEnd(builder, we);
                LLVMValueRef raw = LLVMGetParam(wrapper, 0);
                LLVMValueRef int_arg = LLVMBuildPtrToInt(builder, raw, i64_type(), "arg");
                LLVMValueRef cargs[] = {int_arg};
                LLVMValueRef result = LLVMBuildCall2(builder, LLVMGlobalGetValueType(target_fn), target_fn, cargs, 1, "r");
                LLVMValueRef ret = LLVMBuildIntToPtr(builder, result, i8ptr_type(), "ret");
                LLVMBuildRet(builder, ret);
                LLVMPositionBuilderAtEnd(builder, saved_bb);
            }

            LLVMValueRef spawn_fn = LLVMGetNamedFunction(mod, "wyn_spawn_async");
            LLVMValueRef arg_val = (call->call.arg_count > 0)
                ? LLVMBuildIntToPtr(builder, to_i64(llvm_expr(call->call.args[0])), i8ptr_type(), "sarg")
                : LLVMConstNull(i8ptr_type());
            LLVMValueRef sargs[] = {LLVMBuildBitCast(builder, wrapper, i8ptr_type(), "wfn"), arg_val};
            return LLVMBuildCall2(builder, LLVMGlobalGetValueType(spawn_fn), spawn_fn, sargs, 2, "future");
        }
        return i64_const(0);
    }
    case EXPR_AWAIT: {
        // await future -> result (as i64 via intptr_t)
        LLVMValueRef future = llvm_expr(e->await.expr);
        // Declare future_get if not already
        LLVMValueRef fg = LLVMGetNamedFunction(mod, "future_get");
        if (!fg) {
            LLVMTypeRef fgp[] = {i8ptr_type()};
            LLVMTypeRef fgt = LLVMFunctionType(i8ptr_type(), fgp, 1, 0);
            fg = LLVMAddFunction(mod, "future_get", fgt);
        }
        LLVMValueRef fargs[] = {future};
        LLVMValueRef result = LLVMBuildCall2(builder, LLVMGlobalGetValueType(fg), fg, fargs, 1, "await");
        return LLVMBuildPtrToInt(builder, result, i64_type(), "val");
    }
    case EXPR_ARRAY: {
        // Create WynArray via runtime: array_new() + array_push_int() for each element
        LLVMTypeRef wa_type = LLVMGetTypeByName2(ctx, "WynArray");
        LLVMValueRef new_fn = LLVMGetNamedFunction(mod, "array_new");
        if (!new_fn || !wa_type) {
            // Fallback: stack array
            int count = e->array.count;
            LLVMTypeRef arr_type = LLVMArrayType2(i64_type(), count);
            LLVMValueRef alloca = LLVMBuildAlloca(builder, arr_type, "arr");
            for (int i = 0; i < count; i++) {
                LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0),
                                       LLVMConstInt(LLVMInt32TypeInContext(ctx), i, 0)};
                LLVMValueRef gep = LLVMBuildGEP2(builder, arr_type, alloca, idx, 2, "elem");
                LLVMBuildStore(builder, to_i64(llvm_expr(e->array.elements[i])), gep);
            }
            return alloca;
        }
        // Create WynArray: zero-initialize {null, 0, 0}
        LLVMValueRef arr_alloca = LLVMBuildAlloca(builder, wa_type, "arr_storage");
        LLVMValueRef zero_arr = LLVMConstNull(wa_type);
        LLVMBuildStore(builder, zero_arr, arr_alloca);
        // Push each element
        LLVMValueRef push_fn = LLVMGetNamedFunction(mod, "array_push_int");
        for (int i = 0; i < e->array.count; i++) {
            LLVMValueRef val = to_i64(llvm_expr(e->array.elements[i]));
            LLVMValueRef args[] = {arr_alloca, val};
            LLVMBuildCall2(builder, LLVMGlobalGetValueType(push_fn), push_fn, args, 2, "");
        }
        // Return the alloca (pointer to WynArray)
        return arr_alloca;
    }
    case EXPR_INDEX: {
        LLVMValueRef arr = llvm_expr(e->index.array);
        LLVMValueRef idx_val = to_i64(llvm_expr(e->index.index));
        if (is_ptr_val(arr)) {
            // Check if this is a WynArray or a string
            // If the variable was created from EXPR_ARRAY, it's a WynArray
            // If it's a string (from EXPR_STRING, EXPR_STRING_INTERP, etc.), do char indexing
            // Heuristic: check if the variable kind is VAR_STRUCT (WynArray alloca)
            char vname[256] = "";
            if (e->index.array->type == EXPR_IDENT)
                snprintf(vname, sizeof(vname), "%.*s", (int)e->index.array->token.length, e->index.array->token.start);
            int vkind = get_var_kind(vname);

            if (vkind == VAR_STRUCT) {
                // WynArray indexing
                LLVMTypeRef wa_type = LLVMGetTypeByName2(ctx, "WynArray");
                LLVMTypeRef wv_type = LLVMGetTypeByName2(ctx, "WynValue");
                if (wa_type && wv_type) {
                    LLVMValueRef data_gep = LLVMBuildStructGEP2(builder, wa_type, arr, 0, "data_ptr");
                    LLVMValueRef data = LLVMBuildLoad2(builder, i8ptr_type(), data_gep, "data");
                    LLVMValueRef elem_gep = LLVMBuildGEP2(builder, wv_type, data, &idx_val, 1, "elem");
                    LLVMValueRef val_gep = LLVMBuildStructGEP2(builder, wv_type, elem_gep, 2, "val");
                    return LLVMBuildLoad2(builder, i64_type(), val_gep, "elem_val");
                }
            }
            // String indexing: s[i] -> char at index
            // Use GEP on i8 array
            LLVMValueRef char_gep = LLVMBuildGEP2(builder, LLVMInt8TypeInContext(ctx), arr, &idx_val, 1, "char");
            LLVMValueRef ch = LLVMBuildLoad2(builder, LLVMInt8TypeInContext(ctx), char_gep, "ch");
            return LLVMBuildZExt(builder, ch, i64_type(), "char_ext");
        }
        // Stack array fallback
        LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0), idx_val};
        LLVMTypeRef arr_type = LLVMArrayType2(i64_type(), 256);
        LLVMValueRef gep = LLVMBuildGEP2(builder, arr_type, arr, idx, 2, "idx");
        return LLVMBuildLoad2(builder, i64_type(), gep, "elem");
    }
    case EXPR_STRUCT_INIT: {
        char sname[64]; snprintf(sname, sizeof(sname), "%.*s",
            (int)e->struct_init.type_name.length, e->struct_init.type_name.start);
        int si = find_struct(sname);
        if (si < 0) { fprintf(stderr, "LLVM: unknown struct '%s'\n", sname); return i64_const(0); }
        // Allocate struct on stack
        LLVMValueRef alloca = LLVMBuildAlloca(builder, structs[si].type, "struct");
        // Set fields
        for (int i = 0; i < e->struct_init.field_count; i++) {
            char fname[64]; snprintf(fname, sizeof(fname), "%.*s",
                (int)e->struct_init.field_names[i].length, e->struct_init.field_names[i].start);
            int fi = find_field_index(si, fname, strlen(fname));
            if (fi < 0) continue;
            LLVMValueRef val = to_i64(llvm_expr(e->struct_init.field_values[i]));
            LLVMValueRef gep = LLVMBuildStructGEP2(builder, structs[si].type, alloca, fi, fname);
            LLVMBuildStore(builder, val, gep);
        }
        return alloca;  // Return pointer to struct
    }
    case EXPR_FIELD_ACCESS: {
        char fname[64]; snprintf(fname, sizeof(fname), "%.*s",
            (int)e->field_access.field.length, e->field_access.field.start);

        // Check if this is an enum access (e.g., Color.Green)
        if (e->field_access.object->type == EXPR_IDENT) {
            char ename[64]; snprintf(ename, sizeof(ename), "%.*s",
                (int)e->field_access.object->token.length, e->field_access.object->token.start);
            int vi = find_enum_variant(ename, fname, strlen(fname));
            if (vi >= 0) return i64_const(vi);
        }

        // Struct field access
        LLVMValueRef obj = llvm_expr(e->field_access.object);
        // Try each struct type to find the field
        for (int si = 0; si < struct_count; si++) {
            int fi = find_field_index(si, fname, strlen(fname));
            if (fi >= 0) {
                LLVMValueRef gep = LLVMBuildStructGEP2(builder, structs[si].type, obj, fi, fname);
                return LLVMBuildLoad2(builder, i64_type(), gep, fname);
            }
        }
        fprintf(stderr, "LLVM: unknown field '%s'\n", fname);
        return i64_const(0);
    }
    case EXPR_METHOD_CALL: {
        char method[64]; snprintf(method, sizeof(method), "%.*s", (int)e->method_call.method.length, e->method_call.method.start);
        LLVMValueRef obj = llvm_expr(e->method_call.object);

        // Array methods: push, pop, len on WynArray pointers
        if (strcmp(method, "push") == 0 && is_ptr_val(obj)) {
            LLVMValueRef push_fn = LLVMGetNamedFunction(mod, "array_push_int");
            if (push_fn && e->method_call.arg_count > 0) {
                LLVMValueRef val = to_i64(llvm_expr(e->method_call.args[0]));
                LLVMValueRef args[] = {obj, val};
                LLVMBuildCall2(builder, LLVMGlobalGetValueType(push_fn), push_fn, args, 2, "");
                return i64_const(0);
            }
        }
        if (strcmp(method, "pop") == 0 && is_ptr_val(obj)) {
            LLVMValueRef pop_fn = LLVMGetNamedFunction(mod, "array_pop_int");
            if (pop_fn) {
                LLVMValueRef args[] = {obj};
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(pop_fn), pop_fn, args, 1, "pop");
            }
        }
        if (strcmp(method, "len") == 0 && is_ptr_val(obj)) {
            // Check if this is a WynArray (VAR_STRUCT) or a string (VAR_PTR)
            char vname[256] = "";
            if (e->method_call.object->type == EXPR_IDENT)
                snprintf(vname, sizeof(vname), "%.*s", (int)e->method_call.object->token.length, e->method_call.object->token.start);
            if (get_var_kind(vname) == VAR_STRUCT) {
                // WynArray.len
                LLVMTypeRef wa_type = LLVMGetTypeByName2(ctx, "WynArray");
                LLVMValueRef loaded = LLVMBuildLoad2(builder, wa_type, obj, "wa");
                LLVMValueRef len_fn = LLVMGetNamedFunction(mod, "array_len");
                if (len_fn) {
                    LLVMValueRef args[] = {loaded};
                    return LLVMBuildCall2(builder, LLVMGlobalGetValueType(len_fn), len_fn, args, 1, "len");
                }
            }
            // String.len -> strlen
            LLVMValueRef strlen_fn = LLVMGetNamedFunction(mod, "strlen");
            LLVMValueRef sargs[] = {obj};
            return LLVMBuildCall2(builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, sargs, 1, "len");
        }

        // Try namespaced API: File.read -> File_read, Http.get -> Http_get
        if (e->method_call.object->type == EXPR_IDENT) {
            char ns[64]; snprintf(ns, sizeof(ns), "%.*s",
                (int)e->method_call.object->token.length, e->method_call.object->token.start);
            char ns_fn[128]; snprintf(ns_fn, sizeof(ns_fn), "%s_%s", ns, method);
            LLVMValueRef nfn = LLVMGetNamedFunction(mod, ns_fn);
            if (nfn) {
                int argc = e->method_call.arg_count; if (argc > 15) argc = 15;
                LLVMValueRef args[16];
                for (int i = 0; i < argc; i++) {
                    args[i] = llvm_expr(e->method_call.args[i]);
                    if (!is_ptr_val(args[i])) args[i] = to_i64(args[i]);
                }
                LLVMTypeRef fn_ret = LLVMGetReturnType(LLVMGlobalGetValueType(nfn));
                const char* cn = (LLVMGetTypeKind(fn_ret) == LLVMVoidTypeKind) ? "" : "nscall";
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(nfn), nfn, args, argc, cn);
            }
        }

        // Try extension method: Type_method(self, args...)
        // Check all struct types for a matching function
        for (int si = 0; si < struct_count; si++) {
            char ext_name[128]; snprintf(ext_name, sizeof(ext_name), "%s_%s", structs[si].name, method);
            LLVMValueRef ext_fn = LLVMGetNamedFunction(mod, ext_name);
            if (ext_fn) {
                LLVMValueRef args[16]; args[0] = obj;
                int argc = e->method_call.arg_count; if (argc > 14) argc = 14;
                for (int i = 0; i < argc; i++) args[1 + i] = to_i64(llvm_expr(e->method_call.args[i]));
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(ext_fn), ext_fn, args, 1 + argc, "mcall");
            }
        }

        // Map method names to C runtime functions: string_<method>(obj, args...)
        char cfn_name[128]; snprintf(cfn_name, sizeof(cfn_name), "string_%s", method);
        LLVMValueRef fn = LLVMGetNamedFunction(mod, cfn_name);
        if (!fn) {
            // Try to_string, len, etc.
            if (strcmp(method, "to_string") == 0) {
                // int -> string via snprintf
                LLVMValueRef buf = LLVMBuildArrayAlloca(builder, LLVMInt8TypeInContext(ctx),
                    LLVMConstInt(LLVMInt32TypeInContext(ctx), 32, 0), "buf");
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld", "fmt");
                LLVMValueRef snprintf_fn = LLVMGetNamedFunction(mod, "snprintf");
                LLVMValueRef sargs[] = {buf, i64_const(32), fmt, to_i64(obj)};
                LLVMBuildCall2(builder, LLVMGlobalGetValueType(snprintf_fn), snprintf_fn, sargs, 4, "");
                LLVMValueRef strdup_fn = LLVMGetNamedFunction(mod, "wyn_strdup");
                LLVMValueRef dargs[] = {buf};
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(strdup_fn), strdup_fn, dargs, 1, "str");
            }
            if (strcmp(method, "len") == 0) {
                LLVMValueRef strlen_fn = LLVMGetNamedFunction(mod, "strlen");
                LLVMValueRef sargs[] = {obj};
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(strlen_fn), strlen_fn, sargs, 1, "len");
            }
            if (strcmp(method, "to_int") == 0) {
                LLVMValueRef fn = LLVMGetNamedFunction(mod, "to_int");
                if (fn) { LLVMValueRef a[] = {obj}; return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, a, 1, "toi"); }
            }
            if (strcmp(method, "push") == 0 || strcmp(method, "pop") == 0 ||
                strcmp(method, "split") == 0 || strcmp(method, "trim") == 0 ||
                strcmp(method, "starts_with") == 0 || strcmp(method, "ends_with") == 0 ||
                strcmp(method, "index_of") == 0 || strcmp(method, "concat") == 0 ||
                strcmp(method, "substring") == 0 || strcmp(method, "slice") == 0) {
                // Try string_<method> runtime function
                char rt_name[128]; snprintf(rt_name, sizeof(rt_name), "string_%s", method);
                LLVMValueRef rt_fn = LLVMGetNamedFunction(mod, rt_name);
                if (rt_fn) {
                    LLVMValueRef args[16]; args[0] = obj;
                    int argc = e->method_call.arg_count; if (argc > 14) argc = 14;
                    for (int i = 0; i < argc; i++) args[1+i] = llvm_expr(e->method_call.args[i]);
                    LLVMTypeRef fn_ret = LLVMGetReturnType(LLVMGlobalGetValueType(rt_fn));
                    const char* cn = (LLVMGetTypeKind(fn_ret) == LLVMVoidTypeKind) ? "" : "mcall";
                    return LLVMBuildCall2(builder, LLVMGlobalGetValueType(rt_fn), rt_fn, args, 1+argc, cn);
                }
            }
            fprintf(stderr, "LLVM: unknown method '%s'\n", method);
            return i64_const(0);
        }
        // Build args: obj first, then method args
        LLVMValueRef args[16]; args[0] = obj;
        int argc = e->method_call.arg_count; if (argc > 14) argc = 14;
        for (int i = 0; i < argc; i++) args[1 + i] = llvm_expr(e->method_call.args[i]);
        return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, 1 + argc, "mcall");
    }
    case EXPR_TERNARY: {
        LLVMValueRef cond = to_bool(llvm_expr(e->ternary.condition));
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(ctx, fn, "tern.then");
        LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(ctx, fn, "tern.else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx, fn, "tern.merge");
        LLVMBuildCondBr(builder, cond, then_bb, else_bb);
        LLVMPositionBuilderAtEnd(builder, then_bb);
        LLVMValueRef then_val = to_i64(llvm_expr(e->ternary.then_expr));
        LLVMBasicBlockRef then_end = LLVMGetInsertBlock(builder);
        LLVMBuildBr(builder, merge_bb);
        LLVMPositionBuilderAtEnd(builder, else_bb);
        LLVMValueRef else_val = to_i64(llvm_expr(e->ternary.else_expr));
        LLVMBasicBlockRef else_end = LLVMGetInsertBlock(builder);
        LLVMBuildBr(builder, merge_bb);
        LLVMPositionBuilderAtEnd(builder, merge_bb);
        LLVMValueRef phi = LLVMBuildPhi(builder, i64_type(), "tern");
        LLVMAddIncoming(phi, &then_val, &then_end, 1);
        LLVMAddIncoming(phi, &else_val, &else_end, 1);
        return phi;
    }
    case EXPR_SOME: {
        // Some(value) -> OptionInt_Some(value)
        LLVMValueRef val = to_i64(llvm_expr(e->option.value));
        LLVMValueRef fn = LLVMGetNamedFunction(mod, "OptionInt_Some");
        if (fn) { LLVMValueRef args[] = {val}; return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, 1, "some"); }
        return val;
    }
    case EXPR_NONE: {
        LLVMValueRef fn = LLVMGetNamedFunction(mod, "OptionInt_None");
        if (fn) return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, NULL, 0, "none");
        return i64_const(0);
    }
    case EXPR_OK: {
        LLVMValueRef val = to_i64(llvm_expr(e->option.value));
        LLVMValueRef fn = LLVMGetNamedFunction(mod, "ResultInt_Ok");
        if (fn) { LLVMValueRef args[] = {val}; return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, 1, "ok"); }
        return val;
    }
    case EXPR_ERR: {
        LLVMValueRef val = llvm_expr(e->option.value);
        LLVMValueRef fn = LLVMGetNamedFunction(mod, "ResultInt_Err");
        if (fn) { LLVMValueRef args[] = {val}; return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, 1, "err"); }
        return val;
    }
    case EXPR_UNARY: {
        LLVMValueRef operand = llvm_expr(e->unary.operand);
        if (e->unary.op.length == 1 && e->unary.op.start[0] == '-')
            return LLVMBuildNeg(builder, to_i64(operand), "neg");
        if (e->unary.op.length == 1 && e->unary.op.start[0] == '!') {
            LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(operand), 0, 0);
            return LLVMBuildICmp(builder, LLVMIntEQ, operand, zero, "not");
        }
        return operand;
    }
    case EXPR_ASSIGN: {
        char n[256]; snprintf(n, sizeof(n), "%.*s", (int)e->assign.name.length, e->assign.name.start);
        LLVMValueRef val = to_i64(llvm_expr(e->assign.value));
        set_var(n, val);
        return val;
    }
    default:
        fprintf(stderr, "LLVM: unhandled expr %d\n", e->type);
        return i64_const(0);
    }
}

static void llvm_stmt(Stmt* s) {
    if (!s) return;
    // Don't emit into terminated blocks
    if (LLVMGetInsertBlock(builder) && LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder))) return;

    switch (s->type) {
    case STMT_RETURN: {
        LLVMValueRef val = llvm_expr(s->ret.value);
        // Check if function returns ptr (struct or string)
        LLVMValueRef cur_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMTypeRef ret_type = LLVMGetReturnType(LLVMGlobalGetValueType(cur_fn));
        if (LLVMGetTypeKind(ret_type) == LLVMPointerTypeKind) {
            if (!is_ptr_val(val)) val = LLVMBuildIntToPtr(builder, val, i8ptr_type(), "toptr");
        } else {
            val = to_i64(val);
        }
        LLVMBuildRet(builder, val);
        break;
    }
    case STMT_BLOCK:
        for (int i = 0; i < s->block.count; i++) llvm_stmt(s->block.stmts[i]);
        break;
    case STMT_VAR: {
        char n[256]; snprintf(n, sizeof(n), "%.*s", (int)s->var.name.length, s->var.name.start);
        LLVMValueRef val = llvm_expr(s->var.init);
        // Don't convert pointers to i64 — preserve type
        if (!is_ptr_val(val)) val = to_i64(val);
        set_var(n, val);
        break;
    }
    case STMT_IF: {
        LLVMValueRef cond = to_bool(llvm_expr(s->if_stmt.condition));
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(ctx, fn, "then");
        LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(ctx, fn, "else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx, fn, "merge");
        LLVMBuildCondBr(builder, cond, then_bb, else_bb);

        LLVMPositionBuilderAtEnd(builder, then_bb);
        llvm_stmt(s->if_stmt.then_branch);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
            LLVMBuildBr(builder, merge_bb);

        LLVMPositionBuilderAtEnd(builder, else_bb);
        if (s->if_stmt.else_branch) llvm_stmt(s->if_stmt.else_branch);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
            LLVMBuildBr(builder, merge_bb);

        LLVMPositionBuilderAtEnd(builder, merge_bb);
        break;
    }
    case STMT_FOR: {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));

        if (s->for_stmt.init) {
            // C-style for loop: for (init; cond; inc) { body }
            // Emit init (var declaration)
            llvm_stmt(s->for_stmt.init);
        }

        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.body");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.inc");
        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.end");

        LLVMBuildBr(builder, cond_bb);

        // Condition
        LLVMPositionBuilderAtEnd(builder, cond_bb);
        if (s->for_stmt.condition) {
            LLVMValueRef cond = to_bool(llvm_expr(s->for_stmt.condition));
            LLVMBuildCondBr(builder, cond, body_bb, end_bb);
        } else {
            LLVMBuildBr(builder, body_bb);
        }

        // Body
        LLVMPositionBuilderAtEnd(builder, body_bb);
        llvm_stmt(s->for_stmt.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
            LLVMBuildBr(builder, inc_bb);

        // Increment
        LLVMPositionBuilderAtEnd(builder, inc_bb);
        if (s->for_stmt.increment)
            llvm_expr(s->for_stmt.increment);
        LLVMBuildBr(builder, cond_bb);

        LLVMPositionBuilderAtEnd(builder, end_bb);
        break;
    }
    case STMT_EXPR:
        llvm_expr(s->expr);
        break;
    case STMT_WHILE: {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.body");
        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.end");
        LLVMBuildBr(builder, cond_bb);
        LLVMPositionBuilderAtEnd(builder, cond_bb);
        LLVMValueRef cond = to_bool(llvm_expr(s->while_stmt.condition));
        LLVMBuildCondBr(builder, cond, body_bb, end_bb);
        LLVMPositionBuilderAtEnd(builder, body_bb);
        llvm_stmt(s->while_stmt.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
            LLVMBuildBr(builder, cond_bb);
        LLVMPositionBuilderAtEnd(builder, end_bb);
        break;
    }
    case STMT_SPAWN: {
        // Fire-and-forget spawn: spawn func(arg)
        // Generate: wyn_spawn_fast(wrapper, (void*)(intptr_t)arg)
        if (s->spawn.call && s->spawn.call->type == EXPR_CALL) {
            Expr* call = s->spawn.call;
            char fn_name[256];
            if (call->call.callee->type == EXPR_IDENT)
                snprintf(fn_name, sizeof(fn_name), "%.*s", (int)call->call.callee->token.length, call->call.callee->token.start);
            else strcpy(fn_name, "__unknown");

            // Get the target function
            LLVMValueRef target_fn = LLVMGetNamedFunction(mod, fn_name);
            if (!target_fn) { fprintf(stderr, "LLVM: spawn: undefined fn '%s'\n", fn_name); break; }

            // Create wrapper function if it doesn't exist
            char wrap_name[300]; snprintf(wrap_name, sizeof(wrap_name), "__spawn_wrap_%s", fn_name);
            LLVMValueRef wrapper = LLVMGetNamedFunction(mod, wrap_name);
            if (!wrapper) {
                LLVMTypeRef wrap_params[] = {i8ptr_type()};
                LLVMTypeRef wrap_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx), wrap_params, 1, 0);
                wrapper = LLVMAddFunction(mod, wrap_name, wrap_type);

                LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(builder);
                LLVMBasicBlockRef wrap_entry = LLVMAppendBasicBlockInContext(ctx, wrapper, "entry");
                LLVMPositionBuilderAtEnd(builder, wrap_entry);

                // Unpack arg: (intptr_t)arg
                LLVMValueRef raw_arg = LLVMGetParam(wrapper, 0);
                LLVMValueRef int_arg = LLVMBuildPtrToInt(builder, raw_arg, i64_type(), "arg");

                // Call target function
                LLVMValueRef call_args[] = {int_arg};
                LLVMBuildCall2(builder, LLVMGlobalGetValueType(target_fn), target_fn, call_args,
                    call->call.arg_count > 0 ? 1 : 0, "");
                LLVMBuildRetVoid(builder);

                LLVMPositionBuilderAtEnd(builder, saved_bb);
            }

            // Call wyn_spawn_fast(wrapper, (void*)(intptr_t)arg)
            LLVMValueRef spawn_fn = LLVMGetNamedFunction(mod, "wyn_spawn_fast");
            LLVMValueRef arg_val;
            if (call->call.arg_count > 0) {
                arg_val = LLVMBuildIntToPtr(builder, to_i64(llvm_expr(call->call.args[0])), i8ptr_type(), "sarg");
            } else {
                arg_val = LLVMConstNull(i8ptr_type());
            }
            LLVMValueRef spawn_args[] = {LLVMBuildBitCast(builder, wrapper, i8ptr_type(), "wfn"), arg_val};
            LLVMBuildCall2(builder, LLVMGlobalGetValueType(spawn_fn), spawn_fn, spawn_args, 2, "");
        }
        break;
    }
    case STMT_FN:
        break; // handled at program level
    case STMT_STRUCT:
        break; // handled at program level
    case STMT_ENUM:
        break; // handled at program level
    case STMT_CONST: {
        char n[256]; snprintf(n, sizeof(n), "%.*s", (int)s->const_stmt.name.length, s->const_stmt.name.start);
        LLVMValueRef val = llvm_expr(s->const_stmt.init);
        if (!is_ptr_val(val)) val = to_i64(val);
        set_var(n, val);
        break;
    }
    default:
        fprintf(stderr, "LLVM: unhandled stmt %d\n", s->type);
        break;
    }
}

static void llvm_emit_function(Stmt* fn_stmt) {
    char name[256];
    int is_main = (fn_stmt->fn.name.length == 4 && memcmp(fn_stmt->fn.name.start, "main", 4) == 0);
    if (fn_stmt->fn.is_extension) {
        snprintf(name, sizeof(name), "%.*s_%.*s",
            (int)fn_stmt->fn.receiver_type.length, fn_stmt->fn.receiver_type.start,
            (int)fn_stmt->fn.name.length, fn_stmt->fn.name.start);
    } else {
        snprintf(name, sizeof(name), "%.*s", (int)fn_stmt->fn.name.length, fn_stmt->fn.name.start);
    }
    const char* fn_name = is_main ? "wyn_main" : name;

    LLVMValueRef fn = LLVMGetNamedFunction(mod, fn_name);
    if (!fn) return;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    int saved = var_count;
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        char pn[256]; snprintf(pn, sizeof(pn), "%.*s", (int)fn_stmt->fn.params[i].length, fn_stmt->fn.params[i].start);
        LLVMValueRef p = LLVMGetParam(fn, i);
        LLVMSetValueName2(p, pn, strlen(pn));
        int kind = VAR_INT;
        if (i == 0 && fn_stmt->fn.is_extension) kind = VAR_PTR;
        else if (fn_stmt->fn.param_types && fn_stmt->fn.param_types[i] &&
                 fn_stmt->fn.param_types[i]->type == EXPR_FN_TYPE) kind = VAR_PTR;
        else if (fn_stmt->fn.param_types && fn_stmt->fn.param_types[i] &&
                 fn_stmt->fn.param_types[i]->type == EXPR_ARRAY) kind = VAR_PTR;
        else if (fn_stmt->fn.param_types && fn_stmt->fn.param_types[i] &&
                 fn_stmt->fn.param_types[i]->type == EXPR_IDENT) {
            char tname[64]; snprintf(tname, sizeof(tname), "%.*s",
                (int)fn_stmt->fn.param_types[i]->token.length, fn_stmt->fn.param_types[i]->token.start);
            if (find_struct(tname) >= 0) kind = VAR_PTR;
        }
        create_var(pn, p, kind);
    }

    llvm_stmt(fn_stmt->fn.body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder))) {
        LLVMTypeRef ret_type = LLVMGetReturnType(LLVMGlobalGetValueType(fn));
        if (LLVMGetTypeKind(ret_type) == LLVMPointerTypeKind)
            LLVMBuildRet(builder, LLVMConstNull(i8ptr_type()));
        else
            LLVMBuildRet(builder, i64_const(0));
    }

    var_count = saved;
}

// === Public API ===
int llvm_compile(Program* prog, const char* output_path, const char* wyn_root) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    ctx = LLVMContextCreate();
    mod = LLVMModuleCreateWithNameInContext("wyn", ctx);
    builder = LLVMCreateBuilderInContext(ctx);

    char* triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(mod, triple);

    // Declare external runtime functions
    declare_runtime();

    // Register enum types
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_ENUM) {
            Stmt* s = prog->stmts[i];
            snprintf(enums[enum_count].name, 64, "%.*s",
                (int)s->enum_decl.name.length, s->enum_decl.name.start);
            enums[enum_count].variants = s->enum_decl.variants;
            enums[enum_count].variant_count = s->enum_decl.variant_count;
            enum_count++;
        }
    }

    // Register struct types
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT) {
            Stmt* s = prog->stmts[i];
            char name[64]; snprintf(name, sizeof(name), "%.*s",
                (int)s->struct_decl.name.length, s->struct_decl.name.start);
            int fc = s->struct_decl.field_count;
            LLVMTypeRef* ftypes = malloc(sizeof(LLVMTypeRef) * (fc ? fc : 1));
            for (int j = 0; j < fc; j++) ftypes[j] = i64_type();
            LLVMTypeRef st = LLVMStructCreateNamed(ctx, name);
            LLVMStructSetBody(st, ftypes, fc, 0);
            free(ftypes);
            snprintf(structs[struct_count].name, 64, "%s", name);
            structs[struct_count].type = st;
            structs[struct_count].fields = s->struct_decl.fields;
            structs[struct_count].field_count = fc;
            struct_count++;
        }
    }

    // Forward declare all functions
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            Stmt* fn = prog->stmts[i];
            char n[256];
            int is_main = (fn->fn.name.length == 4 && memcmp(fn->fn.name.start, "main", 4) == 0);
            if (fn->fn.is_extension) {
                snprintf(n, sizeof(n), "%.*s_%.*s",
                    (int)fn->fn.receiver_type.length, fn->fn.receiver_type.start,
                    (int)fn->fn.name.length, fn->fn.name.start);
            } else {
                snprintf(n, sizeof(n), "%.*s", (int)fn->fn.name.length, fn->fn.name.start);
            }
            int pc = fn->fn.param_count;
            LLVMTypeRef* pt = malloc(sizeof(LLVMTypeRef) * (pc ? pc : 1));
            for (int j = 0; j < pc; j++) {
                if (j == 0 && fn->fn.is_extension) pt[j] = i8ptr_type();
                else if (fn->fn.param_types && fn->fn.param_types[j] &&
                         fn->fn.param_types[j]->type == EXPR_FN_TYPE) pt[j] = i8ptr_type();
                else if (fn->fn.param_types && fn->fn.param_types[j] &&
                         fn->fn.param_types[j]->type == EXPR_ARRAY) pt[j] = i8ptr_type();  // [int] -> WynArray*
                else if (fn->fn.param_types && fn->fn.param_types[j] &&
                         fn->fn.param_types[j]->type == EXPR_IDENT) {
                    char tname[64]; snprintf(tname, sizeof(tname), "%.*s",
                        (int)fn->fn.param_types[j]->token.length, fn->fn.param_types[j]->token.start);
                    pt[j] = (find_struct(tname) >= 0 || strcmp(tname, "string") == 0) ? i8ptr_type() : i64_type();
                }
                else pt[j] = i64_type();
            }
            // Determine return type
            LLVMTypeRef ret_type = i64_type();
            if (fn->fn.return_type && fn->fn.return_type->type == EXPR_IDENT) {
                char rname[64]; snprintf(rname, sizeof(rname), "%.*s",
                    (int)fn->fn.return_type->token.length, fn->fn.return_type->token.start);
                if (find_struct(rname) >= 0 || strcmp(rname, "string") == 0) ret_type = i8ptr_type();
                else if (strcmp(rname, "ResultInt") == 0) ret_type = LLVMGetTypeByName2(ctx, "ResultInt");
                else if (strcmp(rname, "OptionInt") == 0) ret_type = LLVMGetTypeByName2(ctx, "OptionInt");
            } else if (fn->fn.return_type && fn->fn.return_type->type == EXPR_CALL) {
                // Generic return type: Result<int, string> -> ResultInt, Option<int> -> OptionInt
                if (fn->fn.return_type->call.callee && fn->fn.return_type->call.callee->type == EXPR_IDENT) {
                    char rname[64]; snprintf(rname, sizeof(rname), "%.*s",
                        (int)fn->fn.return_type->call.callee->token.length, fn->fn.return_type->call.callee->token.start);
                    if (strcmp(rname, "Result") == 0) {
                        LLVMTypeRef t = LLVMGetTypeByName2(ctx, "ResultInt");
                        if (t) ret_type = t;
                    } else if (strcmp(rname, "Option") == 0) {
                        LLVMTypeRef t = LLVMGetTypeByName2(ctx, "OptionInt");
                        if (t) ret_type = t;
                    }
                }
            }
            LLVMTypeRef ft = LLVMFunctionType(ret_type, pt, pc, 0);
            LLVMAddFunction(mod, is_main ? "wyn_main" : n, ft);
            free(pt);
        }
    }

    // Emit all functions
    for (int i = 0; i < prog->count; i++)
        if (prog->stmts[i]->type == STMT_FN)
            llvm_emit_function(prog->stmts[i]);

    // Debug: print IR
    if (getenv("WYN_LLVM_DEBUG")) {
        char* ir = LLVMPrintModuleToString(mod);
        fprintf(stderr, "%s\n", ir);
        LLVMDisposeMessage(ir);
    }

    // Verify
    char* error = NULL;
    if (LLVMVerifyModule(mod, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "LLVM verify: %s\n", error);
        char* ir = LLVMPrintModuleToString(mod);
        fprintf(stderr, "%s\n", ir);
        LLVMDisposeMessage(ir);
        LLVMDisposeMessage(error);
        goto fail;
    }
    if (error) LLVMDisposeMessage(error);

    // Emit object file
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error)) {
        fprintf(stderr, "LLVM target: %s\n", error);
        LLVMDisposeMessage(error); goto fail;
    }

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple,
        LLVMGetHostCPUName(), LLVMGetHostCPUFeatures(),
        LLVMCodeGenLevelAggressive, LLVMRelocDefault, LLVMCodeModelDefault);

    // Run optimization passes (equivalent to -O3)
    LLVMPassBuilderOptionsRef pass_opts = LLVMCreatePassBuilderOptions();
    LLVMRunPasses(mod, "default<O3>", machine, pass_opts);
    LLVMDisposePassBuilderOptions(pass_opts);

    char obj[512]; snprintf(obj, sizeof(obj), "%s.o", output_path);
    if (LLVMTargetMachineEmitToFile(machine, mod, obj, LLVMObjectFile, &error)) {
        fprintf(stderr, "LLVM emit: %s\n", error);
        LLVMDisposeMessage(error); goto fail;
    }

    // Link with runtime library
    char link[2048];
    char rt_lib[512]; snprintf(rt_lib, sizeof(rt_lib), "%s/runtime/libwyn_rt.a", wyn_root);
    if (access(rt_lib, R_OK) == 0) {
        snprintf(link, sizeof(link),
            "cc -O2 -w -o %s %s %s -I %s/src -lpthread -lm",
            output_path, obj, rt_lib, wyn_root);
    } else {
        // Fallback: compile from source
        snprintf(link, sizeof(link),
            "cc -O2 -w -o %s %s %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/wyn_arena.c "
            "%s/src/stdlib_string.c %s/src/spawn_fast.c %s/src/future.c "
            "%s/src/hashmap.c %s/src/json.c "
            "-I %s/src -lpthread -lm",
            output_path, obj, wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root);
    }
    int result = system(link);
    unlink(obj);

    LLVMDisposeMessage(triple);
    LLVMDisposeTargetMachine(machine);
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(mod);
    LLVMContextDispose(ctx);
    return result == 0 ? 0 : 1;

fail:
    LLVMDisposeMessage(triple);
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(mod);
    LLVMContextDispose(ctx);
    return 1;
}
