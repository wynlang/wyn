// codegen_lambda.c - Lambda scanning and generation
// Included from codegen.c - shares all statics

static void scan_expr_for_lambdas(Expr* expr);

// Track array vars found during scan (for type-aware lambda captures) (growable)
static char (*scan_array_vars)[64] = NULL;
static int scan_array_var_count = 0;
static int scan_array_var_cap = 0;
static int is_scan_array_var(const char* name) {
    for (int i = 0; i < scan_array_var_count; i++)
        if (strcmp(scan_array_vars[i], name) == 0) return 1;
    return 0;
}

static void scan_stmt_for_lambdas(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_VAR:
            if (stmt->var.init) {
                scan_expr_for_lambdas(stmt->var.init);
                // Track array vars for type-aware captures
                if (stmt->var.init->type == EXPR_ARRAY) {
                    WYN_ENSURE_CAP(scan_array_vars, scan_array_var_count, scan_array_var_cap);
                    int len = stmt->var.name.length < 63 ? stmt->var.name.length : 63;
                    memcpy(scan_array_vars[scan_array_var_count], stmt->var.name.start, len);
                    scan_array_vars[scan_array_var_count][len] = '\0';
                    scan_array_var_count++;
                }
            }
            break;
        case STMT_CONST:
            if (stmt->const_stmt.init) scan_expr_for_lambdas(stmt->const_stmt.init);
            break;
        case STMT_RETURN:
            if (stmt->ret.value) {
                in_return_lambda = true;
                scan_expr_for_lambdas(stmt->ret.value);
                in_return_lambda = false;
            }
            break;
        case STMT_EXPR:
            scan_expr_for_lambdas(stmt->expr);
            break;
        case STMT_BLOCK:
        case STMT_PARALLEL:
            // Both share the block struct; recurse so spawns/lambdas
            // inside (notably parallel's spawn-bound vars) get their wrappers
            // collected.
            for (int i = 0; i < stmt->block.count; i++) {
                Stmt* _bs = stmt->block.stmts[i];
                // A parallel-block `a = f()` is lowered as an IMPLICIT spawn
                // (see codegen_stmt.c STMT_PARALLEL), so it needs the same
                // __spawn_wrapper_f_N that an explicit `spawn f()` needs.
                // Scan it AS a spawn expression to collect that wrapper.
                if (stmt->type == STMT_PARALLEL && _bs && _bs->type == STMT_VAR &&
                    _bs->var.init && _bs->var.init->type == EXPR_CALL &&
                    _bs->var.init->call.callee->type == EXPR_IDENT) {
                    // MUST use the SAME predicate as the codegen_stmt.c lowering:
                    // get_function_return_type() != NULL, i.e. a known user fn.
                    // Without it, `t = Time::now_millis()` (a builtin, lexed as
                    // ONE EXPR_IDENT token containing "::") emitted
                    // `void* __spawn_wrapper_Time::now_millis(...)` - invalid C.
                    extern const char* get_function_return_type(const char*);
                    char _pfn[256];
                    token_to_cstr(_pfn, sizeof(_pfn), _bs->var.init->call.callee->token);
                    if (get_function_return_type(_pfn)) {
                        Expr _sp;
                        _sp.type = EXPR_SPAWN;
                        _sp.spawn.call = _bs->var.init;
                        _sp._codegen_temp_id = -1;
                        scan_expr_for_lambdas(&_sp);
                    }
                }
                scan_stmt_for_lambdas(_bs);
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
        case STMT_FOR:
            if (stmt->for_stmt.array_expr) scan_expr_for_lambdas(stmt->for_stmt.array_expr);
            if (stmt->for_stmt.body) scan_stmt_for_lambdas(stmt->for_stmt.body);
            break;
        case STMT_SPAWN:
            // Collect spawn wrappers for fire-and-forget spawns
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT) {
                
                Expr* callee = stmt->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                int arg_count = stmt->spawn.call->call.arg_count;
                bool already_added = false;
                for (int i = 0; i < spawn_wrapper_count; i++) {
                    if (strcmp(spawn_wrappers[i].func_name, func_name) == 0 &&
                        spawn_wrappers[i].arg_count == arg_count) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    ensure_spawn_wrapper_cap();
                    strcpy(spawn_wrappers[spawn_wrapper_count].func_name, func_name);
                    spawn_wrappers[spawn_wrapper_count].arg_count = arg_count;
                    spawn_wrappers[spawn_wrapper_count].returns_void = 1;
                    // Single non-word arg (float/struct/array) needs the boxed
                    // _1b wrapper - the word cast truncated floats and couldn't
                    // carry structs at all (fire-and-forget was missing this).
                    spawn_wrappers[spawn_wrapper_count].boxed_arg1 = 0;
                    if (arg_count == 1) {
                        int _w = 1;
                        spawn_param_c_type(func_name, 0, &_w, NULL);
                        if (!_w) spawn_wrappers[spawn_wrapper_count].boxed_arg1 = 1;
                    }
                    spawn_wrapper_count++;
                }
            }
            break;
        default:
            break;
    }
}

// Helper to generate lambda body expression to string
static void collect_idents(Expr* expr, char idents[][64], int* count, int max) {
    if (!expr || *count >= max) return;
    if (expr->type == EXPR_IDENT) {
        char name[64]; int len = expr->token.length < 63 ? expr->token.length : 63;
        memcpy(name, expr->token.start, len); name[len] = '\0';
        for (int i = 0; i < *count; i++) if (strcmp(idents[i], name) == 0) return;
        strcpy(idents[*count], name); (*count)++;
        return;
    }
    if (expr->type == EXPR_BINARY) { collect_idents(expr->binary.left, idents, count, max); collect_idents(expr->binary.right, idents, count, max); }
    else if (expr->type == EXPR_UNARY) { collect_idents(expr->unary.operand, idents, count, max); }
    else if (expr->type == EXPR_CALL) { for (int i = 0; i < expr->call.arg_count; i++) collect_idents(expr->call.args[i], idents, count, max); }
    else if (expr->type == EXPR_METHOD_CALL) { collect_idents(expr->method_call.object, idents, count, max); for (int i = 0; i < expr->method_call.arg_count; i++) collect_idents(expr->method_call.args[i], idents, count, max); }
    else if (expr->type == EXPR_IF_EXPR) { collect_idents(expr->if_expr.condition, idents, count, max); collect_idents(expr->if_expr.then_expr, idents, count, max); collect_idents(expr->if_expr.else_expr, idents, count, max); }
    else if (expr->type == EXPR_INDEX) { collect_idents(expr->index.array, idents, count, max); collect_idents(expr->index.index, idents, count, max); }
    else if (expr->type == EXPR_ASSIGN) { collect_idents(expr->assign.value, idents, count, max); /* also capture the target */ char name[64]; int len = expr->assign.name.length < 63 ? expr->assign.name.length : 63; memcpy(name, expr->assign.name.start, len); name[len] = '\0'; for (int i = 0; i < *count; i++) if (strcmp(idents[i], name) == 0) return; if (*count < max) { strcpy(idents[*count], name); (*count)++; } }
    // Interpolation segments reference outer vars too - "${n}" inside a lambda
    // used to skip capture collection entirely (C error: undeclared 'n').
    else if (expr->type == EXPR_STRING_INTERP) { for (int i = 0; i < expr->string_interp.count; i++) collect_idents(expr->string_interp.expressions[i], idents, count, max); }
}
static void scan_expr_for_lambdas(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_LAMBDA:
            // Found a lambda! Generate it now
            lambda_id_counter++;
            int lambda_id = lambda_id_counter;
            
            // Detect captured variables - recursively collect all identifiers in body
            char captured_vars[16][64];
            int capture_count = 0;
            {
                char all_idents[32][64]; int ident_count = 0;
                collect_idents(expr->lambda.body, all_idents, &ident_count, 32);
                // Also collect from body statements (multiline lambdas)
                for (int si = 0; si < expr->lambda.body_stmt_count; si++) {
                    Stmt* s = expr->lambda.body_stmts[si];
                    if (s && s->type == STMT_VAR && s->var.init) collect_idents(s->var.init, all_idents, &ident_count, 32);
                    if (s && s->type == STMT_EXPR) collect_idents(s->expr, all_idents, &ident_count, 32);
                }
                for (int ai = 0; ai < ident_count; ai++) {
                    int is_param = 0;
                    for (int pi = 0; pi < expr->lambda.param_count; pi++) {
                        char pn[64]; int pl = expr->lambda.params[pi].length < 63 ? expr->lambda.params[pi].length : 63;
                        memcpy(pn, expr->lambda.params[pi].start, pl); pn[pl] = '\0';
                        if (strcmp(all_idents[ai], pn) == 0) { is_param = 1; break; }
                    }
                    // Skip known builtins/modules and lambda-local vars
                    int is_local = 0;
                    for (int si = 0; si < expr->lambda.body_stmt_count; si++) {
                        if (expr->lambda.body_stmts[si] && expr->lambda.body_stmts[si]->type == STMT_VAR) {
                            char ln[64]; int ll = expr->lambda.body_stmts[si]->var.name.length < 63 ? expr->lambda.body_stmts[si]->var.name.length : 63;
                            memcpy(ln, expr->lambda.body_stmts[si]->var.name.start, ll); ln[ll] = '\0';
                            if (strcmp(all_idents[ai], ln) == 0) { is_local = 1; break; }
                        }
                    }
                    if (!is_param && !is_local &&
                        strcmp(all_idents[ai], "true") != 0 && strcmp(all_idents[ai], "false") != 0 &&
                        strcmp(all_idents[ai], "Shared") != 0 && strcmp(all_idents[ai], "Math") != 0 &&
                        strcmp(all_idents[ai], "println") != 0 && strcmp(all_idents[ai], "print") != 0 &&
                        strcmp(all_idents[ai], "Test") != 0 && strcmp(all_idents[ai], "File") != 0 &&
                        strcmp(all_idents[ai], "System") != 0 && strcmp(all_idents[ai], "Json") != 0 &&
                        strcmp(all_idents[ai], "Http") != 0 && strcmp(all_idents[ai], "Time") != 0 &&
                        capture_count < 16) {
                        strcpy(captured_vars[capture_count++], all_idents[ai]);
                    }
                }
            }
            
            {
                ensure_lambda_cap();
                lambda_functions[lambda_count].ast = expr;
                lambda_functions[lambda_count].id = lambda_id;
                lambda_functions[lambda_count].param_count = expr->lambda.param_count;
                lambda_functions[lambda_count].capture_count = capture_count;
                lambda_functions[lambda_count].is_closure = (in_return_lambda && capture_count > 0);
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
        case EXPR_METHOD_CALL:
            scan_expr_for_lambdas(expr->method_call.object);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                scan_expr_for_lambdas(expr->method_call.args[i]);
            }
            // Detect x.push(spawn ...) - register x as spawn array
            if (expr->method_call.method.length == 4 &&
                memcmp(expr->method_call.method.start, "push", 4) == 0 &&
                expr->method_call.arg_count == 1 &&
                expr->method_call.args[0]->type == EXPR_SPAWN &&
                expr->method_call.object->type == EXPR_IDENT) {
                char vn[256];
                int vl = expr->method_call.object->token.length < 255 ? expr->method_call.object->token.length : 255;
                memcpy(vn, expr->method_call.object->token.start, vl); vn[vl] = '\0';
                register_spawn_array(vn);
                // Check if spawned function returns string
                Expr* spawn_call = expr->method_call.args[0]->spawn.call;
                if (spawn_call && spawn_call->type == EXPR_CALL && spawn_call->call.callee->type == EXPR_IDENT) {
                    char sfn[256]; int sfl = spawn_call->call.callee->token.length < 255 ? spawn_call->call.callee->token.length : 255;
                    memcpy(sfn, spawn_call->call.callee->token.start, sfl); sfn[sfl] = '\0';
                    extern const char* get_function_return_type(const char*);
                    const char* srt = get_function_return_type(sfn);
                    if (srt && strcmp(srt, "string") == 0) {
                        extern void register_string_spawn_array(const char*);
                        register_string_spawn_array(vn);
                    }
                }
            }
            break;
        case EXPR_SPAWN:
            // Also scan await expressions - they may contain spawn
        case EXPR_AWAIT:
            if (expr->type == EXPR_AWAIT) {
                scan_expr_for_lambdas(expr->await.expr);
                break;
            }
            // Collect spawn wrapper for this spawn expression
            if (expr->spawn.call && expr->spawn.call->type == EXPR_CALL &&
                expr->spawn.call->call.callee->type == EXPR_IDENT) {
                Expr* callee = expr->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                // Add to spawn wrappers (track by func_name + arg_count)
                int arg_count = expr->spawn.call->call.arg_count;
                bool already_added = false;
                for (int i = 0; i < spawn_wrapper_count; i++) {
                    if (strcmp(spawn_wrappers[i].func_name, func_name) == 0 &&
                        spawn_wrappers[i].arg_count == arg_count) {
                        already_added = true;
                        spawn_wrappers[i].returns_void = 0; // EXPR_SPAWN needs return value
                        break;
                    }
                }
                if (!already_added) {
                    ensure_spawn_wrapper_cap();
                    strcpy(spawn_wrappers[spawn_wrapper_count].func_name, func_name);
                    spawn_wrappers[spawn_wrapper_count].arg_count = arg_count;
                    spawn_wrappers[spawn_wrapper_count].returns_void = 0;
                    extern const char* get_function_return_type(const char*);
                    const char* _rt = get_function_return_type(func_name);
                    spawn_wrappers[spawn_wrapper_count].returns_string = (_rt && strcmp(_rt, "string") == 0) ? 1 : 0;
                    spawn_wrappers[spawn_wrapper_count].return_type[0] = '\0';
                    if (_rt && strcmp(_rt, "int") != 0 && strcmp(_rt, "string") != 0 &&
                        strcmp(_rt, "float") != 0 && strcmp(_rt, "bool") != 0) {
                        strncpy(spawn_wrappers[spawn_wrapper_count].return_type, _rt, 63);
                    }
                    // A float RETURN can't ride (void*)(intptr_t) either - the
                    // double was truncated to its integer part. Box it like a
                    // struct: the wrapper mallocs a double, await derefs it.
                    if (_rt && strcmp(_rt, "float") == 0) {
                        strncpy(spawn_wrappers[spawn_wrapper_count].return_type, "double", 63);
                    }
                    extern int function_can_inline(const char*);
                    spawn_wrappers[spawn_wrapper_count].can_inline = function_can_inline(func_name);
                    // Single non-word arg (float/struct/array) → boxed wrapper.
                    spawn_wrappers[spawn_wrapper_count].boxed_arg1 = 0;
                    if (arg_count == 1 && current_program) {
                        for (int _fi = 0; _fi < current_program->count; _fi++) {
                            Stmt* _fs = current_program->stmts[_fi];
                            if (_fs->type == STMT_FN &&
                                (size_t)_fs->fn.name.length == strlen(func_name) &&
                                memcmp(_fs->fn.name.start, func_name, _fs->fn.name.length) == 0) {
                                if (_fs->fn.param_count > 0 && _fs->fn.param_types[0]) {
                                    Expr* pt0 = _fs->fn.param_types[0];
                                    if (pt0->type == EXPR_ARRAY) spawn_wrappers[spawn_wrapper_count].boxed_arg1 = 1;
                                    else if (pt0->type == EXPR_IDENT) {
                                        Token ptk = pt0->token;
                                        bool is_word = (ptk.length == 3 && memcmp(ptk.start, "int", 3) == 0) ||
                                                       (ptk.length == 4 && memcmp(ptk.start, "bool", 4) == 0) ||
                                                       (ptk.length == 6 && memcmp(ptk.start, "string", 6) == 0);
                                        if (!is_word) spawn_wrappers[spawn_wrapper_count].boxed_arg1 = 1;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    spawn_wrapper_count++;
                }
            }
            break;
        case EXPR_ARRAY:
            for (int i = 0; i < expr->array.count; i++) {
                scan_expr_for_lambdas(expr->array.elements[i]);
            }
            break;
        case EXPR_STRUCT_INIT:
            // A lambda in a struct initializer - `Button { on_click: () => ... }`,
            // the event-handler shape. Without this arm the lambda is never
            // visited here, so it gets no id and no top-level function is emitted,
            // while the initializer still emits a reference to it: the generated C
            // failed with `use of undeclared identifier '__lambda_1'`.
            //
            // Field VALUES only. Field types are type expressions, not values, so
            // they can hold no lambda.
            for (int i = 0; i < expr->struct_init.field_count; i++) {
                scan_expr_for_lambdas(expr->struct_init.field_values[i]);
            }
            break;
        case EXPR_STRING_INTERP:
            // The SAME omission as EXPR_STRUCT_INIT above, in the one place a
            // reader is most likely to write a pipeline:
            //
            //     print("top: ${xs.filter((n) => n > 2)}")
            //
            // An interpolated expression is a value like any other, but this
            // scanner never descended into the parts, so the lambda got no id
            // and no top-level function - while the interpolation still emitted
            // a call referencing it. The generated C failed with `use of
            // undeclared identifier '__lambda_1'`, surfaced to the user only as
            // "compilation failed (internal codegen error)".
            //
            // Assigning the pipeline to a variable first worked, which is why
            // this survived: it reads as a rule about interpolation rather than
            // a missing traversal. Every other walker in this file (collect_idents,
            // veto_scan_expr, veto_all_idents_in) already handles this node.
            for (int i = 0; i < expr->string_interp.count; i++) {
                scan_expr_for_lambdas(expr->string_interp.expressions[i]);
            }
            break;
        default:
            break;
    }
}

static void scan_for_lambdas(Stmt* body) {
    scan_stmt_for_lambdas(body);
}

// --- WynIntArray veto pre-pass -------------------------------------------
//
// See the "int-array veto" block in codegen.c for the why. This pass walks the
// whole program BEFORE any code is emitted and vetoes the packed WynIntArray
// representation for every `[int]`-annotated variable whose uses that
// representation cannot express. Vetoed variables fall back to the generic
// WynArray, which is exactly what the inferred spelling (`xs = [1,2]`) already
// emits - so both spellings agree by construction.
//
// The pass is deliberately CONSERVATIVE (whitelist, not blacklist): a bare
// mention of the name in any position not explicitly known to be safe vetoes
// it. A false veto only costs the packed-representation speedup; a missed veto
// is a miscompile. New syntax therefore fails safe.

// Ops with a real WynIntArray lowering. Keep in sync with the WynIntArray
// dispatch in codegen_expr.c (~:2453) and the for-loop path in
// codegen_stmt.c (~:4155).
//
// `sort` is deliberately NOT here: its packed lowering is the statement form
// `int_array_sort(&xs)`, which yields the WynIntArray as its value. When that
// value is CONSUMED (`var s = xs.sort()`) it flows into a var whose type the
// STMT_VAR path independently decided is WynArray - the same store/load
// disagreement in miniature. Statement-position `xs.sort()` discards the value
// and is handled as a special case in veto_scan_stmt's STMT_EXPR arm.
static int int_array_safe_method(Token m) {
    if (m.length == 4 && memcmp(m.start, "push", 4) == 0) return 1;
    if (m.length == 3 && memcmp(m.start, "len", 3) == 0) return 1;
    return 0;
}

// True for `xs.sort()` where xs is a bare identifier - packed-safe ONLY when
// the resulting value is discarded (statement position).
static int is_bare_sort_call(Expr* e) {
    return e && e->type == EXPR_METHOD_CALL &&
           e->method_call.object->type == EXPR_IDENT &&
           e->method_call.method.length == 4 &&
           memcmp(e->method_call.method.start, "sort", 4) == 0;
}

static void veto_scan_expr(Expr* e);
static void veto_scan_stmt(Stmt* s);
static void veto_all_idents_in(Expr* e);

// Veto the name if `e` is a bare identifier. Used for every position where a
// whole array value would flow into generic-WynArray code.
static void veto_if_ident(Expr* e) {
    if (!e || e->type != EXPR_IDENT) return;
    char n[256]; token_to_cstr(n, sizeof(n), e->token);
    extern void veto_int_array_var(const char*);
    veto_int_array_var(n);
}

static void veto_scan_expr(Expr* e) {
    if (!e) return;
    switch (e->type) {
        case EXPR_METHOD_CALL: {
            // `xs.push(v)` / `xs.sort()` / `xs.len()` on a bare identifier are
            // the packed-safe forms: recurse into the ARGS but do not treat the
            // receiver itself as a generic-array use. Everything else
            // (.map/.filter/.slice/.join/.sum/.first/.contains/...) routes
            // through a WynArray-taking helper, so it vetoes.
            bool receiver_ok = (e->method_call.object->type == EXPR_IDENT &&
                                int_array_safe_method(e->method_call.method));
            if (!receiver_ok) {
                veto_if_ident(e->method_call.object);
                veto_scan_expr(e->method_call.object);
            }
            for (int i = 0; i < e->method_call.arg_count; i++)
                veto_scan_expr(e->method_call.args[i]);
            break;
        }
        case EXPR_INDEX:
            // `xs[i]` has a packed lowering (int_array_get); the index does not
            // carry the array, so only recurse into it. A non-ident base (e.g.
            // `f()[i]`, `m[k][j]`) is scanned normally.
            if (e->index.array->type != EXPR_IDENT) veto_scan_expr(e->index.array);
            veto_scan_expr(e->index.index);
            break;
        case EXPR_INDEX_ASSIGN:
            // `xs[i] = v` has NO packed lowering: the emitter casts &xs to
            // WynArray* and writes a 16-byte WynValue into an 8-byte packed
            // slot - a silent wrong answer plus an out-of-bounds write. This is
            // the one shape that MUST veto (it used to compile "fine").
            veto_if_ident(e->index_assign.object);
            veto_scan_expr(e->index_assign.object);
            veto_scan_expr(e->index_assign.index);
            veto_scan_expr(e->index_assign.value);
            break;
        case EXPR_CALL:
            // Passing the array to any function hands a WynIntArray to a
            // WynArray parameter.
            veto_scan_expr(e->call.callee);
            for (int i = 0; i < e->call.arg_count; i++) {
                veto_if_ident(e->call.args[i]);
                veto_scan_expr(e->call.args[i]);
            }
            break;
        case EXPR_BINARY:
            veto_if_ident(e->binary.left); veto_if_ident(e->binary.right);
            veto_scan_expr(e->binary.left); veto_scan_expr(e->binary.right);
            break;
        case EXPR_UNARY: veto_if_ident(e->unary.operand); veto_scan_expr(e->unary.operand); break;
        case EXPR_ASSIGN:
            // `ys = xs` (whole-array copy) and `xs = <other array>` both mix
            // representations.
            veto_if_ident(e->assign.value); veto_scan_expr(e->assign.value);
            { char n[256]; token_to_cstr(n, sizeof(n), e->assign.name);
              extern void veto_int_array_var(const char*); veto_int_array_var(n); }
            break;
        case EXPR_LAMBDA:
            // Capture machinery declares every captured var as WynArray in the
            // env struct, so ANY mention of the name inside a lambda body -
            // even `xs.len()`, which is packed-safe outside a lambda - would
            // assign a WynIntArray to a WynArray field. Veto every identifier
            // the body mentions, not just the ones in unsafe positions.
            veto_all_idents_in(e->lambda.body);
            break;
        case EXPR_ARRAY:
            for (int i = 0; i < e->array.count; i++) {
                veto_if_ident(e->array.elements[i]); veto_scan_expr(e->array.elements[i]);
            }
            break;
        case EXPR_TUPLE:
            for (int i = 0; i < e->tuple.count; i++) {
                veto_if_ident(e->tuple.elements[i]); veto_scan_expr(e->tuple.elements[i]);
            }
            break;
        case EXPR_MAP:
            for (int i = 0; i < e->map.count; i++) {
                veto_if_ident(e->map.keys[i]); veto_scan_expr(e->map.keys[i]);
                veto_if_ident(e->map.values[i]); veto_scan_expr(e->map.values[i]);
            }
            break;
        case EXPR_STRUCT_INIT:
            for (int i = 0; i < e->struct_init.field_count; i++) {
                veto_if_ident(e->struct_init.field_values[i]);
                veto_scan_expr(e->struct_init.field_values[i]);
            }
            break;
        case EXPR_STRING_INTERP:
            for (int i = 0; i < e->string_interp.count; i++) {
                veto_if_ident(e->string_interp.expressions[i]);
                veto_scan_expr(e->string_interp.expressions[i]);
            }
            break;
        case EXPR_TERNARY:
            veto_scan_expr(e->ternary.condition);
            veto_if_ident(e->ternary.then_expr); veto_scan_expr(e->ternary.then_expr);
            veto_if_ident(e->ternary.else_expr); veto_scan_expr(e->ternary.else_expr);
            break;
        case EXPR_IF_EXPR:
            veto_scan_expr(e->if_expr.condition);
            veto_if_ident(e->if_expr.then_expr); veto_scan_expr(e->if_expr.then_expr);
            veto_if_ident(e->if_expr.else_expr); veto_scan_expr(e->if_expr.else_expr);
            break;
        case EXPR_AWAIT: veto_if_ident(e->await.expr); veto_scan_expr(e->await.expr); break;
        case EXPR_SPAWN: veto_scan_expr(e->spawn.call); break;
        case EXPR_FIELD_ACCESS: veto_if_ident(e->field_access.object); veto_scan_expr(e->field_access.object); break;
        case EXPR_FIELD_ASSIGN:
            veto_if_ident(e->field_assign.object); veto_scan_expr(e->field_assign.object);
            veto_if_ident(e->field_assign.value); veto_scan_expr(e->field_assign.value);
            break;
        case EXPR_RANGE: veto_scan_expr(e->range.start); veto_scan_expr(e->range.end); break;
        case EXPR_SOME: case EXPR_OK: case EXPR_ERR:
            veto_if_ident(e->option.value); veto_scan_expr(e->option.value); break;
        case EXPR_TRY: veto_if_ident(e->try_expr.value); veto_scan_expr(e->try_expr.value); break;
        case EXPR_MATCH:
            veto_if_ident(e->match.value); veto_scan_expr(e->match.value);
            for (int i = 0; i < e->match.arm_count; i++) veto_scan_expr(e->match.arms[i].result);
            break;
        case EXPR_BLOCK:
            for (int i = 0; i < e->block.stmt_count; i++) veto_scan_stmt(e->block.stmts[i]);
            veto_if_ident(e->block.result); veto_scan_expr(e->block.result);
            break;
        case EXPR_LIST_COMP:
            // The generic list-comp lowering builds a WynArray.
            veto_if_ident(e->list_comp.iter_start); veto_scan_expr(e->list_comp.iter_start);
            veto_scan_expr(e->list_comp.iter_end);
            veto_if_ident(e->list_comp.body); veto_scan_expr(e->list_comp.body);
            veto_scan_expr(e->list_comp.condition);
            break;
        case EXPR_TUPLE_INDEX: veto_if_ident(e->tuple_index.tuple); veto_scan_expr(e->tuple_index.tuple); break;
        case EXPR_OPT_CHAIN: veto_if_ident(e->opt_chain.object); veto_scan_expr(e->opt_chain.object); break;
        default: break;
    }
}

static void veto_scan_stmt(Stmt* s) {
    if (!s) return;
    switch (s->type) {
        case STMT_VAR:
            veto_if_ident(s->var.init); veto_scan_expr(s->var.init); break;
        case STMT_CONST:
            veto_if_ident(s->const_stmt.init); veto_scan_expr(s->const_stmt.init); break;
        case STMT_EXPR:
            // Statement-position `xs.sort()` discards the yielded value, so the
            // packed in-place sort is safe. Only recurse into the args.
            if (is_bare_sort_call(s->expr)) {
                for (int i = 0; i < s->expr->method_call.arg_count; i++)
                    veto_scan_expr(s->expr->method_call.args[i]);
            } else {
                veto_scan_expr(s->expr);
            }
            break;
        case STMT_RETURN:
            // Returning the array yields it as a WynArray to the caller.
            veto_if_ident(s->ret.value); veto_scan_expr(s->ret.value); break;
        case STMT_BLOCK: case STMT_UNSAFE: case STMT_PARALLEL:
            for (int i = 0; i < s->block.count; i++) veto_scan_stmt(s->block.stmts[i]);
            break;
        case STMT_IF:
            veto_scan_expr(s->if_stmt.condition);
            veto_scan_stmt(s->if_stmt.then_branch); veto_scan_stmt(s->if_stmt.else_branch);
            break;
        case STMT_WHILE:
            veto_scan_expr(s->while_stmt.condition); veto_scan_stmt(s->while_stmt.body); break;
        case STMT_FOR:
            // `for x in xs` HAS a packed lowering (codegen_stmt.c ~:4155), so a
            // bare-ident iterable does not veto.
            if (s->for_stmt.array_expr && s->for_stmt.array_expr->type != EXPR_IDENT)
                veto_scan_expr(s->for_stmt.array_expr);
            veto_scan_stmt(s->for_stmt.init);
            veto_scan_expr(s->for_stmt.condition);
            veto_scan_expr(s->for_stmt.increment);
            veto_scan_stmt(s->for_stmt.body);
            break;
        case STMT_FN: case STMT_ASYNC_FN: veto_scan_stmt(s->fn.body); break;
        case STMT_MATCH:
            veto_if_ident(s->match_stmt.value); veto_scan_expr(s->match_stmt.value);
            for (int i = 0; i < s->match_stmt.case_count; i++) {
                veto_scan_expr(s->match_stmt.cases[i].guard);
                veto_scan_stmt(s->match_stmt.cases[i].body);
            }
            break;
        case STMT_SPAWN: veto_scan_expr(s->spawn.call); break;
        case STMT_DEFER: veto_scan_expr(s->expr); break;
        case STMT_TEST: veto_scan_stmt(s->test_stmt.body); break;
        case STMT_IMPL:
            for (int i = 0; i < s->impl.method_count; i++)
                if (s->impl.methods[i]) veto_scan_stmt(s->impl.methods[i]->body);
            break;
        case STMT_THROW: veto_scan_expr(s->throw_stmt.value); break;
        case STMT_TRY:
            veto_scan_stmt(s->try_stmt.try_block);
            for (int i = 0; i < s->try_stmt.catch_count; i++) veto_scan_stmt(s->try_stmt.catch_blocks[i]);
            veto_scan_stmt(s->try_stmt.finally_block);
            break;
        default: break;
    }
}

// Veto EVERY identifier mentioned anywhere in `e` (and any nested statements).
// Used for lambda bodies, where the capture machinery types every captured
// variable as WynArray regardless of how it is used.
static void veto_all_idents_in_stmt(Stmt* s);
static void veto_all_idents_in(Expr* e) {
    if (!e) return;
    if (e->type == EXPR_IDENT) { veto_if_ident(e); return; }
    switch (e->type) {
        case EXPR_BINARY: veto_all_idents_in(e->binary.left); veto_all_idents_in(e->binary.right); break;
        case EXPR_UNARY: veto_all_idents_in(e->unary.operand); break;
        case EXPR_TRY: veto_all_idents_in(e->try_expr.value); break;
        case EXPR_AWAIT: veto_all_idents_in(e->await.expr); break;
        case EXPR_SPAWN: veto_all_idents_in(e->spawn.call); break;
        case EXPR_CALL:
            veto_all_idents_in(e->call.callee);
            for (int i = 0; i < e->call.arg_count; i++) veto_all_idents_in(e->call.args[i]);
            break;
        case EXPR_METHOD_CALL:
            veto_all_idents_in(e->method_call.object);
            for (int i = 0; i < e->method_call.arg_count; i++) veto_all_idents_in(e->method_call.args[i]);
            break;
        case EXPR_INDEX: veto_all_idents_in(e->index.array); veto_all_idents_in(e->index.index); break;
        case EXPR_INDEX_ASSIGN:
            veto_all_idents_in(e->index_assign.object);
            veto_all_idents_in(e->index_assign.index);
            veto_all_idents_in(e->index_assign.value);
            break;
        case EXPR_ASSIGN:
            veto_all_idents_in(e->assign.value);
            { char n[256]; token_to_cstr(n, sizeof(n), e->assign.name);
              extern void veto_int_array_var(const char*); veto_int_array_var(n); }
            break;
        case EXPR_ARRAY:
            for (int i = 0; i < e->array.count; i++) veto_all_idents_in(e->array.elements[i]);
            break;
        case EXPR_TUPLE:
            for (int i = 0; i < e->tuple.count; i++) veto_all_idents_in(e->tuple.elements[i]);
            break;
        case EXPR_MAP:
            for (int i = 0; i < e->map.count; i++) {
                veto_all_idents_in(e->map.keys[i]); veto_all_idents_in(e->map.values[i]);
            }
            break;
        case EXPR_STRUCT_INIT:
            for (int i = 0; i < e->struct_init.field_count; i++)
                veto_all_idents_in(e->struct_init.field_values[i]);
            break;
        case EXPR_STRING_INTERP:
            for (int i = 0; i < e->string_interp.count; i++)
                veto_all_idents_in(e->string_interp.expressions[i]);
            break;
        case EXPR_TERNARY:
            veto_all_idents_in(e->ternary.condition);
            veto_all_idents_in(e->ternary.then_expr); veto_all_idents_in(e->ternary.else_expr);
            break;
        case EXPR_IF_EXPR:
            veto_all_idents_in(e->if_expr.condition);
            veto_all_idents_in(e->if_expr.then_expr); veto_all_idents_in(e->if_expr.else_expr);
            break;
        case EXPR_FIELD_ACCESS: veto_all_idents_in(e->field_access.object); break;
        case EXPR_OPT_CHAIN: veto_all_idents_in(e->opt_chain.object); break;
        case EXPR_FIELD_ASSIGN:
            veto_all_idents_in(e->field_assign.object); veto_all_idents_in(e->field_assign.value); break;
        case EXPR_TUPLE_INDEX: veto_all_idents_in(e->tuple_index.tuple); break;
        case EXPR_RANGE: veto_all_idents_in(e->range.start); veto_all_idents_in(e->range.end); break;
        case EXPR_SOME: case EXPR_OK: case EXPR_ERR: veto_all_idents_in(e->option.value); break;
        case EXPR_LAMBDA: veto_all_idents_in(e->lambda.body); break;
        case EXPR_MATCH:
            veto_all_idents_in(e->match.value);
            for (int i = 0; i < e->match.arm_count; i++) veto_all_idents_in(e->match.arms[i].result);
            break;
        case EXPR_BLOCK:
            for (int i = 0; i < e->block.stmt_count; i++) veto_all_idents_in_stmt(e->block.stmts[i]);
            veto_all_idents_in(e->block.result);
            break;
        case EXPR_LIST_COMP:
            veto_all_idents_in(e->list_comp.iter_start); veto_all_idents_in(e->list_comp.iter_end);
            veto_all_idents_in(e->list_comp.body); veto_all_idents_in(e->list_comp.condition);
            break;
        default: break;
    }
}

static void veto_all_idents_in_stmt(Stmt* s) {
    if (!s) return;
    switch (s->type) {
        case STMT_VAR: veto_all_idents_in(s->var.init); break;
        case STMT_CONST: veto_all_idents_in(s->const_stmt.init); break;
        case STMT_EXPR: case STMT_DEFER: veto_all_idents_in(s->expr); break;
        case STMT_RETURN: veto_all_idents_in(s->ret.value); break;
        case STMT_BLOCK: case STMT_UNSAFE: case STMT_PARALLEL:
            for (int i = 0; i < s->block.count; i++) veto_all_idents_in_stmt(s->block.stmts[i]);
            break;
        case STMT_IF:
            veto_all_idents_in(s->if_stmt.condition);
            veto_all_idents_in_stmt(s->if_stmt.then_branch);
            veto_all_idents_in_stmt(s->if_stmt.else_branch);
            break;
        case STMT_WHILE:
            veto_all_idents_in(s->while_stmt.condition); veto_all_idents_in_stmt(s->while_stmt.body); break;
        case STMT_FOR:
            veto_all_idents_in(s->for_stmt.array_expr);
            veto_all_idents_in_stmt(s->for_stmt.init);
            veto_all_idents_in(s->for_stmt.condition);
            veto_all_idents_in(s->for_stmt.increment);
            veto_all_idents_in_stmt(s->for_stmt.body);
            break;
        case STMT_MATCH:
            veto_all_idents_in(s->match_stmt.value);
            for (int i = 0; i < s->match_stmt.case_count; i++) {
                veto_all_idents_in(s->match_stmt.cases[i].guard);
                veto_all_idents_in_stmt(s->match_stmt.cases[i].body);
            }
            break;
        case STMT_SPAWN: veto_all_idents_in(s->spawn.call); break;
        case STMT_THROW: veto_all_idents_in(s->throw_stmt.value); break;
        case STMT_FN: case STMT_ASYNC_FN: veto_all_idents_in_stmt(s->fn.body); break;
        case STMT_TEST: veto_all_idents_in_stmt(s->test_stmt.body); break;
        case STMT_TRY:
            veto_all_idents_in_stmt(s->try_stmt.try_block);
            for (int i = 0; i < s->try_stmt.catch_count; i++)
                veto_all_idents_in_stmt(s->try_stmt.catch_blocks[i]);
            veto_all_idents_in_stmt(s->try_stmt.finally_block);
            break;
        default: break;
    }
}

// Entry point: run over the whole program before emission.
void veto_scan_program(Program* prog) {
    if (!prog) return;
    for (int i = 0; i < prog->count; i++) veto_scan_stmt(prog->stmts[i]);
}

// S2: Emit a lambda function using the real codegen_expr pipeline instead of the
// string-based mini-emitter. This gives string concat, string methods, float ops,
// and block bodies for free - everything codegen_expr already handles.
static void emit_lambda_via_codegen(LambdaFunction* lf) {
    Expr* expr = lf->ast;
    if (!expr || expr->type != EXPR_LAMBDA) return;

    int capture_count = lf->capture_count;
    int lambda_id = lf->id;

    // Determine return type from checker's inference (stored on expr->expr_type)
    const char* ret_c_type = "long long";
    if (expr->expr_type && expr->expr_type->kind == TYPE_FUNCTION &&
        expr->expr_type->fn_type.return_type) {
        const char* inferred = codegen_c_type_from_type(expr->expr_type->fn_type.return_type);
        if (inferred) ret_c_type = inferred;
    }

    // Determine param C types from checker inference
    const char* param_c_types[16];
    for (int i = 0; i < expr->lambda.param_count && i < 16; i++) {
        param_c_types[i] = "long long";
        if (expr->expr_type && expr->expr_type->kind == TYPE_FUNCTION &&
            i < expr->expr_type->fn_type.param_count &&
            expr->expr_type->fn_type.param_types[i]) {
            const char* inferred = codegen_c_type_from_type(expr->expr_type->fn_type.param_types[i]);
            if (inferred) param_c_types[i] = inferred;
        }
    }

    // Capture-cell C type: resolve from the checker's recorded capture types
    // (matched by NAME against the codegen-scan capture list - the two lists
    // are built independently). Hardcoded `long long` cells made a captured
    // string round-trip through an integer: pointer-address output.
    const char* cap_c_types[16];
    for (int i = 0; i < capture_count && i < 16; i++) {
        cap_c_types[i] = is_scan_array_var(lf->captured_vars[i]) ? "WynArray" : "long long";
        for (int j = 0; j < expr->lambda.captured_count && j < 8; j++) {
            Token ct = expr->lambda.captured_vars[j];
            if ((int)strlen(lf->captured_vars[i]) == ct.length &&
                memcmp(lf->captured_vars[i], ct.start, ct.length) == 0 &&
                expr->lambda.captured_types && expr->lambda.captured_types[j]) {
                const char* t = codegen_c_type_from_type(expr->lambda.captured_types[j]);
                if (t) cap_c_types[i] = t;
                break;
            }
        }
    }

    if (capture_count > 0 && lf->is_closure) {
        // Closure with env struct: emit typedef + function taking void* env
        emit("typedef struct { ");
        for (int i = 0; i < capture_count; i++) {
            emit("%s %s; ", i < 16 ? cap_c_types[i] : "long long", lf->captured_vars[i]);
        }
        emit("} __closure_env_%d;\n", lambda_id);
        emit("%s __lambda_%d(void* __env", ret_c_type, lambda_id);
        for (int i = 0; i < expr->lambda.param_count; i++) {
            emit(", %s %.*s", param_c_types[i],
                expr->lambda.params[i].length, expr->lambda.params[i].start);
        }
        emit(") {\n");
        emit("    __closure_env_%d* __e = (__closure_env_%d*)__env;\n", lambda_id, lambda_id);
        for (int i = 0; i < capture_count; i++) {
            emit("    %s %s = __e->%s;\n", i < 16 ? cap_c_types[i] : "long long",
                 lf->captured_vars[i], lf->captured_vars[i]);
        }
    } else {
        // Non-closure: static capture globals + plain function
        if (capture_count > 0) {
            for (int i = 0; i < capture_count; i++) {
                emit("static %s __cap_%d_%s;\n",
                     i < 16 ? cap_c_types[i] : "long long", lambda_id, lf->captured_vars[i]);
            }
        }
        emit("%s __lambda_%d(", ret_c_type, lambda_id);
        for (int i = 0; i < expr->lambda.param_count; i++) {
            if (i > 0) emit(", ");
            emit("%s %.*s", param_c_types[i],
                expr->lambda.params[i].length, expr->lambda.params[i].start);
        }
        emit(") {\n");
        if (capture_count > 0) {
            for (int i = 0; i < capture_count; i++) {
                emit("#define %s __cap_%d_%s\n", lf->captured_vars[i], lambda_id, lf->captured_vars[i]);
            }
        }
    }

    // Emit body statements (multiline lambda)
    for (int si = 0; si < expr->lambda.body_stmt_count; si++) {
        Stmt* s = expr->lambda.body_stmts[si];
        if (s) {
            emit("    ");
            codegen_stmt(s);
        }
    }

    // Emit return expression using the real codegen_expr
    if (expr->lambda.body) {
        emit("    return ");
        codegen_expr(expr->lambda.body);
        emit(";\n");
    } else {
        emit("    return 0;\n");
    }

    emit("}\n");

    // Undef capture aliases (non-closure path)
    if (capture_count > 0 && !lf->is_closure) {
        for (int i = 0; i < capture_count; i++) {
            emit("#undef %s\n", lf->captured_vars[i]);
        }
    }
}

