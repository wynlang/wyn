// codegen_expr.c - Expression code generation
// Included from codegen.c - shares all statics

// Pick the hashmap_insert_* function for a value expression by its type. Consults
// BOTH the literal node type and the checker-resolved expr_type, so a typed
// non-literal value (e.g. `{k: someStringVar}`) inserts with the right function
// instead of silently defaulting to int. Shared by all three map-store sites
// (.set/.insert, map literal, m[k]=v) so they can't drift apart. Default int.
static const char* hashmap_insert_fn_for(Expr* value_expr) {
    if (!value_expr) return "hashmap_insert_int";
    if (value_expr->type == EXPR_STRING) return "hashmap_insert_string";
    if (value_expr->type == EXPR_FLOAT)  return "hashmap_insert_float";
    if (value_expr->type == EXPR_BOOL)   return "hashmap_insert_bool";
    if (value_expr->expr_type) {
        if (value_expr->expr_type->kind == TYPE_STRING) return "hashmap_insert_string";
        if (value_expr->expr_type->kind == TYPE_FLOAT)  return "hashmap_insert_float";
        if (value_expr->expr_type->kind == TYPE_BOOL)   return "hashmap_insert_bool";
    }
    return "hashmap_insert_int";
}

// A string pushed into an array transfers ownership: array_push_str stores the
// pointer without retaining, and array_free releases it. So a local string var
// pushed into an array must NOT also be released at scope exit - that would
// double-free the live element (UAF on read, or empty-string reads after free).
// Mirror the return/assignment move idiom: if the pushed value is a tracked
// string variable, MOVE it (unregister so scope exit skips it) when it's dead
// after this statement, otherwise RETAIN it (the array now co-owns a reference).
// Fresh temporaries (literals, concat results) are not tracked vars, so nothing
// is emitted for them. Call this AFTER the push expression has been emitted.
static void codegen_string_push_transfer(Expr* value) {
    if (!value || value->type != EXPR_IDENT) return;
    char _pvn[256]; token_to_cstr(_pvn, sizeof(_pvn), value->token);
    extern int is_string_var(const char*);
    if (!is_string_var(_pvn)) return;
    // string_var_releasable/_count are statics from codegen.c (this file is #included there)
    extern int var_is_live_after(Stmt**, int, int, const char*);
    extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
    // A releasable (top-level/outer) var is co-owned: retain so the scope-exit
    // release and array_free each balance an owner.
    bool _is_releasable = false;
    for (int i = 0; i < string_var_releasable_count; i++)
        if (strcmp(string_var_releasable[i], _pvn) == 0) { _is_releasable = true; break; }
    if (!_is_releasable && current_block_stmts &&
        !var_is_live_after(current_block_stmts, current_block_count, current_stmt_idx, _pvn)) {
        // Move: local var is dead after this push - array takes sole ownership.
        extern void unregister_string_var(const char*);
        unregister_string_var(_pvn);
    } else {
        // Copy: var is still live (or outer-scope releasable) - array co-owns.
        emit("; wyn_rc_retain((void*)(intptr_t)%s)", _pvn);
    }
}

// Map an Option/Result constructor's PAYLOAD type to the concrete monomorphic
// family name used by the runtime: string payload -> "OptionString"/"ResultString",
// everything else -> "OptionInt"/"ResultInt" (the int family is the catch-all the
// runtime uses for int/bool/float/struct payloads today). `kind` is "Option" or
// "Result". Keying off the payload's own type - rather than the node's resolved
// type - is what makes bare `Some(x)`/`Ok(x)`/`Err(x)` lower correctly outside a
// function-return context (e.g. `var o: string? = Some(s)`). Returns a static
// string constant; NULL only if payload type is unknown.
static const char* wyn_ctor_family(Type* payload, const char* kind) {
    // Suffix from the payload's own kind: string->String, float->Float,
    // bool->Bool, a user struct -> the struct's own name (a monomorphic
    // Option<Struct>/Result<Struct> family emitted per-program), everything else
    // (int/…) falls back to the Int family.
    static char buf[128];
    if (payload && payload->kind == TYPE_STRUCT && payload->struct_type.name.length > 0) {
        char sname[96]; token_to_cstr(sname, sizeof(sname), payload->struct_type.name);
        // Only Option currently supports a struct payload family; Result<Struct,_>
        // stays on the Int catch-all until a Result-struct family exists.
        if (strcmp(kind, "Option") == 0) {
            extern void register_option_struct(const char*);
            register_option_struct(sname);
            snprintf(buf, sizeof(buf), "Option%s", sname);
            return buf;
        }
    }
    const char* suf = "Int";
    if (payload) {
        if (payload->kind == TYPE_STRING) suf = "String";
        else if (payload->kind == TYPE_FLOAT) suf = "Float";
        else if (payload->kind == TYPE_BOOL) suf = "Bool";
    }
    snprintf(buf, sizeof(buf), "%s%s", kind, suf);
    return buf;
}

// Resolve the concrete Option/Result family for a Some/None/Ok/Err node.
// Precedence: (1) the assignment-target annotation, (2) the enclosing function
// return kind - both name the exact declared family and must win over inference;
// (3) the payload's own type (so bare `Some(x)`/`Ok(x)`/`Err(x)` work anywhere);
// (4) the int family as the catch-all default. `kind` is "Option" or "Result".
static const char* wyn_option_ctor_kind(Expr* e, const char* kind) {
    extern const char* current_assign_target_kind;
    extern const char* current_fn_return_kind;
    if (current_assign_target_kind && strncmp(current_assign_target_kind, kind, strlen(kind)) == 0)
        return current_assign_target_kind;
    if (current_fn_return_kind && strncmp(current_fn_return_kind, kind, strlen(kind)) == 0)
        return current_fn_return_kind;
    return wyn_ctor_family(e->option.value ? e->option.value->expr_type : NULL, kind);
}

void codegen_expr(Expr* expr) {
    if (!expr) return;
    // If this expr was pre-evaluated to a temp, emit the temp name
    if (expr->_codegen_temp_id >= 0 && expr->_codegen_temp_id < 1000) {
        emit("__sa%d", expr->_codegen_temp_id);
        return;
    }
    if (expr->_codegen_temp_id >= 1000) {
        emit("__mo%d", expr->_codegen_temp_id - 1000);
        return;
    }
    
    switch (expr->type) {
        case EXPR_INT: {
            // Handle binary literals (0b1010)
            if (expr->token.length > 2 && expr->token.start[0] == '0' && 
                (expr->token.start[1] == 'b' || expr->token.start[1] == 'B')) {
                // Convert binary to decimal
                long long value = 0;
                for (int i = 2; i < expr->token.length; i++) {
                    if (expr->token.start[i] == '0' || expr->token.start[i] == '1') {
                        value = value * 2 + (expr->token.start[i] - '0');
                    }
                }
                emit("%lld", value);
            }
            // Handle numbers with underscores (1_000)
            else if (memchr(expr->token.start, '_', expr->token.length)) {
                // Emit without underscores
                for (int i = 0; i < expr->token.length; i++) {
                    if (expr->token.start[i] != '_') {
                        emit("%c", expr->token.start[i]);
                    }
                }
            }
            // Regular int
            else {
                emit("%.*s", expr->token.length, expr->token.start);
            }
            break;
        }
        case EXPR_FLOAT:
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_STRING: {
            // Check for multi-line string (""")
            bool is_multiline = (expr->token.length >= 6 && 
                                expr->token.start[0] == '"' && 
                                expr->token.start[1] == '"' && 
                                expr->token.start[2] == '"');
            
            int start_offset = is_multiline ? 3 : 1;
            int end_offset = is_multiline ? 3 : 1;
            
            // Emit string literal with proper C escape sequences
            emit("\"");
            for (int i = start_offset; i < expr->token.length - end_offset; i++) {
                char c = expr->token.start[i];
                if (c == '\\' && i + 1 < expr->token.length - end_offset) {
                    char next = expr->token.start[i + 1];
                    // Emit escape sequences as-is for C
                    emit("\\%c", next);
                    i++;
                } else if (c == '"') {
                    emit("\\\"");
                } else if (c == '\n') {
                    emit("\\n");
                } else {
                    emit("%c", c);
                }
            }
            emit("\"");
            break;
        }
        case EXPR_CHAR:
            emit("'%.*s'", expr->token.length - 2, expr->token.start + 1);
            break;
        case EXPR_IDENT: {
            // Convert :: to _ for C compatibility (e.g., Status::DONE -> Status_DONE)
            char* ident = malloc(expr->token.length + 256); // Extra space for alias resolution
            int offset = 0;
            
            // Check if this is a module.function call and resolve alias
            char temp_ident[512];
            token_to_cstr(temp_ident, sizeof temp_ident, expr->token);
            
            // If we're inside a module function, check if this identifier needs module prefix
            if (current_module_prefix && !strchr(temp_ident, ':') && !strchr(temp_ident, '.')) {
                // Check if this is a parameter - never prefix parameters
                if (is_parameter(temp_ident)) {
                    // Dereference mut parameters
                    if (is_mut_parameter(temp_ident)) {
                        emit("(*%s)", temp_ident);
                    } else {
                        emit("%s", temp_ident);
                    }
                    free(ident);
                    break;
                }
                
                // Check if this is a local variable - never prefix local variables
                if (is_local_variable(temp_ident)) {
                    // This is a local variable, emit as-is
                    emit("%s", temp_ident);
                    free(ident);
                    break;
                }
                
                // A KNOWN function of this module always gets the prefix - before
                // any name heuristics. `path` is in common_locals below, so a
                // sibling call to `pub fn path(..)` emitted the bare name and the
                // C compiler saw an undefined identifier (bug M1, 2026-07-18).
                if (is_module_function(temp_ident)) {
                    emit("%s_%s", current_module_prefix, temp_ident);
                    free(ident);
                    break;
                }

                // Check if this is a simple identifier (no :: or .)
                // For module-level identifiers, we need to prefix them
                // Heuristic: uppercase = constant, lowercase = might be module var or local var
                bool looks_like_module_level = (temp_ident[0] >= 'A' && temp_ident[0] <= 'Z') ||
                                               (temp_ident[0] >= 'a' && temp_ident[0] <= 'z');
                
                // Don't prefix common local variable names or single-letter variables
                bool is_single_letter = (strlen(temp_ident) == 1);
                const char* common_locals[] = {"i", "j", "k", "x", "y", "z", "n", "result", "temp", "value", 
                                               "a", "b", "c", "d", "e", "f", "g", "h", "m", "p", "q", "r", "s", "t",
                                               "content", "path", "text", "count", "lines", "words", NULL};
                bool is_common_local = false;
                for (int i = 0; common_locals[i] != NULL; i++) {
                    if (strcmp(temp_ident, common_locals[i]) == 0) {
                        is_common_local = true;
                        break;
                    }
                }
                
                if (looks_like_module_level && !is_common_local && !is_single_letter) {
                    // Try prefixing - if it doesn't exist, C compiler will error
                    emit("%s_%s", current_module_prefix, temp_ident);
                    free(ident);
                    break;
                }
            }
            
            // Check if this is a module.function or module::function call and resolve alias
            char* dot = strchr(temp_ident, '.');
            char* colon = strstr(temp_ident, "::");
            
            if (colon) {
                // Handle module::function syntax
                char function_part[256];
                strcpy(function_part, colon + 2);  // Save function name
                *colon = '\0';  // Split at ::
                
                // Special handling for C_Parser:: module - map to wyn_c_ prefix
                if (strcmp(temp_ident, "C_Parser") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "wyn_c_%s", function_part);
                    // Skip the normal :: to _ conversion
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for HashMap:: module - map to hashmap_ prefix
                if (strcmp(temp_ident, "HashMap") == 0) {
                    if (strcmp(function_part, "get") == 0)
                        snprintf(temp_ident, sizeof(temp_ident), "hashmap_get_string");
                    else if (strcmp(function_part, "set") == 0)
                        snprintf(temp_ident, sizeof(temp_ident), "hashmap_set");
                    else if (strcmp(function_part, "has") == 0)
                        snprintf(temp_ident, sizeof(temp_ident), "hashmap_has");
                    else
                        snprintf(temp_ident, sizeof(temp_ident), "hashmap_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for HashSet:: module - map to hashset_ prefix
                if (strcmp(temp_ident, "HashSet") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "hashset_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for Random:: module - map to random_ prefix
                if (strcmp(temp_ident, "Random") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "random_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for Color:: module - map to Color_ prefix.
                // The runtime exposes Color_red/green/... (string-returning ANSI
                // wrappers). Without this the generic ::->_ path mangled
                // `Color::green` to `_green` (module name dropped) → undefined
                // reference under gcc (clang happened to tolerate it). (2026-07)
                if (strcmp(temp_ident, "Color") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "Color_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }

                // Special handling for Time:: module - map to Time_ prefix.
                // The runtime exposes capitalized Time_* functions with the
                // arities the checker registers (Time_format(ts), Time_sleep(ms),
                // Time_now()); the lowercase time_* set is incomplete/mismatched
                // (e.g. time_format takes 2 args, time_sleep doesn't exist).
                if (strcmp(temp_ident, "Time") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "Time_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for System:: module - map to System_ prefix
                if (strcmp(temp_ident, "System") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "System_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Special handling for File:: module - all functions use file_ prefix (lowercase)
                if (strcmp(temp_ident, "File") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "file_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                // Resolve short name (http -> network/http)
                const char* full_path = resolve_short_module_name(temp_ident);
                const char* resolved = resolve_module_alias(full_path);
                
                // Rebuild identifier with resolved module name
                snprintf(temp_ident, sizeof(temp_ident), "%s::%s", resolved, function_part);
            } else if (dot) {
                // Handle module.function syntax
                char function_part[256];
                strcpy(function_part, dot + 1);  // Save function name
                *dot = '\0';  // Split at dot
                
                // Special handling for Random. module - map to random_ prefix
                if (strcmp(temp_ident, "Random") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "random_%s", function_part);
                    strcpy(ident + offset, temp_ident);
                    emit("%s", ident);
                    free(ident);
                    break;
                }
                
                const char* resolved = resolve_module_alias(temp_ident);
                // Rebuild identifier with resolved module name
                snprintf(temp_ident, sizeof(temp_ident), "%s.%s", resolved, function_part);
            }
            
            // Check if this is a user-defined name that collides with C/runtime
            extern int is_user_collision(const char*);
            if (is_user_collision(temp_ident)) {
                strcpy(ident, WYN_UFN_PFX);
                offset = WYN_UFN_PFX_LEN;
            }
            
            strcpy(ident + offset, temp_ident);
            
            // Replace :: with _ and / with _
            for (int i = offset; ident[i] && ident[i+1]; i++) {
                if (ident[i] == ':' && ident[i+1] == ':') {
                    ident[i] = '_';
                    // Shift rest of string left by 1
                    memmove(ident + i + 1, ident + i + 2, strlen(ident + i + 2) + 1);
                } else if (ident[i] == '/') {
                    ident[i] = '_';
                }
            }
            // Check last character for /
            int len = strlen(ident);
            if (len > 0 && ident[len-1] == '/') {
                ident[len-1] = '_';
            }
            
            // Check if this is a mut parameter that needs dereferencing
            if (is_mut_parameter(ident)) {
                emit("(*%s)", ident);
            } else {
                emit("%s", ident);
            }
            free(ident);
            break;
        }
        case EXPR_BOOL:
            emit("(bool)%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_UNARY:
            if (expr->unary.op.type == TOKEN_NOT || expr->unary.op.type == TOKEN_BANG) {
                emit("(bool)!");
            } else {
                emit("%.*s", expr->unary.op.length, expr->unary.op.start);
            }
            codegen_expr(expr->unary.operand);
            break;
        case EXPR_AWAIT: {
            // Await: get result from future
            // Check if the result should be a string
            bool is_string_return = false;
            Expr* inner = expr->await.expr;
            if (inner && inner->type == EXPR_SPAWN && inner->spawn.call &&
                inner->spawn.call->type == EXPR_CALL &&
                inner->spawn.call->call.callee->type == EXPR_IDENT) {
                char fn_name[256];
                token_to_cstr(fn_name, sizeof(fn_name), inner->spawn.call->call.callee->token);
                extern const char* get_function_return_type(const char* name);
                const char* rt = get_function_return_type(fn_name);
                if (rt && strcmp(rt, "string") == 0) is_string_return = true;
            }
            if (!is_string_return && inner && inner->type == EXPR_IDENT) {
                char vn[256]; token_to_cstr(vn, sizeof(vn), inner->token);
                extern int is_string_future(const char*);
                if (is_string_future(vn)) is_string_return = true;
            }
            // Check for struct return. Floats are boxed like structs - the
            // (void*)(intptr_t) word cast truncated 3.5 to 3 - so a float
            // return maps to the "double" boxed type here.
            const char* struct_return_type = NULL;
            if (!is_string_return) {
                if (inner && inner->type == EXPR_SPAWN && inner->spawn.call &&
                    inner->spawn.call->type == EXPR_CALL &&
                    inner->spawn.call->call.callee->type == EXPR_IDENT) {
                    char fn2[256]; token_to_cstr(fn2, sizeof(fn2), inner->spawn.call->call.callee->token);
                    extern const char* get_function_return_type(const char*);
                    const char* rt2 = get_function_return_type(fn2);
                    if (rt2 && strcmp(rt2, "float") == 0)
                        struct_return_type = "double";
                    else if (rt2 && strcmp(rt2, "int") != 0 && strcmp(rt2, "string") != 0 &&
                        strcmp(rt2, "bool") != 0)
                        struct_return_type = rt2;
                }
                if (!struct_return_type && inner && inner->type == EXPR_IDENT) {
                    char vn2[256]; token_to_cstr(vn2, sizeof(vn2), inner->token);
                    extern const char* get_struct_future_type(const char*);
                    struct_return_type = get_struct_future_type(vn2);
                }
            }
            // Inline `await spawn f()` temporaries have exactly one reader -
            // consume (get + recycle) to keep future memory constant. Named
            // futures (`await f`) use the memoizing get so awaiting twice
            // returns the same value instead of 0/garbage.
            const char* _getfn = (inner && inner->type == EXPR_SPAWN)
                ? "future_get_consume" : "future_get";
            if (struct_return_type) {
                emit("(*(%s*)%s((Future*)(intptr_t)", struct_return_type, _getfn);
                codegen_expr(inner);
                emit("))");
            } else if (is_string_return) {
                emit("(const char*)(intptr_t)%s((Future*)(intptr_t)", _getfn);
                codegen_expr(inner);
                emit(")");
            } else {
                emit("(long long)(intptr_t)%s((Future*)(intptr_t)", _getfn);
                codegen_expr(inner);
                emit(")");
            }
            break;
        }
        case EXPR_BINARY: {
            // Membership: `x in c` / `x not in c`. Dispatch on the container type:
            //   [T] array   -> array_contains / array_contains_str
            //   map          -> hashmap_has (key membership)
            //   set          -> hashset_contains
            //   string       -> wyn_string_contains (substring)
            if (expr->binary.op.type == TOKEN_IN) {
                Expr* elem = expr->binary.left;
                Expr* cont = expr->binary.right;
                Type* ct = cont->expr_type;
                bool neg = expr->binary.is_not_in;
                emit("(bool)(");
                if (neg) emit("!(");
                if (ct && ct->kind == TYPE_MAP) {
                    emit("hashmap_has("); codegen_expr(cont); emit(", "); codegen_expr(elem); emit(")");
                } else if (ct && ct->kind == TYPE_SET) {
                    emit("hashset_contains("); codegen_expr(cont); emit(", "); codegen_expr(elem); emit(")");
                } else if (ct && ct->kind == TYPE_STRING) {
                    emit("wyn_string_contains("); codegen_expr(cont); emit(", "); codegen_expr(elem); emit(")");
                } else {
                    // Array (default). Pick str vs int element form.
                    bool elem_is_str = (elem->type == EXPR_STRING) ||
                        (elem->expr_type && elem->expr_type->kind == TYPE_STRING) ||
                        (ct && ct->kind == TYPE_ARRAY && ct->array_type.element_type &&
                         ct->array_type.element_type->kind == TYPE_STRING);
                    emit(elem_is_str ? "array_contains_str(" : "array_contains(");
                    codegen_expr(cont); emit(", "); codegen_expr(elem); emit(")");
                }
                if (neg) emit(")");
                emit(")");
                break;
            }
            // Check if this is a boolean-producing operator
            bool _is_bool_op = (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ ||
                expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP ||
                expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE);
            if (_is_bool_op) emit("(bool)");
            // Constant folding: if both sides are int literals, compute at compile time
            if (expr->binary.left->type == EXPR_INT && expr->binary.right->type == EXPR_INT) {
                long long l = strtoll(expr->binary.left->token.start, NULL, 0);
                long long r = strtoll(expr->binary.right->token.start, NULL, 0);
                long long result = 0;
                bool folded = true;
                switch (expr->binary.op.type) {
                    case TOKEN_PLUS: result = l + r; break;
                    case TOKEN_MINUS: result = l - r; break;
                    case TOKEN_STAR: result = l * r; break;
                    case TOKEN_SLASH: if (r == 0) folded = false; else result = l / r; break;
                    case TOKEN_PERCENT: if (r == 0) folded = false; else result = l % r; break;
                    case TOKEN_EQEQ: result = l == r; break;
                    case TOKEN_BANGEQ: result = l != r; break;
                    case TOKEN_LT: result = l < r; break;
                    case TOKEN_LTEQ: result = l <= r; break;
                    case TOKEN_GT: result = l > r; break;
                    case TOKEN_GTEQ: result = l >= r; break;
                    default: folded = false; break;
                }
                if (folded) { emit("%lld", result); break; }
            }
            // String repeat: "ha" * 3 → string_repeat("ha", 3)
            if (expr->binary.op.type == TOKEN_STAR) {
                bool left_is_str = (expr->binary.left->type == EXPR_STRING) ||
                    (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                if (left_is_str) {
                    emit("string_repeat(");
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                    break;
                }
            }

            // Special handling for string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS) {
                // Check if either operand is actually a string type. Interp
                // literals count: `"v=${a}" + "v=${b}"` used to emit raw C
                // `char* + char*` (invalid) because neither side matched.
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->type == EXPR_STRING_INTERP) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->type == EXPR_STRING_INTERP) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                // .to_string() always returns string
                if (!left_is_string && expr->binary.left->type == EXPR_METHOD_CALL &&
                    expr->binary.left->method_call.method.length == 9 &&
                    memcmp(expr->binary.left->method_call.method.start, "to_string", 9) == 0)
                    left_is_string = true;
                if (!right_is_string && expr->binary.right->type == EXPR_METHOD_CALL &&
                    expr->binary.right->method_call.method.length == 9 &&
                    memcmp(expr->binary.right->method_call.method.start, "to_string", 9) == 0)
                    right_is_string = true;
                
                bool left_is_int = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_INT);
                bool right_is_int = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_INT);
                
                // Also check if it's an int literal
                if (!left_is_int && expr->binary.left->type == EXPR_INT) left_is_int = true;
                if (!right_is_int && expr->binary.right->type == EXPR_INT) right_is_int = true;
                
                // Check if variable name suggests string type - USE TYPE INFO INSTEAD
                if (!left_is_string && expr->binary.left->type == EXPR_IDENT) {
                    if (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING) {
                        left_is_string = true;
                        left_is_int = false;
                    }
                    // Also check codegen's string var tracking
                    if (!left_is_string) {
                        char _ln[256]; token_to_cstr(_ln, sizeof(_ln), expr->binary.left->token);
                        extern int is_string_var(const char*);
                        if (is_string_var(_ln)) { left_is_string = true; left_is_int = false; }
                    }
                }
                if (!right_is_string && expr->binary.right->type == EXPR_IDENT) {
                    if (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING) {
                        right_is_string = true;
                        right_is_int = false;
                    }
                    if (!right_is_string) {
                        char _rn[256]; token_to_cstr(_rn, sizeof(_rn), expr->binary.right->token);
                        extern int is_string_var(const char*);
                        if (is_string_var(_rn)) { right_is_string = true; right_is_int = false; }
                    }
                }
                // Check EXPR_INDEX on string arrays
                if (!left_is_string && expr->binary.left->type == EXPR_INDEX &&
                    expr->binary.left->index.array->type == EXPR_IDENT) {
                    char _an[256]; token_to_cstr(_an, sizeof(_an), expr->binary.left->index.array->token);
                    extern int is_str_array_var(const char*);
                    if (is_str_array_var(_an)) { left_is_string = true; left_is_int = false; }
                }
                if (!right_is_string && expr->binary.right->type == EXPR_INDEX &&
                    expr->binary.right->index.array->type == EXPR_IDENT) {
                    char _an[256]; token_to_cstr(_an, sizeof(_an), expr->binary.right->index.array->token);
                    extern int is_str_array_var(const char*);
                    if (is_str_array_var(_an)) { right_is_string = true; right_is_int = false; }
                }
                
                // Check EXPR_METHOD_CALL that returns string
                for (int _side = 0; _side < 2; _side++) {
                    Expr* _e = _side == 0 ? expr->binary.left : expr->binary.right;
                    bool* _is_str = _side == 0 ? &left_is_string : &right_is_string;
                    bool* _is_int = _side == 0 ? &left_is_int : &right_is_int;
                    if (!*_is_str && _e->type == EXPR_METHOD_CALL) {
                        Token _m = _e->method_call.method;
                        if ((_m.length == 4 && memcmp(_m.start, "join", 4) == 0) ||
                            (_m.length == 5 && memcmp(_m.start, "upper", 5) == 0) ||
                            (_m.length == 5 && memcmp(_m.start, "lower", 5) == 0) ||
                            (_m.length == 4 && memcmp(_m.start, "trim", 4) == 0) ||
                            (_m.length == 7 && memcmp(_m.start, "replace", 7) == 0) ||
                            (_m.length == 6 && memcmp(_m.start, "repeat", 6) == 0) ||
                            (_m.length == 9 && memcmp(_m.start, "substring", 9) == 0) ||
                            (_m.length == 9 && memcmp(_m.start, "to_string", 9) == 0) ||
                            (_m.length == 8 && memcmp(_m.start, "pad_left", 8) == 0) ||
                            (_m.length == 9 && memcmp(_m.start, "pad_right", 9) == 0))
                            { *_is_str = true; *_is_int = false; }
                        // Check struct method return type
                        if (!*_is_str && _e->method_call.object->type == EXPR_IDENT) {
                            char _on[64]; token_to_cstr(_on, sizeof(_on), _e->method_call.object->token);
                            extern const char* get_struct_var_type(const char*);
                            const char* _st = get_struct_var_type(_on);
                            if (_st) {
                                char _mn[64]; token_to_cstr(_mn, sizeof(_mn), _m);
                                extern const char* lookup_struct_method_return_type(const char*, const char*);
                                const char* _rt = lookup_struct_method_return_type(_st, _mn);
                                if (_rt && strcmp(_rt, "string") == 0) { *_is_str = true; *_is_int = false; }
                            }
                        }
                        // Check function return type for fn().method() chains
                        if (!*_is_str && _e->method_call.object->type == EXPR_CALL &&
                            _e->method_call.object->call.callee->type == EXPR_IDENT) {
                            char _fn[64]; token_to_cstr(_fn, sizeof(_fn), _e->method_call.object->call.callee->token);
                            extern const char* get_function_return_type(const char*);
                            const char* _frt = get_function_return_type(_fn);
                            if (_frt && strcmp(_frt, "string") == 0) { *_is_str = true; *_is_int = false; }
                        }
                    }
                }
                
                if (left_is_string || right_is_string) {
                    // ARC-managed concat - release temporaries after concat
                    // Left temp: only release if it's a chained concat/call (not reused by realloc)
                    // Right temp: always release (never reused)
                    bool left_is_temp = (left_is_int && !left_is_string) ||
                                        expr->binary.left->type == EXPR_CALL ||
                                        expr->binary.left->type == EXPR_METHOD_CALL ||
                                        expr->binary.left->type == EXPR_BINARY;
                    bool right_is_temp = (right_is_int && !right_is_string) ||
                                         expr->binary.right->type == EXPR_CALL ||
                                         expr->binary.right->type == EXPR_METHOD_CALL ||
                                         expr->binary.right->type == EXPR_BINARY;
                    
                    if (left_is_temp || right_is_temp) {
                        emit("({ ");
                        if (left_is_temp) {
                            emit("const char* __cl = ");
                            if (left_is_int && !left_is_string) { emit("int_to_string("); codegen_expr(expr->binary.left); emit(")"); }
                            else codegen_expr(expr->binary.left);
                            emit("; ");
                        }
                        if (right_is_temp) {
                            emit("const char* __cr = ");
                            if (right_is_int && !right_is_string) { emit("int_to_string("); codegen_expr(expr->binary.right); emit(")"); }
                            else codegen_expr(expr->binary.right);
                            emit("; ");
                        }
                        emit("const char* __cx = wyn_string_concat_safe(");
                        if (left_is_temp) emit("__cl");
                        else { if (left_is_int && !left_is_string) { emit("int_to_string("); codegen_expr(expr->binary.left); emit(")"); } else codegen_expr(expr->binary.left); }
                        emit(", ");
                        if (right_is_temp) emit("__cr");
                        else { if (right_is_int && !right_is_string) { emit("int_to_string("); codegen_expr(expr->binary.right); emit(")"); } else codegen_expr(expr->binary.right); }
                        emit(");");
                        // Release temps - but only if concat didn't reuse them
                        // concat reuses left when refcount==1, so check if result != left
                        if (left_is_temp) emit(" if (__cx != __cl) wyn_rc_release(__cl);");
                        if (right_is_temp) emit(" wyn_rc_release(__cr);");
                        emit(" __cx; })");
                    } else {
                        emit("wyn_string_concat_safe(");
                        codegen_expr(expr->binary.left);
                        emit(", ");
                        codegen_expr(expr->binary.right);
                        emit(")");
                    }
                    break;
                }
            }
            
            // Special handling for string comparison operators
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ ||
                expr->binary.op.type == TOKEN_LT || expr->binary.op.type == TOKEN_GT ||
                expr->binary.op.type == TOKEN_LTEQ || expr->binary.op.type == TOKEN_GTEQ) {
                // Check if both operands are strings
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                if (left_is_string || right_is_string) {
                    // Use strcmp for string comparison
                    emit("(strcmp(");
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                    
                    // Map operator to strcmp result comparison
                    if (expr->binary.op.type == TOKEN_EQEQ) {
                        emit(" == 0");
                    } else if (expr->binary.op.type == TOKEN_BANGEQ) {
                        emit(" != 0");
                    } else if (expr->binary.op.type == TOKEN_LT) {
                        emit(" < 0");
                    } else if (expr->binary.op.type == TOKEN_GT) {
                        emit(" > 0");
                    } else if (expr->binary.op.type == TOKEN_LTEQ) {
                        emit(" <= 0");
                    } else if (expr->binary.op.type == TOKEN_GTEQ) {
                        emit(" >= 0");
                    }
                    emit(")");
                    break;
                }
            }
            
            // struct == struct: call the generated field-wise helper. Raw C
            // `==` on struct values doesn't compile (this was an ICE). Detect
            // via expr_type AND the struct-var registry - the checker often
            // doesn't propagate TYPE_STRUCT onto bare ident references.
            if (expr->binary.op.type == TOKEN_EQEQ || expr->binary.op.type == TOKEN_BANGEQ) {
                const char* _eqsn = NULL;
                Expr* _sides[2] = { expr->binary.left, expr->binary.right };
                for (int _si = 0; _si < 2 && !_eqsn; _si++) {
                    Expr* _e = _sides[_si];
                    if (_e->expr_type && _e->expr_type->kind == TYPE_STRUCT) {
                        Token _snt = _e->expr_type->struct_type.name.length > 0
                            ? _e->expr_type->struct_type.name : _e->expr_type->name;
                        if (_snt.length > 0) {
                            static char _eqsb[64]; token_to_cstr(_eqsb, sizeof(_eqsb), _snt);
                            _eqsn = _eqsb;
                        }
                    }
                    if (!_eqsn && _e->type == EXPR_IDENT) {
                        char _vn[128]; token_to_cstr(_vn, sizeof(_vn), _e->token);
                        extern const char* get_struct_var_type(const char*);
                        _eqsn = get_struct_var_type(_vn);
                    }
                }
                // Option families and the FFI `void*` ptr struct keep their
                // existing paths; only user structs route to the helper.
                if (_eqsn && strncmp(_eqsn, "Option", 6) != 0 && strchr(_eqsn, '*') == NULL) {
                    if (expr->binary.op.type == TOKEN_BANGEQ) emit("(!");
                    emit("__wyn_eq_%s(", _eqsn);
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                    if (expr->binary.op.type == TOKEN_BANGEQ) emit(")");
                    break;
                }
            }

            // Default binary expression handling

            // Special handling for nil coalescing operator ??
            if (expr->binary.op.type == TOKEN_QUESTION_QUESTION) {
                // Generate: (left->has_value ? *(int*)left->value : right)
                emit("(");
                codegen_expr(expr->binary.left);
                emit("->has_value ? *(int*)");
                codegen_expr(expr->binary.left);
                emit("->value : ");
                codegen_expr(expr->binary.right);
                emit(")");
            } else {
                // Check for division or modulo - add runtime check
                if (expr->binary.op.type == TOKEN_SLASH || expr->binary.op.type == TOKEN_PERCENT) {
                    bool lf = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_FLOAT) || expr->binary.left->type == EXPR_FLOAT;
                    bool rf = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_FLOAT) || expr->binary.right->type == EXPR_FLOAT;
                    if (lf || rf) {
                        emit("(");
                        if (!lf) emit("(double)");
                        codegen_expr(expr->binary.left);
                        emit(expr->binary.op.type == TOKEN_SLASH ? " / " : " %% ");
                        if (!rf) emit("(double)");
                        codegen_expr(expr->binary.right);
                        emit(")");
                    } else if (expr->binary.right->type == EXPR_INT) {
                        long long dv = strtoll(expr->binary.right->token.start, NULL, 0);
                        if (dv == 0) {
                            emit(expr->binary.op.type == TOKEN_SLASH ? "wyn_safe_div(" : "wyn_safe_mod(");
                            codegen_expr(expr->binary.left); emit(", 0)");
                        } else {
                            // Known non-zero constant - skip runtime check, use C division
                            emit("("); codegen_expr(expr->binary.left);
                            emit(expr->binary.op.type == TOKEN_SLASH ? " / " : " %% ");
                            codegen_expr(expr->binary.right); emit(")");
                        }
                    } else {
                        if (expr->binary.op.type == TOKEN_SLASH) emit("wyn_safe_div(");
                        else emit("wyn_safe_mod(");
                        codegen_expr(expr->binary.left);
                        emit(", ");
                        codegen_expr(expr->binary.right);
                        emit(")");
                    }
                } else {
                    // Strength reduction: x * 2 → x << 1 (only for int * int)
                    bool sr_left_float = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_FLOAT) || expr->binary.left->type == EXPR_FLOAT;
                    if (!sr_left_float && expr->binary.right->type == EXPR_INT) {
                        long long rv = strtoll(expr->binary.right->token.start, NULL, 0);
                        if (expr->binary.op.type == TOKEN_STAR && rv == 2) {
                            emit("("); codegen_expr(expr->binary.left); emit(" << 1)"); break;
                        }
                        if (expr->binary.op.type == TOKEN_STAR && rv == 4) {
                            emit("("); codegen_expr(expr->binary.left); emit(" << 2)"); break;
                        }
                        if (expr->binary.op.type == TOKEN_STAR && rv == 8) {
                            emit("("); codegen_expr(expr->binary.left); emit(" << 3)"); break;
                        }
                        if (expr->binary.op.type == TOKEN_SLASH && rv == 2) {
                            emit("("); codegen_expr(expr->binary.left); emit(" >> 1)"); break;
                        }
                        if (expr->binary.op.type == TOKEN_SLASH && rv == 4) {
                            emit("("); codegen_expr(expr->binary.left); emit(" >> 2)"); break;
                        }
                        if (expr->binary.op.type == TOKEN_PERCENT && rv == 2) {
                            emit("("); codegen_expr(expr->binary.left); emit(" & 1)"); break;
                        }
                    }
                    emit("(");
                    // Auto-promote int to float in mixed arithmetic
                    bool left_is_float = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_FLOAT) ||
                                        expr->binary.left->type == EXPR_FLOAT;
                    bool right_is_float = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_FLOAT) ||
                                         expr->binary.right->type == EXPR_FLOAT;
                    bool left_is_int = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_INT) ||
                                      expr->binary.left->type == EXPR_INT;
                    bool right_is_int = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_INT) ||
                                       expr->binary.right->type == EXPR_INT;
                    if (left_is_float && right_is_int) {
                        codegen_expr(expr->binary.left);
                        if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP) emit(" && ");
                        else if (expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE) emit(" || ");
                        else emit(" %.*s ", expr->binary.op.length, expr->binary.op.start);
                        emit("(double)"); codegen_expr(expr->binary.right);
                    } else if (left_is_int && right_is_float) {
                        emit("(double)"); codegen_expr(expr->binary.left);
                        if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP) emit(" && ");
                        else if (expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE) emit(" || ");
                        else emit(" %.*s ", expr->binary.op.length, expr->binary.op.start);
                        codegen_expr(expr->binary.right);
                    } else {
                        codegen_expr(expr->binary.left);
                        if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP) emit(" && ");
                        else if (expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE) emit(" || ");
                        else emit(" %.*s ", expr->binary.op.length, expr->binary.op.start);
                        codegen_expr(expr->binary.right);
                    }
                    emit(")");
                }
            }
            break;
        }
        case EXPR_CALL:
            // Handle assert() and assert_eq() for test blocks
            if (expr->call.callee->type == EXPR_IDENT) {
                Token fn = expr->call.callee->token;
                if (fn.length == 6 && memcmp(fn.start, "assert", 6) == 0 && expr->call.arg_count == 1) {
                    emit("wyn_assert(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                    break;
                }
                if (fn.length == 9 && memcmp(fn.start, "assert_eq", 9) == 0 && expr->call.arg_count == 2) {
                    // Type dispatch: string vs int
                    Type* arg_type = expr->call.args[0]->expr_type;
                    bool is_str = (arg_type && arg_type->kind == TYPE_STRING);
                    // Also check if first arg is a string literal or method returning string
                    if (!is_str && expr->call.args[0]->type == EXPR_STRING) is_str = true;
                    if (!is_str && expr->call.args[1]->type == EXPR_STRING) is_str = true;
                    if (is_str) {
                        emit("wyn_assert_eq_str(");
                    } else {
                        emit("wyn_assert_eq_int(");
                    }
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(")");
                    break;
                }
                // await_all(futures_array) → collect all results into a WynArray
                if (fn.length == 9 && memcmp(fn.start, "await_all", 9) == 0 && expr->call.arg_count == 1) {
                    // Check if the futures array holds string-returning spawns
                    bool _is_str_arr = false;
                    if (expr->call.args[0]->type == EXPR_IDENT) {
                        char _aan[256]; token_to_cstr(_aan, sizeof(_aan), expr->call.args[0]->token);
                        extern int is_string_spawn_array(const char*);
                        _is_str_arr = is_string_spawn_array(_aan);
                    }
                    emit("_Generic((");
                    codegen_expr(expr->call.args[0]);
                    if (_is_str_arr)
                        emit("), WynIntArray: wyn_await_all_int_str, WynArray: wyn_await_all_str)(");
                    else
                        emit("), WynIntArray: wyn_await_all_int, WynArray: wyn_await_all)(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                    break;
                }
                // await_any(futures_array) → return first completed result
                if (fn.length == 9 && memcmp(fn.start, "await_any", 9) == 0 && expr->call.arg_count == 1) {
                    emit("_Generic((");
                    codegen_expr(expr->call.args[0]);
                    emit("), WynIntArray: wyn_await_any_int, WynArray: wyn_await_any)(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                    break;
                }
                // HashMap::set(map, key, value) - pick the typed insert_* by the
                // VALUE's type. Without this, the qualified free-function form fell
                // through to hashmap_set(), which is hardcoded to insert_string and
                // stored a non-string value AS a char* → the paired read
                // (hashmap_get_*) then dereferenced a non-pointer and crashed. The
                // `m[k]=v` and `m.set()` forms already dispatch via hashmap_insert_fn_for;
                // this closes the last untyped store path.
                if (fn.length == 12 && memcmp(fn.start, "HashMap::set", 12) == 0 && expr->call.arg_count == 3) {
                    const char* insert_func = hashmap_insert_fn_for(expr->call.args[2]);
                    emit("%s(", insert_func);
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(", ");
                    codegen_expr(expr->call.args[2]);
                    emit(")");
                    break;
                }
            }
            // Check if callee is a closure variable (WynClosure)
            // Heuristic: if callee is an identifier that starts with lowercase
            // and is not a known built-in function, it might be a closure variable
            if (expr->call.callee->type == EXPR_IDENT && expr->call.arg_count >= 1) {
                Token fn_name = expr->call.callee->token;
                // Check if this is a variable holding a closure (assigned from a function returning WynClosure)
                // Simple check: if the identifier is tracked as a closure variable
                bool is_closure_var = false;
                for (int li = 0; li < lambda_var_count; li++) {
                    if (lambda_var_info[li].name_len == fn_name.length &&
                        memcmp(lambda_var_info[li].name, fn_name.start, fn_name.length) == 0 &&
                        lambda_var_info[li].is_closure) {
                        is_closure_var = true;
                        break;
                    }
                }
                if (is_closure_var && expr->call.arg_count == 1) {
                    // S3: call through the checker-typed signature. The old
                    // wyn_closure_call_int forced the int ABI, so a float
                    // closure (fn(float) -> float) returned garbage bits.
                    const char* _crc = "long long";
                    const char* _cpc = "long long";
                    Type* _ct = expr->call.callee->expr_type;
                    if (_ct && _ct->kind == TYPE_FUNCTION) {
                        if (_ct->fn_type.return_type) {
                            const char* t = codegen_c_type_from_type(_ct->fn_type.return_type);
                            if (t) _crc = t;
                        }
                        if (_ct->fn_type.param_count >= 1 && _ct->fn_type.param_types[0]) {
                            const char* t = codegen_c_type_from_type(_ct->fn_type.param_types[0]);
                            if (t) _cpc = t;
                        }
                    }
                    emit("({ WynClosure __c = ");
                    codegen_expr(expr->call.callee);
                    emit("; ((%s(*)(void*,%s))__c.fn)(__c.env, ", _crc, _cpc);
                    codegen_expr(expr->call.args[0]);
                    emit("); })");
                    break;
                }
            }
            // TASK-040: Special handling for higher-order functions
            if (expr->call.callee->type == EXPR_IDENT) {
                Token func_name = expr->call.callee->token;
                
                // Handle map function: map(array, lambda) - only if 2 args
                if (func_name.length == 3 && memcmp(func_name.start, "map", 3) == 0 && expr->call.arg_count == 2) {
                    emit("array_map(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // lambda
                    emit(")");
                    break;
                }
                
                // Handle filter function: filter(array, predicate) - only if 2 args
                if (func_name.length == 6 && memcmp(func_name.start, "filter", 6) == 0 && expr->call.arg_count == 2) {
                    emit("array_filter(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // predicate
                    emit(")");
                    break;
                }
                
                // Handle reduce function: reduce(array, func, initial)
                if (func_name.length == 6 && memcmp(func_name.start, "reduce", 6) == 0) {
                    emit("array_reduce(");
                    codegen_expr(expr->call.args[0]); // array
                    emit(", ");
                    codegen_expr(expr->call.args[1]); // function
                    emit(", ");
                    codegen_expr(expr->call.args[2]); // initial value
                    emit(")");
                    break;
                }
            }
            
            // Special handling for print function. Multi-arg and zero-arg
            // `println` route through the same path (true alias) - they used to
            // fall through to the raw C println macro, which is single-arg only
            // and died with an "internal codegen error" the checker never saw.
            // Single-arg println keeps its tuned fast path below.
            if (expr->call.callee->type == EXPR_IDENT &&
                ((expr->call.callee->token.length == 5 &&
                  memcmp(expr->call.callee->token.start, "print", 5) == 0) ||
                 (expr->call.callee->token.length == 7 &&
                  memcmp(expr->call.callee->token.start, "println", 7) == 0 &&
                  expr->call.arg_count != 1))) {
                
                // Python-style print(): print all positional args separated by a
                // space, then a terminator that defaults to "\n". A trailing named
                // `end=` (or `sep=`) argument overrides the terminator/separator.
                // `print(x)` == `println(x)`; `print(x, end="")` suppresses the
                // newline (the old no-newline behavior); `print(a, b)` joins with a
                // space. Emitted as one statement-expr so it stays a single value.
                {
                    // Split positional args from named end=/sep= args.
                    int npos = 0;
                    Expr* end_arg = NULL;   // string expr for the terminator (default "\n")
                    Expr* sep_arg = NULL;   // string expr for the separator (default " ")
                    Expr* pos[64];
                    for (int i = 0; i < expr->call.arg_count && npos < 64; i++) {
                        Token nm = expr->call.arg_names ? expr->call.arg_names[i] : (Token){0};
                        if (nm.length == 3 && memcmp(nm.start, "end", 3) == 0) { end_arg = expr->call.args[i]; continue; }
                        if (nm.length == 3 && memcmp(nm.start, "sep", 3) == 0) { sep_arg = expr->call.args[i]; continue; }
                        pos[npos++] = expr->call.args[i];
                    }

                    bool prev_skip = codegen_skip_strdup;
                    emit("({ ");
                    for (int i = 0; i < npos; i++) {
                        Expr* parg = pos[i];
                        if (i > 0) {
                            if (sep_arg) { emit("print_no_nl("); codegen_expr(sep_arg); emit("); "); }
                            else emit("printf(\" \"); ");
                        }
                        // Per-arg escape analysis + string-temp handling, mirroring
                        // the old single-arg path: a fresh string temp (interp /
                        // concat / non-string .to_string()) must be released after
                        // printing to avoid a leak, unless a statement-level wrapper
                        // already owns it (_codegen_temp_id >= 0).
                        if (parg->type == EXPR_STRING_INTERP) codegen_skip_strdup = true;
                        bool _binary_is_string = false;
                        if (parg->type == EXPR_BINARY) {
                            if (parg->expr_type) _binary_is_string = (parg->expr_type->kind == TYPE_STRING);
                            else {
                                Expr* _l = parg->binary.left; Expr* _r = parg->binary.right;
                                _binary_is_string = (_l->type == EXPR_STRING || _r->type == EXPR_STRING) ||
                                    (_l->expr_type && _l->expr_type->kind == TYPE_STRING) ||
                                    (_r->expr_type && _r->expr_type->kind == TYPE_STRING);
                            }
                        }
                        bool _print_temp = _binary_is_string || (parg->type == EXPR_STRING_INTERP);
                        if (!_print_temp && parg->type == EXPR_METHOD_CALL &&
                            parg->method_call.method.length == 9 &&
                            memcmp(parg->method_call.method.start, "to_string", 9) == 0 &&
                            !(parg->method_call.object->expr_type && parg->method_call.object->expr_type->kind == TYPE_STRING)) {
                            _print_temp = true;
                        }
                        if (_print_temp && parg->_codegen_temp_id < 0) {
                            emit("{ const char* __ps = "); codegen_expr(parg);
                            emit("; print_no_nl(__ps); wyn_rc_release(__ps); } ");
                        } else {
                            emit("print_no_nl("); codegen_expr(parg); emit("); ");
                        }
                        codegen_skip_strdup = prev_skip;
                    }
                    // Terminator: default newline, or the given end= string.
                    if (end_arg) { emit("print_no_nl("); codegen_expr(end_arg); emit("); "); }
                    else emit("printf(\"\\n\"); ");
                    emit("})");
                    codegen_skip_strdup = prev_skip;
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 7 &&
                       memcmp(expr->call.callee->token.start, "println", 7) == 0 &&
                       expr->call.arg_count == 1) {
                // Escape analysis: string interp arg to println doesn't escape
                bool prev_skip = codegen_skip_strdup;
                if (expr->call.args[0]->type == EXPR_STRING_INTERP) codegen_skip_strdup = true;
                Expr* parg = expr->call.args[0];
                
                // Auto-convert non-string args: println(42) → println(to_string(42))
                bool arg_is_string = (parg->type == EXPR_STRING) ||
                    (parg->type == EXPR_STRING_INTERP) ||
                    (parg->expr_type && parg->expr_type->kind == TYPE_STRING) ||
                    (parg->type == EXPR_METHOD_CALL && parg->method_call.method.length == 9 &&
                     memcmp(parg->method_call.method.start, "to_string", 9) == 0);
                // String concat: binary + where at least one side is a string
                if (!arg_is_string && parg->type == EXPR_BINARY && parg->binary.op.type == TOKEN_PLUS) {
                    if ((parg->binary.left->type == EXPR_STRING) ||
                        (parg->binary.left->expr_type && parg->binary.left->expr_type->kind == TYPE_STRING) ||
                        (parg->binary.right->type == EXPR_STRING) ||
                        (parg->binary.right->expr_type && parg->binary.right->expr_type->kind == TYPE_STRING))
                        arg_is_string = true;
                }
                
                // Arrays/maps have no to_string - route to the type-aware
                // print_array (which prints elements by their real type) plus a
                // newline, matching print(arr) but with the trailing '\n'.
                if (!arg_is_string && parg->expr_type &&
                    (parg->expr_type->kind == TYPE_ARRAY)) {
                    emit("({ print_array_no_nl(");
                    codegen_expr(parg);
                    emit("); printf(\"\\n\"); })");
                    codegen_skip_strdup = prev_skip;
                    break;
                }
                // println(struct): print "Name { field: value, ... }" by
                // looking up the struct declaration from current_program.
                // Falling through to to_string(struct) passed a struct by
                // value to a long long param - an internal codegen error on
                // completely reasonable code. Detect via BOTH expr_type
                // (ideal) and the codegen struct-var registry (fallback -
                // the checker often doesn't propagate TYPE_STRUCT onto
                // bare ident references to struct vars).
                {
                    const char* _psn = NULL;
                    if (!arg_is_string && parg->expr_type && parg->expr_type->kind == TYPE_STRUCT) {
                        Token _snt = parg->expr_type->struct_type.name.length > 0
                            ? parg->expr_type->struct_type.name : parg->expr_type->name;
                        static char _psnb[64]; token_to_cstr(_psnb, sizeof(_psnb), _snt);
                        _psn = _psnb;
                    }
                    if (!_psn && !arg_is_string && parg->type == EXPR_IDENT) {
                        char _pvn[128]; token_to_cstr(_pvn, sizeof(_pvn), parg->token);
                        extern const char* get_struct_var_type(const char*);
                        _psn = get_struct_var_type(_pvn);
                    }
                    // println(Option): print "Some(v)" / "none" from the tag.
                    // Detect via the enum-var registry OR the checker typing
                    // the ident as an Option* family struct (_psn) - falling
                    // through to to_string(opt) passed the struct by value to
                    // a long long param - an internal codegen error.
                    if (!arg_is_string && parg->type == EXPR_IDENT) {
                        char _pvn[128]; token_to_cstr(_pvn, sizeof(_pvn), parg->token);
                        extern const char* get_enum_var_type(const char*);
                        const char* _oet = get_enum_var_type(_pvn);
                        if (!_oet && _psn && strncmp(_psn, "Option", 6) == 0) _oet = _psn;
                        if (_oet && strncmp(_oet, "Option", 6) == 0) {
                            const char* _fam = _oet + 6;
                            const char* _fmt =
                                strcmp(_fam, "String") == 0 ? "printf(\"Some(\\\"%%s\\\")\\n\", %s.value);"
                              : strcmp(_fam, "Float") == 0  ? "printf(\"Some(%%g)\\n\", %s.value);"
                              : strcmp(_fam, "Bool") == 0   ? "printf(\"Some(%%s)\\n\", %s.value ? \"true\" : \"false\");"
                              : "printf(\"Some(%%lld)\\n\", (long long)%s.value);";
                            emit("({ if (%s.tag == 1) { ", _pvn);
                            emit(_fmt, _pvn);
                            emit(" } else { printf(\"none\\n\"); } })");
                            codegen_skip_strdup = prev_skip;
                            break;
                        }
                    }
                    if (_psn) {
                    Token _sn = {TOKEN_IDENT, _psn, (int)strlen(_psn), 0};
                    // Find the struct declaration to get field names and types.
                    extern Program* current_program;
                    StructStmt* _sd = NULL;
                    for (int si = 0; si < current_program->count && !_sd; si++) {
                        if (current_program->stmts[si]->type == STMT_STRUCT &&
                            current_program->stmts[si]->struct_decl.name.length == _sn.length &&
                            memcmp(current_program->stmts[si]->struct_decl.name.start, _sn.start, _sn.length) == 0) {
                            _sd = &current_program->stmts[si]->struct_decl;
                        }
                    }
                    if (_sd && _sd->field_count > 0) {
                        char _tmpn[64]; snprintf(_tmpn, sizeof(_tmpn), "__psv_%d", parg->token.line);
                        emit("({ %.*s %s = ", _sn.length, _sn.start, _tmpn);
                        codegen_expr(parg);
                        emit("; printf(\"%.*s { \");", _sn.length, _sn.start);
                        for (int fi = 0; fi < _sd->field_count; fi++) {
                            if (fi > 0) emit(" printf(\", \");");
                            Token fn = _sd->fields[fi];
                            Expr* ftype = _sd->field_types[fi];
                            const char* cft = ftype ? c_type_from_expr(ftype) : "long long";
                            if (strcmp(cft, "const char*") == 0) {
                                emit(" printf(\"%.*s: \\\"%%s\\\"\", %s.%.*s);",
                                     fn.length, fn.start, _tmpn, fn.length, fn.start);
                            } else if (strcmp(cft, "double") == 0) {
                                emit(" printf(\"%.*s: %%g\", %s.%.*s);",
                                     fn.length, fn.start, _tmpn, fn.length, fn.start);
                            } else if (strcmp(cft, "int") == 0 && ftype && ftype->type == EXPR_IDENT &&
                                       ftype->token.length == 4 && memcmp(ftype->token.start, "bool", 4) == 0) {
                                emit(" printf(\"%.*s: %%s\", %s.%.*s ? \"true\" : \"false\");",
                                     fn.length, fn.start, _tmpn, fn.length, fn.start);
                            } else {
                                emit(" printf(\"%.*s: %%lld\", (long long)%s.%.*s);",
                                     fn.length, fn.start, _tmpn, fn.length, fn.start);
                            }
                        }
                        emit(" printf(\" }\\n\"); })");
                        codegen_skip_strdup = prev_skip;
                        break;
                    }
                    } // end if (_psn)
                } // end struct println block
                if (!arg_is_string && (parg->type == EXPR_INT || parg->type == EXPR_FLOAT ||
                    parg->type == EXPR_BOOL || parg->type == EXPR_IDENT || parg->type == EXPR_CALL ||
                    parg->type == EXPR_METHOD_CALL || parg->type == EXPR_INDEX ||
                    parg->type == EXPR_BINARY || parg->type == EXPR_UNARY ||
                    parg->type == EXPR_FIELD_ACCESS)) {
                    // Wrap in to_string for auto-conversion. Skip the self-release
                    // when a statement-level wrapper already owns this temp (see note
                    // at the print() site) to avoid a double free.
                    if (parg->_codegen_temp_id < 0) {
                        emit("({ const char* __ps = to_string(");
                        codegen_expr(parg);
                        emit("); println(__ps); wyn_rc_release(__ps); })");
                    } else {
                        emit("println(to_string(");
                        codegen_expr(parg);
                        emit("))");
                    }
                    codegen_skip_strdup = prev_skip;
                    break;
                }
                
                // Release temp strings passed to println (only fresh allocations)
                // Only release: concat results, string interpolation, to_string calls
                bool _println_temp = (parg->type == EXPR_STRING_INTERP);
                // Binary + with at least one string side is a string concat temp
                if (!_println_temp && parg->type == EXPR_BINARY && parg->binary.op.type == TOKEN_PLUS && arg_is_string) {
                    _println_temp = true;
                }
                // to_string method call on non-string types
                if (!_println_temp && parg->type == EXPR_METHOD_CALL &&
                    parg->method_call.method.length == 9 &&
                    memcmp(parg->method_call.method.start, "to_string", 9) == 0 &&
                    !(parg->method_call.object->expr_type && parg->method_call.object->expr_type->kind == TYPE_STRING)) {
                    _println_temp = true;
                }
                // Skip the self-release when a statement-level wrapper already owns
                // this temp (see note at the print() site) to avoid a double free.
                if (_println_temp && parg->_codegen_temp_id < 0) {
                    emit("({ const char* __ps = ");
                    codegen_expr(parg);
                    emit("; println(__ps); wyn_rc_release(__ps); })");
                } else {
                    emit("println(");
                    codegen_expr(parg);
                    emit(")");
                }
                codegen_skip_strdup = prev_skip;
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 8 &&
                       memcmp(expr->call.callee->token.start, "get_argc", 8) == 0) {
                // Special handling for get_argc() - call C interface function
                emit("wyn_get_argc()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 8 &&
                       memcmp(expr->call.callee->token.start, "get_argv", 8) == 0) {
                // Special handling for get_argv(index) - call C interface function
                if (expr->call.arg_count == 1) {
                    emit("wyn_get_argv(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("wyn_get_argv(0)");  // Default to index 0
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 17 &&
                       memcmp(expr->call.callee->token.start, "check_file_exists", 17) == 0) {
                // Special handling for check_file_exists(path) - call existing C function
                if (expr->call.arg_count == 1) {
                    emit("wyn_file_exists(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 17 &&
                       memcmp(expr->call.callee->token.start, "read_file_content", 17) == 0) {
                // Special handling for read_file_content(path) - call C interface function
                if (expr->call.arg_count == 1) {
                    emit("wyn_read_file(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("NULL");  // Return NULL if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 16 &&
                       memcmp(expr->call.callee->token.start, "is_content_valid", 16) == 0) {
                // Special handling for is_content_valid(content) - check if content is not NULL
                if (expr->call.arg_count == 1) {
                    emit("(");
                    codegen_expr(expr->call.args[0]);
                    emit(" != NULL)");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 10 &&
                       memcmp(expr->call.callee->token.start, "store_argv", 10) == 0) {
                // Special handling for store_argv(index) - store argument in global
                if (expr->call.arg_count == 1) {
                    emit("wyn_store_argv(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("0");  // Return false if no argument
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 24 &&
                       memcmp(expr->call.callee->token.start, "check_file_exists_stored", 24) == 0) {
                // Special handling for check_file_exists_stored() - check stored filename
                emit("wyn_file_exists(global_filename)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 18 &&
                       memcmp(expr->call.callee->token.start, "store_file_content", 18) == 0) {
                // Special handling for store_file_content() - store file content in global
                emit("wyn_store_file_content(global_filename)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 24 &&
                       memcmp(expr->call.callee->token.start, "get_stored_content_valid", 24) == 0) {
                // Special handling for get_stored_content_valid() - check stored content validity
                emit("wyn_get_content_valid()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_init_lexer", 12) == 0) {
                // Compiler function: c_init_lexer(source)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_init_lexer(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 12 &&
                       memcmp(expr->call.callee->token.start, "c_init_parser", 13) == 0) {
                // Compiler function: c_init_parser()
                emit("(wyn_c_init_parser(), 1)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_parse_program", 15) == 0) {
                // Compiler function: c_parse_program()
                emit("wyn_c_parse_program()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_init_checker", 14) == 0) {
                // Compiler function: c_init_checker()
                emit("(wyn_c_init_checker(), 1)");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_check_program", 15) == 0) {
                // Compiler function: c_check_program(ast_ptr)
                if (expr->call.arg_count == 1) {
                    emit("(wyn_c_check_program(");
                    codegen_expr(expr->call.args[0]);
                    emit("), 1)");
                } else {
                    emit("0");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 18 &&
                       memcmp(expr->call.callee->token.start, "c_checker_had_error", 19) == 0) {
                // Compiler function: c_checker_had_error()
                emit("wyn_c_checker_had_error()");
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 15 &&
                       memcmp(expr->call.callee->token.start, "c_generate_code", 15) == 0) {
                // Compiler function: c_generate_code(ast_ptr, filename)
                if (expr->call.arg_count == 2) {
                    emit("wyn_c_generate_code(");
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 19 &&
                       memcmp(expr->call.callee->token.start, "c_create_c_filename", 19) == 0) {
                // Compiler function: c_create_c_filename(filename)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_create_c_filename(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("NULL");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 19 &&
                       memcmp(expr->call.callee->token.start, "c_compile_to_binary", 19) == 0) {
                // Compiler function: c_compile_to_binary(c_filename, wyn_filename)
                if (expr->call.arg_count == 2) {
                    emit("wyn_c_compile_to_binary(");
                    codegen_expr(expr->call.args[0]);
                    emit(", ");
                    codegen_expr(expr->call.args[1]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 13 &&
                       memcmp(expr->call.callee->token.start, "c_remove_file", 13) == 0) {
                // Compiler function: c_remove_file(filename)
                if (expr->call.arg_count == 1) {
                    emit("wyn_c_remove_file(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else {
                    emit("false");
                }
            } else if (expr->call.callee->type == EXPR_IDENT && 
                       expr->call.callee->token.length == 3 &&
                       memcmp(expr->call.callee->token.start, "len", 3) == 0) {
                // Special handling for len() function
                if (expr->call.arg_count == 1) {
                    Expr* arg = expr->call.args[0];
                    if (arg->type == EXPR_ARRAY) {
                        // Array literal - count elements directly
                        emit("%d", arg->array.count);
                    } else if ((arg->expr_type && arg->expr_type->kind == TYPE_STRING) ||
                               arg->type == EXPR_STRING) {
                        // len() on a string is its length - use string_len (matches
                        // the `s.len()` method). Without this, `len(s)` emitted
                        // `(s).count`, treating the char* as a collection struct →
                        // C error "member reference base type 'const char *'…". (2026-07)
                        emit("string_len(");
                        codegen_expr(arg);
                        emit(")");
                    } else {
                        // Variable - assume it's a dynamic array now
                        emit("(");
                        codegen_expr(arg);
                        emit(").count");
                    }
                } else {
                    emit("len(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                }
            } else {
                // Check for special functions that need address-taking
                bool is_array_push = (expr->call.callee->type == EXPR_IDENT && 
                                     expr->call.callee->token.length == 10 &&
                                     memcmp(expr->call.callee->token.start, "array_push", 10) == 0);
                bool is_array_push_str = (expr->call.callee->type == EXPR_IDENT && 
                                     expr->call.callee->token.length == 14 &&
                                     memcmp(expr->call.callee->token.start, "array_push_str", 14) == 0);
                // Auto-detect: array_push with string value → array_push_str
                if (is_array_push && !is_array_push_str && expr->call.arg_count >= 2) {
                    Expr* val = expr->call.args[1];
                    if (val->type == EXPR_STRING || val->type == EXPR_STRING_INTERP ||
                        (val->expr_type && val->expr_type->kind == TYPE_STRING)) {
                        is_array_push_str = true;
                    }
                }
                // Treat array_push_str like array_push for address-taking
                if (is_array_push_str) is_array_push = true;
                bool is_array_pop = (expr->call.callee->type == EXPR_IDENT && 
                                    expr->call.callee->token.length == 9 &&
                                    memcmp(expr->call.callee->token.start, "array_pop", 9) == 0);
                
                // Check if this is a generic function call
                if (expr->call.callee->type == EXPR_IDENT) {
                    Token func_name = expr->call.callee->token;
                    
                    if (wyn_is_generic_function_call(func_name)) {
                        // Collect argument types for generic instantiation
                        Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                        for (int i = 0; i < expr->call.arg_count; i++) {
                            // Infer type from expression if not already set
                            if (!expr->call.args[i]->expr_type) {
                                switch (expr->call.args[i]->type) {
                                    case EXPR_INT:
                                        expr->call.args[i]->expr_type = make_type(TYPE_INT);
                                        break;
                                    case EXPR_FLOAT:
                                        expr->call.args[i]->expr_type = make_type(TYPE_FLOAT);
                                        break;
                                    case EXPR_STRING:
                                        expr->call.args[i]->expr_type = make_type(TYPE_STRING);
                                        break;
                                    case EXPR_BOOL:
                                        expr->call.args[i]->expr_type = make_type(TYPE_BOOL);
                                        break;
                                    default:
                                        expr->call.args[i]->expr_type = make_type(TYPE_INT);
                                        break;
                                }
                            }
                            arg_types[i] = expr->call.args[i]->expr_type;
                        }
                        
                        // Generate monomorphic function name
                        char monomorphic_name[256];
                        wyn_generate_monomorphic_name(func_name, arg_types, expr->call.arg_count, 
                                                      monomorphic_name, sizeof(monomorphic_name));
                        
                        // Register this instantiation for later code generation
                        wyn_register_generic_instantiation(func_name, arg_types, expr->call.arg_count);
                        
                        // Emit call to monomorphic function
                        emit("%s", monomorphic_name);
                        free(arg_types);
                    } else {
                        // Regular function call
                        // A callee that names a local lambda variable is a plain
                        // function pointer - emit its name directly and ignore any
                        // selected_overload the checker may have attached (which can
                        // be a stale/garbage overload symbol for a copied closure).
                        bool _callee_is_lambda_var = false;
                        if (expr->call.callee->type == EXPR_IDENT) {
                            char _lvn[64]; token_to_cstr(_lvn, sizeof(_lvn), expr->call.callee->token);
                            extern int find_lambda_var(const char*);
                            _callee_is_lambda_var = (find_lambda_var(_lvn) >= 0);
                        }
                        // T1.5.3: Use mangled name only for actually overloaded functions
                        if (expr->call.selected_overload && !_callee_is_lambda_var) {
                            Symbol* overload = (Symbol*)expr->call.selected_overload;
                            // Only use mangled name if there are multiple overloads
                            if (overload->mangled_name && overload->next_overload) {
                                emit("%s", overload->mangled_name);
                            } else {
                                // Check if we're in a module and need to prefix
                                // BUT: don't prefix if the callee is already module-qualified (contains ::)
                                bool is_module_qualified = false;
                                if (expr->call.callee->type == EXPR_IDENT) {
                                    for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                                        if (expr->call.callee->token.start[i] == ':' && 
                                            expr->call.callee->token.start[i+1] == ':') {
                                            is_module_qualified = true;
                                            break;
                                        }
                                    }
                                }
                                
                                // Check if this is an internal module function call
                                bool is_internal_call = false;
                                if (current_module_prefix && !is_module_qualified && expr->call.callee->type == EXPR_IDENT) {
                                    char func_name[256];
                                    token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                                    is_internal_call = is_module_function(func_name);
                                }
                                
                                // Only prefix if NOT an internal call
                                if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                    emit("%s_", current_module_prefix);
                                }
                                if (is_array_push_str) {
                                    emit("array_push_str");
                                } else {
                                    codegen_expr(expr->call.callee);
                                }
                            }
                        } else {
                            // Check if we're in a module and need to prefix
                            // BUT: don't prefix if the callee is already module-qualified (contains ::)
                            bool is_module_qualified = false;
                            if (expr->call.callee->type == EXPR_IDENT) {
                                for (int i = 0; i < expr->call.callee->token.length - 1; i++) {
                                    if (expr->call.callee->token.start[i] == ':' && 
                                        expr->call.callee->token.start[i+1] == ':') {
                                        is_module_qualified = true;
                                        break;
                                    }
                                }
                            }
                            
                            // Check if this is an internal module function call
                            bool is_internal_call = false;
                            if (current_module_prefix && !is_module_qualified && expr->call.callee->type == EXPR_IDENT) {
                                char func_name[256];
                                token_to_cstr(func_name, sizeof(func_name), expr->call.callee->token);
                                is_internal_call = is_module_function(func_name);
                            }
                            
                            // Only prefix if NOT an internal call
                            if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                emit("%s_", current_module_prefix);
                            }
                            if (is_array_push_str) {
                                emit("array_push_str");
                            } else {
                                codegen_expr(expr->call.callee);
                            }
                        }
                    }
                } else {
                    // Non-identifier callee (e.g., function pointer)
                    codegen_expr(expr->call.callee);
                }
                
                emit("(");

                // Check if this is a lambda variable call - inject captured variables
                bool is_lambda_call = false;
                int lambda_var_idx = -1;
                if (expr->call.callee->type == EXPR_IDENT) {
                    char callee_name[64];
                    token_to_cstr(callee_name, sizeof(callee_name), expr->call.callee->token);
                    for (int i = 0; i < lambda_var_count; i++) {
                        if (strcmp(lambda_var_info[i].var_name, callee_name) == 0) {
                            is_lambda_call = true;
                            lambda_var_idx = i;
                            break;
                        }
                    }
                }
                
                // Captured variables use static globals, not extra call args
                // (set via ({ __cap_N_var = var; __lambda_N; }) at assignment)
                
                // Named argument reordering - swap args/count before emission
                Expr** _orig_args = expr->call.args;
                int _orig_count = expr->call.arg_count;
                if (expr->call.arg_names) {
                    bool _has_named = false;
                    for (int i = 0; i < expr->call.arg_count; i++)
                        if (expr->call.arg_names[i].length > 0) { _has_named = true; break; }
                    if (_has_named && expr->call.callee->type == EXPR_IDENT) {
                        char _cfn2[128]; token_to_cstr(_cfn2, sizeof(_cfn2), expr->call.callee->token);
                        extern int get_fn_param_count(const char*);
                        extern Expr* get_fn_default(const char*, int);
                        extern int get_fn_param_index(const char*, const char*);
                        int _total = get_fn_param_count(_cfn2);
                        if (_total > 0) {
                            Expr** _ra = calloc(_total, sizeof(Expr*));
                            for (int i = 0; i < expr->call.arg_count; i++) {
                                if (expr->call.arg_names[i].length > 0) {
                                    char pn[64]; token_to_cstr(pn, sizeof(pn), expr->call.arg_names[i]);
                                    int idx = get_fn_param_index(_cfn2, pn);
                                    if (idx >= 0 && idx < _total) _ra[idx] = expr->call.args[i];
                                } else {
                                    for (int j = 0; j < _total; j++) { if (!_ra[j]) { _ra[j] = expr->call.args[i]; break; } }
                                }
                            }
                            for (int i = 0; i < _total; i++)
                                if (!_ra[i]) _ra[i] = get_fn_default(_cfn2, i);
                            expr->call.args = _ra;
                            expr->call.arg_count = _total;
                        }
                    }
                }
                
                for (int i = 0; i < expr->call.arg_count; i++) {
                    if (i > 0 || (is_lambda_call && lambda_var_idx >= 0 && 0)) {
                        emit(", ");
                    }
                    
                    // For array_push, take address of first argument (the array)
                    // and cast second argument to void* for integers
                    // For array_pop, take address of first argument (the array)
                    if ((is_array_push || is_array_pop) && i == 0) {
                        emit("&");
                    }
                    // No cast needed for array_push second arg - 
                    // array_push takes long long, array_push_str takes const char*
                    
                    // Check if this argument needs trait object wrapping
                    if (expr->call.callee->type == EXPR_IDENT) {
                        char callee_buf[64];
                        token_to_cstr(callee_buf, sizeof(callee_buf), expr->call.callee->token);
                        const char* trait = get_fn_trait_param(callee_buf, i);
                        if (trait && expr->call.args[i]->type == EXPR_IDENT && expr->call.args[i]->expr_type &&
                            expr->call.args[i]->expr_type->kind == TYPE_STRUCT) {
                            Token sname = expr->call.args[i]->expr_type->struct_type.name;
                            char sbuf[64];
                            token_to_cstr(sbuf, sizeof(sbuf), sname);
                            const char* impl_trait = find_trait_for_struct(sbuf);
                            if (impl_trait && strcmp(impl_trait, trait) == 0) {
                                emit("(%s){&", trait);
                                codegen_expr(expr->call.args[i]);
                                emit(", &%s_%s_vt}", trait, sbuf);
                                goto arg_done;
                            }
                        }
                    }
                    codegen_expr(expr->call.args[i]);
                    arg_done: ;
                }
                // Fill in default arguments if fewer args provided (skip if named args handled it)
                if (expr->call.args != _orig_args) {
                    // Named args already filled defaults - restore original
                    expr->call.args = _orig_args;
                    expr->call.arg_count = _orig_count;
                } else if (expr->call.callee->type == EXPR_IDENT) {
                    char _cfn[128]; token_to_cstr(_cfn, sizeof(_cfn), expr->call.callee->token);
                    extern int get_fn_param_count(const char*);
                    extern Expr* get_fn_default(const char*, int);
                    int total_params = get_fn_param_count(_cfn);
                    if (total_params > 0 && expr->call.arg_count < total_params) {
                        for (int di = expr->call.arg_count; di < total_params; di++) {
                            Expr* def = get_fn_default(_cfn, di);
                            if (def) {
                                if (di > 0 || (is_lambda_call && lambda_var_idx >= 0 && 0))
                                    emit(", ");
                                codegen_expr(def);
                            }
                        }
                    }
                }
                emit(")");
                // array_push_str transfers string ownership to the array - move
                // or retain the pushed local so scope exit doesn't double-free it.
                if (is_array_push_str && expr->call.arg_count >= 2)
                    codegen_string_push_transfer(expr->call.args[1]);
            }
            break;
        case EXPR_METHOD_CALL: {
            Token method = expr->method_call.method;

            // Channel methods: ch.send(v) / ch.recv() / ch.close() lower to the
            // Task_* runtime. The channel value is a long long handle and the
            // payload moves through one word: ints/bools ride it directly,
            // strings ride it as pointers (recv casts back - the checker now
            // types recv() as the element type), and floats BIT-CAST through
            // the word (a plain double→long long conversion truncated 3.14→3).
            if (expr->method_call.object->expr_type &&
                expr->method_call.object->expr_type->kind == TYPE_CHANNEL) {
                Type* _ch_elem = expr->method_call.object->expr_type->array_type.element_type;
                if (method.length == 4 && memcmp(method.start, "send", 4) == 0) {
                    Expr* _sv = expr->method_call.arg_count > 0 ? expr->method_call.args[0] : NULL;
                    bool _sv_float = _sv && (_sv->type == EXPR_FLOAT ||
                        (_sv->expr_type && _sv->expr_type->kind == TYPE_FLOAT) ||
                        (_ch_elem && _ch_elem->kind == TYPE_FLOAT));
                    emit("Task_send(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    if (_sv && _sv_float) {
                        emit("({ union { double _d; long long _i; } _u; _u._d = ");
                        codegen_expr(_sv);
                        emit("; _u._i; })");
                    } else if (_sv) {
                        emit("(long long)(intptr_t)(");
                        codegen_expr(_sv);
                        emit(")");
                    } else emit("0");
                    emit(")");
                    break;
                }
                if (method.length == 4 && memcmp(method.start, "recv", 4) == 0) {
                    if (_ch_elem && _ch_elem->kind == TYPE_FLOAT) {
                        emit("({ union { long long _i; double _d; } _u; _u._i = Task_recv(");
                        codegen_expr(expr->method_call.object);
                        emit("); _u._d; })");
                    } else if (_ch_elem && _ch_elem->kind == TYPE_STRING) {
                        emit("((const char*)(intptr_t)Task_recv(");
                        codegen_expr(expr->method_call.object);
                        emit("))");
                    } else {
                        emit("Task_recv(");
                        codegen_expr(expr->method_call.object);
                        emit(")");
                    }
                    break;
                }
                if (method.length == 5 && memcmp(method.start, "close", 5) == 0) {
                    emit("Task_close(");
                    codegen_expr(expr->method_call.object);
                    emit(")");
                    break;
                }
            }

            // Release intermediate string temps from chained method calls
            // e.g. "hello".upper().trim() - upper() result leaked without this
            Expr* _mc_obj = expr->method_call.object;
            bool _mc_chain_wrap = false;
            static int _mc_ctr = 0;
            int _mc_id = -1;
            // Only wrap if: (1) object is a fresh string temp, (2) this method returns a value
            // Skip void methods like .push(), .set(), etc.
            bool _mc_returns_value = expr->expr_type != NULL;
            if (_mc_returns_value && _mc_obj->_codegen_temp_id < 0 &&
                ((_mc_obj->type == EXPR_METHOD_CALL && _mc_obj->expr_type && _mc_obj->expr_type->kind == TYPE_STRING) ||
                 (_mc_obj->type == EXPR_BINARY && _mc_obj->expr_type && _mc_obj->expr_type->kind == TYPE_STRING) ||
                 _mc_obj->type == EXPR_STRING_INTERP ||
                 (_mc_obj->type == EXPR_CALL && _mc_obj->expr_type && _mc_obj->expr_type->kind == TYPE_STRING))) {
                _mc_chain_wrap = true;
                _mc_id = _mc_ctr++;
                emit("({ const char* __mo%d = ", _mc_id);
                codegen_expr(_mc_obj);
                emit("; __auto_type __mcr%d = ", _mc_id);
                _mc_obj->_codegen_temp_id = 1000 + _mc_id;
            }

            // L3: Iterator methods - .collect() and .take() are iterator-only
            // .map() and .filter() only use wyn_iter_* when chained on an iterator
            {
                Expr* _obj = expr->method_call.object;
                // Detect if object is an iterator (generator call or chained .map/.filter/.take/.collect)
                bool _is_iter_obj = false;
                if (_obj->type == EXPR_METHOD_CALL) {
                    Token _om = _obj->method_call.method;
                    if ((_om.length == 3 && memcmp(_om.start, "map", 3) == 0) ||
                        (_om.length == 6 && memcmp(_om.start, "filter", 6) == 0) ||
                        (_om.length == 4 && memcmp(_om.start, "take", 4) == 0)) {
                        // Only if the chain root is a generator
                        Expr* _root = _obj;
                        while (_root->type == EXPR_METHOD_CALL) _root = _root->method_call.object;
                        if (_root->type == EXPR_CALL && _root->call.callee->type == EXPR_IDENT) {
                            extern Program* current_program; extern int fn_is_generator(Stmt*);
                            if (current_program) {
                                Token cn = _root->call.callee->token;
                                for (int _fi = 0; _fi < current_program->count; _fi++) {
                                    Stmt* _s = current_program->stmts[_fi];
                                    Stmt* _fs = (_s->type == STMT_EXPORT && _s->export.stmt) ? _s->export.stmt : _s;
                                    if (fn_is_generator(_fs) && _fs->fn.name.length == cn.length &&
                                        memcmp(_fs->fn.name.start, cn.start, cn.length) == 0) { _is_iter_obj = true; break; }
                                }
                            }
                        }
                    }
                }
                if (_obj->type == EXPR_CALL && _obj->call.callee->type == EXPR_IDENT) {
                    extern Program* current_program; extern int fn_is_generator(Stmt*);
                    if (current_program) {
                        Token cn = _obj->call.callee->token;
                        for (int _fi = 0; _fi < current_program->count; _fi++) {
                            Stmt* _s = current_program->stmts[_fi];
                            Stmt* _fs = (_s->type == STMT_EXPORT && _s->export.stmt) ? _s->export.stmt : _s;
                            if (fn_is_generator(_fs) && _fs->fn.name.length == cn.length &&
                                memcmp(_fs->fn.name.start, cn.start, cn.length) == 0) { _is_iter_obj = true; break; }
                        }
                    }
                }
                if (_is_iter_obj) {
                    if (method.length == 7 && memcmp(method.start, "collect", 7) == 0 && expr->method_call.arg_count == 0) {
                        emit("wyn_iter_collect("); codegen_expr(_obj); emit(")"); break;
                    }
                    if (method.length == 4 && memcmp(method.start, "take", 4) == 0 && expr->method_call.arg_count == 1) {
                        emit("wyn_iter_take("); codegen_expr(_obj); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (method.length == 3 && memcmp(method.start, "map", 3) == 0 && expr->method_call.arg_count == 1) {
                        emit("wyn_iter_map("); codegen_expr(_obj); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (method.length == 6 && memcmp(method.start, "filter", 6) == 0 && expr->method_call.arg_count == 1) {
                        emit("wyn_iter_filter("); codegen_expr(_obj); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                }
            }
            
            // Spawn array: intercept .push() and [i] for WynIntArray
            if (expr->method_call.object->type == EXPR_IDENT &&
                method.length == 4 && memcmp(method.start, "push", 4) == 0 &&
                expr->method_call.arg_count == 1) {
                char _on[256]; token_to_cstr(_on, sizeof(_on), expr->method_call.object->token);
                if (is_spawn_array(_on)) {
                    emit("int_array_push(&(");
                    codegen_expr(expr->method_call.object);
                    emit("), (long long)(");
                    codegen_expr(expr->method_call.args[0]);
                    emit("))");
                    break;
                }
            }
            
            // Check if this is an enum constructor: Shape.Circle(5.0)
            if (expr->method_call.object->type == EXPR_IDENT) {
                char _obj[128]; token_to_cstr(_obj, sizeof(_obj), expr->method_call.object->token);
                extern int is_enum_type(const char*);
                if (is_enum_type(_obj)) {
                    emit("%s_%.*s(", _obj, method.length, method.start);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        if (i > 0) emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            
            // Extension methods on struct types - CHECK THIS FIRST
            if (expr->method_call.object->expr_type && 
                expr->method_call.object->expr_type->kind == TYPE_STRUCT) {
                Token type_name = expr->method_call.object->expr_type->struct_type.name;

                // Skip type parameters (T, K, V) - let generic dispatch handle
                // them. BUT only if the single-letter name isn't an actual struct:
                // a real `struct P { ... fn f(self) ... }` must dispatch to P_f,
                // not be mistaken for a generic parameter (that dropped the call
                // entirely → "Unknown method" / codegen error for 1-letter structs).
                if (type_name.length == 1 && type_name.start[0] >= 'A' && type_name.start[0] <= 'Z') {
                    char _tn1[2] = { type_name.start[0], '\0' };
                    extern int is_known_struct(const char*);
                    if (!is_known_struct(_tn1))
                        goto skip_struct_dispatch;
                }
            } else if (expr->method_call.object->type == EXPR_IDENT) {
                // Fallback: check codegen's struct var registry
                char _svn[64]; token_to_cstr(_svn, sizeof(_svn), expr->method_call.object->token);
                extern const char* get_struct_var_type(const char*);
                const char* _svt = get_struct_var_type(_svn);
                if (_svt) {
                    Token method = expr->method_call.method;
                    emit("%s_%.*s(", _svt, method.length, method.start);
                    codegen_expr(expr->method_call.object);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        emit(", "); codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            if (expr->method_call.object->expr_type && 
                expr->method_call.object->expr_type->kind == TYPE_STRUCT) {
                Token type_name = expr->method_call.object->expr_type->struct_type.name;
                
                // Check if this is a trait type - use vtable dispatch
                if (is_trait_type(type_name.start, type_name.length)) {
                    emit("(");
                    codegen_expr(expr->method_call.object);
                    emit(").vtable->%.*s((", method.length, method.start);
                    codegen_expr(expr->method_call.object);
                    emit(").data");
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
                
                // Check if this is a static method call (Type.method() vs instance.method())
                bool is_static_call = false;
                if (expr->method_call.object->type == EXPR_IDENT) {
                    char _tn[128]; token_to_cstr(_tn, sizeof(_tn), type_name);
                    char _on[128]; token_to_cstr(_on, sizeof(_on), expr->method_call.object->token);
                    if (strcmp(_tn, _on) == 0) is_static_call = true;
                }
                
                emit("%.*s_%.*s(", type_name.length, type_name.start, 
                     method.length, method.start);
                if (!is_static_call) {
                    codegen_expr(expr->method_call.object);
                    if (expr->method_call.arg_count > 0) emit(", ");
                }
                for (int i = 0; i < expr->method_call.arg_count; i++) {
                    if (i > 0) emit(", ");
                    codegen_expr(expr->method_call.args[i]);
                }
                emit(")");
                break;
            }
            skip_struct_dispatch:
            
            // Check if object is a parameter with trait type
            if (expr->method_call.object->type == EXPR_IDENT) {
                Token obj = expr->method_call.object->token;
                // Check current function params for trait-typed params
                for (int pi = 0; pi < current_param_count; pi++) {
                    if (current_function_params[pi] && 
                        strlen(current_function_params[pi]) == (size_t)obj.length &&
                        memcmp(current_function_params[pi], obj.start, obj.length) == 0) {
                        // Found the param - check if its type is a trait
                        // We stored param types during STMT_FN processing
                        // (current_param_types is the growable array defined in codegen.c;
                        // this file is #included into codegen.c so it is in scope)
                        if (current_param_types[pi][0] && is_trait_type(current_param_types[pi], strlen(current_param_types[pi]))) {
                            emit("(");
                            codegen_expr(expr->method_call.object);
                            emit(").vtable->%.*s((", method.length, method.start);
                            codegen_expr(expr->method_call.object);
                            emit(").data");
                            for (int i = 0; i < expr->method_call.arg_count; i++) {
                                emit(", ");
                                codegen_expr(expr->method_call.args[i]);
                            }
                            emit(")");
                            goto method_done;
                        }
                    }
                }
            }
            
            // Module method calls (module.function()) - CHECK THIS SECOND
            if (expr->method_call.object->type == EXPR_IDENT) {
                Token obj_name = expr->method_call.object->token;
                // Check if this is actually a module (not a variable)
                char module_name[256]; token_to_cstr(module_name, sizeof(module_name), obj_name);
                
                // Treat as module if it's loaded OR if it's a built-in
                // BUT NOT if it's a local variable or parameter
                bool is_local = is_parameter(module_name) || is_local_variable(module_name);
                // Check if it's a loaded module (by full name or short name)
                bool is_loaded_module = is_module_loaded(module_name) || is_builtin_module(module_name);
                char resolved_mod_name[256] = "";
                // W9: `import m as mm` → mm.foo() lowers to the real module m's
                // symbol. Resolve the alias (unless a local/param shadows it).
                if (!is_local) {
                    extern const char* resolve_parser_module_alias(const char* name);
                    const char* aliased = resolve_parser_module_alias(module_name);
                    if (aliased && strcmp(aliased, module_name) != 0) {
                        strncpy(resolved_mod_name, aliased, 255);
                        resolved_mod_name[255] = '\0';
                        is_loaded_module = true;
                    }
                }
                if (!is_loaded_module && !is_local && resolved_mod_name[0] == '\0') {
                    // Check short names: "utils" might be "lib/utils"
                    extern int get_module_count(void);
                    extern void* get_module_entry_at(int index);
                    int _mc = get_module_count();
                    for (int _mi = 0; _mi < _mc; _mi++) {
                        typedef struct { char* name; void* ast; } _ME;
                        _ME* _mod = (_ME*)get_module_entry_at(_mi);
                        char* _sl = strrchr(_mod->name, '/');
                        const char* _sn = _sl ? _sl + 1 : _mod->name;
                        if (strcmp(_sn, module_name) == 0) {
                            is_loaded_module = true;
                            strncpy(resolved_mod_name, _mod->name, 255);
                            break;
                        }
                    }
                }
                if (!is_local && is_loaded_module) {
                    // Special case: some modules use lowercase C functions
                    if (strcmp(module_name, "Http") == 0) {
                        // Http.get/post/put/delete -> http_ (simple string API)
                        // Http.serve/accept/respond/close_server -> Http_ (server API)
                        if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                            emit("http_get(");
                        } else if (method.length == 4 && memcmp(method.start, "post", 4) == 0) {
                            emit("http_post(");
                        } else if (method.length == 3 && memcmp(method.start, "put", 3) == 0) {
                            emit("http_put(");
                        } else if (method.length == 6 && memcmp(method.start, "delete", 6) == 0) {
                            emit("http_delete(");
                        } else if (method.length == 10 && memcmp(method.start, "set_header", 10) == 0) {
                            emit("http_set_header(");
                        } else {
                            // Server methods: serve, accept, respond, close_server
                            emit("Http_%.*s(", method.length, method.start);
                        }
                    } else if (strcmp(module_name, "Regex") == 0) {
                        emit("regex_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "HashMap") == 0) {
                        // Map common methods to correct C functions
                        if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                            emit("hashmap_get_string(");
                        } else if (method.length == 3 && memcmp(method.start, "set", 3) == 0) {
                            emit("hashmap_set(");
                        } else if (method.length == 3 && memcmp(method.start, "has", 3) == 0) {
                            emit("hashmap_has(");
                        } else {
                            emit("hashmap_%.*s(", method.length, method.start);
                        }
                    } else if (strcmp(module_name, "HashSet") == 0) {
                        emit("hashset_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Task") == 0) {
                        // Task API maps directly to Task_ prefix
                        emit("Task_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "File") == 0) {
                        // File maps to File_ prefix (wrappers in runtime)
                        emit("File_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Net") == 0) {
                        emit("Net_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Url") == 0) {
                        emit("Url_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Db") == 0) {
                        emit("Db_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Gui") == 0) {
                        emit("Gui_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Audio") == 0) {
                        emit("Audio_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "StringBuilder") == 0) {
                        emit("StringBuilder_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Crypto") == 0) {
                        emit("Crypto_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Template") == 0) {
                        emit("Template_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Encoding") == 0) {
                        emit("Encoding_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Os") == 0) {
                        emit("Os_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Uuid") == 0) {
                        emit("Uuid_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Log") == 0) {
                        emit("Log_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Process") == 0) {
                        emit("Process_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "String") == 0) {
                        if (method.length == 4 && memcmp(method.start, "char", 4) == 0) {
                            emit("String_char_from_int(");
                        } else if (method.length == 10 && memcmp(method.start, "from_chars", 10) == 0) {
                            emit("String_from_chars(");
                        } else {
                            emit("String_%.*s(", method.length, method.start);
                        }
                    } else if (strcmp(module_name, "Random") == 0) {
                        emit("random_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Web") == 0) {
                        emit("Web_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Smtp") == 0) {
                        emit("Smtp_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "App") == 0) {
                        emit("App_%.*s(", method.length, method.start);
                    } else if (strcmp(module_name, "Shared") == 0) {
                        emit("Shared_%.*s(", method.length, method.start);
                    } else {
                        // Use resolved module name if available (e.g., "lib/utils" -> "lib_utils")
                        if (resolved_mod_name[0]) {
                            const char* c_mod = module_to_c_ident(resolved_mod_name);
                            emit("%s_%.*s(", c_mod, method.length, method.start);
                        } else {
                            emit("%.*s_%.*s(", obj_name.length, obj_name.start, method.length, method.start);
                        }
                    }
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        if (i > 0) emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            
            // WynIntArray dispatch - typed [int] arrays
            if (expr->method_call.object->type == EXPR_IDENT) {
                char _ian[128]; token_to_cstr(_ian, sizeof(_ian), expr->method_call.object->token);
                extern int is_int_array_var(const char*);
                if (is_int_array_var(_ian)) {
                    Token m = expr->method_call.method;
                    if (m.length == 4 && memcmp(m.start, "push", 4) == 0) {
                        emit("int_array_push(&("); codegen_expr(expr->method_call.object); emit("), ");
                        codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (m.length == 4 && memcmp(m.start, "sort", 4) == 0) {
                        // sort in place AND yield the array (usable as an expression)
                        emit("({ int_array_sort(&("); codegen_expr(expr->method_call.object);
                        emit(")); "); codegen_expr(expr->method_call.object); emit("; })"); break;
                    }
                    if (m.length == 3 && memcmp(m.start, "len", 3) == 0) {
                        emit("int_array_len("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
                }
            }

            // Type-aware method dispatch (Phase 4)
            Type* object_type = expr->method_call.object->expr_type;
            
            // Special handling for array.push() with struct/string elements
            if (object_type && object_type->kind == TYPE_ARRAY) {
                Token method = expr->method_call.method;
                if (method.length == 4 && memcmp(method.start, "push", 4) == 0) {
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type && elem_type->kind == TYPE_STRING) {
                        emit("array_push_str(&(");
                        codegen_expr(expr->method_call.object);
                        emit("), ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                        // Ownership transfers to the array - suppress the scope-exit
                        // release (move) or retain a shared reference (copy).
                        codegen_string_push_transfer(expr->method_call.args[0]);
                        break;
                    }
                    if (elem_type && elem_type->kind == TYPE_STRUCT) {
                        // Use macro for struct push
                        emit("array_push_struct(&(");
                        codegen_expr(expr->method_call.object);
                        emit("), ");
                        
                        // Emit the struct initializer
                        codegen_expr(expr->method_call.args[0]);
                        emit(", ");
                        
                        // Emit the type name for the macro's third argument
                        // This must match the type used in the struct initializer
                        Expr* init_expr = expr->method_call.args[0];
                        if (init_expr->type == EXPR_STRUCT_INIT) {
                            Token type_name = init_expr->struct_init.type_name;
                            
                            // The struct initializer codegen adds current_module_prefix
                            // We need to do the same here to match
                            if (current_module_prefix) {
                                emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                            } else {
                                emit("%.*s", type_name.length, type_name.start);
                            }
                        } else {
                            // Fallback
                            Token type_name = elem_type->struct_type.name;
                            if (current_module_prefix) {
                                emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                            } else {
                                emit("%.*s", type_name.length, type_name.start);
                            }
                        }
                        emit(")");
                        break;
                    }
                    // Default: int/pointer push with cast
                    // Check if this is a spawn array (WynIntArray)
                    if (expr->method_call.object->type == EXPR_IDENT) {
                        char _on[256]; token_to_cstr(_on, sizeof(_on), expr->method_call.object->token);
                        if (is_spawn_array(_on)) {
                            emit("int_array_push(&(");
                            codegen_expr(expr->method_call.object);
                            emit("), (long long)(");
                            codegen_expr(expr->method_call.args[0]);
                            emit("))");
                            break;
                        }
                    }
                    emit("array_push(&(");
                    codegen_expr(expr->method_call.object);
                    emit("), (long long)(");
                    codegen_expr(expr->method_call.args[0]);
                    emit("))");
                    break;
                }
            }
            
            // Special handling for array.get() - use type-specific accessor
            if (object_type && object_type->kind == TYPE_ARRAY) {
                Token method = expr->method_call.method;
                
                // arr.map(fn): pick the runtime variant from BOTH the element
                // type and the lambda's RETURN type - a str->int lambda
                // (`words.map((s) => s.len())`) through the str->str variant
                // treated returned ints as char* and segfaulted at print.
                if (method.length == 3 && memcmp(method.start, "map", 3) == 0 && expr->method_call.arg_count == 1) {
                    Type* _elem_t = object_type->array_type.element_type;
                    bool _elem_is_str = _elem_t && _elem_t->kind == TYPE_STRING;
                    if (!_elem_is_str && expr->method_call.object->type == EXPR_IDENT) {
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), expr->method_call.object->token);
                        extern int is_str_array_var(const char*);
                        if (is_str_array_var(_vn)) _elem_is_str = true;
                    }
                    bool _elem_is_float = _elem_t && _elem_t->kind == TYPE_FLOAT;
                    Expr* _fn_arg0 = expr->method_call.args[0];
                    Type* _lam_ret = NULL;
                    if (_fn_arg0->expr_type && _fn_arg0->expr_type->kind == TYPE_FUNCTION)
                        _lam_ret = _fn_arg0->expr_type->fn_type.return_type;
                    bool _lam_ret_str = _lam_ret ? _lam_ret->kind == TYPE_STRING : true;
                    // S3: [Struct].map(fn) - no generic runtime variant can take a
                    // struct by value, so emit an inline typed loop instead.
                    if (_elem_t && _elem_t->kind == TYPE_STRUCT) {
                        char _ec[128]; const char* _ecp = codegen_c_type_from_type(_elem_t);
                        snprintf(_ec, sizeof(_ec), "%s", _ecp ? _ecp : "long long");
                        char _rc[128]; const char* _rcp = _lam_ret ? codegen_c_type_from_type(_lam_ret) : NULL;
                        snprintf(_rc, sizeof(_rc), "%s", _rcp ? _rcp : "long long");
                        emit("({ WynArray __src = ");
                        codegen_expr(expr->method_call.object);
                        emit("; %s (*__fn)(%s) = ", _rc, _ec);
                        codegen_expr(_fn_arg0);
                        emit("; WynArray __dst = array_new(); ");
                        emit("for (int __i = 0; __i < __src.count; __i++) { ");
                        if (_lam_ret && _lam_ret->kind == TYPE_STRUCT) {
                            emit("array_push_struct(&__dst, __fn(array_get_struct(__src, __i, %s)), %s); ", _ec, _rc);
                        } else if (_lam_ret && _lam_ret->kind == TYPE_STRING) {
                            emit("const char* __m = __fn(array_get_struct(__src, __i, %s)); ", _ec);
                            emit("array_push_str(&__dst, __m ? wyn_strdup(__m) : wyn_strdup(\"\")); ");
                        } else if (_lam_ret && _lam_ret->kind == TYPE_FLOAT) {
                            emit("array_push_float(&__dst, __fn(array_get_struct(__src, __i, %s))); ", _ec);
                        } else if (_lam_ret && _lam_ret->kind == TYPE_BOOL) {
                            emit("array_push_bool(&__dst, __fn(array_get_struct(__src, __i, %s))); ", _ec);
                        } else {
                            emit("array_push_int(&__dst, __fn(array_get_struct(__src, __i, %s))); ", _ec);
                        }
                        emit("} __dst; })");
                        break;
                    }
                    // S3: [bool].map with a bool-returning lambda keeps elements
                    // typed bool (the int variant printed 0/1 instead of true/false).
                    bool _elem_is_bool = _elem_t && _elem_t->kind == TYPE_BOOL;
                    emit(_elem_is_str ? (_lam_ret_str ? "wyn_array_map_str(" : "wyn_array_map_str_to_int(")
                         : _elem_is_float ? "wyn_array_map_float("
                         : (_elem_is_bool && _lam_ret && _lam_ret->kind == TYPE_BOOL) ? "wyn_array_map_bool("
                         : "wyn_array_map(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // arr.filter(fn) -> element-typed variant ([string]/[float]/[bool]/[Struct]/int)
                if (method.length == 6 && memcmp(method.start, "filter", 6) == 0 && expr->method_call.arg_count == 1) {
                    Type* _elem_t = object_type->array_type.element_type;
                    bool _elem_is_str = _elem_t && _elem_t->kind == TYPE_STRING;
                    bool _elem_is_float = _elem_t && _elem_t->kind == TYPE_FLOAT;
                    bool _elem_is_bool = _elem_t && _elem_t->kind == TYPE_BOOL;
                    if (!_elem_is_str && expr->method_call.object->type == EXPR_IDENT) {
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), expr->method_call.object->token);
                        extern int is_str_array_var(const char*);
                        if (is_str_array_var(_vn)) _elem_is_str = true;
                    }
                    // S3: [Struct].filter(fn) - inline typed loop (see map above).
                    if (_elem_t && _elem_t->kind == TYPE_STRUCT) {
                        char _ec[128]; const char* _ecp = codegen_c_type_from_type(_elem_t);
                        snprintf(_ec, sizeof(_ec), "%s", _ecp ? _ecp : "long long");
                        // __fn's return C type must match the emitted lambda's
                        // (the checker types the predicate body - bool or int).
                        Type* _flr = NULL;
                        if (expr->method_call.args[0]->expr_type &&
                            expr->method_call.args[0]->expr_type->kind == TYPE_FUNCTION)
                            _flr = expr->method_call.args[0]->expr_type->fn_type.return_type;
                        char _frc[128]; const char* _frcp = _flr ? codegen_c_type_from_type(_flr) : NULL;
                        snprintf(_frc, sizeof(_frc), "%s", _frcp ? _frcp : "long long");
                        emit("({ WynArray __src = ");
                        codegen_expr(expr->method_call.object);
                        emit("; %s (*__fn)(%s) = ", _frc, _ec);
                        codegen_expr(expr->method_call.args[0]);
                        emit("; WynArray __dst = array_new(); ");
                        emit("for (int __i = 0; __i < __src.count; __i++) { ");
                        emit("%s __v = array_get_struct(__src, __i, %s); ", _ec, _ec);
                        emit("if (__fn(__v)) array_push_struct(&__dst, __v, %s); ", _ec);
                        emit("} __dst; })");
                        break;
                    }
                    emit(_elem_is_str ? "wyn_array_filter_str("
                         : _elem_is_float ? "wyn_array_filter_float("
                         : _elem_is_bool ? "wyn_array_filter_bool("
                         : "wyn_array_filter(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // arr.reduce(fn, init) -> wyn_array_reduce(arr, fn, init).
                // Forgiveness: accept reduce(init, fn) too (Python/JS habit) by
                // putting whichever arg is the lambda in the fn slot - the old
                // positional emit called the int initializer as a function
                // pointer and segfaulted.
                if (method.length == 6 && memcmp(method.start, "reduce", 6) == 0 && expr->method_call.arg_count == 2) {
                    Expr* _fn_arg = expr->method_call.args[0];
                    Expr* _init_arg = expr->method_call.args[1];
                    if (_fn_arg->type != EXPR_LAMBDA && _init_arg->type == EXPR_LAMBDA) {
                        Expr* _sw = _fn_arg; _fn_arg = _init_arg; _init_arg = _sw;
                    }
                    emit("wyn_array_reduce(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(_fn_arg);
                    emit(", ");
                    codegen_expr(_init_arg);
                    emit(")");
                    break;
                }
                // arr.sort() -> arr_sort(arr.data, arr.count) (in-place)
                if (method.length == 4 && memcmp(method.start, "sort", 4) == 0 && expr->method_call.arg_count == 0) {
                    // Check if array contains strings
                    bool _is_str_arr = false;
                    Expr* _sobj = expr->method_call.object;
                    if (_sobj->expr_type && _sobj->expr_type->kind == TYPE_ARRAY &&
                        _sobj->expr_type->array_type.element_type &&
                        _sobj->expr_type->array_type.element_type->kind == TYPE_STRING) {
                        _is_str_arr = true;
                    }
                    // Also check if it's a known string array variable
                    if (!_is_str_arr && _sobj->type == EXPR_IDENT) {
                        extern int is_str_array_var(const char*);
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), _sobj->token);
                        if (is_str_array_var(_vn)) _is_str_arr = true;
                    }
                    // Sort in place AND evaluate to the array, so `.sort()` works in
                    // expression position too (`print(xs.sort())`, `xs.sort().reverse()`).
                    // Emitting the void `array_sort(&xs)` alone made `.sort()` unusable
                    // as a value ("incompatible type 'void'") - a regression that broke
                    // the project's own test_array_ops / test_showcase. (2026-07)
                    emit(_is_str_arr ? "({ array_sort_str(&(" : "({ array_sort(&(");
                    codegen_expr(expr->method_call.object);
                    emit(")); ");
                    codegen_expr(expr->method_call.object);
                    emit("; })");
                    goto method_done;
                }
                // arr.sort_by(cmp_fn) -> wyn_array_sort_by(&arr, cmp_fn)
                if (method.length == 7 && memcmp(method.start, "sort_by", 7) == 0 && expr->method_call.arg_count == 1) {
                    emit("({ wyn_array_sort_by(&(");
                    codegen_expr(expr->method_call.object);
                    emit("), ");
                    codegen_expr(expr->method_call.args[0]);
                    emit("); ");
                    codegen_expr(expr->method_call.object);
                    emit("; })");
                    break;
                }
                // arr.contains(val)
                if (method.length == 8 && memcmp(method.start, "contains", 8) == 0 && expr->method_call.arg_count == 1) {
                    emit("arr_contains(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.object);
                    emit(".count, ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // arr.find(val)
                if (method.length == 4 && memcmp(method.start, "find", 4) == 0 && expr->method_call.arg_count == 1) {
                    if (expr->method_call.args[0]->type == EXPR_LAMBDA) {
                        emit("array_find_fn(");
                        codegen_expr(expr->method_call.object);
                        emit(", ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                    } else {
                        emit("arr_find(");
                        codegen_expr(expr->method_call.object);
                        emit(", ");
                        codegen_expr(expr->method_call.object);
                        emit(".count, ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                    }
                    break;
                }
                // arr.pop()
                if (method.length == 3 && memcmp(method.start, "pop", 3) == 0 && expr->method_call.arg_count == 0) {
                    emit("array_pop_int(&("); codegen_expr(expr->method_call.object); emit("))"); break;
                }
                // arr.slice(start, end)
                if (method.length == 5 && memcmp(method.start, "slice", 5) == 0 && expr->method_call.arg_count == 2) {
                    emit("wyn_array_slice_range("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]); emit(")"); break;
                }
                // arr.reverse()
                if (method.length == 7 && memcmp(method.start, "reverse", 7) == 0 && expr->method_call.arg_count == 0) {
                    emit("array_reverse_copy("); codegen_expr(expr->method_call.object); emit(")"); break;
                }
                // arr.join(sep)
                if (method.length == 4 && memcmp(method.start, "join", 4) == 0 && expr->method_call.arg_count == 1) {
                    emit("array_join_str("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                // arr.index_of(val)
                if (method.length == 8 && memcmp(method.start, "index_of", 8) == 0 && expr->method_call.arg_count == 1) {
                    emit("array_index_of_int("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                // arr.remove(index)
                if (method.length == 6 && memcmp(method.start, "remove", 6) == 0 && expr->method_call.arg_count == 1) {
                    emit("array_remove_at(&("); codegen_expr(expr->method_call.object);
                    emit("), "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                // arr.insert(index, val)
                if (method.length == 6 && memcmp(method.start, "insert", 6) == 0 && expr->method_call.arg_count == 2) {
                    emit("array_insert_at(&("); codegen_expr(expr->method_call.object);
                    emit("), "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]); emit(")"); break;
                }
                // arr.unique()
                if (method.length == 6 && memcmp(method.start, "unique", 6) == 0 && expr->method_call.arg_count == 0) {
                    emit("array_unique_int("); codegen_expr(expr->method_call.object); emit(")"); break;
                }
                // arr.concat(other)
                if (method.length == 6 && memcmp(method.start, "concat", 6) == 0 && expr->method_call.arg_count == 1) {
                    emit("array_concat("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                // arr.any(fn)
                if (method.length == 3 && memcmp(method.start, "any", 3) == 0 && expr->method_call.arg_count == 1) {
                    emit("wyn_arr_any("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                // arr.all(fn)
                if (method.length == 3 && memcmp(method.start, "all", 3) == 0 && expr->method_call.arg_count == 1) {
                    emit("wyn_arr_all("); codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                }
                
                if (method.length == 3 && memcmp(method.start, "get", 3) == 0) {
                    // Determine element type and use appropriate accessor
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type) {
                        if (elem_type->kind == TYPE_STRING) {
                            emit("array_get_str(");
                        } else if (elem_type->kind == TYPE_STRUCT) {
                            // Use struct accessor
                            emit("array_get_struct(");
                        } else {
                            // Default to int accessor
                            emit("array_get_int(");
                        }
                    } else {
                        // No element type info, default to int
                        emit("array_get_int(");
                    }
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    if (elem_type && elem_type->kind == TYPE_STRUCT) {
                        emit(", ");
                        Token type_name = elem_type->struct_type.name;
                        emit("%.*s", type_name.length, type_name.start);
                    }
                    emit(")");
                    break;
                }
                // Special handling for array.contains() - use type-specific function
                if (method.length == 8 && memcmp(method.start, "contains", 8) == 0) {
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type && elem_type->kind == TYPE_STRING) {
                        emit("array_contains_str(");
                        codegen_expr(expr->method_call.object);
                        emit(", ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                        break;
                    }
                }
                // Special handling for array.index_of() - use type-specific function
                if (method.length == 8 && memcmp(method.start, "index_of", 8) == 0) {
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type && elem_type->kind == TYPE_STRING) {
                        emit("array_index_of_str(");
                        codegen_expr(expr->method_call.object);
                        emit(", ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                        break;
                    }
                }
                // Special handling for array.remove() - use type-specific function
                if (method.length == 6 && memcmp(method.start, "remove", 6) == 0) {
                    Type* elem_type = object_type->array_type.element_type;
                    if (elem_type && elem_type->kind == TYPE_STRING) {
                        emit("array_remove_str(&(");
                        codegen_expr(expr->method_call.object);
                        emit("), ");
                        codegen_expr(expr->method_call.args[0]);
                        emit(")");
                        break;
                    }
                }
            }
            
            // Enum .to_string() - route to generated EnumName_toString function
            if (method.length == 9 && memcmp(method.start, "to_string", 9) == 0 &&
                expr->method_call.arg_count == 0) {
                // Case 1: Color.Red.to_string() - object is field access on enum type
                if (expr->method_call.object->type == EXPR_FIELD_ACCESS) {
                    Expr* fa_obj = expr->method_call.object->field_access.object;
                    if (fa_obj->type == EXPR_IDENT) {
                        char _en[128]; token_to_cstr(_en, sizeof(_en), fa_obj->token);
                        extern int is_enum_type(const char*);
                        if (is_enum_type(_en)) {
                            emit("%s_toString(", _en);
                            codegen_expr(expr->method_call.object);
                            emit(")");
                            break;
                        }
                    }
                }
                // Case 2: c.to_string() where c is an enum variable
                if (expr->method_call.object->type == EXPR_IDENT) {
                    char _vn[128]; token_to_cstr(_vn, sizeof(_vn), expr->method_call.object->token);
                    extern const char* get_enum_var_type(const char*);
                    const char* _et = get_enum_var_type(_vn);
                    if (_et) {
                        emit("%s_toString(", _et);
                        codegen_expr(expr->method_call.object);
                        emit(")");
                        break;
                    }
                }
            }

            // When .to_string() is called on an expression whose type can't be
            // statically resolved, use _Generic so C dispatches correctly
            if (method.length == 9 && memcmp(method.start, "to_string", 9) == 0 &&
                expr->method_call.arg_count == 0 &&
                (expr->method_call.object->type == EXPR_METHOD_CALL ||
                 expr->method_call.object->type == EXPR_FIELD_ACCESS ||
                 expr->method_call.object->type == EXPR_AWAIT ||
                 expr->method_call.object->type == EXPR_CALL ||
                 expr->method_call.object->type == EXPR_BINARY ||
                 expr->method_call.object->type == EXPR_UNARY ||
                 expr->method_call.object->type == EXPR_IDENT)) {
                emit("to_string(");
                codegen_expr(expr->method_call.object);
                emit(")");
                break;
            }
            
            const char* receiver_type = get_receiver_type_string(object_type);
            
            // Check if variable is a known array (from list comp or array literal)
            if (expr->method_call.object->type == EXPR_IDENT) {
                char vn[64];
                token_to_cstr(vn, sizeof(vn), expr->method_call.object->token);
                extern int is_known_array_var(const char*);
                if (is_known_array_var(vn)) receiver_type = "array";
                extern int is_known_sb_var(const char*);
                if (is_known_sb_var(vn)) receiver_type = "stringbuilder";
                extern int is_known_float_var(const char*);
                if (is_known_float_var(vn)) receiver_type = "float";
            }
            
            // Method chaining: if object is a method call that returns an array, treat as array
            if (expr->method_call.object->type == EXPR_METHOD_CALL) {
                Token inner = expr->method_call.object->method_call.method;
                // Trust the checker's type first: `.reverse()`/`.sort()` on a
                // STRING return a string, on an array return the array. Keying
                // on the method NAME alone routed `s.reverse().len()` to
                // array_len(const char*) - an internal codegen error. (2026-07)
                Type* _inner_t = expr->method_call.object->expr_type;
                bool _inner_is_string = _inner_t && _inner_t->kind == TYPE_STRING;
                if (!_inner_is_string &&
                    ((inner.length == 4 && memcmp(inner.start, "sort", 4) == 0) ||
                    (inner.length == 7 && memcmp(inner.start, "reverse", 7) == 0) ||
                    (inner.length == 6 && memcmp(inner.start, "filter", 6) == 0) ||
                    (inner.length == 3 && memcmp(inner.start, "map", 3) == 0) ||
                    (inner.length == 6 && memcmp(inner.start, "unique", 6) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "slice", 5) == 0) ||
                    (inner.length == 7 && memcmp(inner.start, "flat_map", 7) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "split", 5) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "chars", 5) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "words", 5) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "lines", 5) == 0)))
                    receiver_type = "array";
                if (_inner_is_string) receiver_type = "string";
                // String-returning methods
                if ((inner.length == 5 && memcmp(inner.start, "upper", 5) == 0) ||
                    (inner.length == 5 && memcmp(inner.start, "lower", 5) == 0) ||
                    (inner.length == 4 && memcmp(inner.start, "trim", 4) == 0) ||
                    (inner.length == 7 && memcmp(inner.start, "replace", 7) == 0) ||
                    (inner.length == 6 && memcmp(inner.start, "repeat", 6) == 0) ||
                    (inner.length == 9 && memcmp(inner.start, "substring", 9) == 0) ||
                    (inner.length == 4 && memcmp(inner.start, "join", 4) == 0) ||
                    (inner.length == 9 && memcmp(inner.start, "to_string", 9) == 0))
                    receiver_type = "string";
            }
            // Method chaining: function calls that return arrays
            if (!receiver_type && expr->method_call.object->type == EXPR_CALL) {
                // await_all returns array
                if (expr->method_call.object->call.callee->type == EXPR_IDENT) {
                    Token fn = expr->method_call.object->call.callee->token;
                    if ((fn.length == 9 && memcmp(fn.start, "await_all", 9) == 0))
                        receiver_type = "array";
                }
            }
            
            // Tuple element access: result.0.to_string() - element is int
            if (!receiver_type && expr->method_call.object->type == EXPR_TUPLE_INDEX)
                receiver_type = "int";
            
            // Struct method chaining: walk up chain to find root struct type
            if (expr->method_call.object->type == EXPR_METHOD_CALL) {
                // Walk up the chain to find the root struct
                Expr* cur = expr->method_call.object;
                const char* chain_struct = NULL;
                while (cur && cur->type == EXPR_METHOD_CALL) {
                    Expr* obj = cur->method_call.object;
                    if (obj->type == EXPR_IDENT) {
                        char _vn[64]; token_to_cstr(_vn, sizeof(_vn), obj->token);
                        extern const char* get_struct_var_type(const char*);
                        chain_struct = get_struct_var_type(_vn);
                        if (!chain_struct && obj->expr_type && obj->expr_type->kind == TYPE_STRUCT) {
                            static char _csn[64]; token_to_cstr(_csn, sizeof(_csn), obj->expr_type->struct_type.name);
                            chain_struct = _csn;
                        }
                        break;
                    }
                    if (obj->type == EXPR_CALL && obj->call.callee->type == EXPR_IDENT) {
                        // Function call - look up return type
                        char _fn[64]; token_to_cstr(_fn, sizeof(_fn), obj->call.callee->token);
                        extern const char* get_function_return_type(const char*);
                        const char* _rt = get_function_return_type(_fn);
                        if (_rt) {
                            extern int is_known_struct(const char*);
                            if (is_known_struct(_rt)) chain_struct = _rt;
                        }
                        break;
                    }
                    cur = obj;
                }
                // Walk forward through the chain resolving return types
                if (chain_struct) {
                    cur = expr->method_call.object;
                    // Collect method names in the chain
                    Token chain_methods[16]; int chain_len = 0;
                    Expr* walk = expr->method_call.object;
                    while (walk && walk->type == EXPR_METHOD_CALL && chain_len < 16) {
                        chain_methods[chain_len++] = walk->method_call.method;
                        walk = walk->method_call.object;
                    }
                    // Resolve from root to tip
                    const char* cur_type = chain_struct;
                    for (int ci = chain_len - 1; ci >= 0; ci--) {
                        char _cm[64]; token_to_cstr(_cm, sizeof(_cm), chain_methods[ci]);
                        extern const char* lookup_struct_method_return_type(const char*, const char*);
                        const char* ret = lookup_struct_method_return_type(cur_type, _cm);
                        if (ret) cur_type = ret; else break;
                    }
                    extern int is_known_struct(const char*);
                    if (is_known_struct(cur_type)) {
                        Token method = expr->method_call.method;
                        emit("%s_%.*s(", cur_type, method.length, method.start);
                        codegen_expr(expr->method_call.object);
                        for (int i = 0; i < expr->method_call.arg_count; i++) {
                            emit(", "); codegen_expr(expr->method_call.args[i]);
                        }
                        emit(")");
                        break;
                    }
                    // Primitive return type
                    if (strcmp(cur_type, "int") == 0) receiver_type = "int";
                    else if (strcmp(cur_type, "string") == 0) receiver_type = "string";
                }
            }
            
            // Fallback: if no type info, try to infer from expression
            if (!receiver_type && expr->method_call.object->type == EXPR_IDENT) {
                char mname[64]; token_to_cstr(mname, sizeof(mname), method);
                // Only assume string if the object's type is actually string (or unknown)
                bool _obj_is_int = expr->method_call.object->expr_type &&
                    expr->method_call.object->expr_type->kind == TYPE_INT;
                bool _obj_is_float = expr->method_call.object->expr_type &&
                    expr->method_call.object->expr_type->kind == TYPE_FLOAT;
                bool _obj_is_bool = expr->method_call.object->expr_type &&
                    expr->method_call.object->expr_type->kind == TYPE_BOOL;
                if (!_obj_is_int && !_obj_is_float && !_obj_is_bool &&
                    (strcmp(mname, "len") == 0 || strcmp(mname, "upper") == 0 ||
                    strcmp(mname, "lower") == 0 || strcmp(mname, "trim") == 0 ||
                    strcmp(mname, "contains") == 0 || strcmp(mname, "starts_with") == 0 ||
                    strcmp(mname, "ends_with") == 0 || strcmp(mname, "replace") == 0 ||
                    strcmp(mname, "repeat") == 0 || strcmp(mname, "index_of") == 0 ||
                    strcmp(mname, "substring") == 0 || strcmp(mname, "split_at") == 0 ||
                    strcmp(mname, "split_count") == 0 || strcmp(mname, "to_int") == 0 ||
                    strcmp(mname, "to_float") == 0 || strcmp(mname, "bytes") == 0 || strcmp(mname, "chars") == 0)) {
                    receiver_type = "string";
                }
            }
            // Also override int type when method is clearly a string method
            if (receiver_type && strcmp(receiver_type, "int") == 0) {
                char mname[64]; token_to_cstr(mname, sizeof(mname), method);
                // Only override to string for methods that make sense on both types
                // (to_int, to_float are valid on strings; upper/lower/trim are NOT valid on int)
                if (strcmp(mname, "to_int") == 0 || strcmp(mname, "to_float") == 0 ||
                    strcmp(mname, "split_at") == 0 || strcmp(mname, "split_count") == 0) {
                    receiver_type = "string";
                }
            }
            
            // Debug: print what we got
            if (object_type) {
            } else {
            }
            if (receiver_type) {
            } else {
            }
            
            // Special handling for map.set() - need to determine insert function from value type
            if (receiver_type && strcmp(receiver_type, "map") == 0) {
                char method_name[256]; token_to_cstr(method_name, sizeof(method_name), method);
                
                if ((strcmp(method_name, "set") == 0 || strcmp(method_name, "insert") == 0) && 
                    expr->method_call.arg_count == 2) {
                    // Determine insert function based on value type (shared helper).
                    Expr* value_expr = expr->method_call.args[1];
                    const char* insert_func = hashmap_insert_fn_for(value_expr);
                    emit("%s(", insert_func);
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(", ");
                    codegen_expr(expr->method_call.args[1]);
                    emit(")");
                    break;
                }
                
                // map.get(k): pick the getter matching the value type the checker
                // resolved onto this node (from the map's value_type). Mirrors the
                // index m[k] path so getter/var-decl/comparison all agree.
                if (strcmp(method_name, "get") == 0 && expr->method_call.arg_count == 1) {
                    const char* _getter = "hashmap_get_string";
                    if (expr->expr_type) {
                        switch (expr->expr_type->kind) {
                            case TYPE_INT:   _getter = "hashmap_get_int"; break;
                            case TYPE_BOOL:  _getter = "hashmap_get_bool"; break;
                            case TYPE_FLOAT: _getter = "hashmap_get_float"; break;
                            case TYPE_STRING: default: _getter = "hashmap_get_string"; break;
                        }
                    }
                    emit("%s(", _getter);
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // Json-style methods on map-typed objects
                if (strcmp(method_name, "set_string") == 0 && expr->method_call.arg_count == 2) {
                    emit("json_set_string(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "set_int") == 0 && expr->method_call.arg_count == 2) {
                    emit("json_set_int(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "get_string") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_get_string(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "get_int") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_get_int(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "stringify") == 0 && expr->method_call.arg_count == 0) {
                    emit("json_stringify(");
                    codegen_expr(expr->method_call.object);
                    emit(")"); break;
                }
                // HashMap typed methods
                if (strcmp(method_name, "insert_int") == 0 && expr->method_call.arg_count == 2) {
                    emit("hashmap_insert_int(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "insert_string") == 0 && expr->method_call.arg_count == 2) {
                    emit("hashmap_insert_string(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "get_int") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_get_int(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "keys") == 0 && expr->method_call.arg_count == 0) {
                    emit("hashmap_keys(");
                    codegen_expr(expr->method_call.object);
                    emit(")"); break;
                }
                if (strcmp(method_name, "len") == 0 && expr->method_call.arg_count == 0) {
                    emit("hashmap_len(");
                    codegen_expr(expr->method_call.object);
                    emit(")"); break;
                }
                if (strcmp(method_name, "contains") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_has(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "remove") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_remove(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(")"); break;
                }
                if (strcmp(method_name, "clear") == 0 && expr->method_call.arg_count == 0) {
                    emit("hashmap_clear(");
                    codegen_expr(expr->method_call.object);
                    emit(")"); break;
                }
                if (strcmp(method_name, "values") == 0 && expr->method_call.arg_count == 0) {
                    emit("hashmap_values(");
                    codegen_expr(expr->method_call.object);
                    emit(")"); break;
                }
                if (strcmp(method_name, "get_or") == 0 && expr->method_call.arg_count == 2) {
                    emit("hashmap_get_or_int(");
                    codegen_expr(expr->method_call.object);
                    emit(", "); codegen_expr(expr->method_call.args[0]);
                    emit(", "); codegen_expr(expr->method_call.args[1]);
                    emit(")"); break;
                }
            }
            
            if (receiver_type) {
                char method_name[256]; token_to_cstr(method_name, sizeof(method_name), method);
                
                MethodDispatch dispatch;
                // Resolve type parameters (T, K, V) to concrete types
                if (receiver_type && strlen(receiver_type) == 1 && receiver_type[0] >= 'A' && receiver_type[0] <= 'Z')
                    receiver_type = "int";
                if (dispatch_method(receiver_type, method_name, expr->method_call.arg_count, &dispatch)) {
                    // When to_string is dispatched as int_to_string but the object is a method call,
                    // use _Generic to_string macro so the C compiler picks the right variant
                    const char* fn_name = dispatch.c_function;
                    if (strcmp(fn_name, "int_to_string") == 0 &&
                        expr->method_call.object->type == EXPR_METHOD_CALL) {
                        fn_name = "to_string";
                    }
                    emit("%s(", fn_name);
                    if (dispatch.pass_by_ref) {
                        emit("&(");
                        codegen_expr(expr->method_call.object);
                        emit(")");
                    } else {
                        codegen_expr(expr->method_call.object);
                    }
                    // Special handling for reduce: swap args (initial, fn) -> (fn, initial)
                    if (strcmp(method_name, "reduce") == 0 && expr->method_call.arg_count == 2) {
                        emit(", ");
                        codegen_expr(expr->method_call.args[1]); // fn
                        emit(", ");
                        codegen_expr(expr->method_call.args[0]); // initial
                    } else if (strcmp(method_name, "format") == 0) {
                        // Convert all args to strings using to_string macro
                        for (int i = 0; i < expr->method_call.arg_count; i++) {
                            emit(", to_string(");
                            codegen_expr(expr->method_call.args[i]);
                            emit(")");
                        }
                    } else {
                        for (int i = 0; i < expr->method_call.arg_count; i++) {
                            emit(", ");
                            codegen_expr(expr->method_call.args[i]);
                        }
                    }
                    emit(")");
                    goto method_done;
                }
            }
            
            // Special case: `m.get(k).unwrap_or(default)`. HashMap.get returns the
            // raw value (not an Option - many programs rely on that), so routing it
            // through OptionInt_unwrap_or passed a raw int where an OptionInt was
            // expected → C type error. Lower the whole idiom to the runtime's
            // key-or-default lookup (hashmap_get_or_int / _str). (2026-07)
            if (method.length == 9 && memcmp(method.start, "unwrap_or", 9) == 0 &&
                expr->method_call.arg_count == 1 &&
                expr->method_call.object->type == EXPR_METHOD_CALL) {
                Expr* inner = expr->method_call.object;
                Token im = inner->method_call.method;
                bool inner_is_get = (im.length == 3 && memcmp(im.start, "get", 3) == 0 &&
                                     inner->method_call.arg_count == 1);
                Type* rot = inner->method_call.object->expr_type;
                // Scoped to non-string value maps: the string path collides with the
                // print/assign string-method-object wrapper (double-wrap + stray
                // paren). Int/bool/float value maps lower cleanly here; string-value
                // `.get().unwrap_or` remains routed through the general path (logged).
                if (inner_is_get && rot && rot->kind == TYPE_MAP &&
                    !(rot->map_type.value_type && rot->map_type.value_type->kind == TYPE_STRING)) {
                    emit("hashmap_get_or_int(");
                    codegen_expr(inner->method_call.object);       // the map
                    emit(", ");
                    codegen_expr(inner->method_call.args[0]);      // the key
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);       // the default
                    emit(")");
                    break;
                }
            }

            // Fallback: try Result/Option method dispatch
            // When type info is missing, check if method matches known Result/Option API
            {
                bool is_result_method = (
                    (method.length == 5 && memcmp(method.start, "is_ok", 5) == 0) ||
                    (method.length == 6 && memcmp(method.start, "is_err", 6) == 0) ||
                    (method.length == 6 && memcmp(method.start, "unwrap", 6) == 0) ||
                    (method.length == 10 && memcmp(method.start, "unwrap_err", 10) == 0));
                bool is_option_method = (
                    (method.length == 7 && memcmp(method.start, "is_some", 7) == 0) ||
                    (method.length == 7 && memcmp(method.start, "is_none", 7) == 0) ||
                    (method.length == 6 && memcmp(method.start, "unwrap", 6) == 0) ||
                    (method.length == 9 && memcmp(method.start, "unwrap_or", 9) == 0));
                
                if (is_result_method) {
                    emit("ResultInt_%.*s(", method.length, method.start);
                    codegen_expr(expr->method_call.object);
                    emit(")");
                    break;
                }
                if (is_option_method) {
                    emit("OptionInt_%.*s(", method.length, method.start);
                    codegen_expr(expr->method_call.object);
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
                }
            }
            if (receiver_type) {
                char method_name[256]; token_to_cstr(method_name, sizeof(method_name), method);
                
                // HashMap instance methods
                if (strcmp(receiver_type, "map") == 0) {
                    if (strcmp(method_name, "keys") == 0) {
                        emit("hashmap_keys("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
                    if (strcmp(method_name, "len") == 0) {
                        emit("hashmap_len("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
                    if (strcmp(method_name, "contains") == 0) {
                        emit("hashmap_has("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (strcmp(method_name, "get") == 0) {
                        emit("hashmap_get_string("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (strcmp(method_name, "get_int") == 0) {
                        emit("hashmap_get_int("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (strcmp(method_name, "insert_int") == 0) {
                        emit("hashmap_insert_int("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]);
                        emit(", "); codegen_expr(expr->method_call.args[1]); emit(")"); break;
                    }
                    if (strcmp(method_name, "insert_string") == 0) {
                        emit("hashmap_insert_string("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]);
                        emit(", "); codegen_expr(expr->method_call.args[1]); emit(")"); break;
                    }
                    if (strcmp(method_name, "remove") == 0) {
                        emit("hashmap_remove("); codegen_expr(expr->method_call.object);
                        emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    if (strcmp(method_name, "clear") == 0) {
                        emit("hashmap_clear("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
                    if (strcmp(method_name, "values") == 0) {
                        emit("hashmap_values("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
                }
                
                // StringBuilder dispatch
                if (strcmp(receiver_type, "stringbuilder") == 0) {
                    char mname[64]; token_to_cstr(mname, sizeof(mname), method);
                    emit("StringBuilder_%s(", mname);
                    codegen_expr(expr->method_call.object);
                    for (int ai = 0; ai < expr->method_call.arg_count; ai++) {
                        emit(", "); codegen_expr(expr->method_call.args[ai]);
                    }
                    emit(")"); break;
                }
                
                fprintf(stderr, "Error: Unknown method '%.*s' for type '%s'\n", 
                        method.length, method.start, receiver_type);
                
                // Provide helpful hints
                if (strcmp(receiver_type, "map") == 0) {
                    fprintf(stderr, "Hint: Available HashMap methods: .has(key), .get(key), .remove(key), .len()\n");
                    fprintf(stderr, "      Or use indexing: map[\"key\"] to get/set values\n");
                } else if (strcmp(receiver_type, "set") == 0) {
                    fprintf(stderr, "Hint: Available HashSet methods: .add(item), .contains(item), .remove(item), .len()\n");
                } else if (strcmp(receiver_type, "array") == 0) {
                    char mname[64];
                    token_to_cstr(mname, sizeof(mname), method);
                    if (strcmp(mname, "len") == 0) {
                        emit("("); codegen_expr(expr->method_call.object); emit(").count"); break;
                    } else if (strcmp(mname, "join") == 0 && expr->method_call.arg_count == 1) {
                        emit("array_join_str("); codegen_expr(expr->method_call.object); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    } else if (strcmp(mname, "slice") == 0 && expr->method_call.arg_count == 2) {
                        emit("wyn_array_slice_range("); codegen_expr(expr->method_call.object); emit(", "); codegen_expr(expr->method_call.args[0]); emit(", "); codegen_expr(expr->method_call.args[1]); emit(")"); break;
                    } else if (strcmp(mname, "reverse") == 0) {
                        emit("array_reverse_copy("); codegen_expr(expr->method_call.object); emit(")"); break;
                    } else if (strcmp(mname, "push") == 0) {
                        // Check if this is a spawn array (WynIntArray)
                        if (expr->method_call.object->type == EXPR_IDENT) {
                            char _on[256]; token_to_cstr(_on, sizeof(_on), expr->method_call.object->token);
                            if (is_spawn_array(_on)) {
                                emit("int_array_push(&("); codegen_expr(expr->method_call.object); emit("), (long long)("); codegen_expr(expr->method_call.args[0]); emit("))"); break;
                            }
                        }
                        emit("array_push(&("); codegen_expr(expr->method_call.object); emit("), (long long)("); codegen_expr(expr->method_call.args[0]); emit("))"); break;
                    } else if (strcmp(mname, "pop") == 0) {
                        emit("array_pop_int(&("); codegen_expr(expr->method_call.object); emit("))"); break;
                    } else if (strcmp(mname, "filter") == 0) {
                        emit("wyn_array_filter("); codegen_expr(expr->method_call.object); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    } else if (strcmp(mname, "map") == 0) {
                        emit("wyn_array_map("); codegen_expr(expr->method_call.object); emit(", "); codegen_expr(expr->method_call.args[0]); emit(")"); break;
                    }
                    fprintf(stderr, "Hint: Available array methods: .len(), .push(), .pop(), .slice(), .join(), .filter(), .map()\n");
                } else if (strcmp(receiver_type, "string") == 0) {
                    fprintf(stderr, "Hint: Available string methods: .len(), .upper(), .lower(), .trim(), .contains(substr)\n");
                }
            } else {
                fprintf(stderr, "Error: Unknown method '%.*s' (no type info)\n", 
                        method.length, method.start);
            }
            method_done:
            if (_mc_chain_wrap && _mc_id >= 0) {
                emit("; wyn_rc_release(__mo%d); __mcr%d; })", _mc_id, _mc_id);
                _mc_obj->_codegen_temp_id = -1;
            }
            break;
        }
        case EXPR_ARRAY: {
            // Generate simple array creation
            static int arr_counter = 0;
            int arr_id = arr_counter++;
            if (codegen_emit_int_array && expr->array.count == 0) {
                emit("({ WynIntArray __arr_%d = int_array_new(); __arr_%d; })", arr_id, arr_id);
                break;
            }
            if (codegen_emit_int_array && expr->array.count > 0) {
                emit("({ WynIntArray __arr_%d = int_array_new(); ", arr_id);
                for (int i = 0; i < expr->array.count; i++) {
                    emit("int_array_push(&__arr_%d, ", arr_id);
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                }
                emit("__arr_%d; })", arr_id);
                break;
            }
            emit("({ WynArray __arr_%d = array_new(); ", arr_id);
            for (int i = 0; i < expr->array.count; i++) {
                Expr* elem = expr->array.elements[i];
                if (elem->type == EXPR_STRING) {
                    emit("array_push_str(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit("); ");
                } else if (elem->type == EXPR_FLOAT) {
                    emit("array_push_float(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit("); ");
                } else if (elem->type == EXPR_BOOL) {
                    emit("array_push_bool(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit("); ");
                } else if (elem->type == EXPR_STRUCT_INIT) {
                    // Struct literal
                    Token type_name = elem->struct_init.type_name;
                    emit("array_push_struct(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit(", ");
                    if (current_module_prefix) {
                        emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                    } else {
                        emit("%.*s", type_name.length, type_name.start);
                    }
                    emit("); ");
                } else if (elem->expr_type && elem->expr_type->kind == TYPE_STRUCT) {
                    // Variable or function call returning struct
                    Token type_name = elem->expr_type->struct_type.name;
                    emit("array_push_struct(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit(", ");
                    if (current_module_prefix) {
                        emit("%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                    } else {
                        emit("%.*s", type_name.length, type_name.start);
                    }
                    emit("); ");
                } else if (elem->type == EXPR_ARRAY) {
                    // Nested array: push as array value
                    static int nested_arr_counter = 10000;
                    int nested_id = nested_arr_counter++;
                    emit("{ WynArray __nested_%d = ", nested_id);
                    codegen_expr(elem);
                    emit("; array_push_array(&__arr_%d, &__nested_%d); } ", arr_id, nested_id);
                } else if (elem->expr_type && elem->expr_type->kind == TYPE_ARRAY) {
                    // Variable holding an array
                    emit("{ WynArray __tmp_%d = ", arr_id * 100 + i);
                    codegen_expr(elem);
                    emit("; array_push_array(&__arr_%d, &__tmp_%d); } ", arr_id, arr_id * 100 + i);
                } else if ((elem->expr_type && elem->expr_type->kind == TYPE_ENUM) ||
                           (elem->type == EXPR_METHOD_CALL &&
                            elem->method_call.object->type == EXPR_IDENT &&
                            ({ char _en[128]; token_to_cstr(_en, sizeof(_en), elem->method_call.object->token);
                               extern int is_data_enum_type(const char*); is_data_enum_type(_en); }))) {
                    // Data-enum value (a tagged-union struct), e.g. `E.A(5)` - push
                    // by value like a struct, using the enum type as the C type.
                    char _en[128];
                    if (elem->expr_type && elem->expr_type->kind == TYPE_ENUM && elem->expr_type->name.length > 0)
                        token_to_cstr(_en, sizeof(_en), elem->expr_type->name);
                    else
                        token_to_cstr(_en, sizeof(_en), elem->method_call.object->token);
                    emit("array_push_struct(&__arr_%d, ", arr_id);
                    codegen_expr(elem);
                    emit(", %s); ", _en);
                } else {
                    // Check element type to use correct push function
                    Type* etype = elem->expr_type;
                    if (etype && etype->kind == TYPE_STRING) {
                        emit("array_push_str(&__arr_%d, ", arr_id);
                    } else if (etype && etype->kind == TYPE_FLOAT) {
                        emit("array_push_float(&__arr_%d, ", arr_id);
                    } else if (etype && etype->kind == TYPE_BOOL) {
                        emit("array_push_bool(&__arr_%d, ", arr_id);
                    } else {
                        emit("array_push_int(&__arr_%d, ", arr_id);
                    }
                    codegen_expr(elem);
                    emit("); ");
                }
            }
            emit("__arr_%d; })", arr_id);
            break;
        }
        case EXPR_HASHMAP_LITERAL: {
            // v1.3.0: {} with initialization supporting multiple types
            if (expr->array.count == 0) {
                // Empty hashmap
                emit("hashmap_new()");
            } else {
                // HashMap with initial values
                static int map_counter = 0;
                int map_id = map_counter++;
                emit("({ WynHashMap* __map_%d = hashmap_new(); ", map_id);
                
                // Insert key-value pairs (stored as key, value, key, value...)
                for (int i = 0; i < expr->array.count; i += 2) {
                    Expr* value_expr = expr->array.elements[i+1];
                    // Shared type→insert-fn selection (consults expr_type too, so a
                    // typed non-literal value like `{k: someVar}` isn't defaulted to int).
                    const char* insert_func = hashmap_insert_fn_for(value_expr);
                    emit("%s(__map_%d, ", insert_func, map_id);
                    codegen_expr(expr->array.elements[i]);    // key
                    emit(", ");
                    codegen_expr(expr->array.elements[i+1]);  // value
                    emit("); ");
                }
                
                emit("__map_%d; })", map_id);
            }
            break;
        }
        case EXPR_HASHSET_LITERAL: {
            // v1.3.0: {:} with initialization
            if (expr->array.count == 0) {
                // Empty hashset
                emit("hashset_new()");
            } else {
                // HashSet with initial values
                static int set_counter = 0;
                int set_id = set_counter++;
                emit("({ WynHashSet* __set_%d = hashset_new(); ", set_id);
                
                // Add elements
                for (int i = 0; i < expr->array.count; i++) {
                    emit("hashset_add(__set_%d, ", set_id);
                    codegen_expr(expr->array.elements[i]);
                    emit("); ");
                }
                
                emit("__set_%d; })", set_id);
            }
            break;
        }
        case EXPR_INDEX: {
            // Check if this is string indexing
            if (expr->index.array->expr_type && expr->index.array->expr_type->kind == TYPE_STRING) {
                // String indexing: s[i] -> wyn_string_charat(s, i)
                emit("wyn_string_charat(");
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
                break;
            }
            
            // Check if this is map indexing by looking at the array type
            bool is_map_index = false;
            if (expr->index.array->expr_type && expr->index.array->expr_type->kind == TYPE_MAP) {
                is_map_index = true;
            } else if (expr->index.index->type == EXPR_STRING || 
                      (expr->index.index->expr_type && expr->index.index->expr_type->kind == TYPE_STRING)) {
                is_map_index = true;
            }
            
            if (is_map_index) {
                // Map indexing: pick the getter matching the element type the
                // checker resolved for this index (from the map's value_type),
                // so int/float/bool maps don't read through hashmap_get_string.
                const char* _getter = "hashmap_get_string";
                if (expr->expr_type) {
                    switch (expr->expr_type->kind) {
                        case TYPE_INT:   case TYPE_BOOL: _getter = (expr->expr_type->kind == TYPE_BOOL) ? "hashmap_get_bool" : "hashmap_get_int"; break;
                        case TYPE_FLOAT: _getter = "hashmap_get_float"; break;
                        case TYPE_STRING: default: _getter = "hashmap_get_string"; break;
                    }
                }
                emit("%s(", _getter);
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
            } else {
                // Check if this is a spawn array (WynIntArray)
                if (expr->index.array->type == EXPR_IDENT) {
                    char _on[256]; token_to_cstr(_on, sizeof(_on), expr->index.array->token);
                    extern int is_int_array_var(const char*);
                    if (is_spawn_array(_on) || is_int_array_var(_on)) {
                        emit("int_array_get(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                        break;
                    }
                }
                // Array indexing with tagged union support
                // Determine if this is a string array by checking the source
                bool is_string_array = false;
                
                // Check method calls that return string arrays
                if (expr->index.array->type == EXPR_METHOD_CALL) {
                    Token method = expr->index.array->method_call.method;
                    if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "lines", 5) == 0)) {
                        is_string_array = true;
                    }
                }
                
                // Check function calls that return string arrays
                if (expr->index.array->type == EXPR_CALL) {
                    Token callee = expr->index.array->call.callee->token;
                    // System::args returns string array
                    if (callee.length == 12 && memcmp(callee.start, "System::args", 12) == 0) {
                        is_string_array = true;
                    }
                    // File::list_dir returns string array
                    if (callee.length == 14 && memcmp(callee.start, "File::list_dir", 14) == 0) {
                        is_string_array = true;
                    }
                }
                
                // Check identifiers - if variable was assigned from string array
                // For now, we track common patterns
                if (expr->index.array->type == EXPR_IDENT) {
                    Token var_name = expr->index.array->token;
                    // Check tracked string content arrays
                    char _van[256]; token_to_cstr(_van, sizeof(_van), var_name);
                    extern int is_str_array_var(const char*);
                    // Content-tracked string arrays (populated when the array is
                    // declared). The old code ALSO force-typed any array literally
                    // named args/files/names/parts/entries as string regardless of
                    // its real element type - a miscompile (`var x = parts[1]` on an
                    // int array read through array_get_str). Removed: the real
                    // element type above + this content registry are authoritative.
                    if (is_str_array_var(_van)) is_string_array = true;
                }
                
                // Check array literals - if first element is string, assume string array
                if (expr->index.array->type == EXPR_ARRAY) {
                    if (expr->index.array->array.count > 0) {
                        Expr* first = expr->index.array->array.elements[0];
                        if (first->type == EXPR_STRING) {
                            is_string_array = true;
                        }
                    }
                }
                
                // Check identifiers - look up in symbol table (TODO: needs type info)
                // For now, we can't determine this without better type tracking
                
                // Check if this is chained indexing (matrix[0][1])
                if (expr->index.array->type == EXPR_INDEX) {
                    // This is chained indexing: arr[i][j]
                    if (expr->index.array->index.array->type == EXPR_INDEX) {
                        // Triple chained: arr[i][j][k]
                        emit("array_get_nested3_int(");
                        codegen_expr(expr->index.array->index.array->index.array);
                        emit(", ");
                        codegen_expr(expr->index.array->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else {
                        // Double chained: arr[i][j]
                        emit("array_get_nested_int(");
                        codegen_expr(expr->index.array->index.array);
                        emit(", ");
                        codegen_expr(expr->index.array->index.index);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    }
                } else {
                    // Single indexing: arr[i]
                    // Check element type from type annotation
                    bool is_struct_array = false;
                    bool is_float_array = false;
                    Type* elem_type = NULL;
                    if (expr->index.array->expr_type && 
                        expr->index.array->expr_type->kind == TYPE_ARRAY) {
                        elem_type = expr->index.array->expr_type->array_type.element_type;
                        if (elem_type) {
                            if (elem_type->kind == TYPE_STRUCT) {
                                is_struct_array = true;
                            } else if (elem_type->kind == TYPE_STRING) {
                                is_string_array = true;
                            } else if (elem_type->kind == TYPE_FLOAT) {
                                is_float_array = true;
                            }
                        }
                    }
                    // Array literal with a float first element -> float array.
                    if (!is_float_array && expr->index.array->type == EXPR_ARRAY &&
                        expr->index.array->array.count > 0) {
                        Expr* f0 = expr->index.array->array.elements[0];
                        if (f0->type == EXPR_FLOAT ||
                            (f0->expr_type && f0->expr_type->kind == TYPE_FLOAT))
                            is_float_array = true;
                    }
                    
                    // Map array: cast result to WynHashMap*
                    if (elem_type && elem_type->kind == TYPE_MAP) {
                        emit("(WynHashMap*)array_get_int(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else if (elem_type && elem_type->kind == TYPE_FUNCTION) {
                        // Function pointer array: cast result to function pointer
                        // Determine return type
                        const char* ret = "long long";
                        if (elem_type->fn_type.return_type) {
                            if (elem_type->fn_type.return_type->kind == TYPE_STRING) ret = "const char*";
                            else if (elem_type->fn_type.return_type->kind == TYPE_FLOAT) ret = "double";
                            else if (elem_type->fn_type.return_type->kind == TYPE_BOOL) ret = "bool";
                        }
                        emit("((%s (*)(", ret);
                        for (int pi = 0; pi < elem_type->fn_type.param_count; pi++) {
                            if (pi > 0) emit(", ");
                            Type* pt = elem_type->fn_type.param_types[pi];
                            if (pt && pt->kind == TYPE_STRING) emit("const char*");
                            else if (pt && pt->kind == TYPE_FLOAT) emit("double");
                            else emit("long long");
                        }
                        emit("))array_get_int(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit("))");
                    } else if (is_struct_array) {
                        emit("array_get_struct(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(", ");
                        Token type_name = elem_type->struct_type.name;
                        emit("%.*s", type_name.length, type_name.start);
                        emit(")");
                    } else if (is_float_array) {
                        emit("array_get_float(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else if (is_string_array) {
                        emit("array_get_str(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    } else {
                        // Default to int array (most common case)
                        emit("array_get_int(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(")");
                    }
                }
            }
            break;
        }
        case EXPR_ASSIGN: {
            // RC: release old string value AFTER evaluating new value
            // (old value might be referenced in the RHS expression)
            char target_name[512];
            token_to_cstr(target_name, sizeof target_name, expr->assign.name);
            // A var whose name is a C keyword was emitted with the wynfn_ prefix at
            // its declaration; the assignment must use the same prefixed C name.
            { extern int is_user_collision(const char*);
              if (is_user_collision(target_name)) {
                memmove(target_name + WYN_UFN_PFX_LEN, target_name, strlen(target_name) + 1);
                memcpy(target_name, WYN_UFN_PFX, WYN_UFN_PFX_LEN); } }
            bool _rc_string_assign = false;
            {
                extern int is_string_var(const char*);
                if (is_string_var(target_name)) _rc_string_assign = true;
            }
            if (_rc_string_assign) {
                Expr* rhs = expr->assign.value;
                bool rhs_is_fresh = (rhs->type == EXPR_BINARY || rhs->type == EXPR_CALL ||
                                     rhs->type == EXPR_METHOD_CALL || rhs->type == EXPR_STRING_INTERP);
                if (rhs_is_fresh) {
                    // Fresh temporary: ownership transfer
                    // If concat reused the buffer (same pointer), don't release
                    emit("({ const char* __rc_tmp = ");
                    codegen_expr(expr->assign.value);
                    emit("; if (__rc_tmp != %s) { wyn_rc_release(%s); } %s = __rc_tmp; })", target_name, target_name, target_name);
                } else {
                    // Shared reference: retain new, release old
                    emit("({ const char* __rc_tmp = ");
                    codegen_expr(expr->assign.value);
                    emit("; wyn_rc_retain(__rc_tmp); wyn_rc_release(%s); %s = __rc_tmp; })", target_name, target_name);
                }
                break;
            }
            // RC: closure reassignment. The target holds a WynClosure whose env
            // it owns (registered at declaration); assigning over it must release
            // the old env or it leaks. Evaluate the RHS first (it may reference
            // the target), then retain-if-shared / release-old, mirroring the
            // string assign above. A fresh closure (call returning fn type)
            // transfers ownership; an ident copy shares the env so retain it.
            {
                extern int is_closure_scope_var(const char*);
                if (is_closure_scope_var(target_name)) {
                    Expr* rhs = expr->assign.value;
                    bool rhs_is_shared = (rhs->type == EXPR_IDENT);
                    emit("({ WynClosure __cc_tmp = ");
                    codegen_expr(rhs);
                    if (rhs_is_shared)
                        emit("; wyn_rc_retain(__cc_tmp.env); wyn_rc_release(%s.env); %s = __cc_tmp; })", target_name, target_name);
                    else
                        emit("; if (__cc_tmp.env != %s.env) { wyn_rc_release(%s.env); } %s = __cc_tmp; })", target_name, target_name, target_name);
                    break;
                }
            }

            // Check if we need to prefix the assignment target with module name
            
            if (current_module_prefix && !strchr(target_name, ':') && !strchr(target_name, '.')) {
                // Check if this is a parameter - never prefix parameters
                if (is_parameter(target_name)) {
                    if (is_mut_parameter(target_name)) {
                        emit("(*%.*s) = ", expr->assign.name.length, expr->assign.name.start);
                    } else {
                        emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                    }
                    codegen_expr(expr->assign.value);
                    break;
                }
                
                // Check if this is a local variable - never prefix local variables
                // (target_name already carries the wynfn_ prefix if it's a C-keyword
                // collision, so use it rather than the raw token).
                if (is_local_variable(target_name)) {
                    emit("%s = ", target_name);
                    codegen_expr(expr->assign.value);
                    break;
                }
                
                // Check if this looks like a module-level variable
                // Don't prefix common local variable names or single-letter variables
                bool is_single_letter = (strlen(target_name) == 1);
                const char* common_locals[] = {"i", "j", "k", "x", "y", "z", "n", "result", "temp", "value",
                                               "a", "b", "c", "d", "e", "f", "g", "h", "m", "p", "q", "r", "s", "t", NULL};
                bool is_common_local = false;
                for (int i = 0; common_locals[i] != NULL; i++) {
                    if (strcmp(target_name, common_locals[i]) == 0) {
                        is_common_local = true;
                        break;
                    }
                }
                
                if (!is_common_local && !is_single_letter) {
                    emit("%s_%.*s = ", current_module_prefix, expr->assign.name.length, expr->assign.name.start);
                } else {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                }
            } else {
                if (is_mut_parameter(target_name)) {
                    emit("(*%s) = ", target_name);
                } else {
                    emit("%s = ", target_name);
                }
            }
            codegen_expr(expr->assign.value);
            break;
        }
        case EXPR_STRUCT_INIT: {
            Token type_name = expr->struct_init.type_name;
            
            // Use monomorphic name if available (for generic structs)
            const char* actual_type_name;
            int actual_type_name_len;
            static char prefixed_type_name[128];
            
            if (expr->struct_init.monomorphic_name) {
                actual_type_name = expr->struct_init.monomorphic_name;
                actual_type_name_len = strlen(actual_type_name);
            } else {
                // Check if type_name contains a module prefix (from member expression)
                // e.g., point.Point should become point_Point
                char temp_name[128];
                token_to_cstr(temp_name, sizeof(temp_name), type_name);
                
                // Check if there's a dot in the name (module.Type)
                char* dot = strchr(temp_name, '.');
                if (dot) {
                    // Replace dot with underscore: point.Point → point_Point
                    *dot = '_';
                    snprintf(prefixed_type_name, 128, "%s", temp_name);
                    actual_type_name = prefixed_type_name;
                    actual_type_name_len = strlen(prefixed_type_name);
                } else if (current_module_prefix) {
                    // Add module prefix if in module context
                    snprintf(prefixed_type_name, 128, "%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                    actual_type_name = prefixed_type_name;
                    actual_type_name_len = strlen(prefixed_type_name);
                } else {
                    actual_type_name = type_name.start;
                    actual_type_name_len = type_name.length;
                }
            }
            
            // Simple struct initialization - stack allocated
            // Wrapped in extra parens to protect commas in macro contexts
            {
                emit("((%.*s){", actual_type_name_len, actual_type_name);
                char _sn[96]; token_to_cstr(_sn, sizeof(_sn), type_name);
                extern const char* current_assign_target_kind;
                extern int get_struct_field_option_family(const char*, const char*, char*, size_t);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    if (i > 0) emit(", ");
                    emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                    // If the field's declared type is optional (`f: T?`), tell
                    // Some/None codegen the exact Option family so a bare
                    // `f: None` / `f: Some(x)` lowers to that family, not OptionInt.
                    char _fn[96]; token_to_cstr(_fn, sizeof(_fn), expr->struct_init.field_names[i]);
                    char _fam[128];
                    const char* _prev_kind = current_assign_target_kind;
                    if (get_struct_field_option_family(_sn, _fn, _fam, sizeof(_fam)))
                        current_assign_target_kind = _fam;
                    codegen_expr(expr->struct_init.field_values[i]);
                    current_assign_target_kind = _prev_kind;
                }
                emit("})");
            }
            break;
        }
        case EXPR_OPT_CHAIN: {
            // `opt?.field` -> ({ OptionX __oc = <opt>;
            //    __oc.tag==1 ? OptionField_Some(__oc.value.field) : OptionField_None(); })
            // The result family is the checker-resolved expr_type (Option<FieldType>).
            // Use LOCAL buffers, not static: the object may itself be an opt-chain
            // whose codegen would clobber a shared static buffer mid-emission.
            char result_fam[128] = "OptionInt";
            if (expr->expr_type && expr->expr_type->kind == TYPE_STRUCT &&
                expr->expr_type->struct_type.name.length > 0) {
                token_to_cstr(result_fam, sizeof(result_fam), expr->expr_type->struct_type.name);
            }
            char obj_fam[128] = "OptionInt";
            if (expr->opt_chain.object->expr_type &&
                expr->opt_chain.object->expr_type->kind == TYPE_STRUCT &&
                expr->opt_chain.object->expr_type->struct_type.name.length > 0) {
                token_to_cstr(obj_fam, sizeof(obj_fam), expr->opt_chain.object->expr_type->struct_type.name);
            }
            static int _oc_id = 0; int _id = _oc_id++;
            emit("({ %s __oc%d = ", obj_fam, _id);
            codegen_expr(expr->opt_chain.object);
            emit("; __oc%d.tag == 1 ? %s_Some(__oc%d.value.%.*s) : %s_None(); })",
                 _id, result_fam, _id,
                 expr->opt_chain.field.length, expr->opt_chain.field.start,
                 result_fam);
            break;
        }
        case EXPR_FIELD_ACCESS: {
            // Check if this is enum member access by looking at the pattern
            Token obj_name = expr->field_access.object->token;
            Token field_name = expr->field_access.field;
            
            // Simple heuristic: if object is an identifier starting with uppercase
            // and we're accessing a field that's all uppercase, it's likely enum access
            if (expr->field_access.object->type == EXPR_IDENT &&
                obj_name.length > 0 && obj_name.start[0] >= 'A' && obj_name.start[0] <= 'Z' &&
                field_name.length > 0 && field_name.start[0] >= 'A' && field_name.start[0] <= 'Z') {
                
                // Generate the enum constant name (EnumName_MEMBER)
                // For data enums, zero-arg variants are constructor functions needing ()
                extern int is_data_enum_type(const char*);
                char _obj_name[128];
                token_to_cstr(_obj_name, sizeof(_obj_name), obj_name);
                emit("%.*s_%.*s",
                     obj_name.length, obj_name.start,
                     field_name.length, field_name.start);
                if (is_data_enum_type(_obj_name)) {
                    // Check if this variant has no data (zero-arg constructor)
                    extern const char* get_enum_variant_c_type(const char*, const char*);
                    char _field[128];
                    token_to_cstr(_field, sizeof(_field), field_name);
                    const char* vtype = get_enum_variant_c_type(_obj_name, _field);
                    if (strcmp(vtype, "void") == 0) {
                        emit("()");
                    }
                }
                return;
            }
            
            // Handle module.function calls
            // Check if this is a module call (resolve aliases)
            char module_name[256];
            token_to_cstr(module_name, sizeof(module_name), obj_name);
            const char* resolved_module = resolve_module_alias(module_name);
            
            // Check if it's a known module
            extern bool is_module_loaded(const char* name);
            extern bool is_builtin_module(const char* name);
            if (is_module_loaded(resolved_module) || is_builtin_module(resolved_module)) {
                // Emit as module_function
                emit("%s_%.*s", resolved_module,
                     field_name.length, field_name.start);
            } else if (expr->field_access.field.length == 6 && 
                       memcmp(expr->field_access.field.start, "length", 6) == 0) {
                // Special case for array.length -> arr.count for dynamic arrays
                emit("(");
                codegen_expr(expr->field_access.object);
                emit(").count");
            } else {
                codegen_expr(expr->field_access.object);
                emit(".%.*s", expr->field_access.field.length, expr->field_access.field.start);
            }
            break;
        }
        case EXPR_MATCH: {
            // Generate match expression using if-else chain
            static int match_counter = 0;
            int match_id = match_counter++;
            
            emit("({ ");
            
            // Get the type of the match value
            Type* match_type = expr->match.value->expr_type;
            const char* type_name = "int";
            int type_name_len = 3;
            int is_data_enum = 0;
            
            // Check if match value is an enum variable. Only mark it a data
            // enum if it actually carries payloads - a plain (dataless) enum is
            // a bare C enum and must be matched with `==`, not `.tag ==`.
            if (expr->match.value->type == EXPR_IDENT) {
                char _mv[128]; token_to_cstr(_mv, sizeof(_mv), expr->match.value->token);
                extern const char* get_enum_var_type(const char*);
                const char* _et = get_enum_var_type(_mv);
                if (_et) {
                    type_name = _et; type_name_len = strlen(_et);
                    extern int is_data_enum_type(const char*);
                    if (is_data_enum_type(_et)) is_data_enum = 1;
                }
            }
            
            // Also check match arms - if any arm uses Shape.Variant pattern, detect data enum
            if (!is_data_enum) {
                for (int _a = 0; _a < expr->match.arm_count; _a++) {
                    Pattern* _p = expr->match.arms[_a].pattern;
                    if (_p && _p->type == PATTERN_OPTION) {
                        if (_p->option.enum_name.length > 0) {
                            char _en[128]; token_to_cstr(_en, sizeof(_en), _p->option.enum_name);
                            extern int is_data_enum_type(const char*);
                            if (is_data_enum_type(_en)) {
                                type_name = _p->option.enum_name.start;
                                type_name_len = _p->option.enum_name.length;
                                is_data_enum = 1;
                                break;
                            }
                        } else {
                            // No prefix - look up variant name in all enums
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), _p->option.variant_name);
                            extern const char* find_enum_for_variant(const char*);
                            const char* _found = find_enum_for_variant(_vn);
                            if (_found) {
                                type_name = _found;
                                type_name_len = strlen(_found);
                                extern int is_data_enum_type(const char*);
                                if (is_data_enum_type(_found)) is_data_enum = 1;
                                break;
                            }
                        }
                    }
                }
            }
            
            if (!is_data_enum) {
                if (match_type && match_type->kind == TYPE_ENUM && match_type->name.length > 0) {
                    type_name = match_type->name.start;
                    type_name_len = match_type->name.length;
                    // Check if this enum has data variants
                    char _en2[128]; snprintf(_en2, 128, "%.*s", type_name_len, type_name);
                    extern int is_data_enum_type(const char*);
                    if (is_data_enum_type(_en2)) { is_data_enum = 1; }
                } else if (match_type && match_type->kind == TYPE_STRING) {
                    type_name = "const char*";
                    type_name_len = 12;
                } else if (match_type && match_type->kind == TYPE_STRUCT &&
                           match_type->struct_type.name.length > 0) {
                    // Matching a struct value: declare __match_val with the
                    // struct's C type, not the default `int`.
                    type_name = match_type->struct_type.name.start;
                    type_name_len = match_type->struct_type.name.length;
                }
            }
            // Check bare identifiers against known enum variants
            if (!is_data_enum && !(match_type && match_type->kind == TYPE_ENUM)) {
                extern const char* find_enum_for_variant(const char*);
                for (int _a = 0; _a < expr->match.arm_count; _a++) {
                    Pattern* _p = expr->match.arms[_a].pattern;
                    if (_p && _p->type == PATTERN_IDENT) {
                        char _vn[128]; token_to_cstr(_vn, sizeof(_vn), _p->ident.name);
                        const char* found = find_enum_for_variant(_vn);
                        if (found) {
                            type_name = found;
                            type_name_len = strlen(found);
                            break;
                        }
                    }
                }
            }
            
            // Detect a built-in Option/Result match value (OptionInt/OptionString/
            // ResultInt/ResultString). The checker types these as structs; without
            // this the Some/None/Ok/Err arms fell through to the bare-enum `==`
            // path and emitted broken C. `opt_kind`: 1=Option, 2=Result, 0=neither.
            // opt_val_is_str drives string-var registration; opt_val_cty is the C
            // type of the Some/Ok payload (long long | const char* | double | bool).
            int opt_kind = 0; int opt_val_is_str = 0; const char* opt_val_cty = "long long";
            if (match_type && match_type->kind == TYPE_STRUCT && match_type->struct_type.name.length > 0) {
                char _mtn[64]; token_to_cstr(_mtn, sizeof(_mtn), match_type->struct_type.name);
                if (strcmp(_mtn, "OptionInt") == 0) { opt_kind = 1; opt_val_cty = "long long"; }
                else if (strcmp(_mtn, "OptionString") == 0) { opt_kind = 1; opt_val_is_str = 1; opt_val_cty = "const char*"; }
                else if (strcmp(_mtn, "OptionFloat") == 0) { opt_kind = 1; opt_val_cty = "double"; }
                else if (strcmp(_mtn, "OptionBool") == 0) { opt_kind = 1; opt_val_cty = "bool"; }
                else if (strncmp(_mtn, "Option", 6) == 0) {
                    // Monomorphic Option<Struct> (OptionUser): payload is the struct
                    // value, bound by value.
                    static char _osmcty[96]; snprintf(_osmcty, sizeof(_osmcty), "%s", _mtn + 6);
                    opt_kind = 1; opt_val_cty = _osmcty;
                }
                else if (strcmp(_mtn, "ResultInt") == 0) { opt_kind = 2; opt_val_cty = "long long"; }
                else if (strcmp(_mtn, "ResultString") == 0) { opt_kind = 2; opt_val_is_str = 1; opt_val_cty = "const char*"; }
                else if (strcmp(_mtn, "ResultFloat") == 0) { opt_kind = 2; opt_val_cty = "double"; }
                else if (strcmp(_mtn, "ResultBool") == 0) { opt_kind = 2; opt_val_cty = "bool"; }
            }

            // Store match value in temp variable
            emit("%.*s __match_val_%d = ", type_name_len, type_name, match_id);
            codegen_expr(expr->match.value);
            emit("; ");
            
            // Determine result type from first arm's expression
            const char* result_type = "long long";
            if (expr->match.arm_count > 0 && expr->match.arms[0].result) {
                // Check all arms to determine result type
                for (int _a = 0; _a < expr->match.arm_count; _a++) {
                    Expr* arm_result = expr->match.arms[_a].result;
                    if (!arm_result) continue;
                    if (arm_result->type == EXPR_STRING) {
                        result_type = "const char*"; break;
                    } else if (arm_result->type == EXPR_FLOAT) {
                        result_type = "double"; break;
                    } else if (arm_result->expr_type) {
                        if (arm_result->expr_type->kind == TYPE_STRING) {
                            result_type = "const char*"; break;
                        } else if (arm_result->expr_type->kind == TYPE_FLOAT) {
                            result_type = "double"; break;
                        } else if (arm_result->expr_type->kind == TYPE_BOOL) {
                            result_type = "bool"; break;
                        }
                    }
                }
            }
            
            // Generate result variable, zero-initialized. A non-exhaustive match
            // (checker warns, but enum value typing may not always flag it) emits
            // no default arm; without this the result would be read uninitialized
            // on an unmatched value. {0} is valid for scalars, pointers, and
            // structs (NULL / 0 / zeroed).
            emit("%s __match_result_%d = {0}; ", result_type, match_id);
            
            // Generate if-else chain for each arm
            for (int i = 0; i < expr->match.arm_count; i++) {
                Pattern* pat = expr->match.arms[i].pattern;
                
                // Unwrap guard pattern: `_ if cond =>` becomes wildcard + guard
                Expr* guard_expr = NULL;
                if (pat->type == PATTERN_GUARD) {
                    guard_expr = pat->guard.guard;
                    pat = pat->guard.pattern;
                }
                
                if (i > 0) emit("else ");
                
                // Built-in Option/Result arms: Some(v)/None, Ok(v)/Err(e).
                // Uses the struct .tag (Some/Ok = the truthy tag) and the payload
                // field (.value for Option; .data.ok_value/.err_value for Result),
                // binding the payload with the family's value type.
                if (opt_kind && pat->type != PATTERN_WILDCARD) {
                    // is_some_arm = "the tag-truthy variant" (Some for Option, Ok for
                    // Result). Some/None are dedicated tokens (PATTERN_OPTION.is_some,
                    // empty variant_name); Ok/Err arrive as variant_name.
                    int is_some_arm = 0; Pattern* binder = NULL;
                    if (pat->type == PATTERN_OPTION) {
                        binder = pat->option.inner;
                        char _pv[32] = ""; int _pl = pat->option.variant_name.length;
                        if (_pl > 0 && _pl < 31) { memcpy(_pv, pat->option.variant_name.start, _pl); _pv[_pl]=0; }
                        if (_pl > 0) {
                            // Named variant (Ok/Err, or an explicit Some/None).
                            if (strcmp(_pv, "Some") == 0 || strcmp(_pv, "Ok") == 0) is_some_arm = 1;
                            else is_some_arm = 0;   // None / Err
                        } else {
                            // Bare Some(..)/None token - trust the is_some flag.
                            is_some_arm = pat->option.is_some ? 1 : 0;
                        }
                    } else if (pat->type == PATTERN_IDENT) {
                        char _pv[32] = ""; int _pl = pat->ident.name.length;
                        if (_pl > 0 && _pl < 31) { memcpy(_pv, pat->ident.name.start, _pl); _pv[_pl]=0; }
                        if (strcmp(_pv, "Some") == 0 || strcmp(_pv, "Ok") == 0) is_some_arm = 1;
                        else is_some_arm = 0;   // None / Err
                    }
                    // Option: Some tag=1, None tag=0. Result: Ok tag=0, Err tag=1.
                    int want_tag;
                    if (opt_kind == 1) want_tag = is_some_arm ? 1 : 0;
                    else want_tag = is_some_arm ? 0 : 1;   // Result Ok=0 / Err=1
                    if (guard_expr) {
                        emit("if (__match_val_%d.tag == %d && (", match_id, want_tag);
                        codegen_expr(guard_expr); emit(")) { ");
                    } else {
                        emit("if (__match_val_%d.tag == %d) { ", match_id, want_tag);
                    }
                    // Bind the payload if the arm names it.
                    if (binder && binder->type == PATTERN_IDENT) {
                        const char* cty = opt_val_cty;
                        if (opt_kind == 1) {
                            emit("%s %.*s = __match_val_%d.value; ", cty,
                                 binder->ident.name.length, binder->ident.name.start, match_id);
                        } else {
                            // Result: ok arm uses ok_value, err arm uses err_value.
                            const char* field = is_some_arm ? "ok_value" : "err_value";
                            // Err payload is always a string in the builtin Result.
                            const char* bcty = is_some_arm ? opt_val_cty : "const char*";
                            emit("%s %.*s = __match_val_%d.data.%s; ", bcty,
                                 binder->ident.name.length, binder->ident.name.start, match_id, field);
                        }
                        // Register a string binding so the arm body concats correctly.
                        int binder_is_str = (opt_kind == 1) ? opt_val_is_str : (!is_some_arm ? 1 : opt_val_is_str);
                        if (binder_is_str) {
                            char _bb[256]; token_to_cstr(_bb, sizeof(_bb), binder->ident.name);
                            extern void register_string_var(const char*);
                            register_string_var(_bb);
                        }
                    }
                    // Emit arm body + result assignment, then close - mirrors the
                    // generic path's tail. We do it inline and `continue`.
                    {
                        extern void register_string_var(const char*);
                        (void)register_string_var;
                    }
                    emit("__match_result_%d = ", match_id);
                    codegen_expr(expr->match.arms[i].result);
                    emit("; } ");
                    continue;
                }

                // Check pattern type
                if (pat->type == PATTERN_WILDCARD) {
                    if (guard_expr) {
                        emit("if (");
                        codegen_expr(guard_expr);
                        emit(") { ");
                    } else {
                        emit("{ ");
                    }
                } else if (pat->type == PATTERN_LITERAL) {
                    // Check if matching against a string
                    bool is_string_match = (match_type && match_type->kind == TYPE_STRING) ||
                                          (pat->literal.value.start[0] == '"');
                    if (is_string_match) {
                        emit("if (strcmp(__match_val_%d, %.*s) == 0) { ",
                             match_id,
                             pat->literal.value.length,
                             pat->literal.value.start);
                    } else {
                        emit("if (__match_val_%d == %.*s) { ",
                             match_id,
                             pat->literal.value.length,
                             pat->literal.value.start);
                    }
                } else if (pat->type == PATTERN_IDENT) {
                    // Check if this looks like an enum variant
                    bool is_enum_variant = false;
                    // Check for underscore (prefixed variant like Color_Red)
                    for (int j = 0; j < pat->ident.name.length; j++) {
                        if (pat->ident.name.start[j] == '_') {
                            is_enum_variant = true;
                            break;
                        }
                    }
                    // Also check if this is a bare variant name matching the enum type
                    if (!is_enum_variant && type_name_len > 0) {
                        extern const char* find_enum_for_variant(const char* variant);
                        char _vn[128];
                        token_to_cstr(_vn, sizeof(_vn), pat->ident.name);
                        const char* found = find_enum_for_variant(_vn);
                        if (found) is_enum_variant = true;
                    }
                    
                    if (is_enum_variant) {
                        if (is_data_enum) {
                            // Check if it already has the enum prefix
                            bool has_prefix = false;
                            for (int j = 0; j < pat->ident.name.length; j++) {
                                if (pat->ident.name.start[j] == '_') { has_prefix = true; break; }
                            }
                            if (has_prefix) {
                                emit("if (__match_val_%d.tag == %.*s_TAG) { ",
                                     match_id, pat->ident.name.length, pat->ident.name.start);
                            } else {
                                emit("if (__match_val_%d.tag == %.*s_%.*s_TAG) { ",
                                     match_id, type_name_len, type_name,
                                     pat->ident.name.length, pat->ident.name.start);
                            }
                        } else {
                            // Check if it already has the enum prefix
                            bool has_prefix = false;
                            for (int j = 0; j < pat->ident.name.length; j++) {
                                if (pat->ident.name.start[j] == '_') { has_prefix = true; break; }
                            }
                            if (has_prefix) {
                                emit("if (__match_val_%d == %.*s) { ",
                                     match_id, pat->ident.name.length, pat->ident.name.start);
                            } else {
                                emit("if (__match_val_%d == %.*s_%.*s) { ",
                                     match_id, type_name_len, type_name,
                                     pat->ident.name.length, pat->ident.name.start);
                            }
                        }
                    } else {
                        // Variable binding. Binds the whole matched value, so it
                        // always matches - unless there's a guard, in which case
                        // the binding must be visible to the guard. Emit the guard
                        // as a statement-expression that binds the name first, so
                        // the arm stays a proper `if`/`else if` link in the chain.
                        if (guard_expr) {
                            emit("if (({ %.*s %.*s = __match_val_%d; (",
                                 type_name_len, type_name,
                                 pat->ident.name.length, pat->ident.name.start,
                                 match_id);
                            codegen_expr(guard_expr);
                            emit("); })) { %.*s %.*s = __match_val_%d; ",
                                 type_name_len, type_name,
                                 pat->ident.name.length, pat->ident.name.start,
                                 match_id);
                            guard_expr = NULL; // consumed
                        } else {
                            emit("{ %.*s %.*s = __match_val_%d; ",
                                 type_name_len, type_name,
                                 pat->ident.name.length,
                                 pat->ident.name.start,
                                 match_id);
                        }
                    }
                } else if (pat->type == PATTERN_OPTION && !pat->option.is_some) {
                    // Simple enum variant: Color.Red or Shape.Point
                    if (is_data_enum) {
                        emit("if (__match_val_%d.tag == %.*s_%.*s_TAG) { ",
                             match_id,
                             pat->option.enum_name.length, pat->option.enum_name.start,
                             pat->option.variant_name.length, pat->option.variant_name.start);
                    } else {
                        emit("if (__match_val_%d == %.*s_%.*s) { ",
                             match_id,
                             pat->option.enum_name.length, pat->option.enum_name.start,
                             pat->option.variant_name.length, pat->option.variant_name.start);
                    }
                } else if (pat->type == PATTERN_OPTION && pat->option.is_some) {
                    // Enum variant with data: Shape.Circle(r), Some(x), Ok(x)
                    if (!is_data_enum) {
                        // Simple enum - treat like a plain variant match, ignore inner binding
                        if (pat->option.enum_name.length > 0) {
                            emit("if (__match_val_%d == %.*s_%.*s) { ",
                                 match_id,
                                 pat->option.enum_name.length, pat->option.enum_name.start,
                                 pat->option.variant_name.length, pat->option.variant_name.start);
                        } else {
                            emit("if (__match_val_%d == %.*s) { ",
                                 match_id,
                                 pat->option.variant_name.length, pat->option.variant_name.start);
                        }
                        // Declare inner variable as 0 (data not available in simple enums)
                        if (pat->option.inner && pat->option.inner->type == PATTERN_IDENT) {
                            emit("int %.*s = 0; ",
                                 pat->option.inner->ident.name.length, pat->option.inner->ident.name.start);
                        }
                    } else if (is_data_enum && pat->option.enum_name.length > 0) {
                        // Data enum with explicit prefix: Shape.Circle(r)
                        emit("if (__match_val_%d.tag == %.*s_%.*s_TAG) { ",
                             match_id,
                             pat->option.enum_name.length, pat->option.enum_name.start,
                             pat->option.variant_name.length, pat->option.variant_name.start);
                        // Bind inner variables
                        if (pat->option.inner_count > 1) {
                            extern const char* get_enum_variant_field_type(const char*, const char*, int);
                            char _mfen[128], _mfvn[128];
                            token_to_cstr(_mfen, sizeof(_mfen), pat->option.enum_name);
                            token_to_cstr(_mfvn, sizeof(_mfvn), pat->option.variant_name);
                            for (int pi = 0; pi < pat->option.inner_count; pi++) {
                                if (pat->option.inners[pi] && pat->option.inners[pi]->type == PATTERN_IDENT) {
                                    const char* _fty = get_enum_variant_field_type(_mfen, _mfvn, pi);
                                    emit("%s %.*s = __match_val_%d.data.%.*s_value.f%d; ",
                                         _fty,
                                         pat->option.inners[pi]->ident.name.length, pat->option.inners[pi]->ident.name.start,
                                         match_id,
                                         pat->option.variant_name.length, pat->option.variant_name.start, pi);
                                    // Register a string field binding so the arm body
                                    // concatenates it instead of int_to_string(it).
                                    if (strcmp(_fty, "const char*") == 0 || strcmp(_fty, "char*") == 0) {
                                        char _fb[256]; token_to_cstr(_fb, sizeof(_fb), pat->option.inners[pi]->ident.name);
                                        extern void register_string_var(const char*);
                                        register_string_var(_fb);
                                    }
                                }
                            }
                        } else if (pat->option.inner && pat->option.inner->type == PATTERN_IDENT) {
                            extern const char* get_enum_variant_c_type(const char*, const char*);
                            char _en[128], _vn[128];
                            token_to_cstr(_en, sizeof(_en), pat->option.enum_name);
                            token_to_cstr(_vn, sizeof(_vn), pat->option.variant_name);
                            const char* vtype = get_enum_variant_c_type(_en, _vn);
                            emit("%s %.*s = __match_val_%d.data.%.*s_value; ",
                                 vtype,
                                 pat->option.inner->ident.name.length, pat->option.inner->ident.name.start,
                                 match_id,
                                 pat->option.variant_name.length, pat->option.variant_name.start);
                        }
                    } else if (is_data_enum && pat->option.enum_name.length == 0) {
                        // Data enum without prefix: Circle(r) - use type_name as prefix
                        emit("if (__match_val_%d.tag == %.*s_%.*s_TAG) { ",
                             match_id,
                             type_name_len, type_name,
                             pat->option.variant_name.length, pat->option.variant_name.start);
                        if (pat->option.inner_count > 1) {
                            extern const char* get_enum_variant_field_type(const char*, const char*, int);
                            char _mfen[128], _mfvn[128];
                            snprintf(_mfen, sizeof(_mfen), "%.*s", type_name_len, type_name);
                            token_to_cstr(_mfvn, sizeof(_mfvn), pat->option.variant_name);
                            for (int pi = 0; pi < pat->option.inner_count; pi++) {
                                if (pat->option.inners[pi] && pat->option.inners[pi]->type == PATTERN_IDENT) {
                                    const char* _fty = get_enum_variant_field_type(_mfen, _mfvn, pi);
                                    emit("%s %.*s = __match_val_%d.data.%.*s_value.f%d; ",
                                         _fty,
                                         pat->option.inners[pi]->ident.name.length, pat->option.inners[pi]->ident.name.start,
                                         match_id,
                                         pat->option.variant_name.length, pat->option.variant_name.start, pi);
                                    // Register a string field binding so the arm body
                                    // concatenates it instead of int_to_string(it).
                                    if (strcmp(_fty, "const char*") == 0 || strcmp(_fty, "char*") == 0) {
                                        char _fb[256]; token_to_cstr(_fb, sizeof(_fb), pat->option.inners[pi]->ident.name);
                                        extern void register_string_var(const char*);
                                        register_string_var(_fb);
                                    }
                                }
                            }
                        } else if (pat->option.inner && pat->option.inner->type == PATTERN_IDENT) {
                            extern const char* get_enum_variant_c_type(const char*, const char*);
                            char _en[128], _vn[128];
                            snprintf(_en, 128, "%.*s", type_name_len, type_name);
                            token_to_cstr(_vn, sizeof(_vn), pat->option.variant_name);
                            const char* vtype = get_enum_variant_c_type(_en, _vn);
                            if (!vtype) vtype = "double";
                            emit("%s %.*s = __match_val_%d.data.%.*s_value; ",
                                 vtype,
                                 pat->option.inner->ident.name.length, pat->option.inner->ident.name.start,
                                 match_id,
                                 pat->option.variant_name.length, pat->option.variant_name.start);
                        }
                    } else {
                    // Legacy: Some(x), Ok(x) etc.
                    emit("if (__match_val_%d.tag == %.*s_TAG) { ",
                         match_id,
                         pat->option.variant_name.length,
                         pat->option.variant_name.start);
                    
                    // Bind inner variable if present
                    if (pat->option.inner && pat->option.inner->type == PATTERN_IDENT) {
                        // Extract variant name (e.g., "Some" from "Option_Some")
                        const char* variant_start = pat->option.variant_name.start;
                        int variant_len = pat->option.variant_name.length;
                        
                        // Find the last underscore to get the variant name
                        const char* underscore = NULL;
                        for (int j = 0; j < variant_len; j++) {
                            if (variant_start[j] == '_') {
                                underscore = variant_start + j;
                            }
                        }
                        
                        // Determine value type (heuristic: Err variants use const char*)
                        const char* value_type = "int";
                        int value_type_len = 3;
                        if (underscore) {
                            int short_variant_len = variant_len - (underscore - variant_start + 1);
                            const char* short_variant = underscore + 1;
                            // Check if variant name contains "Err"
                            if (short_variant_len >= 3 && 
                                memcmp(short_variant, "Err", 3) == 0) {
                                value_type = "const char*";
                                value_type_len = 11;
                            }
                        }
                        
                        if (underscore) {
                            // Use the part after the last underscore
                            int short_variant_len = variant_len - (underscore - variant_start + 1);
                            emit("%.*s %.*s = __match_val_%d.data.%.*s_value; ",
                                 value_type_len, value_type,
                                 pat->option.inner->ident.name.length,
                                 pat->option.inner->ident.name.start,
                                 match_id,
                                 short_variant_len,
                                 underscore + 1);
                        } else {
                            // No underscore, use full name
                            emit("%.*s %.*s = __match_val_%d.data.%.*s_value; ",
                                 value_type_len, value_type,
                                 pat->option.inner->ident.name.length,
                                 pat->option.inner->ident.name.start,
                                 match_id,
                                 variant_len,
                                 variant_start);
                        }
                    }
                    } // close legacy else
                } else if (pat->type == PATTERN_RANGE) {
                    // Range pattern: 0..10 (exclusive) or 0..=10 (inclusive).
                    emit("if (__match_val_%d >= ", match_id);
                    codegen_expr(pat->range.start);
                    emit(" && __match_val_%d %s ", match_id, pat->range.inclusive ? "<=" : "<");
                    codegen_expr(pat->range.end);
                    emit(") { ");
                } else if (pat->type == PATTERN_OR) {
                    // Or pattern: 1 | 2 | 3. Matches if the value equals any
                    // sub-pattern's literal.
                    bool is_string_match = (match_type && match_type->kind == TYPE_STRING);
                    emit("if (");
                    for (int oi = 0; oi < pat->or_pat.pattern_count; oi++) {
                        Pattern* sub = pat->or_pat.patterns[oi];
                        if (oi > 0) emit(" || ");
                        if (sub->type == PATTERN_LITERAL) {
                            if (is_string_match || sub->literal.value.start[0] == '"') {
                                emit("strcmp(__match_val_%d, %.*s) == 0",
                                     match_id, sub->literal.value.length, sub->literal.value.start);
                            } else {
                                emit("__match_val_%d == %.*s",
                                     match_id, sub->literal.value.length, sub->literal.value.start);
                            }
                        } else if (sub->type == PATTERN_RANGE) {
                            emit("(__match_val_%d >= ", match_id);
                            codegen_expr(sub->range.start);
                            emit(" && __match_val_%d %s ", match_id, sub->range.inclusive ? "<=" : "<");
                            codegen_expr(sub->range.end);
                            emit(")");
                        } else {
                            // Unknown sub-pattern: be conservative, never match it.
                            emit("0");
                        }
                    }
                    emit(") { ");
                } else if (pat->type == PATTERN_STRUCT) {
                    // Struct destructuring: Point { x, y } => ...
                    // The matched value is already this struct type, so the
                    // pattern always matches structurally; bind each named field
                    // to the corresponding member of __match_val. An optional
                    // guard gates the arm.
                    // A struct pattern always matches structurally (the value is
                    // already this struct type). When there's a guard, gate the
                    // arm on it via a statement-expression that binds the fields
                    // first so the guard can reference them; otherwise use
                    // `if (1)` so a following `else` arm has an `if` to attach to.
                    // The per-field binding string is emitted twice (once for the
                    // guard test, once inside the taken branch), so build it once.
                    char bind_buf[1024]; bind_buf[0] = '\0'; int bind_off = 0;
                    for (int fi = 0; fi < pat->struct_pat.field_count; fi++) {
                        Token fname = pat->struct_pat.field_names[fi];
                        const char* fctype = "long long";
                        if (match_type && match_type->kind == TYPE_STRUCT) {
                            for (int sfi = 0; sfi < match_type->struct_type.field_count; sfi++) {
                                Token sf = match_type->struct_type.field_names[sfi];
                                if (sf.length == fname.length &&
                                    memcmp(sf.start, fname.start, fname.length) == 0) {
                                    const char* m = codegen_c_type_from_type(
                                        match_type->struct_type.field_types[sfi]);
                                    if (m) fctype = m;
                                    break;
                                }
                            }
                        }
                        bind_off += snprintf(bind_buf + bind_off, sizeof(bind_buf) - bind_off,
                             "%s %.*s = __match_val_%d.%.*s; ",
                             fctype, fname.length, fname.start,
                             match_id, fname.length, fname.start);
                        if (bind_off >= (int)sizeof(bind_buf)) { bind_off = sizeof(bind_buf) - 1; break; }
                    }
                    if (guard_expr) {
                        emit("if (({ %s(", bind_buf);
                        codegen_expr(guard_expr);
                        emit("); })) { %s", bind_buf);
                        guard_expr = NULL; // consumed
                    } else {
                        emit("if (1) { %s", bind_buf);
                    }
                } else {
                    // Unsupported pattern - treat as wildcard
                    emit("{ ");
                }

                // Generate result
                emit("__match_result_%d = ", match_id);
                codegen_expr(expr->match.arms[i].result);
                emit("; } ");
            }
            
            emit("__match_result_%d; })", match_id);
            break;
        };
        case EXPR_BLOCK: {
            // Generate block expression as compound statement
            emit("({ ");
            for (int i = 0; i < expr->block.stmt_count; i++) {
                codegen_stmt(expr->block.stmts[i]);
            }
            if (expr->block.result) {
                codegen_expr(expr->block.result);
            }
            emit("; })");
            break;
        }
        case EXPR_SOME: {
            // Resolve the concrete Option family. Precedence: an explicit
            // assignment-target annotation (`var o: string? = Some(..)`) or an
            // enclosing function-return kind wins (they name the exact declared
            // family); otherwise infer from the payload's own type so bare
            // `Some(x)` lowers correctly for any payload, not just int.
            const char* _fam = wyn_option_ctor_kind(expr, "Option");
            emit("%s_Some(", _fam);
            // Peel the target family for the payload: inside Some(Some(5)) the
            // inner ctor's target is the OUTER family's payload (OptionOptionInt
            // → OptionInt), not the outer family itself - without the peel the
            // inner Some also emitted OptionOptionInt_Some (a C type error).
            extern const char* current_assign_target_kind;
            const char* _prev_tk = current_assign_target_kind;
            current_assign_target_kind =
                (strncmp(_fam, "OptionOption", 12) == 0) ? _fam + 6 : NULL;
            if (expr->option.value) codegen_expr(expr->option.value);
            current_assign_target_kind = _prev_tk;
            emit(")");
            break;
        }
        case EXPR_NONE: {
            // No payload to infer from - rely on the target/return annotation,
            // defaulting to OptionInt.
            emit("%s_None()", wyn_option_ctor_kind(expr, "Option"));
            break;
        }
        case EXPR_OK: {
            emit("%s_Ok(", wyn_option_ctor_kind(expr, "Result"));
            // Same peel rationale as EXPR_SOME: the payload must not inherit
            // the outer Result family as its own target.
            extern const char* current_assign_target_kind;
            const char* _prev_tk_ok = current_assign_target_kind;
            current_assign_target_kind = NULL;
            if (expr->option.value) codegen_expr(expr->option.value);
            current_assign_target_kind = _prev_tk_ok;
            emit(")");
            break;
        }
        case EXPR_ERR: {
            emit("%s_Err(", wyn_option_ctor_kind(expr, "Result"));
            extern const char* current_assign_target_kind;
            const char* _prev_tk_err = current_assign_target_kind;
            current_assign_target_kind = NULL;
            if (expr->option.value) codegen_expr(expr->option.value);
            current_assign_target_kind = _prev_tk_err;
            emit(")");
            break;
        }
        case EXPR_TRY: {
            // ? operator: unwrap Result or propagate error
            // In Result-returning function: return the error
            // In non-Result function: print error and exit
            static int _try_id = 0; _try_id++;
            emit("({ ResultInt __try_%d = ", _try_id);
            codegen_expr(expr->try_expr.value);
            extern const char* current_fn_return_kind;
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "Result", 6) == 0) {
                emit("; if (__try_%d.tag == 1) return __try_%d; __try_%d.data.ok_value; })", _try_id, _try_id, _try_id);
            } else {
                emit("; if (__try_%d.tag == 1) { fprintf(stderr, \"Error: %%s\\n\", __try_%d.data.err_value); exit(1); } __try_%d.data.ok_value; })", _try_id, _try_id, _try_id);
            }
            break;
        }
        case EXPR_TERNARY:
            emit("(");
            codegen_expr(expr->ternary.condition);
            emit(" ? ");
            codegen_expr(expr->ternary.then_expr);
            emit(" : ");
            codegen_expr(expr->ternary.else_expr);
            emit(")");
            break;
        case EXPR_IF_EXPR:
            emit("(");
            codegen_expr(expr->if_expr.condition);
            emit(" ? ");
            if (expr->if_expr.then_expr) {
                codegen_expr(expr->if_expr.then_expr);
            } else {
                emit("0");
            }
            emit(" : ");
            if (expr->if_expr.else_expr) {
                codegen_expr(expr->if_expr.else_expr);
            } else {
                emit("0");
            }
            emit(")");
            break;
        case EXPR_STRING_INTERP: {
            // String interpolation: "Hello ${name}" -> sprintf format
            emit("({ ");
            // Capture temporaries
            int _ti = 0;
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.expressions[i]) {
                    Expr* e = expr->string_interp.expressions[i];
                    emit("const char* __si%d = ", _ti);
                    if (e->type == EXPR_STRING) {
                        codegen_expr(e);
                    } else {
                        emit("to_string(");
                        codegen_expr(e);
                        emit(")");
                    }
                    emit("; ");
                    _ti++;
                }
            }

            // Size-probe then heap-allocate so interpolation is NOT truncated at
            // a fixed buffer (was char __buf[512]). __buf is RC-allocated via
            // wyn_str_alloc so the existing release / skip-strdup ownership holds.
            // The format string + args are emitted twice: once for the probe
            // (snprintf into NULL,0) and once for the real write.
            for (int pass = 0; pass < 2; pass++) {
                if (pass == 0) emit("int __n = snprintf(NULL, 0, \"");
                else           emit("char* __buf = wyn_str_alloc(__n + 1); snprintf(__buf, __n + 1, \"");
                for (int i = 0; i < expr->string_interp.count; i++) {
                    if (expr->string_interp.parts[i]) {
                        const char* part = expr->string_interp.parts[i];
                        while (*part) {
                            if (*part == '%') emit("%%%%");
                            else if (*part == '\n') emit("\\n");
                            else if (*part == '\\' && *(part+1) == '"') { emit("\\\""); part++; }
                            else if (*part == '"') emit("\\\"");
                            else emit("%c", *part);
                            part++;
                        }
                    } else {
                        emit("%%s");
                    }
                }
                emit("\"");
                _ti = 0;
                for (int i = 0; i < expr->string_interp.count; i++) {
                    if (expr->string_interp.expressions[i]) {
                        emit(", __si%d", _ti++);
                    }
                }
                emit("); ");
            }
            // Release temporaries (only non-string expressions that created new strings)
            _ti = 0;
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.expressions[i]) {
                    Expr* e = expr->string_interp.expressions[i];
                    // Only release if the expression is NOT already a string
                    // (to_string on a string returns the same pointer - releasing it frees the original)
                    bool is_str_expr = (e->type == EXPR_STRING) ||
                                       (e->expr_type && e->expr_type->kind == TYPE_STRING) ||
                                       (e->type == EXPR_IDENT && e->expr_type && e->expr_type->kind == TYPE_STRING);
                    if (!is_str_expr) {
                        emit("wyn_rc_release(__si%d); ", _ti);
                    }
                    _ti++;
                }
            }
            // __buf is already an owned heap RC string (wyn_str_alloc), so both
            // the skip and non-skip cases just yield it - no extra wyn_strdup copy
            // (which would leak __buf and double-allocate).
            emit("__buf; })");
            break;
        }
        case EXPR_RANGE:
            // Generate range struct: {start, end}
            emit("({ struct { int start; int end; } __range = {");
            codegen_expr(expr->range.start);
            emit(", ");
            codegen_expr(expr->range.end);
            emit("}; __range; })");
            break;
        case EXPR_LIST_COMP: {
            // [expr for x in start..end] or [expr for x in array] or [expr for x in range if cond]
            static int lc_id = 0;
            int id = lc_id++;
            if (expr->list_comp.iter_end) {
                // Range-based: [expr for x in start..end]
                emit("({ WynArray __lc_%d = array_new(); ", id);
                emit("for (long long %.*s = ", expr->list_comp.var_name.length, expr->list_comp.var_name.start);
                codegen_expr(expr->list_comp.iter_start);
                emit("; %.*s < ", expr->list_comp.var_name.length, expr->list_comp.var_name.start);
                codegen_expr(expr->list_comp.iter_end);
                emit("; %.*s++) { ", expr->list_comp.var_name.length, expr->list_comp.var_name.start);
                if (expr->list_comp.condition) {
                    emit("if (");
                    codegen_expr(expr->list_comp.condition);
                    emit(") ");
                }
                emit("array_push(&__lc_%d, (long long)(", id);
                codegen_expr(expr->list_comp.body);
                emit(")); } __lc_%d; })", id);
            } else {
                // Array-based: [expr for x in array]
                emit("({ WynArray __lc_%d = array_new(); WynArray __lc_src_%d = ", id, id);
                codegen_expr(expr->list_comp.iter_start);
                emit("; for (int __lc_i_%d = 0; __lc_i_%d < __lc_src_%d.count; __lc_i_%d++) { ", id, id, id, id);
                emit("long long %.*s = __lc_src_%d.data[__lc_i_%d].data.int_val; ",
                     expr->list_comp.var_name.length, expr->list_comp.var_name.start, id, id);
                if (expr->list_comp.condition) {
                    emit("if (");
                    codegen_expr(expr->list_comp.condition);
                    emit(") ");
                }
                emit("array_push(&__lc_%d, (long long)(", id);
                codegen_expr(expr->list_comp.body);
                emit(")); } __lc_%d; })", id);
            }
            break;
        }
        case EXPR_CHANNEL: {
            // channel() / channel(cap) -> Task_channel(cap). Default cap = 0
            // (unbuffered/rendezvous-ish; runtime treats <=0 sensibly).
            emit("Task_channel(");
            if (expr->channel.capacity) codegen_expr(expr->channel.capacity);
            else emit("0");
            emit(")");
            break;
        }
        case EXPR_SPAWN: {
            // Spawn expression: spawn fn(args) returns a future
            if (expr->spawn.call && expr->spawn.call->type == EXPR_CALL &&
                expr->spawn.call->call.callee->type == EXPR_IDENT) {
                Expr* call = expr->spawn.call;
                Expr* callee = call->call.callee;
                char func_name[256]; token_to_cstr(func_name, sizeof(func_name), callee->token);
                
                int arg_count = call->call.arg_count;
                
                // Check if this function can be inlined (no yield points)
                int _can_inline = 0;
                for (int _si = 0; _si < spawn_wrapper_count; _si++) {
                    if (strcmp(spawn_wrappers[_si].func_name, func_name) == 0) {
                        _can_inline = spawn_wrappers[_si].can_inline;
                        break;
                    }
                }
                
                // A single argument that is NOT a machine word (float/struct/
                // array) cannot ride the (void*)(intptr_t) cast: floats were
                // silently TRUNCATED (spawn half(7.0) -> 3), structs were a C
                // type error. Route those through the args-struct path.
                bool _arg1_needs_box = false;
                if (arg_count == 1 && current_program) {
                    for (int _fi = 0; _fi < current_program->count; _fi++) {
                        Stmt* _fs = current_program->stmts[_fi];
                        if (_fs->type == STMT_FN &&
                            (size_t)_fs->fn.name.length == strlen(func_name) &&
                            memcmp(_fs->fn.name.start, func_name, _fs->fn.name.length) == 0) {
                            if (_fs->fn.param_count > 0 && _fs->fn.param_types[0]) {
                                Expr* pt0 = _fs->fn.param_types[0];
                                if (pt0->type == EXPR_ARRAY) _arg1_needs_box = true;
                                else if (pt0->type == EXPR_IDENT) {
                                    Token ptk = pt0->token;
                                    bool is_word = (ptk.length == 3 && memcmp(ptk.start, "int", 3) == 0) ||
                                                   (ptk.length == 4 && memcmp(ptk.start, "bool", 4) == 0) ||
                                                   (ptk.length == 6 && memcmp(ptk.start, "string", 6) == 0);
                                    if (!is_word) _arg1_needs_box = true;
                                }
                            }
                            break;
                        }
                    }
                }

                if (arg_count == 0) {
                    if (_can_inline)
                        emit("wyn_spawn_inline((TaskFuncWithReturn)__spawn_wrapper_%s, NULL)", func_name);
                    else
                        emit("wyn_spawn_async_traced((TaskFuncWithReturn)__spawn_wrapper_%s, NULL, __FILE__, __LINE__)", func_name);
                } else if (arg_count == 1 && !_arg1_needs_box) {
                    if (_can_inline) {
                        emit("wyn_spawn_inline((TaskFuncWithReturn)__spawn_wrapper_%s_1, (void*)(intptr_t)(", func_name);
                    } else {
                        emit("wyn_spawn_async_traced((TaskFuncWithReturn)__spawn_wrapper_%s_1, (void*)(intptr_t)(", func_name);
                    }
                    codegen_expr(call->call.args[0]);
                    if (_can_inline)
                        emit("))");
                    else
                        emit("), __FILE__, __LINE__)");
                } else if (arg_count == 1 && _arg1_needs_box) {
                    // Boxed single arg: same shape as the multi-arg path - the
                    // wrapper (which knows the real param type) unpacks it.
                    spawn_id_counter++;
                    int sid = spawn_id_counter;
                    const char* _box_ct = "long long";
                    {   // recover the param's C type for the box field
                        for (int _fi = 0; _fi < current_program->count; _fi++) {
                            Stmt* _fs = current_program->stmts[_fi];
                            if (_fs->type == STMT_FN &&
                                (size_t)_fs->fn.name.length == strlen(func_name) &&
                                memcmp(_fs->fn.name.start, func_name, _fs->fn.name.length) == 0) {
                                Expr* pt0 = _fs->fn.param_types[0];
                                if (pt0->type == EXPR_ARRAY) _box_ct = "WynArray";
                                else if (pt0->type == EXPR_IDENT) {
                                    Token ptk = pt0->token;
                                    if (ptk.length == 5 && memcmp(ptk.start, "float", 5) == 0) _box_ct = "double";
                                    else { static char _bcb[96]; snprintf(_bcb, sizeof(_bcb), "%.*s", ptk.length, ptk.start); _box_ct = _bcb; }
                                }
                                break;
                            }
                        }
                    }
                    emit("({ struct __spawn_args_%d { %s a0; } *__sa_%d = malloc(sizeof(struct __spawn_args_%d)); ",
                         sid, _box_ct, sid, sid);
                    emit("__sa_%d->a0 = ", sid);
                    codegen_expr(call->call.args[0]);
                    emit("; ");
                    if (_can_inline)
                        emit("wyn_spawn_inline((TaskFuncWithReturn)__spawn_wrapper_%s_1b, __sa_%d); })", func_name, sid);
                    else
                        emit("wyn_spawn_async_traced((TaskFuncWithReturn)__spawn_wrapper_%s_1b, __sa_%d, __FILE__, __LINE__); })", func_name, sid);
                } else {
                    // Create args struct and pass to wrapper
                    spawn_id_counter++;
                    int sid = spawn_id_counter;
                    emit("({ struct __spawn_args_%d { ", sid);
                    for (int i = 0; i < arg_count; i++) {
                        emit("long long a%d; ", i);
                    }
                    emit("} *__sa_%d = malloc(sizeof(struct __spawn_args_%d)); ", sid, sid);
                    for (int i = 0; i < arg_count; i++) {
                        emit("__sa_%d->a%d = ", sid, i);
                        codegen_expr(call->call.args[i]);
                        emit("; ");
                    }
                    if (_can_inline)
                        emit("wyn_spawn_inline((TaskFuncWithReturn)__spawn_wrapper_%s_%d, __sa_%d); })", func_name, arg_count, sid);
                    else
                        emit("wyn_spawn_async_traced((TaskFuncWithReturn)__spawn_wrapper_%s_%d, __sa_%d, __FILE__, __LINE__); })", func_name, arg_count, sid);
                }
            } else {
                emit("NULL /* spawn fallback */");
            }
            break;
        }
        case EXPR_LAMBDA: {
            lambda_ref_counter++;
            int lid = lambda_ref_counter;
            // Check if this lambda is a closure (returned from function)
            bool is_closure_lambda = false;
            for (int i = 0; i < lambda_count; i++) {
                if (lambda_functions[i].id == lid && lambda_functions[i].is_closure) {
                    is_closure_lambda = true;
                    // Emit closure construction: allocate env, fill captured vars, return WynClosure.
                    // The env is RC-allocated (not plain malloc) so its lifetime can be
                    // managed by wyn_rc_retain/release on WynClosure.env - see the scope-exit
                    // release for non-escaping closure vars in codegen.c pop_scope. wyn_rc_release
                    // is a no-op on non-RC/NULL pointers, so this stays safe even where a closure
                    // env isn't (yet) tracked.
                    emit("({ __closure_env_%d* __env = wyn_rc_alloc(sizeof(__closure_env_%d)); ", lid, lid);
                    for (int j = 0; j < lambda_functions[i].capture_count; j++) {
                        emit("__env->%s = %s; ", 
                            lambda_functions[i].captured_vars[j],
                            lambda_functions[i].captured_vars[j]);
                    }
                    emit("wyn_closure_new((void*)__lambda_%d, __env); })", lid);
                    break;
                }
            }
            if (!is_closure_lambda) {
                // Check if this lambda has captures (non-return style with static globals)
                bool has_captures = false;
                for (int i = 0; i < lambda_count; i++) {
                    if (lambda_functions[i].id == lid && lambda_functions[i].capture_count > 0 && !lambda_functions[i].is_closure) {
                        has_captures = true;
                        // Set static globals before using the function pointer
                        emit("({ ");
                        for (int j = 0; j < lambda_functions[i].capture_count; j++) {
                            emit("__cap_%d_%s = %s; ", lid, 
                                lambda_functions[i].captured_vars[j],
                                lambda_functions[i].captured_vars[j]);
                        }
                        emit("__lambda_%d; })", lid);
                        break;
                    }
                }
                if (!has_captures) {
                    emit("__lambda_%d", lid);
                }
            }
            break;
        }
        case EXPR_MAP: {
            // Generate map using the typedef
            emit("({ ");
            emit("WynMap __map = {0}; ");
            emit("__map.count = %d; ", expr->map.count);
            if (expr->map.count > 0) {
                emit("__map.keys = malloc(sizeof(void*) * %d); ", expr->map.count);
                emit("__map.values = malloc(sizeof(void*) * %d); ", expr->map.count);
                for (int i = 0; i < expr->map.count; i++) {
                    emit("__map.keys[%d] = (void*)strdup(", i);
                    codegen_expr(expr->map.keys[i]);
                    emit("); ");
                    emit("__map.values[%d] = (void*)(intptr_t)", i);
                    codegen_expr(expr->map.values[i]);
                    emit("; ");
                }
            }
            emit("__map; })");
            break;
        }
        case EXPR_TUPLE: {
            // Generate tuple as a struct literal
            // If inside a function returning a tuple, use the typedef
            extern const char* current_fn_return_kind;
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "_wyn_tup_", 9) == 0) {
                emit("(%s){ ", current_fn_return_kind);
            } else {
                emit("(struct { ");
                for (int i = 0; i < expr->tuple.count; i++) {
                    const char* et = "long long";
                    if (expr->tuple.elements[i]->type == EXPR_STRING) et = "const char*";
                    else if (expr->tuple.elements[i]->type == EXPR_FLOAT) et = "double";
                    else if (expr->tuple.elements[i]->type == EXPR_BOOL) et = "bool";
                    emit("%s item%d; ", et, i);
                }
                emit("}){ ");
            }
            for (int i = 0; i < expr->tuple.count; i++) {
                if (i > 0) emit(", ");
                codegen_expr(expr->tuple.elements[i]);
            }
            emit(" }");
            break;
        }
        case EXPR_TUPLE_INDEX: {
            // Access tuple element: tuple.0 -> tuple.item0
            emit("(");
            codegen_expr(expr->tuple_index.tuple);
            emit(").item%d", expr->tuple_index.index);
            break;
        }
        case EXPR_INDEX_ASSIGN: {
            // Check if this is map assignment
            bool is_map_assign = false;
            if (expr->index_assign.object->expr_type && expr->index_assign.object->expr_type->kind == TYPE_MAP) {
                is_map_assign = true;
            } else if (expr->index_assign.index->type == EXPR_STRING || 
                      (expr->index_assign.index->expr_type && expr->index_assign.index->expr_type->kind == TYPE_STRING)) {
                is_map_assign = true;
            }
            
            if (is_map_assign) {
                // Map assignment: map["key"] = value -> hashmap_insert_*(map, "key", value)
                // Determine insert function based on value type (shared helper).
                const char* insert_func = hashmap_insert_fn_for(expr->index_assign.value);
                emit("%s(", insert_func);
                codegen_expr(expr->index_assign.object);
                emit(", ");
                codegen_expr(expr->index_assign.index);
                emit(", ");
                codegen_expr(expr->index_assign.value);
                emit(")");
            } else if (expr->index_assign.object->type == EXPR_INDEX) {
                // Nested element assign: m[0][1] = v. The object m[0] is an
                // RVALUE (array_get_* call) - taking its address was a C error.
                // Reach the inner array THROUGH the WynValue pointer instead.
                emit("{ WynArray* __arr_ptr = (");
                codegen_expr(expr->index_assign.object->index.array);
                emit(").data[");
                codegen_expr(expr->index_assign.object->index.index);
                emit("].data.array_val; int __idx = ");
                codegen_expr(expr->index_assign.index);
                emit("; if (__arr_ptr && __idx >= 0 && __idx < __arr_ptr->count) { ");
                emit("__arr_ptr->data[__idx].type = WYN_TYPE_INT; __arr_ptr->data[__idx].data.int_val = ");
                codegen_expr(expr->index_assign.value);
                emit("; } }");
                break;
            } else {
                // ARC-managed array assignment
                emit("{ WynArray* __arr_ptr = &(");
                codegen_expr(expr->index_assign.object);
                emit("); int __idx = ");
                codegen_expr(expr->index_assign.index);
                emit("; if (__idx >= 0 && __idx < __arr_ptr->count) { ");
                
                // Release old value if it exists
                emit("if (__arr_ptr->data[__idx].type == WYN_TYPE_STRING && __arr_ptr->data[__idx].data.string_val) { ");
                emit("/* ARC release old string */ } ");
                
                // Set new value with proper type
                {
                    int is_string = 0;
                    int is_struct = 0;
                    int is_float = 0;
                    Token struct_name = {0};
                    
                    // Check value expression type
                    if (expr->index_assign.value->type == EXPR_STRING) {
                        is_string = 1;
                    } else if (expr->index_assign.value->type == EXPR_STRUCT_INIT) {
                        is_struct = 1;
                        struct_name = expr->index_assign.value->struct_init.type_name;
                    } else if (expr->index_assign.value->expr_type) {
                        if (expr->index_assign.value->expr_type->kind == TYPE_STRING) {
                            is_string = 1;
                        } else if (expr->index_assign.value->expr_type->kind == TYPE_STRUCT) {
                            is_struct = 1;
                            if (expr->index_assign.value->expr_type->struct_type.name.start) {
                                struct_name = expr->index_assign.value->expr_type->struct_type.name;
                            } else {
                                struct_name = expr->index_assign.value->expr_type->name;
                            }
                        } else if (expr->index_assign.value->expr_type->kind == TYPE_FLOAT) {
                            is_float = 1;
                        }
                    }
                    
                    // If value type unknown, check the array's element type
                    if (!is_string && !is_struct && !is_float) {
                        Type* obj_type = expr->index_assign.object->expr_type;
                        if (obj_type && obj_type->kind == TYPE_ARRAY && obj_type->array_type.element_type) {
                            Type* elem = obj_type->array_type.element_type;
                            if (elem->kind == TYPE_STRING) {
                                is_string = 1;
                            } else if (elem->kind == TYPE_STRUCT) {
                                is_struct = 1;
                                // Try struct_type.name first, then type->name
                                if (elem->struct_type.name.start && elem->struct_type.name.length > 0) {
                                    struct_name = elem->struct_type.name;
                                } else if (elem->name.start && elem->name.length > 0) {
                                    struct_name = elem->name;
                                }
                            } else if (elem->kind == TYPE_FLOAT) {
                                is_float = 1;
                            }
                        }
                    }
                    
                    if (is_string) {
                        emit("__arr_ptr->data[__idx].type = WYN_TYPE_STRING; __arr_ptr->data[__idx].data.string_val = ");
                    } else if (is_struct && struct_name.start && struct_name.length > 0) {
                        emit("__arr_ptr->data[__idx].type = WYN_TYPE_STRUCT; { ");
                        emit("%.*s __temp_struct = ", struct_name.length, struct_name.start);
                        codegen_expr(expr->index_assign.value);
                        emit("; __arr_ptr->data[__idx].data.struct_val = malloc(sizeof(%.*s)); ", struct_name.length, struct_name.start);
                        emit("memcpy(__arr_ptr->data[__idx].data.struct_val, &__temp_struct, sizeof(%.*s)); } } }", struct_name.length, struct_name.start);
                    } else if (is_float) {
                        emit("__arr_ptr->data[__idx].type = WYN_TYPE_FLOAT; __arr_ptr->data[__idx].data.float_val = ");
                    } else {
                        emit("__arr_ptr->data[__idx].type = WYN_TYPE_INT; __arr_ptr->data[__idx].data.int_val = ");
                    }
                    if (!(is_struct && struct_name.start && struct_name.length > 0)) {
                        codegen_expr(expr->index_assign.value);
                        emit("; } }");
                    }
                }
            }
            break;
        }
        case EXPR_FIELD_ASSIGN: {
            // Handle field assignment: obj.field = value
            emit("(");
            codegen_expr(expr->field_assign.object);
            emit(").%.*s = ", expr->field_assign.field.length, expr->field_assign.field.start);
            codegen_expr(expr->field_assign.value);
            break;
        }
        case EXPR_OPTIONAL_TYPE:
            // T2.5.1: Optional Type Implementation - For type expressions, just emit the inner type
            // In a real implementation, this would generate optional type metadata
            codegen_expr(expr->optional_type.inner_type);
            break;
        case EXPR_UNION_TYPE:
            // T2.5.2: Union Type Support - For type expressions, emit union type representation
            // In a real implementation, this would generate union type metadata
            emit("union { ");
            for (int i = 0; i < expr->union_type.type_count; i++) {
                if (i > 0) emit("; ");
                codegen_expr(expr->union_type.types[i]);
            }
            emit(" }");
            break;
        default:
            break;
    }
}

