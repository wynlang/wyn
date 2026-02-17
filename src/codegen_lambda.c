// codegen_lambda.c - Lambda scanning and generation
// Included from codegen.c - shares all statics

static void scan_expr_for_lambdas(Expr* expr);

static void scan_stmt_for_lambdas(Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_VAR:
            if (stmt->var.init) scan_expr_for_lambdas(stmt->var.init);
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
            if (stmt->for_stmt.body) scan_stmt_for_lambdas(stmt->for_stmt.body);
            break;
        case STMT_SPAWN:
            // Collect spawn wrappers
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT &&
                stmt->spawn.call->call.arg_count == 0) {
                
                Expr* callee = stmt->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                // Add to spawn wrappers (track by func_name + arg_count)
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
                    spawn_wrapper_count++;
                }
            }
            break;
        default:
            break;
    }
}

// Helper to generate lambda body expression to string
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
            
            if (c_func) {
                pos += snprintf(buf + pos, max_len - pos, "%s(", c_func);
                pos += lambda_expr_to_string(expr->method_call.object, buf + pos, max_len - pos);
                pos += snprintf(buf + pos, max_len - pos, ")");
            } else {
                pos += snprintf(buf + pos, max_len - pos, "0");
            }
            break;
        }
        default:
            pos += snprintf(buf + pos, max_len - pos, "0");
            break;
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
            
            // Detect captured variables (simple: check if body uses identifiers not in params)
            char captured_vars[16][64];
            int capture_count = 0;
            
            if (expr->lambda.body->type == EXPR_BINARY) {
                Expr* bin = expr->lambda.body;
                // Check left operand
                if (bin->binary.left->type == EXPR_IDENT) {
                    int is_param = 0;
                    for (int i = 0; i < expr->lambda.param_count; i++) {
                        if (expr->lambda.params[i].length == bin->binary.left->token.length &&
                            memcmp(expr->lambda.params[i].start, bin->binary.left->token.start, 
                                   expr->lambda.params[i].length) == 0) {
                            is_param = 1;
                            break;
                        }
                    }
                    if (!is_param && capture_count < 16) {
                        snprintf(captured_vars[capture_count], 64, "%.*s", 
                                bin->binary.left->token.length, bin->binary.left->token.start);
                        capture_count++;
                    }
                }
                // Check right operand
                if (bin->binary.right->type == EXPR_IDENT) {
                    int is_param = 0;
                    for (int i = 0; i < expr->lambda.param_count; i++) {
                        if (expr->lambda.params[i].length == bin->binary.right->token.length &&
                            memcmp(expr->lambda.params[i].start, bin->binary.right->token.start, 
                                   expr->lambda.params[i].length) == 0) {
                            is_param = 1;
                            break;
                        }
                    }
                    if (!is_param && capture_count < 16) {
                        // Check if already captured
                        int already = 0;
                        for (int i = 0; i < capture_count; i++) {
                            if (strcmp(captured_vars[i], "") != 0) {
                                char temp[64];
                                snprintf(temp, 64, "%.*s", bin->binary.right->token.length, 
                                        bin->binary.right->token.start);
                                if (strcmp(captured_vars[i], temp) == 0) {
                                    already = 1;
                                    break;
                                }
                            }
                        }
                        if (!already) {
                            snprintf(captured_vars[capture_count], 64, "%.*s", 
                                    bin->binary.right->token.length, bin->binary.right->token.start);
                            capture_count++;
                        }
                    }
                }
            }
            
            char* func_code = malloc(8192);
            int pos = 0;
            
            if (in_return_lambda && capture_count > 0) {
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
                pos += snprintf(func_code + pos, 8192 - pos, "    return ");
                pos += lambda_expr_to_string(expr->lambda.body, func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, ";\n}\n");
            } else {
                // Original style: captured vars as extra params
                pos += snprintf(func_code + pos, 8192 - pos, "long long __lambda_%d(", lambda_id);
                for (int i = 0; i < capture_count; i++) {
                    if (i > 0) pos += snprintf(func_code + pos, 8192 - pos, ", ");
                    pos += snprintf(func_code + pos, 8192 - pos, "long long %s", captured_vars[i]);
                }
                for (int i = 0; i < expr->lambda.param_count; i++) {
                    if (i > 0 || capture_count > 0) pos += snprintf(func_code + pos, 8192 - pos, ", ");
                    pos += snprintf(func_code + pos, 8192 - pos, "long long %.*s", 
                                   expr->lambda.params[i].length, expr->lambda.params[i].start);
                }
                pos += snprintf(func_code + pos, 8192 - pos, ") {\n    return ");
                pos += lambda_expr_to_string(expr->lambda.body, func_code + pos, 8192 - pos);
                pos += snprintf(func_code + pos, 8192 - pos, ";\n}\n");
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
            break;
        case EXPR_SPAWN:
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
                        break;
                    }
                }
                if (!already_added && spawn_wrapper_count < 256) {
                    strcpy(spawn_wrappers[spawn_wrapper_count].func_name, func_name);
                    spawn_wrappers[spawn_wrapper_count].arg_count = arg_count;
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

