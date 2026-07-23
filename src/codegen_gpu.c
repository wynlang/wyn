// codegen_gpu.c - GPU dispatch spike: eligibility check + MSL kernel emission.
// Included from codegen.c - shares all statics (emit, lambda tables).
//
// Scope (see internal-docs/GPU_DESIGN.md): when the project's wyn.toml has
// [gpu] enabled = true, an ELIGIBLE [float].map(lambda) call site compiles to
// a dual path - try the Metal runtime (wyn_gpu_try_map_float, cost model
// inside), fall back to the existing wyn_array_map_float. With the flag off
// (the default, and always when no wyn.toml is present) codegen here is
// bypassed entirely and emitted C is byte-identical to before - the golden-C
// snapshot suite proves that.
//
// Eligibility (spike-minimal, deliberately conservative):
//   - single-parameter, single-expression lambda (no body statements)
//   - body is pure arithmetic: int/float literals, the parameter itself,
//     binary + - * /, unary minus
//   - every identifier IS the parameter (this alone rules out captures,
//     function calls, methods, strings, allocation, and I/O)
// Anything else falls through to the normal CPU-only emission.

// Flag set from wyn.toml [gpu] by main.c before codegen runs.
static bool gpu_dispatch_enabled = false;
void codegen_set_gpu_enabled(bool on) { gpu_dispatch_enabled = on; }

// How many dual-path dispatch sites this compilation emitted. main.c links
// Metal (gpu_metal.o, -DWYN_GPU_METAL, -framework Metal) only when > 0, so
// GPU-flagged projects with no eligible call site link exactly as before.
static int gpu_dispatch_sites = 0;
int codegen_gpu_dispatch_count(void) { return gpu_dispatch_sites; }

// Arithmetic-only AST walk. `param` is the lambda's single parameter token.
static bool gpu_expr_eligible(Expr* e, Token param) {
    if (!e) return false;
    switch (e->type) {
        case EXPR_INT:
        case EXPR_FLOAT:
            return true;
        case EXPR_IDENT:
            return e->token.length == param.length &&
                   memcmp(e->token.start, param.start, param.length) == 0;
        case EXPR_BINARY: {
            WynTokenType op = e->binary.op.type;
            if (op != TOKEN_PLUS && op != TOKEN_MINUS &&
                op != TOKEN_STAR && op != TOKEN_SLASH) return false;
            return gpu_expr_eligible(e->binary.left, param) &&
                   gpu_expr_eligible(e->binary.right, param);
        }
        case EXPR_UNARY:
            if (e->unary.op.type != TOKEN_MINUS) return false;
            return gpu_expr_eligible(e->unary.operand, param);
        default:
            return false;
    }
}

static bool gpu_lambda_eligible(Expr* lam) {
    if (!lam || lam->type != EXPR_LAMBDA) return false;
    if (lam->lambda.param_count != 1) return false;
    if (lam->lambda.body_stmt_count != 0) return false;
    if (!lam->lambda.body) return false;
    return gpu_expr_eligible(lam->lambda.body, lam->lambda.params[0]);
}

// Append the lambda body as MSL (float32) into buf. The allowed node set
// contains no quotes or backslashes, so the result is safe to splice into a
// C string literal as-is. Int literals become float literals (MSL int*float
// is legal but this keeps the whole kernel in one type). Float literals get
// the f suffix so nothing promotes to (unsupported) double.
static void gpu_msl_expr(Expr* e, char* buf, size_t bufsz) {
    size_t len = strlen(buf);
    char* p = buf + len;
    size_t rem = bufsz - len;
    if (rem < 64) return;   // truncation guard; eligibility keeps bodies small
    switch (e->type) {
        case EXPR_INT:
            snprintf(p, rem, "%.*s.0f", e->token.length, e->token.start);
            break;
        case EXPR_FLOAT:
            snprintf(p, rem, "%.*sf", e->token.length, e->token.start);
            break;
        case EXPR_IDENT:
            snprintf(p, rem, "%.*s", e->token.length, e->token.start);
            break;
        case EXPR_BINARY: {
            const char* op = e->binary.op.type == TOKEN_PLUS  ? "+"
                           : e->binary.op.type == TOKEN_MINUS ? "-"
                           : e->binary.op.type == TOKEN_STAR  ? "*" : "/";
            snprintf(p, rem, "(");
            gpu_msl_expr(e->binary.left, buf, bufsz);
            len = strlen(buf);
            snprintf(buf + len, bufsz - len, " %s ", op);
            gpu_msl_expr(e->binary.right, buf, bufsz);
            len = strlen(buf);
            snprintf(buf + len, bufsz - len, ")");
            break;
        }
        case EXPR_UNARY:
            snprintf(p, rem, "(-");
            gpu_msl_expr(e->unary.operand, buf, bufsz);
            len = strlen(buf);
            snprintf(buf + len, bufsz - len, ")");
            break;
        default:
            break;   // unreachable: gpu_expr_eligible filtered these out
    }
}

// Build the full MSL kernel source for an eligible lambda. Returns false if
// the body would not fit. The kernel is a C string literal in the generated
// C, so newlines are written as the two characters backslash-n.
static bool gpu_msl_kernel(Expr* lam, char* out, size_t outsz) {
    char body[2048]; body[0] = '\0';
    gpu_msl_expr(lam->lambda.body, body, sizeof(body));
    if (body[0] == '\0') return false;
    Token param = lam->lambda.params[0];
    int n = snprintf(out, outsz,
        "#include <metal_stdlib>\\nusing namespace metal;\\n"
        "kernel void wyn_map_kernel(device const float* in [[buffer(0)]], "
        "device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) "
        "{ float %.*s = in[i]; out[i] = %s; }",
        param.length, param.start, body);
    return n > 0 && (size_t)n < outsz;
}

// Called from the [float].map(...) branch in codegen_expr.c. Returns true if
// it emitted the dual-path dispatch (caller then skips normal emission).
static bool gpu_try_emit_map_dispatch(Expr* expr) {
    if (!gpu_dispatch_enabled) return false;
    Expr* lam = expr->method_call.args[0];
    if (!gpu_lambda_eligible(lam)) return false;
    // Result element type must stay float: reject lambdas the checker typed
    // as returning int (those route to wyn_array_map_float_to_int anyway).
    Type* lam_ret = NULL;
    if (lam->expr_type && lam->expr_type->kind == TYPE_FUNCTION)
        lam_ret = lam->expr_type->fn_type.return_type;
    if (lam_ret && lam_ret->kind != TYPE_FLOAT) return false;
    char kernel[4096];
    if (!gpu_msl_kernel(lam, kernel, sizeof(kernel))) return false;

    emit("({ WynArray __gpu_src = ");
    codegen_expr(expr->method_call.object);
    emit("; WynArray __gpu_res; if (!wyn_gpu_try_map_float(__gpu_src, \"%s\", &__gpu_res)) __gpu_res = wyn_array_map_float(__gpu_src, ", kernel);
    codegen_expr(lam);
    emit("); __gpu_res; })");
    gpu_dispatch_sites++;
    return true;
}
