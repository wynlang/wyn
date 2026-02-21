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

#define MAX_VARS 256
static struct { char name[64]; LLVMValueRef val; } vars[MAX_VARS];
static int var_count = 0;

static LLVMValueRef lookup_var(const char* name) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return vars[i].val;
    return NULL;
}
static void set_var(const char* name, LLVMValueRef val) {
    for (int i = var_count - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) { vars[i].val = val; return; }
    snprintf(vars[var_count].name, 64, "%s", name);
    vars[var_count].val = val;
    var_count++;
}

static long long parse_int_token(Token t) {
    char buf[64]; int len = t.length < 63 ? t.length : 63;
    memcpy(buf, t.start, len); buf[len] = 0;
    return strtoll(buf, NULL, 0);
}

static LLVMTypeRef i64_type(void) { return LLVMInt64TypeInContext(ctx); }
static LLVMTypeRef i1_type(void) { return LLVMInt1TypeInContext(ctx); }
static LLVMValueRef i64_const(long long v) { return LLVMConstInt(i64_type(), v, 1); }

static LLVMValueRef to_i64(LLVMValueRef v) {
    if (LLVMTypeOf(v) == i1_type()) return LLVMBuildZExt(builder, v, i64_type(), "ext");
    return v;
}
static LLVMValueRef to_bool(LLVMValueRef v) {
    if (LLVMTypeOf(v) != i1_type())
        return LLVMBuildICmp(builder, LLVMIntNE, v, LLVMConstInt(LLVMTypeOf(v), 0, 0), "tobool");
    return v;
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
        LLVMValueRef fn = LLVMGetNamedFunction(mod, fn_name);
        if (!fn) { fprintf(stderr, "LLVM: undefined fn '%s'\n", fn_name); return i64_const(0); }
        int argc = e->call.arg_count; if (argc > 16) argc = 16;
        LLVMValueRef args[16];
        for (int i = 0; i < argc; i++) args[i] = to_i64(llvm_expr(e->call.args[i]));
        return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, argc, "call");
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
        set_var(n, to_i64(llvm_expr(s->var.init)));
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
        // for i in start..end { body }
        char var_name[256];
        snprintf(var_name, sizeof(var_name), "%.*s", (int)s->for_stmt.loop_var.length, s->for_stmt.loop_var.start);

        LLVMValueRef start_val = i64_const(0), end_val = i64_const(0);
        if (s->for_stmt.array_expr && s->for_stmt.array_expr->type == EXPR_RANGE) {
            start_val = to_i64(llvm_expr(s->for_stmt.array_expr->range.start));
            end_val = to_i64(llvm_expr(s->for_stmt.array_expr->range.end));
        }

        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.body");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.inc");
        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(ctx, fn, "for.end");

        // Init: set loop var
        set_var(var_name, start_val);
        LLVMBuildBr(builder, cond_bb);

        // Cond
        LLVMPositionBuilderAtEnd(builder, cond_bb);
        LLVMValueRef cur = lookup_var(var_name);
        LLVMValueRef cmp = LLVMBuildICmp(builder, LLVMIntSLT, cur, end_val, "for.cmp");
        LLVMBuildCondBr(builder, cmp, body_bb, end_bb);

        // Body
        LLVMPositionBuilderAtEnd(builder, body_bb);
        llvm_stmt(s->for_stmt.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
            LLVMBuildBr(builder, inc_bb);

        // Inc
        LLVMPositionBuilderAtEnd(builder, inc_bb);
        LLVMValueRef next = LLVMBuildAdd(builder, lookup_var(var_name), i64_const(1), "inc");
        set_var(var_name, next);
        LLVMBuildBr(builder, cond_bb);

        LLVMPositionBuilderAtEnd(builder, end_bb);
        break;
    }
    case STMT_EXPR:
        llvm_expr(s->expr);
        break;
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
        set_var(pn, p);
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
            "%s/src/stdlib_string.c -I %s/src -lpthread -lm",
            output_path, obj, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
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
