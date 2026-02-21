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

static LLVMTypeRef i64_type(void) { return LLVMInt64TypeInContext(ctx); }
static LLVMTypeRef i1_type(void) { return LLVMInt1TypeInContext(ctx); }
static LLVMTypeRef i8ptr_type(void) { return LLVMPointerTypeInContext(ctx, 0); }
static LLVMValueRef i64_const(long long v) { return LLVMConstInt(i64_type(), v, 1); }

#define MAX_VARS 256
#define VAR_INT 0
#define VAR_PTR 1
static struct { char name[64]; LLVMValueRef alloca; int kind; } vars[MAX_VARS];
static int var_count = 0;

static LLVMValueRef lookup_var(const char* name) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) {
            LLVMTypeRef lt = vars[i].kind == VAR_PTR ? i8ptr_type() : i64_type();
            return LLVMBuildLoad2(builder, lt, vars[i].alloca, name);
        }
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
    LLVMTypeRef lt = kind == VAR_PTR ? i8ptr_type() : i64_type();
    LLVMValueRef a = LLVMBuildAlloca(tmp, lt, name);
    LLVMDisposeBuilder(tmp);
    snprintf(vars[var_count].name, 64, "%s", name);
    vars[var_count].alloca = a;
    vars[var_count].kind = kind;
    var_count++;
    LLVMBuildStore(builder, val, a);
}
static void set_var(const char* name, LLVMValueRef val) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) {
            LLVMBuildStore(builder, val, vars[i].alloca);
            return;
        }
    // Determine kind from LLVM type
    int kind = (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) ? VAR_PTR : VAR_INT;
    create_var(name, val, kind);
}

static long long parse_int_token(Token t) {
    char buf[64]; int len = t.length < 63 ? t.length : 63;
    memcpy(buf, t.start, len); buf[len] = 0;
    return strtoll(buf, NULL, 0);
}

static LLVMValueRef to_i64(LLVMValueRef v) {
    if (LLVMTypeOf(v) == i1_type()) return LLVMBuildZExt(builder, v, i64_type(), "ext");
    if (LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind)
        return LLVMBuildPtrToInt(builder, v, i64_type(), "ptoi");
    return v;
}
static int is_ptr_val(LLVMValueRef v) {
    return LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind;
}
static LLVMValueRef to_bool(LLVMValueRef v) {
    if (LLVMTypeOf(v) != i1_type())
        return LLVMBuildICmp(builder, LLVMIntNE, v, LLVMConstInt(LLVMTypeOf(v), 0, 0), "tobool");
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
        l = to_i64(l); r = to_i64(r);
        const char* op = e->binary.op.start; int ol = e->binary.op.length;
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
        if (!fn) { fprintf(stderr, "LLVM: undefined fn '%s'\n", fn_name); return i64_const(0); }
        int argc = e->call.arg_count; if (argc > 16) argc = 16;
        LLVMValueRef args[16];
        for (int i = 0; i < argc; i++) args[i] = to_i64(llvm_expr(e->call.args[i]));
        return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, argc, "call");
    }
    case EXPR_METHOD_CALL: {
        char method[64]; snprintf(method, sizeof(method), "%.*s", (int)e->method_call.method.length, e->method_call.method.start);
        LLVMValueRef obj = llvm_expr(e->method_call.object);

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
            fprintf(stderr, "LLVM: unknown method '%s'\n", method);
            return i64_const(0);
        }
        // Build args: obj first, then method args
        LLVMValueRef args[16]; args[0] = obj;
        int argc = e->method_call.arg_count; if (argc > 14) argc = 14;
        for (int i = 0; i < argc; i++) args[1 + i] = llvm_expr(e->method_call.args[i]);
        return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, 1 + argc, "mcall");
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
    case STMT_RETURN:
        LLVMBuildRet(builder, to_i64(llvm_expr(s->ret.value)));
        break;
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
    default:
        fprintf(stderr, "LLVM: unhandled stmt %d\n", s->type);
        break;
    }
}

static void llvm_emit_function(Stmt* fn_stmt) {
    char name[256];
    snprintf(name, sizeof(name), "%.*s", (int)fn_stmt->fn.name.length, fn_stmt->fn.name.start);
    int is_main = (fn_stmt->fn.name.length == 4 && memcmp(fn_stmt->fn.name.start, "main", 4) == 0);
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
        create_var(pn, p, VAR_INT);  // All params are i64 for now
    }

    llvm_stmt(fn_stmt->fn.body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
        LLVMBuildRet(builder, i64_const(0));

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

    // Forward declare all functions
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            Stmt* fn = prog->stmts[i];
            char n[256]; snprintf(n, sizeof(n), "%.*s", (int)fn->fn.name.length, fn->fn.name.start);
            int is_main = (fn->fn.name.length == 4 && memcmp(fn->fn.name.start, "main", 4) == 0);
            int pc = fn->fn.param_count;
            LLVMTypeRef* pt = malloc(sizeof(LLVMTypeRef) * (pc ? pc : 1));
            for (int j = 0; j < pc; j++) pt[j] = i64_type();
            LLVMTypeRef ft = LLVMFunctionType(i64_type(), pt, pc, 0);
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

    // Link with wyn_wrapper + runtime
    char link[2048];
    char rt_lib[512]; snprintf(rt_lib, sizeof(rt_lib), "%s/runtime/libwyn_rt.a", wyn_root);
    // Check if runtime library exists
    if (access(rt_lib, R_OK) == 0) {
        snprintf(link, sizeof(link),
            "cc -o %s %s %s/src/wyn_wrapper.c %s/src/wyn_interface.c "
            "-I %s/src %s -lpthread -lm",
            output_path, obj, wyn_root, wyn_root, wyn_root, rt_lib);
    } else {
        // Compile from source
        snprintf(link, sizeof(link),
            "cc -O2 -o %s %s %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/wyn_arena.c "
            "%s/src/stdlib_string.c %s/src/spawn_fast.c %s/src/future.c -I %s/src -lpthread -lm",
            output_path, obj, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
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
