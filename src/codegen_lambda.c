// codegen_lambda.c - Lambda scanning and generation
// Included from codegen.c - shares all statics

static void scan_expr_for_lambdas(Expr* expr);

// Track array vars found during scan (for type-aware lambda captures)
static char scan_array_vars[64][64];
static int scan_array_var_count = 0;
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
                if (stmt->var.init->type == EXPR_ARRAY && scan_array_var_count < 64) {
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
            for (int i = 0; i < stmt->block.count; i++) {
                scan_stmt_for_lambdas(stmt->block.stmts[i]);
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
                if (!already_added && spawn_wrapper_count < 256) {
                    strcpy(spawn_wrappers[spawn_wrapper_count].func_name, func_name);
                    spawn_wrappers[spawn_wrapper_count].arg_count = arg_count;
                    spawn_wrappers[spawn_wrapper_count].returns_void = 1;
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
}
static int lambda_expr_to_string(Expr* expr, char* buf, int max_len) {
    if (!expr) return 0;
    int pos = 0;
    
    switch (expr->type) {
        case EXPR_INT:
        case EXPR_IDENT:
            pos += snprintf(buf + pos, max_len - pos, "%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_BINARY: {
            pos += snprintf(buf + pos, max_len - pos, "(");
            pos += lambda_expr_to_string(expr->binary.left, buf + pos, max_len - pos);
            const char* op = " ? ";
            switch (expr->binary.op.type) {
                case TOKEN_PLUS: op = " + "; break;
                case TOKEN_MINUS: op = " - "; break;
                case TOKEN_STAR: op = " * "; break;
                case TOKEN_SLASH: op = " / "; break;
                case TOKEN_PERCENT: op = " % "; break;
                case TOKEN_EQEQ: op = " == "; break;
                case TOKEN_BANGEQ: op = " != "; break;
                case TOKEN_LT: op = " < "; break;
                case TOKEN_GT: op = " > "; break;
                case TOKEN_LTEQ: op = " <= "; break;
                case TOKEN_GTEQ: op = " >= "; break;
                case TOKEN_AND: case TOKEN_AMPAMP: op = " && "; break;
                case TOKEN_OR: case TOKEN_PIPEPIPE: op = " || "; break;
                default: break;
            }
            pos += snprintf(buf + pos, max_len - pos, "%s", op);
            pos += lambda_expr_to_string(expr->binary.right, buf + pos, max_len - pos);
            pos += snprintf(buf + pos, max_len - pos, ")");
            break;
        }
        case EXPR_UNARY:
            if (expr->unary.op.type == TOKEN_BANG || expr->unary.op.type == TOKEN_NOT) {
                pos += snprintf(buf + pos, max_len - pos, "!");
            } else if (expr->unary.op.type == TOKEN_MINUS) {
                pos += snprintf(buf + pos, max_len - pos, "-");
            }
            pos += lambda_expr_to_string(expr->unary.operand, buf + pos, max_len - pos);
            break;
        case EXPR_IF_EXPR:
            pos += snprintf(buf + pos, max_len - pos, "(");
            pos += lambda_expr_to_string(expr->if_expr.condition, buf + pos, max_len - pos);
            pos += snprintf(buf + pos, max_len - pos, " ? ");
            if (expr->if_expr.then_expr) {
                pos += lambda_expr_to_string(expr->if_expr.then_expr, buf + pos, max_len - pos);
            } else {
                pos += snprintf(buf + pos, max_len - pos, "0");
            }
            pos += snprintf(buf + pos, max_len - pos, " : ");
            if (expr->if_expr.else_expr) {
                pos += lambda_expr_to_string(expr->if_expr.else_expr, buf + pos, max_len - pos);
            } else {
                pos += snprintf(buf + pos, max_len - pos, "0");
            }
            pos += snprintf(buf + pos, max_len - pos, ")");
            break;
        case EXPR_METHOD_CALL: {
            // Generate method call: obj.method(args) -> c_function(obj, args)
            Token method = expr->method_call.method;
            char method_name[64];
            snprintf(method_name, sizeof(method_name), "%.*s", method.length, method.start);
            
            // Map common string methods
            const char* c_func = NULL;
            if (strcmp(method_name, "upper") == 0) c_func = "string_upper";
            else if (strcmp(method_name, "lower") == 0) c_func = "string_lower";
            else if (strcmp(method_name, "trim") == 0) c_func = "string_trim";
            else if (strcmp(method_name, "len") == 0) c_func = "string_len";
            else if (strcmp(method_name, "reverse") == 0) c_func = "string_reverse";
            else if (strcmp(method_name, "to_string") == 0) c_func = "to_string";
            
            if (c_func) {
                pos += snprintf(buf + pos, max_len - pos, "%s(", c_func);
                pos += lambda_expr_to_string(expr->method_call.object, buf + pos, max_len - pos);
                pos += snprintf(buf + pos, max_len - pos, ")");
            } else {
                // Check for Module.method() pattern (e.g., Shared.add, Math.pow)
                if (expr->method_call.object->type == EXPR_IDENT) {
                    char obj_name[64]; snprintf(obj_name, 64, "%.*s", expr->method_call.object->token.length, expr->method_call.object->token.start);
                    // Known modules use Module_method() pattern
                    bool is_module = (obj_name[0] >= 'A' && obj_name[0] <= 'Z');
                    if (is_module) {
                        pos += snprintf(buf + pos, max_len - pos, "%s_%s(", obj_name, method_name);
                        for (int i = 0; i < expr->method_call.arg_count; i++) {
                            if (i > 0) pos += snprintf(buf + pos, max_len - pos, ", ");
                            pos += lambda_expr_to_string(expr->method_call.args[i], buf + pos, max_len - pos);
                        }
                        pos += snprintf(buf + pos, max_len - pos, ")");
                    } else {
                        // Variable method: arr.sum() -> array_sum(arr), s.len() -> string_len(s)
                        const char* arr_func = NULL;
                        if (strcmp(method_name, "sum") == 0) arr_func = "array_sum";
                        else if (strcmp(method_name, "len") == 0) arr_func = "array_len";
                        else if (strcmp(method_name, "min") == 0) arr_func = "array_min";
                        else if (strcmp(method_name, "max") == 0) arr_func = "array_max";
                        else if (strcmp(method_name, "first") == 0) arr_func = "array_first";
                        else if (strcmp(method_name, "last") == 0) arr_func = "array_last";
                        else if (strcmp(method_name, "contains") == 0) arr_func = "string_contains";
                        else if (strcmp(method_name, "to_int") == 0) arr_func = "string_to_int";
                        if (arr_func) {
                            pos += snprintf(buf + pos, max_len - pos, "%s(", arr_func);
                            pos += lambda_expr_to_string(expr->method_call.object, buf + pos, max_len - pos);
                            for (int i = 0; i < expr->method_call.arg_count; i++) {
                                pos += snprintf(buf + pos, max_len - pos, ", ");
                                pos += lambda_expr_to_string(expr->method_call.args[i], buf + pos, max_len - pos);
                            }
                            pos += snprintf(buf + pos, max_len - pos, ")");
                        } else {
                            // Generic: obj.method(args) -> method(obj, args)
                            pos += snprintf(buf + pos, max_len - pos, "%s(", method_name);
                            pos += lambda_expr_to_string(expr->method_call.object, buf + pos, max_len - pos);
                            for (int i = 0; i < expr->method_call.arg_count; i++) {
                                pos += snprintf(buf + pos, max_len - pos, ", ");
                                pos += lambda_expr_to_string(expr->method_call.args[i], buf + pos, max_len - pos);
                            }
                            pos += snprintf(buf + pos, max_len - pos, ")");
                        }
                    }
                } else {
                    // Object.method(args) -> method(object, args)
                    pos += snprintf(buf + pos, max_len - pos, "%s(", method_name);
                    pos += lambda_expr_to_string(expr->method_call.object, buf + pos, max_len - pos);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        pos += snprintf(buf + pos, max_len - pos, ", ");
                        pos += lambda_expr_to_string(expr->method_call.args[i], buf + pos, max_len - pos);
                    }
                    pos += snprintf(buf + pos, max_len - pos, ")");
                }
            }
            break;
        }
        case EXPR_CALL: {
            // fn(args) -> fn(args)
            if (expr->call.callee->type == EXPR_IDENT) {
                pos += snprintf(buf + pos, max_len - pos, "%.*s(", expr->call.callee->token.length, expr->call.callee->token.start);
                for (int i = 0; i < expr->call.arg_count; i++) {
                    if (i > 0) pos += snprintf(buf + pos, max_len - pos, ", ");
                    pos += lambda_expr_to_string(expr->call.args[i], buf + pos, max_len - pos);
                }
                pos += snprintf(buf + pos, max_len - pos, ")");
            } else {
                pos += snprintf(buf + pos, max_len - pos, "0");
            }
            break;
        }
        case EXPR_STRING:
            pos += snprintf(buf + pos, max_len - pos, "\"%.*s\"", expr->token.length, expr->token.start);
            break;
        case EXPR_FLOAT:
            pos += snprintf(buf + pos, max_len - pos, "%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_ASSIGN: {
            pos += snprintf(buf + pos, max_len - pos, "%.*s = ", expr->assign.name.length, expr->assign.name.start);
            pos += lambda_expr_to_string(expr->assign.value, buf + pos, max_len - pos);
            break;
        }
        case EXPR_BOOL:
            pos += snprintf(buf + pos, max_len - pos, "%.*s", expr->token.length, expr->token.start);
            break;
        default:
            pos += snprintf(buf + pos, max_len - pos, "0");
            break;
    }
    return pos;
}

static int lambda_stmt_to_string(Stmt* stmt, char* buf, int max_len) {
    if (!stmt) return 0;
    int pos = 0;
    if (stmt->type == STMT_VAR) {
        pos += snprintf(buf + pos, max_len - pos, "    long long %.*s = ",
            stmt->var.name.length, stmt->var.name.start);
        if (stmt->var.init) pos += lambda_expr_to_string(stmt->var.init, buf + pos, max_len - pos);
        else pos += snprintf(buf + pos, max_len - pos, "0");
        pos += snprintf(buf + pos, max_len - pos, ";\n");
    } else if (stmt->type == STMT_EXPR) {
        pos += snprintf(buf + pos, max_len - pos, "    ");
        pos += lambda_expr_to_string(stmt->expr, buf + pos, max_len - pos);
        pos += snprintf(buf + pos, max_len - pos, ";\n");
    }
    return pos;
}

static void scan_expr_for_lambdas(Expr* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_LAMBDA:
            // Found a lambda! Generate it now
            lambda_id_counter++;
            int lambda_id = lambda_id_counter;
            
            // Detect captured variables — recursively collect all identifiers in body
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
            
            char* func_code = malloc(8192);
            int pos = 0;
            
            if (capture_count > 0 && in_return_lambda) {
                // Generate closure-style: env struct + function taking void* env
                pos += snprintf(func_code + pos, 8192 - pos, 
                    "typedef struct { ");
                for (int i = 0; i < capture_count; i++) {
                    pos += snprintf(func_code + pos, 8192 - pos, "long long %s; ", captured_vars[i]);
                }
                pos += snprintf(func_code + pos, 8192 - pos, 
                    "} __closure_env_%d;\n", lambda_id);
                pos += snprintf(func_code + pos, 8192 - pos, 
                    "long long __lambda_%d(void* __env", lambda_id);
                for (int i = 0; i < expr->lambda.param_count; i++) {
                    pos += snprintf(func_code + pos, 8192 - pos, ", long long %.*s",
                        expr->lambda.params[i].length, expr->lambda.params[i].start);
                }
                pos += snprintf(func_code + pos, 8192 - pos, ") {\n");
                pos += snprintf(func_code + pos, 8192 - pos, 
                    "    __closure_env_%d* __e = (__closure_env_%d*)__env;\n", lambda_id, lambda_id);
                // Unpack captured vars from env
                for (int i = 0; i < capture_count; i++) {
                    pos += snprintf(func_code + pos, 8192 - pos, 
                        "    long long %s = __e->%s;\n", captured_vars[i], captured_vars[i]);
                }
                // Emit body statements (multiline lambda)
                for (int si = 0; si < expr->lambda.body_stmt_count; si++)
                    pos += lambda_stmt_to_string(expr->lambda.body_stmts[si], func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, "    return ");
                pos += lambda_expr_to_string(expr->lambda.body, func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, ";\n}\n");
            } else {
                // Non-return lambda with captures: use static globals
                if (capture_count > 0) {
                    for (int i = 0; i < capture_count; i++) {
                        extern int is_known_array_var(const char*);
                        const char* cap_type = is_scan_array_var(captured_vars[i]) ? "WynArray" : "long long";
                        pos += snprintf(func_code + pos, 8192 - pos, "static %s __cap_%d_%s;\n", cap_type, lambda_id, captured_vars[i]);
                    }
                }
                pos += snprintf(func_code + pos, 8192 - pos, "long long __lambda_%d(", lambda_id);
                for (int i = 0; i < expr->lambda.param_count; i++) {
                    if (i > 0) pos += snprintf(func_code + pos, 8192 - pos, ", ");
                    pos += snprintf(func_code + pos, 8192 - pos, "long long %.*s", 
                                   expr->lambda.params[i].length, expr->lambda.params[i].start);
                }
                pos += snprintf(func_code + pos, 8192 - pos, ") {\n");
                if (capture_count > 0) {
                    for (int i = 0; i < capture_count; i++) {
                        // Use #define to alias variable to global (capture by reference)
                        pos += snprintf(func_code + pos, 8192 - pos, "#define %s __cap_%d_%s\n", captured_vars[i], lambda_id, captured_vars[i]);
                    }
                }
                // Emit body statements (multiline lambda)
                for (int si = 0; si < expr->lambda.body_stmt_count; si++)
                    pos += lambda_stmt_to_string(expr->lambda.body_stmts[si], func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, "    return ");
                pos += lambda_expr_to_string(expr->lambda.body, func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, ";\n}\n");
                // Undef capture aliases
                for (int i = 0; i < capture_count; i++)
                    pos += snprintf(func_code + pos, 8192 - pos, "#undef %s\n", captured_vars[i]);
            }
            
            if (lambda_count < 256) {
                lambda_functions[lambda_count].code = func_code;
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
            // Detect x.push(spawn ...) — register x as spawn array
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
            // Also scan await expressions — they may contain spawn
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
                if (!already_added && spawn_wrapper_count < 256) {
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
                    extern int function_can_inline(const char*);
                    spawn_wrappers[spawn_wrapper_count].can_inline = function_can_inline(func_name);
                    spawn_wrapper_count++;
                }
            }
            break;
        case EXPR_PIPELINE:
            for (int i = 0; i < expr->pipeline.stage_count; i++) {
                scan_expr_for_lambdas(expr->pipeline.stages[i]);
            }
            break;
        default:
            break;
    }
}

static void scan_for_lambdas(Stmt* body) {
    scan_stmt_for_lambdas(body);
}

