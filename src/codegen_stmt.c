// codegen_stmt.c - Statement code generation
// Included from codegen.c - shares all statics

// Check if expression produces a fresh RC string that needs release
// Conservative: only match patterns known to allocate new strings
static bool is_fresh_string_temp(Expr* e) {
    if (!e) return false;
    // String concat (binary + with at least one string operand) always allocates
    if (e->type == EXPR_BINARY && e->binary.op.type == TOKEN_PLUS) {
        bool has_str = (e->binary.left->type == EXPR_STRING) ||
            (e->binary.left->expr_type && e->binary.left->expr_type->kind == TYPE_STRING) ||
            (e->binary.right->type == EXPR_STRING) ||
            (e->binary.right->expr_type && e->binary.right->expr_type->kind == TYPE_STRING) ||
            (e->binary.left->type == EXPR_STRING_INTERP) ||
            (e->binary.right->type == EXPR_STRING_INTERP) ||
            is_fresh_string_temp(e->binary.left) ||
            is_fresh_string_temp(e->binary.right);
        if (has_str) return true;
    }
    // String interpolation always allocates
    if (e->type == EXPR_STRING_INTERP) return true;
    // to_string() on non-string types always allocates
    if (e->type == EXPR_METHOD_CALL &&
        e->method_call.method.length == 9 &&
        memcmp(e->method_call.method.start, "to_string", 9) == 0 &&
        !(e->method_call.object->expr_type && e->method_call.object->expr_type->kind == TYPE_STRING))
        return true;
    // Free function calls returning string (user-defined functions allocate fresh strings)
    if (e->type == EXPR_CALL && e->call.callee->type == EXPR_IDENT &&
        e->expr_type && e->expr_type->kind == TYPE_STRING) {
        // Exclude builtins that return shared refs
        int len = e->call.callee->token.length;
        const char* name = e->call.callee->token.start;
        if (len == 5 && memcmp(name, "input", 5) == 0) return false;
        if (len == 8 && memcmp(name, "get_argv", 8) == 0) return false;
        return true;
    }
    return false;
}

static void emit_function_with_prefix(Stmt* fn_stmt, const char* prefix) {
    if (!fn_stmt || fn_stmt->type != STMT_FN) {
        return;
    }
    
    // Convert module name to C identifier
    const char* c_prefix = module_to_c_ident(prefix);
    
    // Set module context for internal call prefixing
    const char* saved_prefix = current_module_prefix;
    current_module_prefix = prefix;
    
    // Register function parameters for scope tracking
    clear_parameters();
    clear_local_variables();
    { extern void reset_shadow_vars(void); reset_shadow_vars(); }
    { extern void reset_string_vars(void); reset_string_vars(); }
    { extern void reset_array_scope(void); reset_array_scope(); } { extern void reset_hashmap_scope(void); reset_hashmap_scope(); } { extern void reset_closure_scope(void); reset_closure_scope(); }
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        char param_name[256]; token_to_cstr(param_name, sizeof(param_name), fn_stmt->fn.params[i]);
        bool is_mut = fn_stmt->fn.param_mutable && fn_stmt->fn.param_mutable[i];
        register_parameter_mut(param_name, is_mut);
    }
    
    // Emit function signature with module prefix
    // Private functions are static (not accessible from outside)
    if (!fn_stmt->fn.is_public) {
        emit("static ");
    }
    
    // Determine return type. No annotation (and nothing synthesized by the
    // checker's inference) means VOID - the old "long long" default made a
    // module fn with a bare `return` guard emit non-void C ("should return a
    // value", bug M4 2026-07-18). Matches the same-file STMT_FN default.
    const char* return_type = fn_stmt->fn.return_type ? "long long" : "void";
    static char custom_return_type[128];
    if (fn_stmt->fn.return_type) {
        if (fn_stmt->fn.return_type->type == EXPR_ARRAY) {
            return_type = "WynArray";
        } else if (fn_stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
            // `-> int?` / `-> string?` → Option struct C type.
            Expr* inner = fn_stmt->fn.return_type->optional_type.inner_type;
            if (inner && inner->type == EXPR_IDENT && inner->token.length == 6 &&
                memcmp(inner->token.start, "string", 6) == 0)
                return_type = "OptionString";
            else if (inner && inner->type == EXPR_IDENT && inner->token.length == 5 &&
                memcmp(inner->token.start, "float", 5) == 0)
                return_type = "OptionFloat";
            else if (inner && inner->type == EXPR_IDENT && inner->token.length == 4 &&
                memcmp(inner->token.start, "bool", 4) == 0)
                return_type = "OptionBool";
            else
                return_type = "OptionInt";
        } else if (fn_stmt->fn.return_type->type == EXPR_CALL &&
                   fn_stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
            // Generic return type: Result<T,E> / Option<T> → the monomorphic struct.
            Token gt = fn_stmt->fn.return_type->call.callee->token;
            const char* fam = NULL;
            if (gt.length == 6 && memcmp(gt.start, "Result", 6) == 0) fam = "Result";
            else if (gt.length == 6 && memcmp(gt.start, "Option", 6) == 0) fam = "Option";
            if (fam) {
                const char* suf = "Int";
                if (fn_stmt->fn.return_type->call.arg_count > 0 &&
                    fn_stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                    Token in0 = fn_stmt->fn.return_type->call.args[0]->token;
                    if (in0.length == 6 && memcmp(in0.start, "string", 6) == 0) suf = "String";
                    else if (in0.length == 5 && memcmp(in0.start, "float", 5) == 0) suf = "Float";
                    else if (in0.length == 4 && memcmp(in0.start, "bool", 4) == 0) suf = "Bool";
                }
                snprintf(custom_return_type, 128, "%s%s", fam, suf);
                return_type = custom_return_type;
            }
        } else if (fn_stmt->fn.return_type->type == EXPR_IDENT) {
            Token rt = fn_stmt->fn.return_type->token;
            if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) {
                return_type = "const char*";
            } else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) {
                return_type = "double";
            } else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) {
                return_type = "bool";
            } else if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) {
                return_type = "long long";
            } else if (rt.length == 7 && memcmp(rt.start, "HashMap", 7) == 0) {
                return_type = "WynHashMap*";
            } else if (rt.length == 7 && memcmp(rt.start, "HashSet", 7) == 0) {
                return_type = "WynHashSet*";
            } else {
                // Custom struct type - add module prefix if in module context
                if (current_module_prefix) {
                    snprintf(custom_return_type, 128, "%s_%.*s", current_module_prefix, rt.length, rt.start);
                } else {
                    token_to_cstr(custom_return_type, sizeof(custom_return_type), rt);
                }
                return_type = custom_return_type;
            }
        }
    }
    
    emit("%s %s_%.*s(", return_type, c_prefix, fn_stmt->fn.name.length, fn_stmt->fn.name.start);
    
    // Parameters
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        if (i > 0) emit(", ");
        
        // Check if parameter has a type expression
        if (fn_stmt->fn.param_types && fn_stmt->fn.param_types[i]) {
            Expr* param_type = fn_stmt->fn.param_types[i];
            if (param_type->type == EXPR_IDENT) {
                // Convert Wyn types to C types
                Token type_token = param_type->token;
                const char* c_type = "long long";
                
                if (type_token.length == 6 && memcmp(type_token.start, "string", 6) == 0) {
                    c_type = "const char*";
                } else if (type_token.length == 5 && memcmp(type_token.start, "float", 5) == 0) {
                    c_type = "double";
                } else if (type_token.length == 4 && memcmp(type_token.start, "bool", 4) == 0) {
                    c_type = "bool";
                } else if (type_token.length == 5 && memcmp(type_token.start, "array", 5) == 0) {
                    c_type = "WynArray";
                } else if (type_token.length == 3 && memcmp(type_token.start, "int", 3) == 0) {
                    c_type = "long long";
                } else if (type_token.length == 7 && memcmp(type_token.start, "HashMap", 7) == 0) {
                    c_type = "WynHashMap*";
                } else if (type_token.length == 7 && memcmp(type_token.start, "HashSet", 7) == 0) {
                    c_type = "WynHashSet*";
                } else if (type_token.length == 3 && memcmp(type_token.start, "ptr", 3) == 0) {
                    c_type = "void*";   // FFI opaque pointer passed through a user fn
                } else if (type_token.length == 4 && memcmp(type_token.start, "cstr", 4) == 0) {
                    c_type = "char*";   // raw C string
                } else {
                    // Custom struct type - add module prefix if in module context
                    if (current_module_prefix) {
                        emit("%s_%.*s ", current_module_prefix, type_token.length, type_token.start);
                    } else {
                        emit("%.*s ", type_token.length, type_token.start);
                    }
                    goto emit_param_name;
                }
                
                emit("%s ", c_type);
            } else {
                emit("long long ");
            }
        } else {
            emit("long long ");
        }
        
        emit_param_name:
        // Emit parameter name (with pointer for mut params)
        {
            bool is_mut = fn_stmt->fn.param_mutable && fn_stmt->fn.param_mutable[i];
            if (is_mut) emit("*");
        }
        Token param_name = fn_stmt->fn.params[i];
        emit("%.*s", param_name.length, param_name.start);
    }
    emit(") ");
    
    // Body
    if (fn_stmt->fn.body) {
        if (fn_stmt->fn.body->type == STMT_BLOCK) {
            emit("{\n");
            codegen_stmt(fn_stmt->fn.body);
            emit("}\n");
        } else {
            codegen_stmt(fn_stmt->fn.body);
        }
    }
    emit("\n");
    
    // Restore context
    current_module_prefix = saved_prefix;
}

static int stmt_line(Stmt* s) {
    if (!s) return 0;
    switch (s->type) {
        case STMT_VAR: return s->var.name.line;
        case STMT_EXPR: return s->expr ? s->expr->token.line : 0;
        case STMT_RETURN: return s->ret.value ? s->ret.value->token.line : 0;
        case STMT_YIELD: return s->yield_stmt.value ? s->yield_stmt.value->token.line : 0;        case STMT_FN: return s->fn.name.line;
        default: return 0;
    }
}

static const char* source_file_name = NULL;
void codegen_set_source_file(const char* name) { source_file_name = name; }

static void emit_line(Stmt* s) {
    int line = stmt_line(s);
    if (line > 0 && source_file_name) {
        emit("/* @%s:%d */ ", source_file_name, line);
        emit("\n#line %d \"%s\"\n", line, source_file_name);
    }
}

void codegen_stmt(Stmt* stmt) {
    if (!stmt) return;
    emit_line(stmt);
    
    switch (stmt->type) {
        case STMT_EXPR:
            if (!stmt->expr) {
                break;  // Skip NULL expressions
            }
            // Check if this expression is marked as an implicit return
            if (stmt->expr->is_implicit_return) {
                if (in_async_function) {
                    emit("*temp = ");
                    codegen_expr(stmt->expr);
                    emit("; goto async_return;\n");
                } else {
                    emit("return ");
                    codegen_expr(stmt->expr);
                    emit(";\n");
                }
            } else {
                // Release unused string return values to prevent leaks
                bool _is_str_call = (stmt->expr->type == EXPR_CALL || stmt->expr->type == EXPR_METHOD_CALL) &&
                    stmt->expr->expr_type && stmt->expr->expr_type->kind == TYPE_STRING;
                // Check for fresh string args that need release
                Expr* _se = stmt->expr;
                int _nargs = 0;
                Expr** _args = NULL;
                if (_se->type == EXPR_CALL) { _nargs = _se->call.arg_count; _args = _se->call.args; }
                else if (_se->type == EXPR_METHOD_CALL) { _nargs = _se->method_call.arg_count; _args = _se->method_call.args; }
                int _fresh[16]; int _fc = 0;
                for (int ai = 0; ai < _nargs && _fc < 16; ai++) {
                    if (is_fresh_string_temp(_args[ai])) _fresh[_fc++] = ai;
                }
                // A fresh string pushed into a string array transfers ownership to
                // the array (array_push_str stores it raw; array_free releases it).
                // Such an argument must NOT be released after the call, or the live
                // array element is double-freed. Identify the transferring arg index
                // (-1 = none): method `arr.push(x)` on a [string] → arg 0;
                // free call `array_push_str(&arr, x)` → arg 1.
                int _push_move_arg = -1;
                if (_se->type == EXPR_METHOD_CALL &&
                    _se->method_call.method.length == 4 &&
                    memcmp(_se->method_call.method.start, "push", 4) == 0 &&
                    _se->method_call.object->expr_type &&
                    _se->method_call.object->expr_type->kind == TYPE_ARRAY &&
                    _se->method_call.object->expr_type->array_type.element_type &&
                    _se->method_call.object->expr_type->array_type.element_type->kind == TYPE_STRING) {
                    _push_move_arg = 0;
                } else if (_se->type == EXPR_CALL && _se->call.callee->type == EXPR_IDENT &&
                    _se->call.callee->token.length == 14 &&
                    memcmp(_se->call.callee->token.start, "array_push_str", 14) == 0) {
                    _push_move_arg = 1;
                }
                if (_fc > 0 && !_is_str_call) {
                    emit("{ ");
                    // Evaluate fresh args to temps
                    for (int si = 0; si < _fc; si++) {
                        emit("const char* __sa%d = ", si);
                        codegen_expr(_args[_fresh[si]]);
                        emit("; ");
                        _args[_fresh[si]]->_codegen_temp_id = si;
                    }
                    // Emit call (args with temp IDs emit __sa{id} instead)
                    codegen_expr(_se);
                    emit("; ");
                    // Release temps - except one whose ownership transferred to
                    // an array via push (releasing it would double-free the element).
                    for (int si = 0; si < _fc; si++) {
                        if (_fresh[si] != _push_move_arg)
                            emit("wyn_rc_release(__sa%d); ", si);
                        _args[_fresh[si]]->_codegen_temp_id = -1;
                    }
                    emit("}\n");
                } else if (_is_str_call) {
                    emit("wyn_rc_release(");
                    codegen_expr(stmt->expr);
                    emit(");\n");
                } else {
                    codegen_expr(stmt->expr);
                    emit(";\n");
                }
                // Readback captured variables after lambda calls (capture by reference)
                if (stmt->expr->type == EXPR_METHOD_CALL) {
                    for (int ai = 0; ai < stmt->expr->method_call.arg_count; ai++) {
                        if (stmt->expr->method_call.args[ai]->type == EXPR_LAMBDA) {
                            // Find the lambda ID for this expression (sequential counter)
                            // The lambda_ref_counter tracks which lambda was last referenced
                            int target_lid = lambda_ref_counter;
                            for (int li = 0; li < lambda_count; li++) {
                                if (lambda_functions[li].id == target_lid && lambda_functions[li].capture_count > 0 && !lambda_functions[li].is_closure) {
                                    for (int ci = 0; ci < lambda_functions[li].capture_count; ci++) {
                                        emit("%s = __cap_%d_%s;\n",
                                            lambda_functions[li].captured_vars[ci],
                                            lambda_functions[li].id,
                                            lambda_functions[li].captured_vars[ci]);
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            break;
        case STMT_VAR: {
            // Determine C type based on explicit type annotation or initializer
            const char* c_type = "long long";
            bool is_already_const = false;  // Track if type already has const
            bool needs_arc_management = false;
            
            // Check for explicit type annotation first
            if (stmt->var.type) {
                if (stmt->var.type->type == EXPR_OPTIONAL_TYPE) {
                    // Handle optional type annotation like int?, string?
                    Expr* inner = stmt->var.type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT && inner->token.length == 3 && memcmp(inner->token.start, "int", 3) == 0) {
                        c_type = "OptionInt";
                    } else if (inner && inner->type == EXPR_IDENT && inner->token.length == 6 && memcmp(inner->token.start, "string", 6) == 0) {
                        c_type = "OptionString";
                    } else if (inner && inner->type == EXPR_IDENT && inner->token.length == 5 && memcmp(inner->token.start, "float", 5) == 0) {
                        c_type = "OptionFloat";
                    } else if (inner && inner->type == EXPR_IDENT && inner->token.length == 4 && memcmp(inner->token.start, "bool", 4) == 0) {
                        c_type = "OptionBool";
                    } else if (inner && inner->type == EXPR_IDENT &&
                               ({ char _stn[96]; token_to_cstr(_stn, sizeof(_stn), inner->token);
                                  extern int is_known_struct(const char*); is_known_struct(_stn); })) {
                        // `var v: Struct? = ...` -> the Option<Struct> family.
                        char _stn[96]; token_to_cstr(_stn, sizeof(_stn), inner->token);
                        static char _osann[128]; snprintf(_osann, sizeof(_osann), "Option%s", _stn);
                        c_type = _osann;
                    } else {
                        c_type = "WynOptional*";
                        needs_arc_management = true;
                    }
                } else if (stmt->var.type->type == EXPR_ARRAY) {
                    // Handle typed array annotation like [int], [TokenType]
                    // Use WynIntArray for [int] for performance (only when not returned from function)
                    bool is_int_array = false;
                    extern bool codegen_fn_returns_array;
                    if (!codegen_fn_returns_array && stmt->var.type->array.count > 0 && stmt->var.type->array.elements[0]->type == EXPR_IDENT) {
                        Token et = stmt->var.type->array.elements[0]->token;
                        if (et.length == 3 && memcmp(et.start, "int", 3) == 0) is_int_array = true;
                    }
                    if (is_int_array) {
                        c_type = "WynIntArray";
                        char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                        extern void register_int_array_var(const char*);
                        register_int_array_var(_vn);
                    } else {
                        c_type = "WynArray";
                    }
                    needs_arc_management = false;
                } else if (stmt->var.type->type == EXPR_CALL) {
                    // Handle generic type instantiation: Array<T>, HashMap<K,V>, Option<T>, etc.
                    if (stmt->var.type->call.callee->type == EXPR_IDENT) {
                        Token type_name = stmt->var.type->call.callee->token;
                        if ((type_name.length == 5 && memcmp(type_name.start, "Array", 5) == 0) ||
                            (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0)) {
                            c_type = "WynArray";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            c_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            c_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            c_type = "WynOptional*";
                            needs_arc_management = true;
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            c_type = "WynResult*";
                            needs_arc_management = true;
                        }
                    }
                } else if (stmt->var.type->type == EXPR_IDENT) {
                    Token type_name = stmt->var.type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        c_type = "long long";
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        c_type = "const char*";
                        is_already_const = true;  // String type already has const
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        c_type = "double";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        c_type = "bool";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "char", 4) == 0) {
                        c_type = "char";
                    } else {
                        // Custom struct/enum type - use the type name as-is
                        static char custom_type_buf[256]; token_to_cstr(custom_type_buf, sizeof(custom_type_buf), type_name);
                        c_type = custom_type_buf;
                        // Register enum var if this is an enum type
                        extern int is_enum_type(const char*);
                        if (is_enum_type(custom_type_buf)) {
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_vn, custom_type_buf);
                        }
                    }
                }
            } else if (stmt->var.init) {
                // Infer type from initializer if no explicit type
                if (stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                    is_already_const = true;
                } else if (stmt->var.init->type == EXPR_IDENT) {
                    // Check if init is a known string variable
                    char _idn[256]; token_to_cstr(_idn, sizeof(_idn), stmt->var.init->token);
                    extern int is_string_var(const char*);
                    if (is_string_var(_idn)) {
                        c_type = "const char*";
                        is_already_const = true;
                    }
                    extern int is_known_array_var(const char*);
                    if (is_known_array_var(_idn)) {
                        c_type = "WynArray";
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                        extern void register_array_var(const char*); register_array_var(_vn);
                    }
                    // Copying a tuple var (`var q = p`, incl. the temp emitted by
                    // tuple destructuring `var a, b = p`): keep the tuple struct
                    // type via __auto_type instead of truncating to long long.
                    extern int is_tuple_var(const char*);
                    if (is_tuple_var(_idn)) {
                        c_type = "__auto_type";
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                        extern void register_tuple_var(const char*); register_tuple_var(_vn);
                    }
                } else if (stmt->var.init->type == EXPR_TUPLE_INDEX) {
                    // `var x = tup.N` - including the desugaring of tuple
                    // destructuring `var a, b = tup`. The element can be any type
                    // (int/string/…); __auto_type copies the field's real C type
                    // instead of truncating a string field to long long.
                    c_type = "__auto_type";
                    // If the element is a string, track it so later concat/print
                    // treats it as a string (register_string_var).
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_STRING) {
                        char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                        extern void register_string_var(const char*); register_string_var(_vn);
                    }
                } else if (stmt->var.init->type == EXPR_STRING_INTERP) {
                    c_type = "char*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_INDEX &&
                           stmt->var.init->index.array->type == EXPR_IDENT) {
                    char _ixn[256]; token_to_cstr(_ixn, sizeof(_ixn), stmt->var.init->index.array->token);
                    extern int is_str_array_var(const char*);
                    // Prefer the array's REAL element type when known - otherwise an
                    // indexed element (`var d = items[i]`) defaulted to long long even
                    // for a struct/float/string array, so `var d = structArray[i]`
                    // emitted `long long d = array_get_struct(..)` → C type error. Use
                    // the index expr's checked type, then the array's element type. (2026-07)
                    Type* _idx_t = stmt->var.init->expr_type;
                    Type* _arr_t = stmt->var.init->index.array->expr_type;
                    Type* _elt = (_idx_t && _idx_t->kind != TYPE_INT) ? _idx_t
                               : (_arr_t && _arr_t->kind == TYPE_ARRAY) ? _arr_t->array_type.element_type
                               : NULL;
                    if (_elt && _elt->kind == TYPE_STRUCT) {
                        static char _sa_buf[256];
                        token_to_cstr(_sa_buf, sizeof(_sa_buf), _elt->struct_type.name);
                        c_type = _sa_buf;
                        char _dvn[256]; token_to_cstr(_dvn, sizeof(_dvn), stmt->var.name);
                        extern void register_struct_var(const char*, const char*);
                        register_struct_var(_dvn, _sa_buf);
                    } else if (_elt && _elt->kind == TYPE_STRING) {
                        c_type = "const char*"; is_already_const = true;
                    } else if (_elt && _elt->kind == TYPE_FLOAT) {
                        c_type = "double";
                    } else if (_elt && _elt->kind == TYPE_BOOL) {
                        c_type = "bool";
                    } else if (is_str_array_var(_ixn)) {
                        c_type = "const char*"; is_already_const = true;
                    }
                } else if (stmt->var.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (stmt->var.init->type == EXPR_BOOL) {
                    c_type = "bool";
                } else if (stmt->var.init->type == EXPR_UNARY) {
                    // Handle unary expressions like -3.5
                    if (stmt->var.init->unary.operand->type == EXPR_FLOAT) {
                        c_type = "double";
                    } else if (stmt->var.init->unary.operand->type == EXPR_INT) {
                        c_type = "long long";
                    }
                } else if (stmt->var.init->type == EXPR_ARRAY || stmt->var.init->type == EXPR_LIST_COMP) {
                    // Check if this array holds spawn futures (detected in pre-scan)
                    char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                    if (is_spawn_array(_vn)) {
                        c_type = "WynIntArray";
                    } else {
                        c_type = "WynArray";
                        register_array_var(_vn);
                    }
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_MAP) {
                    // Map type - use the typedef
                    c_type = "WynMap";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_METHOD_CALL) {
                    // Enum constructor `var s = Shape.Rect(...)` (object is the enum
                    // type name): the var holds the enum value. Register it as an
                    // enum var + use the enum's C type so `match s { ... }` lowers
                    // via tags. (Without this, an inferred-type enum var wasn't
                    // tracked and statement-form match misrouted it.)
                    if (stmt->var.init->method_call.object->type == EXPR_IDENT) {
                        char _en[128]; token_to_cstr(_en, sizeof(_en), stmt->var.init->method_call.object->token);
                        extern int is_enum_type(const char*);
                        if (is_enum_type(_en)) {
                            static char _enbuf[128]; snprintf(_enbuf, sizeof(_enbuf), "%s", _en);
                            c_type = _enbuf;
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_vn, _en);
                            goto var_type_done;
                        }
                    }
                    // Check if this is a struct method call - infer return type
                    bool _found_method_rt = false;
                    if (stmt->var.init->method_call.object->type == EXPR_IDENT) {
                        // Try expr_type first
                        const char* _struct_name = NULL;
                        if (stmt->var.init->method_call.object->expr_type &&
                            stmt->var.init->method_call.object->expr_type->kind == TYPE_STRUCT) {
                            Token sn = stmt->var.init->method_call.object->expr_type->struct_type.name;
                            static char _sn_buf[64]; token_to_cstr(_sn_buf, sizeof(_sn_buf), sn);
                            _struct_name = _sn_buf;
                        }
                        // Fallback: look up variable's struct type from registry
                        if (!_struct_name) {
                            char _vn[64]; token_to_cstr(_vn, sizeof(_vn), stmt->var.init->method_call.object->token);
                            extern const char* get_struct_var_type(const char*);
                            _struct_name = get_struct_var_type(_vn);
                        }
                        if (_struct_name) {
                            char _mn[64]; token_to_cstr(_mn, sizeof(_mn), stmt->var.init->method_call.method);
                            extern const char* lookup_struct_method_return_type(const char*, const char*);
                            const char* rt = lookup_struct_method_return_type(_struct_name, _mn);
                            if (rt) {
                                if (strcmp(rt, "float") == 0) { c_type = "double"; _found_method_rt = true; }
                                else if (strcmp(rt, "string") == 0) { c_type = "const char*"; is_already_const = true; _found_method_rt = true; }
                                else if (strcmp(rt, "bool") == 0) { c_type = "bool"; _found_method_rt = true; }
                                else if (strcmp(rt, "int") == 0) { c_type = "long long"; _found_method_rt = true; }
                                else {
                                    static char _srt[128]; snprintf(_srt, 128, "%s", rt); c_type = _srt; _found_method_rt = true;
                                    // Register the result variable as a struct var too
                                    char _rvn[128]; token_to_cstr(_rvn, sizeof(_rvn), stmt->var.name);
                                    extern void register_struct_var(const char*, const char*);
                                    register_struct_var(_rvn, rt);
                                }
                                if (_found_method_rt) goto var_type_done;
                            }
                        }
                    }
                    // Methods that return arrays → WynArray (only when object is array, not string)
                    if (!_found_method_rt) {
                        char _mn2[64]; token_to_cstr(_mn2, sizeof(_mn2), stmt->var.init->method_call.method);
                        // Check if object is a string - these methods return string, not array
                        bool _obj_is_string = stmt->var.init->method_call.object->expr_type &&
                            stmt->var.init->method_call.object->expr_type->kind == TYPE_STRING;
                        if (!_obj_is_string && (strcmp(_mn2, "sort") == 0 || strcmp(_mn2, "reverse") == 0 ||
                            strcmp(_mn2, "filter") == 0 || strcmp(_mn2, "map") == 0 ||
                            strcmp(_mn2, "unique") == 0 || strcmp(_mn2, "slice") == 0 ||
                            strcmp(_mn2, "flat_map") == 0 || strcmp(_mn2, "collect") == 0)) {
                            c_type = "WynArray";
                            char _vn2[256]; token_to_cstr(_vn2, sizeof(_vn2), stmt->var.name);
                            extern void register_array_var(const char*); register_array_var(_vn2);
                            // S2: register the RESULT as a string array only
                            // when the checker typed it [string] - a str->int
                            // .map result registered by SOURCE type made
                            // c = b.map((s) => s.len()) read ints as char*.
                            bool _res_is_str_arr =
                                stmt->var.init->expr_type &&
                                stmt->var.init->expr_type->kind == TYPE_ARRAY &&
                                stmt->var.init->expr_type->array_type.element_type &&
                                stmt->var.init->expr_type->array_type.element_type->kind == TYPE_STRING;
                            // Fallback (no expr_type): propagate from the source.
                            if (!_res_is_str_arr && !stmt->var.init->expr_type &&
                                stmt->var.init->method_call.object->type == EXPR_IDENT) {
                                char _srcn[256]; token_to_cstr(_srcn, sizeof(_srcn), stmt->var.init->method_call.object->token);
                                extern int is_str_array_var(const char*);
                                if (is_str_array_var(_srcn)) _res_is_str_arr = true;
                            }
                            if (_res_is_str_arr) {
                                extern void register_str_array_var(const char*);
                                register_str_array_var(_vn2);
                            }
                            goto var_type_done;
                        }
                        // String-returning methods
                        if (strcmp(_mn2, "upper") == 0 || strcmp(_mn2, "lower") == 0 ||
                            strcmp(_mn2, "trim") == 0 || strcmp(_mn2, "replace") == 0 ||
                            strcmp(_mn2, "repeat") == 0 || strcmp(_mn2, "substring") == 0 ||
                            strcmp(_mn2, "join") == 0) {
                            c_type = "const char*"; is_already_const = true;
                            goto var_type_done;
                        }
                    }
                    // Struct method chain return type
                    if (!_found_method_rt && stmt->var.init->type == EXPR_METHOD_CALL) {
                        Expr* _obj = stmt->var.init->method_call.object;
                        const char* _stype = NULL;
                        // Direct struct var
                        if (_obj->type == EXPR_IDENT) {
                            char _vn3[64]; token_to_cstr(_vn3, sizeof(_vn3), _obj->token);
                            extern const char* get_struct_var_type(const char*);
                            _stype = get_struct_var_type(_vn3);
                            // Static constructor: Type.new() - object is the type name itself
                            if (!_stype) {
                                extern int is_known_struct(const char*);
                                if (is_known_struct(_vn3)) _stype = _vn3;
                            }
                        }
                        // Chained method on struct
                        if (!_stype && _obj->type == EXPR_METHOD_CALL && _obj->method_call.object->type == EXPR_IDENT) {
                            char _vn3[64]; token_to_cstr(_vn3, sizeof(_vn3), _obj->method_call.object->token);
                            extern const char* get_struct_var_type(const char*);
                            _stype = get_struct_var_type(_vn3);
                            if (_stype) {
                                char _mn3[64]; token_to_cstr(_mn3, sizeof(_mn3), _obj->method_call.method);
                                extern const char* lookup_struct_method_return_type(const char*, const char*);
                                const char* _ret = lookup_struct_method_return_type(_stype, _mn3);
                                if (_ret) _stype = _ret;
                            }
                        }
                        // Function call root: from_table("x").where_clause(...)
                        if (!_stype && _obj->type == EXPR_CALL && _obj->call.callee->type == EXPR_IDENT) {
                            char _fn[64]; token_to_cstr(_fn, sizeof(_fn), _obj->call.callee->token);
                            extern const char* get_function_return_type(const char*);
                            const char* _frt = get_function_return_type(_fn);
                            if (_frt) { extern int is_known_struct(const char*); if (is_known_struct(_frt)) _stype = _frt; }
                        }
                        if (!_stype && _obj->type == EXPR_METHOD_CALL) {
                            // Walk deeper - function call at root of chain
                            Expr* _walk = _obj;
                            while (_walk && _walk->type == EXPR_METHOD_CALL) _walk = _walk->method_call.object;
                            if (_walk && _walk->type == EXPR_CALL && _walk->call.callee->type == EXPR_IDENT) {
                                char _fn[64]; token_to_cstr(_fn, sizeof(_fn), _walk->call.callee->token);
                                extern const char* get_function_return_type(const char*);
                                const char* _frt = get_function_return_type(_fn);
                                if (_frt) { extern int is_known_struct(const char*); if (is_known_struct(_frt)) _stype = _frt; }
                            }
                        }
                        if (_stype) {
                            char _mn3[64]; token_to_cstr(_mn3, sizeof(_mn3), stmt->var.init->method_call.method);
                            extern const char* lookup_struct_method_return_type(const char*, const char*);
                            const char* _ret = lookup_struct_method_return_type(_stype, _mn3);
                            if (_ret) {
                                extern int is_known_struct(const char*);
                                if (is_known_struct(_ret)) {
                                    c_type = _ret;
                                    char _vn4[256]; token_to_cstr(_vn4, sizeof(_vn4), stmt->var.name);
                                    extern void register_struct_var(const char*, const char*);
                                    register_struct_var(_vn4, _ret);
                                    goto var_type_done;
                                }
                                if (strcmp(_ret, "string") == 0) { c_type = "const char*"; is_already_const = true; goto var_type_done; }
                                if (strcmp(_ret, "float") == 0) { c_type = "double"; goto var_type_done; }
                                if (strcmp(_ret, "int") == 0) { c_type = "long long"; goto var_type_done; }
                            }
                        }
                    }
                    // Check for module constructor: StringBuilder.new(), etc.
                    if (stmt->var.init->method_call.object->type == EXPR_IDENT) {
                        char _on[64]; token_to_cstr(_on, sizeof(_on), stmt->var.init->method_call.object->token);
                        char _mn[64]; token_to_cstr(_mn, sizeof(_mn), stmt->var.init->method_call.method);
                        if (strcmp(_on, "StringBuilder") == 0 && strcmp(_mn, "new") == 0) {
                            c_type = "long long";
                            { char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name); extern void register_sb_var(const char*); register_sb_var(_vn); }
                            goto var_type_done;
                        }
                        // Check if it's an enum constructor: Shape.Circle(5.0)
                        extern int is_enum_type(const char*);
                        if (is_enum_type(_on)) {
                            c_type = _on;
                            { char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name); extern void register_enum_var(const char*, const char*); register_enum_var(_vn, _on); }
                            goto var_type_done;
                        }
                        // Check module function return type: Template.render -> string
                        {
                            extern const char* lookup_module_fn_return_type(const char*);
                            char _fn[128]; snprintf(_fn, 128, "%s_%.*s", _on, stmt->var.init->method_call.method.length, stmt->var.init->method_call.method.start);
                            const char* _rt = lookup_module_fn_return_type(_fn);
                            if (_rt) {
                                if (strcmp(_rt, "string") == 0) { c_type = "char*"; goto var_type_done; }
                                if (strcmp(_rt, "array") == 0) { c_type = "WynArray"; goto var_type_done; }
                                if (strcmp(_rt, "bool") == 0) { c_type = "bool"; goto var_type_done; }
                                if (strcmp(_rt, "float") == 0) { c_type = "double"; goto var_type_done; }
                            }
                        }
                    }
                    // Check if object is a trait - look up method return type from trait decl
                    if (!_found_method_rt && stmt->var.init->method_call.object->type == EXPR_IDENT && current_program) {
                        char _on2[64]; token_to_cstr(_on2, sizeof(_on2), stmt->var.init->method_call.object->token);
                        extern const char* get_struct_var_type(const char*);
                        const char* _vtype = get_struct_var_type(_on2);
                        if (!_vtype) {
                            // Check function params for trait type
                            // The param type might be a trait name
                        }
                        // Search trait declarations for this method
                        char _mn2[64]; token_to_cstr(_mn2, sizeof(_mn2), stmt->var.init->method_call.method);
                        for (int _ti = 0; _ti < current_program->count && !_found_method_rt; _ti++) {
                            if (current_program->stmts[_ti]->type == STMT_TRAIT) {
                                Stmt* _ts = current_program->stmts[_ti];
                                for (int _mi = 0; _mi < _ts->trait_decl.method_count; _mi++) {
                                    FnStmt* _tm = _ts->trait_decl.methods[_mi];
                                    if (_tm->name.length == stmt->var.init->method_call.method.length &&
                                        memcmp(_tm->name.start, stmt->var.init->method_call.method.start, _tm->name.length) == 0) {
                                        if (_tm->return_type && _tm->return_type->type == EXPR_CALL &&
                                            _tm->return_type->call.callee->type == EXPR_IDENT) {
                                            Token rt = _tm->return_type->call.callee->token;
                                            if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) {
                                                c_type = "ResultInt"; _found_method_rt = true;
                                            }
                                        } else if (_tm->return_type && _tm->return_type->type == EXPR_IDENT) {
                                            Token rt = _tm->return_type->token;
                                            if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) { c_type = "const char*"; _found_method_rt = true; }
                                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) { c_type = "double"; _found_method_rt = true; }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    // Quick check: if object is a known array and method returns array, type as WynArray
                    if (stmt->var.init->method_call.object->type == EXPR_IDENT) {
                        char _vn[64]; token_to_cstr(_vn, sizeof(_vn), stmt->var.init->method_call.object->token);
                        extern int is_known_array_var(const char*);
                        if (is_known_array_var(_vn)) {
                            char _mn[64]; token_to_cstr(_mn, sizeof(_mn), stmt->var.init->method_call.method);
                            if (strcmp(_mn, "slice") == 0 || strcmp(_mn, "reverse") == 0 || strcmp(_mn, "filter") == 0 || strcmp(_mn, "map") == 0 || strcmp(_mn, "concat") == 0 || strcmp(_mn, "unique") == 0 || strcmp(_mn, "sort") == 0 || strcmp(_mn, "sort_by") == 0) {
                                c_type = "WynArray";
                                needs_arc_management = false;
                                { char vn2[256]; token_to_cstr(vn2, sizeof(vn2), stmt->var.name); register_array_var(vn2); }
                                goto var_type_done;
                            }
                        }
                    }
                    // Phase 1 Task 1.3: Infer type from method call
                    // First check if the expression has a type from the checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "long long";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            case TYPE_ARRAY:
                                c_type = "WynArray";
                                break;
                            case TYPE_MAP:
                                c_type = "WynHashMap*";
                                break;
                            case TYPE_SET:
                                c_type = "WynHashSet*";
                                break;
                            case TYPE_JSON:
                                c_type = "WynJson*";
                                break;
                            case TYPE_STRUCT: {
                                static char method_struct_buf[256];
                                Token sn = stmt->var.init->expr_type->struct_type.name;
                                token_to_cstr(method_struct_buf, sizeof(method_struct_buf), sn);
                                c_type = method_struct_buf;
                                break;
                            }
                            default:
                                c_type = "long long";
                        }
                    } else {
                        // Fallback: Determine receiver type from expr_type (populated by checker)
                        const char* receiver_type = "int";  // default
                        
                        Expr* obj = stmt->var.init->method_call.object;
                        if (obj->expr_type) {
                            // Use type information from checker
                            switch (obj->expr_type->kind) {
                                case TYPE_STRING:
                                    receiver_type = "string";
                                    break;
                                case TYPE_INT:
                                    receiver_type = "int";
                                    break;
                                case TYPE_FLOAT:
                                    receiver_type = "float";
                                    break;
                                case TYPE_BOOL:
                                    receiver_type = "bool";
                                    break;
                                case TYPE_ARRAY:
                                    receiver_type = "array";
                                    break;
                                case TYPE_MAP:
                                    receiver_type = "map";
                                    break;
                                case TYPE_SET:
                                    receiver_type = "set";
                                    break;
                                default:
                                    receiver_type = "int";
                            }
                        } else {
                            // Fallback: infer from expression type
                            if (obj->type == EXPR_STRING) {
                                receiver_type = "string";
                            } else if (obj->type == EXPR_FLOAT) {
                                receiver_type = "float";
                            } else if (obj->type == EXPR_INT) {
                                receiver_type = "int";
                            } else if (obj->type == EXPR_BOOL) {
                                receiver_type = "bool";
                            } else if (obj->type == EXPR_ARRAY) {
                                receiver_type = "array";
                            } else if (obj->type == EXPR_MAP) {
                                receiver_type = "map";
                            } else if (obj->type == EXPR_METHOD_CALL) {
                                // Nested method call (chaining) - assume string for now
                                receiver_type = "string";
                            }
                        }
                        
                        // Check if object is a known array variable
                        if (obj->type == EXPR_IDENT) {
                            char vn[64]; token_to_cstr(vn, sizeof(vn), obj->token);
                            extern int is_known_array_var(const char*);
                            if (is_known_array_var(vn)) receiver_type = "array";
                        }
                        
                        // Look up method return type
                        Token method = stmt->var.init->method_call.method;
                        char method_name[64]; token_to_cstr(method_name, sizeof(method_name), method);
                        
                        const char* return_type = lookup_method_return_type(receiver_type, method_name);
                        if (!return_type && stmt->var.init->method_call.object->type == EXPR_IDENT) {
                            Token mod = stmt->var.init->method_call.object->token;
                            char fn_name[128];
                            snprintf(fn_name, sizeof(fn_name), "%.*s_%s", mod.length, mod.start, method_name);
                            return_type = lookup_module_fn_return_type(fn_name);
                        }
                        if (return_type) {
                            if (strcmp(return_type, "string") == 0) {
                                c_type = "char*";
                                needs_arc_management = false;  // Disable ARC for now (Phase 1 focus)
                            } else if (strcmp(return_type, "int") == 0) {
                                c_type = "long long";
                            } else if (strcmp(return_type, "float") == 0) {
                                c_type = "double";
                            } else if (strcmp(return_type, "bool") == 0) {
                                c_type = "bool";
                            } else if (strcmp(return_type, "array") == 0) {
                                c_type = "WynArray";
                                { char vn[256]; token_to_cstr(vn, sizeof(vn), stmt->var.name); register_array_var(vn); }
                            } else if (strcmp(return_type, "optional") == 0) {
                                c_type = "WynOptional*";
                                needs_arc_management = true;
                            } else if (strcmp(return_type, "void") == 0) {
                                c_type = "void";
                            } else if (strcmp(return_type, "json") == 0) {
                                c_type = "WynJson*";
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_MATCH) {
                    // Infer type from match expression arms
                    for (int _a = 0; _a < stmt->var.init->match.arm_count; _a++) {
                        Expr* arm_r = stmt->var.init->match.arms[_a].result;
                        if (!arm_r) continue;
                        if (arm_r->type == EXPR_STRING) {
                            c_type = "const char*"; is_already_const = true; break;
                        } else if (arm_r->type == EXPR_FLOAT) {
                            c_type = "double"; break;
                        } else if (arm_r->type == EXPR_BOOL) {
                            c_type = "bool"; break;
                        } else if (arm_r->expr_type) {
                            if (arm_r->expr_type->kind == TYPE_STRING) {
                                c_type = "const char*"; is_already_const = true; break;
                            } else if (arm_r->expr_type->kind == TYPE_FLOAT) {
                                c_type = "double"; break;
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_SPAWN) {
                    // Spawn returns a future
                    c_type = "Future*";
                    // Track if this future holds a string result
                    if (stmt->var.init->spawn.call && stmt->var.init->spawn.call->type == EXPR_CALL &&
                        stmt->var.init->spawn.call->call.callee->type == EXPR_IDENT) {
                        char _sfn[256]; token_to_cstr(_sfn, sizeof(_sfn), stmt->var.init->spawn.call->call.callee->token);
                        extern const char* get_function_return_type(const char*);
                        const char* _srt = get_function_return_type(_sfn);
                        if (_srt && strcmp(_srt, "string") == 0) {
                            char _vn2[256]; token_to_cstr(_vn2, sizeof(_vn2), stmt->var.name);
                            extern void register_string_future(const char*);
                            register_string_future(_vn2);
                        } else if (_srt && strcmp(_srt, "float") == 0) {
                            // Float results are BOXED (a malloc'd double) like
                            // structs - the word cast truncated them. Register
                            // so await derefs *(double*) instead.
                            char _vn2[256]; token_to_cstr(_vn2, sizeof(_vn2), stmt->var.name);
                            extern void register_struct_future(const char*, const char*);
                            register_struct_future(_vn2, "double");
                        } else if (_srt && strcmp(_srt, "int") != 0 && strcmp(_srt, "bool") != 0) {
                            char _vn2[256]; token_to_cstr(_vn2, sizeof(_vn2), stmt->var.name);
                            extern void register_struct_future(const char*, const char*);
                            register_struct_future(_vn2, _srt);
                        }
                    }
                } else if (stmt->var.init->type == EXPR_AWAIT) {
                    // Await: check if the future holds a string
                    Expr* await_inner = stmt->var.init->await.expr;
                    bool _is_str_await = false;
                    if (await_inner && await_inner->type == EXPR_IDENT) {
                        char _avn[256]; token_to_cstr(_avn, sizeof(_avn), await_inner->token);
                        extern int is_string_future(const char*);
                        if (is_string_future(_avn)) _is_str_await = true;
                    }
                    if (await_inner && await_inner->type == EXPR_SPAWN && await_inner->spawn.call &&
                        await_inner->spawn.call->type == EXPR_CALL &&
                        await_inner->spawn.call->call.callee->type == EXPR_IDENT) {
                        char _afn[256]; token_to_cstr(_afn, sizeof(_afn), await_inner->spawn.call->call.callee->token);
                        extern const char* get_function_return_type(const char*);
                        const char* _art = get_function_return_type(_afn);
                        if (_art && strcmp(_art, "string") == 0) _is_str_await = true;
                    }
                    if (_is_str_await) c_type = "const char*";
                    // Check for struct future
                    if (!_is_str_await) {
                        const char* _struct_type = NULL;
                        if (await_inner && await_inner->type == EXPR_IDENT) {
                            char _avn2[256]; token_to_cstr(_avn2, sizeof(_avn2), await_inner->token);
                            extern const char* get_struct_future_type(const char*);
                            _struct_type = get_struct_future_type(_avn2);
                        }
                        if (!_struct_type && await_inner && await_inner->type == EXPR_SPAWN && await_inner->spawn.call &&
                            await_inner->spawn.call->type == EXPR_CALL &&
                            await_inner->spawn.call->call.callee->type == EXPR_IDENT) {
                            char _afn2[256]; token_to_cstr(_afn2, sizeof(_afn2), await_inner->spawn.call->call.callee->token);
                            extern const char* get_function_return_type(const char*);
                            const char* _art2 = get_function_return_type(_afn2);
                            if (_art2 && strcmp(_art2, "int") != 0 && strcmp(_art2, "string") != 0 &&
                                strcmp(_art2, "float") != 0 && strcmp(_art2, "bool") != 0)
                                _struct_type = _art2;
                        }
                        if (_struct_type) {
                            static char _st_buf[128];
                            snprintf(_st_buf, 128, "%s", _struct_type);
                            c_type = _st_buf;
                        }
                    }
                } else if (stmt->var.init->type == EXPR_STRUCT_INIT) {
                    // Use the struct type name (monomorphic if available)
                    static char struct_type[128];
                    if (stmt->var.init->struct_init.monomorphic_name) {
                        snprintf(struct_type, 128, "%s", stmt->var.init->struct_init.monomorphic_name);
                    } else {
                        // Check if type_name contains a module prefix (from member expression)
                        // e.g., point.Point should become point_Point
                        Token type_name = stmt->var.init->struct_init.type_name;
                        
                        char temp_name[128]; token_to_cstr(temp_name, sizeof(temp_name), type_name);
                        
                        // Check if there's a dot in the name (module.Type)
                        char* dot = strchr(temp_name, '.');
                        if (dot) {
                            // Replace dot with underscore: point.Point → point_Point
                            *dot = '_';
                            snprintf(struct_type, 128, "%s", temp_name);
                        } else if (current_module_prefix) {
                            // Add module prefix if in module context
                            snprintf(struct_type, 128, "%s_%.*s", current_module_prefix, type_name.length, type_name.start);
                        } else {
                            token_to_cstr(struct_type, sizeof(struct_type), type_name);
                        }
                    }
                    c_type = struct_type;
                    // Register this variable as a struct var for method return type inference
                    { char _svn[128]; token_to_cstr(_svn, sizeof(_svn), stmt->var.name);
                      extern void register_struct_var(const char*, const char*);
                      register_struct_var(_svn, struct_type); }
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_SOME || stmt->var.init->type == EXPR_NONE) {
                    // Optional type. If the checker resolved a concrete Option family
                    // (OptionInt/String/…/OptionStruct), use it so the var-decl C type
                    // matches the Some(...)/None initializer and a later match detects
                    // the family. Falls back to the generic boxed WynOptional*.
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_STRUCT &&
                        stmt->var.init->expr_type->struct_type.name.length > 0) {
                        static char _osvbuf[128];
                        token_to_cstr(_osvbuf, sizeof(_osvbuf), stmt->var.init->expr_type->struct_type.name);
                        if (strncmp(_osvbuf, "Option", 6) == 0) {
                            c_type = _osvbuf;
                            needs_arc_management = false;
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_vn, _osvbuf);
                        } else { c_type = "WynOptional*"; needs_arc_management = true; }
                    } else {
                        c_type = "WynOptional*";
                        needs_arc_management = true;
                    }
                } else if (stmt->var.init->type == EXPR_OK || stmt->var.init->type == EXPR_ERR) {
                    // TASK-026: Result type
                    c_type = "WynResult*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_OPT_CHAIN) {
                    // `var x = opt?.field` - the checker resolved the result Option
                    // family (Option<FieldType>); use it and register for match.
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_STRUCT &&
                        stmt->var.init->expr_type->struct_type.name.length > 0) {
                        static char _ocvbuf[128];
                        token_to_cstr(_ocvbuf, sizeof(_ocvbuf), stmt->var.init->expr_type->struct_type.name);
                        c_type = _ocvbuf;
                        needs_arc_management = false;
                        char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                        extern void register_enum_var(const char*, const char*);
                        register_enum_var(_vn, _ocvbuf);
                    } else { c_type = "OptionInt"; }
                } else if (stmt->var.init->type == EXPR_LAMBDA) {
                    // Lambda/closure type - function pointer matching actual params
                    // S2: use checker-inferred types instead of hardcoding long long
                    int nparams = stmt->var.init->lambda.param_count;
                    static char _fn_ptr_buf[256];
                    int _fp = 0;
                    const char* _rct = "long long";
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_FUNCTION &&
                        stmt->var.init->expr_type->fn_type.return_type) {
                        const char* _rt = codegen_c_type_from_type(stmt->var.init->expr_type->fn_type.return_type);
                        if (_rt) _rct = _rt;
                    }
                    _fp += snprintf(_fn_ptr_buf + _fp, sizeof(_fn_ptr_buf) - _fp, "%s (*)(", _rct);
                    for (int _pi = 0; _pi < nparams; _pi++) {
                        if (_pi > 0) _fp += snprintf(_fn_ptr_buf + _fp, sizeof(_fn_ptr_buf) - _fp, ", ");
                        const char* _pct = "long long";
                        if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_FUNCTION &&
                            _pi < stmt->var.init->expr_type->fn_type.param_count &&
                            stmt->var.init->expr_type->fn_type.param_types[_pi]) {
                            const char* _pt = codegen_c_type_from_type(stmt->var.init->expr_type->fn_type.param_types[_pi]);
                            if (_pt) _pct = _pt;
                        }
                        _fp += snprintf(_fn_ptr_buf + _fp, sizeof(_fn_ptr_buf) - _fp, "%s", _pct);
                    }
                    _fp += snprintf(_fn_ptr_buf + _fp, sizeof(_fn_ptr_buf) - _fp, ")");
                    c_type = _fn_ptr_buf;
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_HASHMAP_LITERAL) {
                    // v1.2.3: HashMap literal
                    c_type = "WynHashMap*";
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_HASHSET_LITERAL) {
                    // v1.2.3: HashSet literal
                    c_type = "WynHashSet*";
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_INDEX) {
                    // Array/map/string indexing - check expr_type from checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "long long";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            case TYPE_MAP:
                                c_type = "WynHashMap*";
                                break;
                            case TYPE_STRUCT: {
                                // Use the struct type name
                                static char struct_type_buf[256]; token_to_cstr(struct_type_buf, sizeof(struct_type_buf), stmt->var.init->expr_type->struct_type.name);
                                c_type = struct_type_buf;
                                break;
                            }
                            default:
                                c_type = "long long";
                        }
                    } else if (stmt->var.init->index.array->expr_type &&
                               stmt->var.init->index.array->expr_type->kind == TYPE_ARRAY &&
                               stmt->var.init->index.array->expr_type->array_type.element_type) {
                        // The indexed array has a KNOWN element type - use it, rather
                        // than falling through to the name-substring heuristics below
                        // (which mis-typed e.g. `var x = parts[1]` on an int array as
                        // const char* purely because the var was named "parts").
                        Type* _et = stmt->var.init->index.array->expr_type->array_type.element_type;
                        switch (_et->kind) {
                            case TYPE_STRING: c_type = "const char*"; is_already_const = true; break;
                            case TYPE_INT:    c_type = "long long"; break;
                            case TYPE_FLOAT:  c_type = "double"; break;
                            case TYPE_BOOL:   c_type = "bool"; break;
                            case TYPE_MAP:    c_type = "WynHashMap*"; break;
                            case TYPE_STRUCT: {
                                static char _elt_struct_buf[256];
                                token_to_cstr(_elt_struct_buf, sizeof(_elt_struct_buf), _et->struct_type.name);
                                c_type = _elt_struct_buf;
                                break;
                            }
                            default: c_type = "long long";
                        }
                    } else {
                        // Fallback to heuristics
                        if (stmt->var.init->index.array->type == EXPR_CALL) {
                            Token callee = stmt->var.init->index.array->call.callee->token;
                            if ((callee.length == 12 && memcmp(callee.start, "System::args", 12) == 0) ||
                                (callee.length == 14 && memcmp(callee.start, "File::list_dir", 14) == 0)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_METHOD_CALL) {
                            Token method = stmt->var.init->index.array->method_call.method;
                            if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                                (method.length == 5 && memcmp(method.start, "lines", 5) == 0)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_IDENT) {
                            // Content-tracked string arrays only. (Removed the old
                            // args/files/names/parts/entries name-substring list - it
                            // mis-typed int/float arrays that happened to share those
                            // names; the real element type above is authoritative.)
                            char _idxvan[256];
                            token_to_cstr(_idxvan, sizeof(_idxvan), stmt->var.init->index.array->token);
                            extern int is_str_array_var(const char*);
                            if (is_str_array_var(_idxvan)) {
                                c_type = "const char*";
                                is_already_const = true;
                            }
                        } else if (stmt->var.init->index.array->type == EXPR_ARRAY) {
                            if (stmt->var.init->index.array->array.count > 0) {
                                if (stmt->var.init->index.array->array.elements[0]->type == EXPR_STRING) {
                                    c_type = "const char*";
                                    is_already_const = true;
                                }
                            }
                        }
                        // Check if array element type is HashMap
                        if (strcmp(c_type, "long long") == 0 && stmt->var.init->index.array->type == EXPR_IDENT) {
                            if (stmt->var.init->index.array->expr_type && 
                                stmt->var.init->index.array->expr_type->kind == TYPE_ARRAY &&
                                stmt->var.init->index.array->expr_type->array_type.element_type &&
                                stmt->var.init->index.array->expr_type->array_type.element_type->kind == TYPE_MAP) {
                                c_type = "WynHashMap*";
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_TUPLE) {
                    // Tuple type - use __auto_type (clang only, TCC uses typeof path)
                    c_type = "__auto_type";
                    { char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                      extern void register_tuple_var(const char*); register_tuple_var(_vn); }
                } else if (stmt->var.init->type == EXPR_CALL) {
                    // Detect module constructor calls first: Module.new()
                    bool detected = false;
                    if (stmt->var.init->call.callee->type == EXPR_FIELD_ACCESS) {
                        Token mod = stmt->var.init->call.callee->field_access.object->token;
                        Token meth = stmt->var.init->call.callee->field_access.field;
                        if (meth.length == 3 && memcmp(meth.start, "new", 3) == 0) {
                            if (mod.length == 13 && memcmp(mod.start, "StringBuilder", 13) == 0) {
                                c_type = "long long"; detected = true;
                                { char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name); extern void register_sb_var(const char*); register_sb_var(_vn); }
                            } else if (mod.length == 7 && memcmp(mod.start, "HashMap", 7) == 0) {
                                c_type = "WynHashMap*"; detected = true;
                            }
                        }
                    }
                    if (!detected) {
                    // Function call - check expr_type first, then look up function return type
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_MAP:
                                c_type = "WynHashMap*";
                                break;
                            case TYPE_SET:
                                c_type = "WynHashSet*";
                                break;
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_RESULT:
                                c_type = "ResultInt";
                                break;
                            case TYPE_STRUCT: {
                                // Call resolved by the checker to a concrete struct
                                // (e.g. an Option/Result family member OptionFloat,
                                // ResultBool, …). Trust the checker's name and
                                // register the var so a later match detects the family.
                                static char call_struct_buf[256];
                                if (stmt->var.init->expr_type->struct_type.name.length > 0) {
                                    token_to_cstr(call_struct_buf, sizeof(call_struct_buf), stmt->var.init->expr_type->struct_type.name);
                                    c_type = call_struct_buf;
                                    char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                                    extern void register_enum_var(const char*, const char*);
                                    register_enum_var(_vn, call_struct_buf);
                                }
                                break;
                            }
                            case TYPE_ENUM: {
                                // Use the enum type name
                                static char enum_ret_buf2[256];
                                if (stmt->var.init->expr_type->name.length > 0) {
                                    token_to_cstr(enum_ret_buf2, sizeof(enum_ret_buf2), stmt->var.init->expr_type->name);
                                    c_type = enum_ret_buf2;
                                    // Register as enum var
                                    char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                                    extern void register_enum_var(const char*, const char*);
                                    register_enum_var(_vn, enum_ret_buf2);
                                }
                                break;
                            }
                            default: {
                                // Call returning an Option/Result family - set the
                                // concrete struct c_type AND register the var so a
                                // subsequent match detects the family. Consult the
                                // fn-return registry (covers int?/string?, Option<>,
                                // Result<>) with a fallback program scan.
                                bool _found_result = false;
                                if (stmt->var.init->call.callee->type == EXPR_IDENT) {
                                    char _cfn[128]; token_to_cstr(_cfn, sizeof(_cfn), stmt->var.init->call.callee->token);
                                    extern const char* get_function_return_type(const char*);
                                    const char* _frt = get_function_return_type(_cfn);
                                    // Accept any Option*/Result* family - including a
                                    // monomorphic OptionStruct (e.g. OptionUser) - not
                                    // just the four builtin scalar families.
                                    if (_frt && (strncmp(_frt,"Option",6)==0 || strncmp(_frt,"Result",6)==0)) {
                                        static char _orbuf[128]; snprintf(_orbuf, sizeof(_orbuf), "%s", _frt);
                                        c_type = _orbuf; _found_result = true;
                                        char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                                        extern void register_enum_var(const char*, const char*);
                                        register_enum_var(_vn, _orbuf);
                                    }
                                }
                                if (!_found_result) c_type = "__auto_type";
                            }
                        }
                    } else {
                        // Check if called function returns ResultInt or enum
                        if (stmt->var.init->call.callee->type == EXPR_IDENT && current_program) {
                            Token fn_name = stmt->var.init->call.callee->token;
                            for (int fi = 0; fi < current_program->count; fi++) {
                                Stmt* fs = current_program->stmts[fi];
                                if (fs->type == STMT_FN && fs->fn.name.length == fn_name.length &&
                                    memcmp(fs->fn.name.start, fn_name.start, fn_name.length) == 0 &&
                                    fs->fn.return_type) {
                                    if (fs->fn.return_type->type == EXPR_CALL &&
                                        fs->fn.return_type->call.callee->type == EXPR_IDENT) {
                                        Token rt = fs->fn.return_type->call.callee->token;
                                        if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) {
                                            c_type = "ResultInt";
                                            break;
                                        }
                                    } else if (fs->fn.return_type->type == EXPR_TUPLE) {
                                        // Tuple return type: use _wyn_tup_<fn_name>
                                        static char _tup_var_buf[256];
                                        snprintf(_tup_var_buf, sizeof(_tup_var_buf), "_wyn_tup_%.*s", (int)fn_name.length, fn_name.start);
                                        c_type = _tup_var_buf;
                                        break;
                                    } else if (fs->fn.return_type->type == EXPR_IDENT) {
                                    Token rt = fs->fn.return_type->token;
                                    if (rt.length == 9 && memcmp(rt.start, "ResultInt", 9) == 0) {
                                        c_type = "ResultInt";
                                    } else {
                                        // Check if it's an enum type
                                        extern int is_enum_type(const char*);
                                        char _rtn[128]; token_to_cstr(_rtn, sizeof(_rtn), rt);
                                        if (is_enum_type(_rtn)) {
                                            static char _enum_var_buf[128];
                                            snprintf(_enum_var_buf, 128, "%s", _rtn);
                                            c_type = _enum_var_buf;
                                            // Register this variable as holding an enum
                                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                                            extern void register_enum_var(const char*, const char*);
                                            register_enum_var(_vn, _rtn);
                                        }
                                    }
                                    break;
                                }
                                }
                            }
                        }
                        if (strcmp(c_type, "long long") == 0) c_type = "__auto_type";
                    }
                    }
                    // Track if this variable holds a closure (from a function returning fn type)
                    if (stmt->var.init->call.callee->type == EXPR_IDENT) {
                        Token call_name = stmt->var.init->call.callee->token;
                        if (current_program) {
                            for (int fi = 0; fi < current_program->count; fi++) {
                                Stmt* fs = current_program->stmts[fi];
                                if (fs->type == STMT_FN &&
                                    fs->fn.name.length == call_name.length &&
                                    memcmp(fs->fn.name.start, call_name.start, call_name.length) == 0 &&
                                    fs->fn.return_type && fs->fn.return_type->type == EXPR_FN_TYPE) {
                                    // This function returns a closure!
                                    ensure_lambda_var_cap();
                                    token_to_cstr(lambda_var_info[lambda_var_count].var_name, sizeof(lambda_var_info[lambda_var_count].var_name), stmt->var.name);
                                    token_to_cstr(lambda_var_info[lambda_var_count].name, sizeof(lambda_var_info[lambda_var_count].name), stmt->var.name);
                                    lambda_var_info[lambda_var_count].name_len = stmt->var.name.length;
                                    lambda_var_info[lambda_var_count].is_closure = true;
                                    lambda_var_info[lambda_var_count].capture_count = 0;
                                    lambda_var_count++;
                                    // Track for scope-exit env release (RC). Only functions
                                    // returning a closure reach here, so the var holds a
                                    // WynClosure with an RC-allocated env to reclaim.
                                    {
                                        extern void register_closure_scope_var(const char*);
                                        char _cvn[512]; token_to_cstr(_cvn, sizeof(_cvn), stmt->var.name);
                                        register_closure_scope_var(_cvn);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_BINARY) {
                    // Binary expression - check if it's string concatenation or arithmetic
                    if (stmt->var.init->binary.op.type == TOKEN_PLUS) {
                        // Check if either operand is explicitly a string literal
                        bool left_is_string = (stmt->var.init->binary.left->type == EXPR_STRING);
                        bool right_is_string = (stmt->var.init->binary.right->type == EXPR_STRING);
                        
                        if (left_is_string || right_is_string) {
                            // String concatenation
                            c_type = "const char*";
                            is_already_const = true;
                            needs_arc_management = true;
                        } else {
                            // Arithmetic - use __auto_type to infer from expression
                            c_type = "long long";
                        }
                    } else {
                        // Other binary operations (-, *, /, ==, etc.) - use __auto_type
                        c_type = "long long";
                    }
                } else if (stmt->var.init->type == EXPR_IF_EXPR) {
                    // If expression - use expr_type from checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "long long";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            default:
                                c_type = "long long";
                        }
                    }
                } else if (stmt->var.init->type == EXPR_IDENT || stmt->var.init->type == EXPR_FIELD_ACCESS) {
                    // Variable reference or field access - use expr_type from checker
                    if (stmt->var.init->expr_type) {
                        switch (stmt->var.init->expr_type->kind) {
                            case TYPE_STRING:
                                c_type = "const char*";
                                is_already_const = true;
                                break;
                            case TYPE_INT:
                                c_type = "long long";
                                break;
                            case TYPE_FLOAT:
                                c_type = "double";
                                break;
                            case TYPE_BOOL:
                                c_type = "bool";
                                break;
                            case TYPE_STRUCT: {
                                // Use struct type name (resolve type params to long long)
                                static char struct_type_buf[128];
                                Token type_name = stmt->var.init->expr_type->struct_type.name;
                                token_to_cstr(struct_type_buf, sizeof(struct_type_buf), type_name);
                                if (type_name.length == 1 && type_name.start[0] >= 'A' && type_name.start[0] <= 'Z')
                                    c_type = "long long";
                                else
                                    c_type = struct_type_buf;
                                break;
                            }
                            case TYPE_ENUM: {
                                // Use enum type name
                                static char enum_type_buf[128];
                                Token type_name = stmt->var.init->expr_type->name;
                                token_to_cstr(enum_type_buf, sizeof(enum_type_buf), type_name);
                                c_type = enum_type_buf;
                                // Register enum var for to_string dispatch
                                { char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name); extern void register_enum_var(const char*, const char*); register_enum_var(_vn, enum_type_buf); }
                                break;
                            }
                            case TYPE_ARRAY:
                                c_type = "WynArray";
                                break;
                            default:
                                c_type = "long long";
                        }
                    }
                    // Fallback: detect enum field access (Color.Red) when expr_type not set
                    if (strcmp(c_type, "long long") == 0 && stmt->var.init->type == EXPR_FIELD_ACCESS &&
                        stmt->var.init->field_access.object->type == EXPR_IDENT) {
                        char _en[128]; token_to_cstr(_en, sizeof(_en), stmt->var.init->field_access.object->token);
                        extern int is_enum_type(const char*);
                        if (is_enum_type(_en)) {
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_vn, _en);
                        }
                    }
                }
                // ... rest of type determination logic
            }
            
            // Fallback: use checker's resolved type if we still have default
            if (strcmp(c_type, "long long") == 0 && stmt->var.init && stmt->var.init->expr_type) {
                if (stmt->var.init->expr_type->kind == TYPE_BOOL) c_type = "bool";
                else if (stmt->var.init->expr_type->kind == TYPE_FLOAT) c_type = "double";
                else if (stmt->var.init->expr_type->kind == TYPE_STRING) { c_type = "const char*"; is_already_const = true; }
            }
            
            var_type_done:
            // Post-check: if type is still long long and this is a method call on a known struct,
            // look up the method return type from impl blocks
            if (strcmp(c_type, "long long") == 0 && stmt->var.init && stmt->var.init->type == EXPR_METHOD_CALL &&
                stmt->var.init->method_call.object->type == EXPR_IDENT) {
                char _pvn[64]; token_to_cstr(_pvn, sizeof(_pvn), stmt->var.init->method_call.object->token);
                extern const char* get_struct_var_type(const char*);
                const char* _psn = get_struct_var_type(_pvn);
                if (_psn) {
                    char _pmn[64]; token_to_cstr(_pmn, sizeof(_pmn), stmt->var.init->method_call.method);
                    extern const char* lookup_struct_method_return_type(const char*, const char*);
                    const char* _prt = lookup_struct_method_return_type(_psn, _pmn);
                    if (_prt) {
                        if (strcmp(_prt, "float") == 0) c_type = "double";
                        else if (strcmp(_prt, "string") == 0) { c_type = "const char*"; is_already_const = true; }
                        else if (strcmp(_prt, "bool") == 0) c_type = "bool";
                        else if (strcmp(_prt, "int") != 0) {
                            static char _post_rt[128]; snprintf(_post_rt, 128, "%s", _prt); c_type = _post_rt;
                            char _rvn2[128]; token_to_cstr(_rvn2, sizeof(_rvn2), stmt->var.name);
                            extern void register_struct_var(const char*, const char*);
                            register_struct_var(_rvn2, _prt);
                        }
                    }
                }
                // If still long long, search trait declarations for method return type
                if (strcmp(c_type, "long long") == 0 && current_program) {
                    char _tmn[64]; token_to_cstr(_tmn, sizeof(_tmn), stmt->var.init->method_call.method);
                    for (int _ti = 0; _ti < current_program->count; _ti++) {
                        if (current_program->stmts[_ti]->type == STMT_TRAIT) {
                            Stmt* _ts = current_program->stmts[_ti];
                            for (int _mi = 0; _mi < _ts->trait_decl.method_count; _mi++) {
                                FnStmt* _tm = _ts->trait_decl.methods[_mi];
                                if (_tm->name.length == (int)strlen(_tmn) && memcmp(_tm->name.start, _tmn, _tm->name.length) == 0) {
                                    if (_tm->return_type && _tm->return_type->type == EXPR_CALL &&
                                        _tm->return_type->call.callee->type == EXPR_IDENT) {
                                        Token rt = _tm->return_type->call.callee->token;
                                        if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) c_type = "ResultInt";
                                        else if (rt.length == 6 && memcmp(rt.start, "Option", 6) == 0) c_type = "OptionInt";
                                    } else if (_tm->return_type && _tm->return_type->type == EXPR_IDENT) {
                                        Token rt = _tm->return_type->token;
                                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) { c_type = "const char*"; is_already_const = true; }
                                        else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) c_type = "double";
                                    }
                                    goto var_type_done2;
                                }
                            }
                        }
                    }
                    var_type_done2: ;
                }
            }
            // Emit variable declaration - avoid double const
            // Register string variables for RC tracking
            if (strcmp(c_type, "char*") == 0 || strcmp(c_type, "const char*") == 0) {
                char _svn[256]; token_to_cstr(_svn, sizeof(_svn), stmt->var.name);
                extern void register_string_var(const char*);
                extern void register_releasable_string_var(const char*);
                register_string_var(_svn);
                register_releasable_string_var(_svn);
            }
            // Detect await_all on string spawn arrays → register result as string array
            if (stmt->var.init && stmt->var.init->type == EXPR_CALL &&
                stmt->var.init->call.callee->type == EXPR_IDENT &&
                stmt->var.init->call.callee->token.length == 9 &&
                memcmp(stmt->var.init->call.callee->token.start, "await_all", 9) == 0 &&
                stmt->var.init->call.arg_count == 1 && stmt->var.init->call.args[0]->type == EXPR_IDENT) {
                char _aav[256]; token_to_cstr(_aav, sizeof(_aav), stmt->var.init->call.args[0]->token);
                extern int is_string_spawn_array(const char*);
                if (is_string_spawn_array(_aav)) {
                    char _rv[256]; token_to_cstr(_rv, sizeof(_rv), stmt->var.name);
                    extern void register_str_array_var(const char*);
                    register_str_array_var(_rv);
                }
            }
            // Closure copy: `var g = f` where f is a known lambda variable.
            // Emit a function pointer of matching arity and carry f's captures
            // forward so calls on g inject the same captured globals.
            int _cc_src_idx = -1;
            if (stmt->var.init && stmt->var.init->type == EXPR_IDENT) {
                char _cc_src[64]; token_to_cstr(_cc_src, sizeof(_cc_src), stmt->var.init->token);
                extern int find_lambda_var(const char*);
                _cc_src_idx = find_lambda_var(_cc_src);
            }
            if (_cc_src_idx >= 0) {
                extern int is_c_name_collision(const char*);
                extern void register_user_collision(const char*);
                extern int lambda_var_param_count(int);
                char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                bool is_c_keyword = is_c_name_collision(_vn);
                if (is_c_keyword) register_user_collision(_vn);
                int nparams = lambda_var_param_count(_cc_src_idx);
                if (nparams < 0) nparams = 0;
                // Register g as a lambda var, copying the source's captures so
                // call-site injection works identically through the alias.
                {
                    ensure_lambda_var_cap();
                    LambdaVarInfo* _lvi = &lambda_var_info[lambda_var_count];
                    token_to_cstr(_lvi->var_name, sizeof(_lvi->var_name), stmt->var.name);
                    // name/name_len/is_closure are read at call sites - must be set,
                    // not left as stale garbage (an uninitialized is_closure would
                    // mis-route calls through the WynClosure path and emit garbage).
                    token_to_cstr(_lvi->name, sizeof(_lvi->name), stmt->var.name);
                    _lvi->name_len = stmt->var.name.length;
                    _lvi->is_closure = lambda_var_info[_cc_src_idx].is_closure;
                    _lvi->capture_count = lambda_var_info[_cc_src_idx].capture_count;
                    _lvi->param_count = nparams;
                    for (int i = 0; i < lambda_var_info[_cc_src_idx].capture_count; i++) {
                        strcpy(_lvi->captured_vars[i],
                               lambda_var_info[_cc_src_idx].captured_vars[i]);
                    }
                    lambda_var_count++;
                }
                if (lambda_var_info[_cc_src_idx].is_closure) {
                    // Source holds a WynClosure (e.g. returned by a fn returning
                    // fn(int)->int). The copy must be WynClosure too - the old
                    // fn-pointer emission mis-typed it as `long long (*)()` and
                    // the C compile failed. Both copies share one RC env:
                    // retain on copy, and register the copy for scope-exit
                    // release, so f and g each release exactly once (no
                    // double-free, no leak). Composes with the env-leak fix:
                    // a returned copy is unregistered at STMT_RETURN (move).
                    emit("WynClosure %s%.*s = ", is_c_keyword ? WYN_UFN_PFX : "", stmt->var.name.length, stmt->var.name.start);
                    codegen_expr(stmt->var.init);
                    emit(";\n");
                    emit("wyn_rc_retain(%s%.*s.env);\n", is_c_keyword ? WYN_UFN_PFX : "", stmt->var.name.length, stmt->var.name.start);
                    {
                        extern void register_closure_scope_var(const char*);
                        char _rvn[256]; token_to_cstr(_rvn, sizeof(_rvn), stmt->var.name);
                        register_closure_scope_var(_rvn);
                        register_local_variable(_rvn);
                    }
                    break;
                }
                emit("long long (*%s%.*s)(", is_c_keyword ? WYN_UFN_PFX : "", stmt->var.name.length, stmt->var.name.start);
                for (int i = 0; i < nparams; i++) {
                    if (i > 0) emit(", ");
                    emit("long long");
                }
                emit(") = ");
                codegen_expr(stmt->var.init);
                emit(";\n");
                {
                    char _rvn[256]; token_to_cstr(_rvn, sizeof(_rvn), stmt->var.name);
                    register_local_variable(_rvn);
                }
                break;
            }
            // Special handling for function pointers (lambdas)
            if (stmt->var.init && stmt->var.init->type == EXPR_LAMBDA) {
                // Function pointer syntax: int (*name)(params...)
                // Check if name is a C keyword
                // Check if name is a C keyword or runtime collision
                extern int is_c_name_collision(const char*);
                extern void register_user_collision(const char*);
                char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                bool is_c_keyword = is_c_name_collision(_vn);
                if (is_c_keyword) register_user_collision(_vn);
                
                // Find this lambda in the lambda_functions array to get capture count
                int total_params = stmt->var.init->lambda.param_count;
                int lambda_idx = -1;
                for (int i = 0; i < lambda_count; i++) {
                    static int lambda_var_counter = 0;
                    if (i == lambda_var_counter) {
                        // Captures use static globals, not extra params
                        lambda_idx = i;
                        lambda_var_counter++;
                        break;
                    }
                }
                
                // Store lambda variable info for call site injection
                if (lambda_idx >= 0) {
                    ensure_lambda_var_cap();
                    token_to_cstr(lambda_var_info[lambda_var_count].var_name, sizeof(lambda_var_info[lambda_var_count].var_name), stmt->var.name);
                    lambda_var_info[lambda_var_count].capture_count = lambda_functions[lambda_idx].capture_count;
                    lambda_var_info[lambda_var_count].param_count = total_params;
                    for (int i = 0; i < lambda_functions[lambda_idx].capture_count; i++) {
                        strcpy(lambda_var_info[lambda_var_count].captured_vars[i],
                               lambda_functions[lambda_idx].captured_vars[i]);
                    }
                    lambda_var_count++;
                }
                
                // S2: use checker-inferred types for the function pointer declaration
                const char* _ret_ct = "long long";
                if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_FUNCTION &&
                    stmt->var.init->expr_type->fn_type.return_type) {
                    const char* _rt = codegen_c_type_from_type(stmt->var.init->expr_type->fn_type.return_type);
                    if (_rt) _ret_ct = _rt;
                }
                emit("%s (*%s%.*s)(", _ret_ct, is_c_keyword ? WYN_UFN_PFX : "", stmt->var.name.length, stmt->var.name.start);
                for (int i = 0; i < total_params; i++) {
                    if (i > 0) emit(", ");
                    const char* _pct = "long long";
                    if (stmt->var.init->expr_type && stmt->var.init->expr_type->kind == TYPE_FUNCTION &&
                        i < stmt->var.init->expr_type->fn_type.param_count &&
                        stmt->var.init->expr_type->fn_type.param_types[i]) {
                        const char* _pt = codegen_c_type_from_type(stmt->var.init->expr_type->fn_type.param_types[i]);
                        if (_pt) _pct = _pt;
                    }
                    emit("%s", _pct);
                }
                emit(") = ");
            } else if (stmt->var.is_const && !stmt->var.is_mutable && !is_already_const) {
                char _vn[512]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                // A var whose name is a C keyword/reserved symbol (long/int/char/
                // double/…) must be emitted with the wynfn_ prefix, and registered
                // so its later uses (EXPR_IDENT) prefix too - else the C is invalid.
                { extern int is_c_name_collision(const char*); extern void register_user_collision(const char*);
                  if (is_c_name_collision(_vn)) { register_user_collision(_vn);
                    memmove(_vn + WYN_UFN_PFX_LEN, _vn, strlen(_vn) + 1); memcpy(_vn, WYN_UFN_PFX, WYN_UFN_PFX_LEN); } }
                extern int get_shadow_suffix(const char*);
                int _ss = get_shadow_suffix(_vn);
                if (_ss > 0) {
                    emit("#undef %s\n", _vn);
                    emit("const %s %s__%d = ", c_type, _vn, _ss);
                    // We'll add the #define after the initializer
                } else {
                    emit("const %s %s = ", c_type, _vn);
                }
            } else {
                char _vn[512]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                { extern int is_c_name_collision(const char*); extern void register_user_collision(const char*);
                  if (is_c_name_collision(_vn)) { register_user_collision(_vn);
                    memmove(_vn + WYN_UFN_PFX_LEN, _vn, strlen(_vn) + 1); memcpy(_vn, WYN_UFN_PFX, WYN_UFN_PFX_LEN); } }
                extern int get_shadow_suffix(const char*);
                int _ss = get_shadow_suffix(_vn);
                if (_ss > 0) {
                    emit("#undef %s\n", _vn);
                    emit("%s %s__%d = ", c_type, _vn, _ss);
                } else {
                    emit("%s %s = ", c_type, _vn);
                }
            }
            
            // Register local variable for scope tracking
            {
                char var_name[256]; token_to_cstr(var_name, sizeof(var_name), stmt->var.name);
                register_local_variable(var_name);
                // Track arrays for scope-based cleanup
                if (strcmp(c_type, "WynArray") == 0) {
                    extern void register_array_scope_var(const char*);
                    register_array_scope_var(var_name);
                }
                if (strcmp(c_type, "WynHashMap*") == 0) {
                    extern void register_hashmap_scope_var(const char*);
                    register_hashmap_scope_var(var_name);
                }
            }
            
            // Tell Some/None/Ok/Err codegen the declared target family so the
            // initializer lowers to the exact Option*/Result* type the
            // annotation names (e.g. `var o: string? = None()` -> OptionString).
            extern const char* current_assign_target_kind;
            const char* _prev_assign_kind = current_assign_target_kind;
            // Any concrete Option*/Result* family (including a monomorphic
            // OptionStruct like OptionBox) sets the target so a bare Some/None/Ok/Err
            // initializer lowers to the exact family the annotation names.
            if (strncmp(c_type, "Option", 6) == 0 || strncmp(c_type, "Result", 6) == 0)
                current_assign_target_kind = c_type;
            else
                current_assign_target_kind = NULL;
            if (needs_arc_management) {
                codegen_expr(stmt->var.init);
            } else {
                // Set spawn/int array flag for WynIntArray emission
                bool _was_int_array = codegen_emit_int_array;
                { char _vn[256]; token_to_cstr(_vn, sizeof(_vn), stmt->var.name);
                  extern int is_int_array_var(const char*);
                  if (is_spawn_array(_vn) || is_int_array_var(_vn)) codegen_emit_int_array = true; }
                codegen_expr(stmt->var.init);
                codegen_emit_int_array = _was_int_array;
            }
            current_assign_target_kind = _prev_assign_kind;
            emit(";\n");
            // RC: retain when copying a string variable (unless source is last-use → move)
            if ((strcmp(c_type, "const char*") == 0 || strcmp(c_type, "char*") == 0) &&
                stmt->var.init && stmt->var.init->type == EXPR_IDENT) {
                char _ivn[256]; token_to_cstr(_ivn, sizeof(_ivn), stmt->var.init->token);
                extern int is_string_var(const char*);
                if (is_string_var(_ivn)) {
                    // Check if source var is used after this statement
                    // Only apply move optimization for vars in the SAME scope
                    extern int var_is_live_after(Stmt**, int, int, const char*);
                    extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
                    extern int string_var_scope_depth;
                    // string_var_releasable/_count are statics from codegen.c (this file is #included there)
                    // Check if source is a top-level (releasable) var - if so, don't move
                    bool _source_is_outer = false;
                    for (int _ri = 0; _ri < string_var_releasable_count; _ri++) {
                        if (strcmp(string_var_releasable[_ri], _ivn) == 0) { _source_is_outer = true; break; }
                    }
                    if (!_source_is_outer && current_block_stmts && !var_is_live_after(current_block_stmts, current_block_count, current_stmt_idx, _ivn)) {
                        // Move: source is dead after this in same scope
                        extern void unregister_string_var(const char*);
                        unregister_string_var(_ivn);
                    } else {
                        // Copy: source is still live or from outer scope - retain
                        emit("wyn_rc_retain(%.*s);\n", stmt->var.name.length, stmt->var.name.start);
                    }
                }
            }
            // RC: a local string stored into a struct field is aliased by that
            // field, but the field holds the pointer raw (no retain) and struct
            // _cleanup does not release string fields - so structs do not own
            // their string fields. If we ALSO release the local at scope/return
            // exit and the struct (or a field of it) escapes the local's scope
            // (e.g. `var p = P{name: s}; return p.name`), the field is left
            // dangling → use-after-free (empty/garbage reads). Fix: MOVE any
            // dead local string stored into a field - drop it from the string
            // release/tracking lists so scope exit skips its release. This only
            // ever REMOVES a release, so it can never cause a double-free or
            // UAF; a still-live local (used after the struct-init in the same
            // block) is left untouched - its read happens before scope exit, so
            // it is already safe.
            if (stmt->var.init && stmt->var.init->type == EXPR_STRUCT_INIT) {
                extern int is_string_var(const char*);
                extern int var_is_live_after(Stmt**, int, int, const char*);
                extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
                extern void unregister_string_var(const char*);
                Expr* _si = stmt->var.init;
                for (int _fi = 0; _fi < _si->struct_init.field_count; _fi++) {
                    Expr* _fv = _si->struct_init.field_values[_fi];
                    if (!_fv || _fv->type != EXPR_IDENT) continue;
                    if (!_fv->expr_type || _fv->expr_type->kind != TYPE_STRING) continue;
                    char _sfn[256]; token_to_cstr(_sfn, sizeof(_sfn), _fv->token);
                    if (!is_string_var(_sfn)) continue;
                    // Move only when the local is provably dead after this
                    // statement in the current block (no later textual use).
                    if (current_block_stmts &&
                        !var_is_live_after(current_block_stmts, current_block_count, current_stmt_idx, _sfn))
                        unregister_string_var(_sfn);
                }
            }
            // RC: the same raw-store ownership issue applies to the string
            // Option/Result constructors - OptionString_Some / ResultString_Ok /
            // ResultString_Err store the pointer without retaining, and the
            // wrapper never releases it. `var o = OptionString_Some(s); return
            // OptionString_unwrap(o)` releases s at scope exit while o.value (and
            // the returned pointer) still alias it → use-after-free. MOVE a dead
            // local string argument (drop its scope-exit release). Move-only, so
            // it can only remove a UAF-causing release, never add a double-free.
            // A local string moved into a constructor that stores it raw - an
            // Option/Result (Some/Ok/Err, surface or mangled) or a user enum
            // variant (E.A(s)) - is aliased by the wrapper and released with it;
            // also releasing the local at scope exit would leave the payload
            // dangling (use-after-free / empty reads). Collect every string-var
            // payload argument and MOVE the dead ones (drop scope-exit release).
            // Move-only, so it can never introduce a double-free; still-live locals
            // are left untouched.
            {
                Expr* _init = stmt->var.init;
                Expr* _payloads[8]; int _np = 0;
                if (_init && (_init->type == EXPR_SOME || _init->type == EXPR_OK ||
                              _init->type == EXPR_ERR)) {
                    if (_init->option.value) _payloads[_np++] = _init->option.value;
                } else if (_init && _init->type == EXPR_CALL &&
                           _init->call.callee->type == EXPR_IDENT &&
                           _init->call.arg_count == 1) {
                    char _cn[64]; token_to_cstr(_cn, sizeof(_cn), _init->call.callee->token);
                    if (strcmp(_cn, "OptionString_Some") == 0 ||
                        strcmp(_cn, "ResultString_Ok") == 0 ||
                        strcmp(_cn, "ResultString_Err") == 0)
                        _payloads[_np++] = _init->call.args[0];
                } else if (_init && _init->type == EXPR_METHOD_CALL &&
                           _init->method_call.object->type == EXPR_IDENT) {
                    // User enum variant constructor: E.A(s), E.Pair(s, t)
                    char _en[128]; token_to_cstr(_en, sizeof(_en), _init->method_call.object->token);
                    extern int is_enum_type(const char*);
                    if (is_enum_type(_en))
                        for (int _ai = 0; _ai < _init->method_call.arg_count && _np < 8; _ai++)
                            _payloads[_np++] = _init->method_call.args[_ai];
                }
                extern int is_string_var(const char*);
                extern int var_is_live_after(Stmt**, int, int, const char*);
                extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
                extern void unregister_string_var(const char*);
                for (int _pi = 0; _pi < _np; _pi++) {
                    Expr* _payload = _payloads[_pi];
                    if (!_payload || _payload->type != EXPR_IDENT) continue;
                    if (!_payload->expr_type || _payload->expr_type->kind != TYPE_STRING) continue;
                    char _avn[256]; token_to_cstr(_avn, sizeof(_avn), _payload->token);
                    if (is_string_var(_avn) && current_block_stmts &&
                        !var_is_live_after(current_block_stmts, current_block_count, current_stmt_idx, _avn))
                        unregister_string_var(_avn);
                }
            }
            // Shadow define: redirect original name to suffixed version
            {
                char _svn[128]; token_to_cstr(_svn, sizeof(_svn), stmt->var.name);
                extern int get_current_shadow(const char*);
                int _sc = get_current_shadow(_svn);
                if (_sc > 0) {
                    emit("#define %s %s__%d\n", _svn, _svn, _sc);
                }
            }
            
            // Track ARC-managed variables for automatic cleanup
            if (needs_arc_management && !stmt->var.is_const) {
                track_var_with_type(stmt->var.name.start, stmt->var.name.length, c_type);
            }
            // Track float variables for method dispatch
            if (strcmp(c_type, "double") == 0) {
                char _fvn[256]; token_to_cstr(_fvn, sizeof(_fvn), stmt->var.name);
                extern void register_float_var(const char*); register_float_var(_fvn);
            }
            break;
        }
        case STMT_YIELD:
            emit("    wyn_yield("); if (stmt->yield_stmt.value) codegen_expr(stmt->yield_stmt.value); else emit("0"); emit(");\n");
            break;        case STMT_RETURN:
            // Emit deferred calls before return (LIFO order)
            {
                extern int get_defer_count(); extern Expr* get_defer(int);
                for (int _d = get_defer_count() - 1; _d >= 0; _d--) {
                    codegen_expr(get_defer(_d)); emit(";\n");
                }
            }
            // RC: a local string moved into a RETURNED enum-variant constructor
            // (e.g. `return E.A(local)`) is stored raw in the enum payload and
            // escapes with it; releasing it before the return would leave the
            // payload dangling (use-after-free / empty reads) - same class as the
            // struct-field / Some-Ok-Err ownership transfer. Unregister such a
            // dead string arg so the release below skips it (MOVE). Move-only, so
            // it can never introduce a double-free.
            if (stmt->ret.value && stmt->ret.value->type == EXPR_METHOD_CALL &&
                stmt->ret.value->method_call.object->type == EXPR_IDENT) {
                char _en[128]; token_to_cstr(_en, sizeof(_en), stmt->ret.value->method_call.object->token);
                extern int is_enum_type(const char*);
                if (is_enum_type(_en)) {
                    extern int is_string_var(const char*);
                    extern int var_is_live_after(Stmt**, int, int, const char*);
                    extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
                    extern void unregister_string_var(const char*);
                    for (int _ai = 0; _ai < stmt->ret.value->method_call.arg_count; _ai++) {
                        Expr* _arg = stmt->ret.value->method_call.args[_ai];
                        if (!_arg || _arg->type != EXPR_IDENT) continue;
                        if (!_arg->expr_type || _arg->expr_type->kind != TYPE_STRING) continue;
                        char _an[256]; token_to_cstr(_an, sizeof(_an), _arg->token);
                        if (is_string_var(_an) && current_block_stmts &&
                            !var_is_live_after(current_block_stmts, current_block_count, current_stmt_idx, _an))
                            unregister_string_var(_an);
                    }
                }
            }
            // RC: release local string variables before return - EXCEPT any
            // referenced by the return expression itself (an exact-identifier
            // exception wasn't enough: `return "<y>" + t + "</y>"` released t
            // before the concat read it - use-after-free, bug M3 2026-07-18).
            {
                extern void emit_string_releases_for_return(Expr*);
                extern int get_string_var_count(void);
                if (get_string_var_count() > 0) {
                    emit_string_releases_for_return(stmt->ret.value);
                }
            }
            // RC: release closure-env-owning locals before return, EXCEPT a
            // returned closure var (its env ownership moves to the caller).
            {
                extern void unregister_closure_scope_var(const char*);
                extern void emit_block_closure_releases(void);
                if (stmt->ret.value && stmt->ret.value->type == EXPR_IDENT) {
                    char _cret[512]; token_to_cstr(_cret, sizeof(_cret), stmt->ret.value->token);
                    unregister_closure_scope_var(_cret);  // move: don't free what we return
                }
                emit_block_closure_releases();
            }
            if (in_async_function) {
                emit("*temp = ");
                codegen_expr(stmt->ret.value);
                emit("; goto async_return;\n");
            } else if (!stmt->ret.value) {
                // Bare `return;`. If the emitted C function is non-void (e.g.
                // `long long wyn_main()` for an inferred-void main), a valueless
                // return is a C error - emit `return 0;` instead.
                extern bool current_fn_c_nonvoid;
                emit(current_fn_c_nonvoid ? "return 0;\n" : "return;\n");
            } else {
                // Auto-wrap bare returns into Option/Result constructors when the
                // current function returns an Option family type (e.g. `return x` in
                // a `fn -> int?` emits `return OptionInt_Some(x)`; `return none`
                // emits `return OptionInt_None()`).
                extern const char* current_fn_return_kind;
                if (current_fn_return_kind &&
                    strncmp(current_fn_return_kind, "Option", 6) == 0 &&
                    stmt->ret.value->type != EXPR_SOME &&
                    stmt->ret.value->type != EXPR_NONE) {
                    // `return none` → emit <Family>_None()
                    if (stmt->ret.value->type == EXPR_IDENT &&
                        stmt->ret.value->token.length == 4 &&
                        memcmp(stmt->ret.value->token.start, "none", 4) == 0) {
                        emit("return %s_None();\n", current_fn_return_kind);
                    } else {
                        // `return x` → emit <Family>_Some(x)
                        emit("return %s_Some(", current_fn_return_kind);
                        codegen_expr(stmt->ret.value);
                        emit(");\n");
                    }
                } else {
                    emit("return ");
                    codegen_expr(stmt->ret.value);
                    emit(";\n");
                }
            }
            break;
        case STMT_BREAK:
            { extern void emit_block_string_releases(void); emit_block_string_releases(); }
            emit("break;\n");
            break;
        case STMT_DEFER:
            { extern void push_defer(Expr*); push_defer(stmt->expr); }
            break;
        case STMT_CONTINUE:
            { extern void emit_block_string_releases(void); emit_block_string_releases(); }
            emit("continue;\n");
            break;
        case STMT_SPAWN: {
            // Fire-and-forget spawn: no Future, no return value
            // Uses wyn_spawn_fast for maximum throughput
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT) {
                
                Expr* call = stmt->spawn.call;
                Expr* callee = call->call.callee;
                char func_name[256]; token_to_cstr(func_name, sizeof(func_name), callee->token);
                
                int arg_count = call->call.arg_count;
                
                if (arg_count == 0) {
                    emit("wyn_spawn_fast_traced((TaskFunc)__spawn_wrapper_%s, NULL, __FILE__, __LINE__);\n", func_name);
                } else if (arg_count == 1) {
                    // Single arg: pass directly, no malloc
                    emit("wyn_spawn_fast_traced((TaskFunc)__spawn_wrapper_%s_1, (void*)(intptr_t)(", func_name);
                    codegen_expr(call->call.args[0]);
                    emit("), __FILE__, __LINE__);\n");
                } else {
                    spawn_id_counter++;
                    int sid = spawn_id_counter;
                    emit("{ struct __spawn_args_%d { ", sid);
                    for (int i = 0; i < arg_count; i++) {
                        const char* ptype = "long long";
                        if (current_program) {
                            for (int fi = 0; fi < current_program->count; fi++) {
                                Stmt* fs = current_program->stmts[fi];
                                if (fs->type == STMT_FN && strlen(func_name) == (size_t)fs->fn.name.length &&
                                    memcmp(func_name, fs->fn.name.start, fs->fn.name.length) == 0) {
                                    if (i < fs->fn.param_count && fs->fn.param_types[i]) {
                                        if (fs->fn.param_types[i]->type == EXPR_ARRAY) ptype = "WynArray";
                                        else if (fs->fn.param_types[i]->type == EXPR_IDENT) {
                                            Token pt = fs->fn.param_types[i]->token;
                                            if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) ptype = "const char*";
                                            else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) ptype = "double";
                                            else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) ptype = "bool";
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        emit("%s a%d; ", ptype, i);
                    }
                    emit("} *__sa_%d = malloc(sizeof(struct __spawn_args_%d)); ", sid, sid);
                    for (int i = 0; i < arg_count; i++) {
                        emit("__sa_%d->a%d = ", sid, i);
                        codegen_expr(call->call.args[i]);
                        emit("; ");
                    }
                    emit("wyn_spawn_fast_traced((TaskFunc)__spawn_wrapper_%s_%d, __sa_%d, __FILE__, __LINE__); }\n",
                         func_name, arg_count, sid);
                }
            } else {
                emit("/* spawn (fallback) */ ");
                codegen_expr(stmt->spawn.call);
                emit(";\n");
            }
            break;
        }
        case STMT_BLOCK:
            { extern int string_var_scope_depth; string_var_scope_depth++; }
            {
                extern int string_var_scope_depth;
                bool _is_inner_block = (string_var_scope_depth > 0);
                
                extern void push_string_scope(void);
                extern void pop_string_scope_and_release(void);
                extern void push_array_scope(void);
                extern void pop_array_scope_and_release(void);
                extern void push_hashmap_scope(void);
                extern void pop_hashmap_scope_and_release(void);
                extern void push_closure_scope(void);
                extern void pop_closure_scope_and_release(void);
                if (_is_inner_block) { push_string_scope(); push_array_scope(); push_hashmap_scope(); push_closure_scope(); }
                
                // Save shadow state for vars declared in inner blocks
                extern int get_current_shadow(const char*);
                extern void set_shadow_count(const char*, int);
                extern void remove_shadow_entry(const char*);
                typedef struct { char name[64]; int prev; } _ShSave;
                _ShSave _shsaves[64];
                int _shcount = 0;
                if (_is_inner_block) {
                    for (int _i = 0; _i < stmt->block.count && _shcount < 64; _i++) {
                        if (stmt->block.stmts[_i]->type == STMT_VAR) {
                            token_to_cstr(_shsaves[_shcount].name, sizeof(_shsaves[_shcount].name), stmt->block.stmts[_i]->var.name);
                            _shsaves[_shcount].prev = get_current_shadow(_shsaves[_shcount].name);
                            _shcount++;
                        }
                    }
                }
                
                extern Stmt** current_block_stmts; extern int current_block_count; extern int current_stmt_idx;
                Stmt** _saved_stmts = current_block_stmts; int _saved_count = current_block_count; int _saved_idx = current_stmt_idx;
                current_block_stmts = stmt->block.stmts; current_block_count = stmt->block.count;
                for (int i = 0; i < stmt->block.count; i++) {
                    current_stmt_idx = i;
                    emit("    ");
                    codegen_stmt(stmt->block.stmts[i]);
                }
                current_block_stmts = _saved_stmts; current_block_count = _saved_count; current_stmt_idx = _saved_idx;
                
                // String cleanup first (needs macros still defined)
                if (_is_inner_block) { pop_string_scope_and_release(); pop_array_scope_and_release(); pop_hashmap_scope_and_release(); pop_closure_scope_and_release(); }
                
                // Then restore shadow state for shadowed variables
                if (_is_inner_block) {
                    for (int _i = 0; _i < _shcount; _i++) {
                        if (_shsaves[_i].prev < 0) continue;  // New var, no restore needed
                        int cur = get_current_shadow(_shsaves[_i].name);
                        if (cur > _shsaves[_i].prev) {
                            emit("\n#undef %s\n", _shsaves[_i].name);
                            if (_shsaves[_i].prev > 0) {
                                emit("#define %s %s__%d\n", _shsaves[_i].name, _shsaves[_i].name, _shsaves[_i].prev);
                            }
                            // Restore scope level (next counter stays high for unique C names)
                            set_shadow_count(_shsaves[_i].name, _shsaves[_i].prev);
                        }
                    }
                }
            }
            { extern int string_var_scope_depth; string_var_scope_depth--; }
            break;
        case STMT_PARALLEL: {
            // Structured concurrency. Every `x = spawn f(...)` inside runs
            // concurrently; all spawned tasks are joined at the end of the
            // block, so none can outlive it. `x` holds the VALUE after the
            // block (not a Future) and stays visible in the enclosing scope.
            //
            // Lowering: each spawn-bound var is declared with its value type,
            // its Future captured in a hidden temp, and joined via future_get
            // after every statement in the block has run.
            extern const char* get_function_return_type(const char*);
            static int parallel_block_counter = 0;
            int par_id = parallel_block_counter++;
            emit("/* parallel */\n");

            // First pass: for spawn-bound vars, declare the value variable and
            // spawn into a hidden future temp.
            char joined_names[64][128];
            char joined_futs[64][160];
            const char* joined_ctypes[64];
            int joined_count = 0;

            for (int i = 0; i < stmt->block.count; i++) {
                Stmt* s = stmt->block.stmts[i];
                bool is_spawn_var = (s->type == STMT_VAR && s->var.init &&
                                     s->var.init->type == EXPR_SPAWN);
                // An UNBOUND spawn - bare `spawn f()` inside the block (its own
                // STMT_SPAWN) - must ALSO be joined at the closing brace, else it
                // escapes the structured-concurrency barrier (it would lower to a
                // fire-and-forget wyn_spawn_fast_traced and could outlive the block).
                // Wrap its call in an EXPR_SPAWN and reuse the joinable expression
                // lowering, capturing the future with no value var (empty name →
                // joined for the barrier only).
                if (s->type == STMT_SPAWN && s->spawn.call && joined_count < 64) {
                    Expr spawn_expr; spawn_expr.type = EXPR_SPAWN;
                    spawn_expr.spawn.call = s->spawn.call;
                    spawn_expr._codegen_temp_id = -1;
                    snprintf(joined_names[joined_count], 128, "%s", "");   // no binding
                    snprintf(joined_futs[joined_count], 160, "__par_fut_%d_%d", par_id, joined_count);
                    joined_ctypes[joined_count] = "long long";
                    emit("    Future* %s = ", joined_futs[joined_count]);
                    codegen_expr(&spawn_expr);   // emits a joinable wyn_spawn_*(...)
                    emit(";\n");
                    joined_count++;
                    continue;
                }
                if (is_spawn_var && joined_count < 64) {
                    // Resolve the value C type from the spawned fn's return type.
                    const char* vctype = "long long";
                    Expr* call = s->var.init->spawn.call;
                    if (call && call->type == EXPR_CALL &&
                        call->call.callee->type == EXPR_IDENT) {
                        char fn[256]; token_to_cstr(fn, sizeof(fn), call->call.callee->token);
                        const char* rt = get_function_return_type(fn);
                        if (rt) {
                            if (strcmp(rt, "string") == 0) vctype = "const char*";
                            else if (strcmp(rt, "float") == 0) vctype = "double";
                            else if (strcmp(rt, "bool") == 0) vctype = "bool";
                            else if (strcmp(rt, "int") == 0) vctype = "long long";
                        }
                    }
                    char vn[128]; token_to_cstr(vn, sizeof(vn), s->var.name);
                    snprintf(joined_names[joined_count], 128, "%s", vn);
                    snprintf(joined_futs[joined_count], 160, "__par_fut_%d_%d", par_id, joined_count);
                    joined_ctypes[joined_count] = vctype;
                    // Declare value var + spawn into hidden future.
                    emit("    %s %s;\n", vctype, vn);
                    emit("    Future* %s = ", joined_futs[joined_count]);
                    codegen_expr(s->var.init);   // emits wyn_spawn_*(...)
                    emit(";\n");
                    joined_count++;
                } else {
                    emit("    ");
                    codegen_stmt(s);
                }
            }

            // Join all spawned tasks before leaving the block. With a timeout,
            // each join waits at most N ms (future_get_timeout returns NULL/0
            // past the deadline), so a stuck task can't hang the block.
            int has_timeout = (stmt->block.timeout != NULL);
            if (has_timeout) {
                emit("    int __par_to_%d = (int)(", par_id);
                codegen_expr(stmt->block.timeout);
                emit(");\n");
            }
            for (int j = 0; j < joined_count; j++) {
                char getcall[192];
                if (has_timeout)
                    snprintf(getcall, sizeof(getcall), "future_get_timeout(%s, __par_to_%d)", joined_futs[j], par_id);
                else
                    // Parallel-block futures are hidden temps with one reader -
                    // consume so the slot recycles.
                    snprintf(getcall, sizeof(getcall), "future_get_consume(%s)", joined_futs[j]);
                if (joined_names[j][0] == '\0')
                    // Unbound spawn: join for the barrier, discard the value.
                    emit("    (void)%s;\n", getcall);
                else if (strcmp(joined_ctypes[j], "const char*") == 0)
                    emit("    %s = (const char*)(intptr_t)%s;\n", joined_names[j], getcall);
                else if (strcmp(joined_ctypes[j], "double") == 0)
                    emit("    { long long __r = (long long)(intptr_t)%s; %s = *(double*)&__r; }\n", getcall, joined_names[j]);
                else
                    emit("    %s = (%s)(intptr_t)%s;\n", joined_names[j], joined_ctypes[j], getcall);
            }
            break;
        }
        case STMT_SELECT: {
            // select { v = ch.recv() => body ... } lowers to Task_select_N over
            // the arm channels, then dispatches to the ready arm, receiving from
            // its channel and binding the value.
            int n = stmt->select_stmt.arm_count;
            static int select_ctr = 0;
            int sid = select_ctr++;
            if (n == 0) { break; }
            // Task_select_n handles every arity. The old n==1 / n>3 fallback
            // hardcoded __sel = 0, unconditionally dispatching arm 0 - a
            // 4-arm select where only channel 4 had data blocked forever in
            // Task_recv on empty channel 1.
            emit("{ long long __selch_%d[%d] = { ", sid, n);
            for (int i = 0; i < n; i++) {
                if (i) emit(", ");
                codegen_expr(stmt->select_stmt.channels[i]);
            }
            emit(" };\n");
            emit("  long long __sel_%d = Task_select_n(__selch_%d, %d);\n", sid, sid, n);
            for (int i = 0; i < n; i++) {
                emit("    %s if (__sel_%d == %d) {\n", i ? "else" : "", sid, i);
                // Bind the received value and run the body.
                char vn[128]; token_to_cstr(vn, sizeof(vn), stmt->select_stmt.bind_names[i]);
                emit("        long long %s = Task_recv(", vn);
                codegen_expr(stmt->select_stmt.channels[i]);
                emit(");\n        ");
                codegen_stmt(stmt->select_stmt.bodies[i]);
                emit("    }\n");
            }
            emit("}\n");
            break;
        }
        case STMT_FN: {
            // Determine return type
            { extern void reset_defers(); reset_defers(); }
            extern bool codegen_fn_returns_array;
            bool _prev_fn_returns_array = codegen_fn_returns_array;
            codegen_fn_returns_array = (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_ARRAY);
            const char* return_type = stmt->fn.return_type ? "long long" : "void"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = stmt->fn.is_async;
            (void)is_async;
            
            // main() always returns long long, even without -> int
            bool is_main_fn = (stmt->fn.name.length == 4 && memcmp(stmt->fn.name.start, "main", 4) == 0);
            if (is_main_fn) return_type = "long long";
            
            if (stmt->fn.return_type) {
                if (stmt->fn.return_type->type == EXPR_CALL) {
                    // Generic type instantiation: HashMap<K,V>, Option<T>, Result<T,E>
                    if (stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = stmt->fn.return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            return_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            return_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            if (stmt->fn.return_type->call.arg_count > 0 &&
                                stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = stmt->fn.return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "OptionString";
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    return_type = "OptionFloat";
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    return_type = "OptionBool";
                                else return_type = "OptionInt";
                            } else return_type = "OptionInt";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            if (stmt->fn.return_type->call.arg_count > 0 &&
                                stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = stmt->fn.return_type->call.args[0]->token;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "ResultString";
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    return_type = "ResultFloat";
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    return_type = "ResultBool";
                                else return_type = "ResultInt";
                            } else return_type = "ResultInt";
                        }
                    }
                } else if (stmt->fn.return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
                } else if (stmt->fn.return_type->type == EXPR_TUPLE) {
                    // Tuple return type - typedef already emitted by forward declaration
                    // Use the same name: _wyn_tup_<fn_name>
                    static char _trt2[256];
                    snprintf(_trt2, sizeof(_trt2), "_wyn_tup_%.*s", stmt->fn.name.length, stmt->fn.name.start);
                    return_type = _trt2;
                } else if (stmt->fn.return_type->type == EXPR_IDENT) {
                    Token type_name = stmt->fn.return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        return_type = "long long";
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        return_type = "char*";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                        return_type = "WynArray";
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                        return_type = "WynHashMap*";
                    } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                        return_type = "WynHashSet*";
                    } else {
                        // Assume it's a custom struct type
                        token_to_cstr(return_type_buf, sizeof(return_type_buf), type_name);
                        return_type = return_type_buf;
                    }
                } else if (stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                    // Map TYPE? to the concrete optional type
                    Expr* inner = stmt->fn.return_type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT) {
                        Token t = inner->token;
                        if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return_type = "OptionInt";
                        else if (t.length == 6 && memcmp(t.start, "string", 6) == 0) return_type = "OptionString";
                        else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) return_type = "OptionFloat";
                        else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) return_type = "OptionBool";
                        else {
                            // `-> Struct?` -> the monomorphic Option<Struct> family.
                            char _stn[96]; token_to_cstr(_stn, sizeof(_stn), t);
                            extern int is_known_struct(const char*);
                            if (is_known_struct(_stn)) {
                                static char _ostrt[128];
                                snprintf(_ostrt, sizeof(_ostrt), "Option%s", _stn);
                                return_type = _ostrt;
                            } else return_type = "WynOptional*";
                        }
                    } else {
                        return_type = "WynOptional*";
                    }
                } else if (stmt->fn.return_type->type == EXPR_FN_TYPE) {
                    return_type = "WynClosure";
                }
            }
            
            // L3: Generator detection
            extern int fn_is_generator(Stmt*);
            bool is_generator = fn_is_generator(stmt);
            static int _gen_emitting = 0; // prevent re-entry
            if (is_generator && !_gen_emitting) {
                return_type = "WynIter*";
                char gn[256]; token_to_cstr(gn, sizeof(gn), stmt->fn.name);
                // Emit coroutine body function BEFORE the wrapper
                _gen_emitting = 1;
                emit("static void _gen_body_%s(void* _arg) {\n", gn);
                if (stmt->fn.param_count > 0) {
                    emit("    struct { ");
                    for (int i = 0; i < stmt->fn.param_count; i++) {
                        const char* pt = "long long";
                        if (stmt->fn.param_types[i] && stmt->fn.param_types[i]->type == EXPR_IDENT) {
                            Token t = stmt->fn.param_types[i]->token;
                            if (t.length == 6 && memcmp(t.start, "string", 6) == 0) pt = "char*";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) pt = "double";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) pt = "bool";
                        }
                        emit("%s _p%d; ", pt, i);
                    }
                    emit("} *_a = _arg;\n");
                    for (int i = 0; i < stmt->fn.param_count; i++) {
                        const char* pt = "long long";
                        if (stmt->fn.param_types[i] && stmt->fn.param_types[i]->type == EXPR_IDENT) {
                            Token t = stmt->fn.param_types[i]->token;
                            if (t.length == 6 && memcmp(t.start, "string", 6) == 0) pt = "char*";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) pt = "double";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) pt = "bool";
                        }
                        emit("    %s %.*s = _a->_p%d;\n", pt, stmt->fn.params[i].length, stmt->fn.params[i].start, i);
                    }
                    emit("    free(_arg);\n");
                }
                if (stmt->fn.body) codegen_stmt(stmt->fn.body);
                emit("}\n\n");
                _gen_emitting = 0;
            } else if (is_generator) {
                return_type = "WynIter*";
            }
            
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (stmt->fn.name.length == 4 && 
                                   memcmp(stmt->fn.name.start, "main", 4) == 0);
            
            // Function signature
            // Optimization: inline hint for small non-main functions
            // BUT: don't inline functions used in spawn (wrapper calls them from worker threads)
            {
                int _bsc = 0;
                if (stmt->fn.body && stmt->fn.body->type == STMT_BLOCK) _bsc = stmt->fn.body->block.count;
                bool _is_spawned = false;
                char _fn[256]; token_to_cstr(_fn, sizeof(_fn), stmt->fn.name);
                for (int _si = 0; _si < spawn_wrapper_count; _si++) {
                    if (strcmp(spawn_wrappers[_si].func_name, _fn) == 0) { _is_spawned = true; break; }
                }
                if (!is_main_fn && !_is_spawned && _bsc > 0 && _bsc <= 5) emit("__attribute__((hot)) static inline ");
                else if (!is_main_fn) emit("__attribute__((hot)) ");
            }
            
            if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (stmt->fn.is_extension) {
                // Extension method: fn Type.method() -> Type_method()
                emit("%s %.*s_%.*s(", return_type, 
                     stmt->fn.receiver_type.length, stmt->fn.receiver_type.start,
                     stmt->fn.name.length, stmt->fn.name.start);
            } else {
                // Check for C keyword collision
                char _fn_name[256]; token_to_cstr(_fn_name, sizeof(_fn_name), stmt->fn.name);
                extern int is_c_name_collision(const char*);
                extern void register_user_collision(const char*);
                bool _is_ckw = is_c_name_collision(_fn_name);
                if (_is_ckw) register_user_collision(_fn_name);
                emit("%s %s%.*s(", return_type, _is_ckw ? WYN_UFN_PFX : "", stmt->fn.name.length, stmt->fn.name.start);
            }
            for (int i = 0; i < stmt->fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "long long"; // default
                char custom_type_buf[256] = {0};  // Buffer for custom types
                
                // FIX: For extension methods, first parameter (self) gets receiver type
                if (stmt->fn.is_extension && i == 0) {
                    token_to_cstr(custom_type_buf, sizeof(custom_type_buf), stmt->fn.receiver_type);
                    param_type = custom_type_buf;
                } else if (stmt->fn.param_types[i]) {
                    if (stmt->fn.param_types[i]->type == EXPR_FN_TYPE) {
                        // Function type: fn(T) -> R becomes function pointer
                        FnTypeExpr* fn_type = &stmt->fn.param_types[i]->fn_type;
                        
                        // Build return type
                        const char* ret_type = "int";
                        if (fn_type->return_type && fn_type->return_type->type == EXPR_IDENT) {
                            Token rt = fn_type->return_type->token;
                            if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) ret_type = "long long";
                            else if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret_type = "char*";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret_type = "double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) ret_type = "bool";
                        }
                        
                        // Build parameter types
                        char params_buf[256] = "";
                        int pb_len = 0;
                        for (int j = 0; j < fn_type->param_count; j++) {
                            if (j > 0) { memcpy(params_buf + pb_len, ", ", 2); pb_len += 2; }
                            const char* pt = "long long";
                            if (fn_type->param_types[j] && fn_type->param_types[j]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[j]->token;
                                if (pt_tok.length == 3 && memcmp(pt_tok.start, "int", 3) == 0) pt = "long long";
                                else if (pt_tok.length == 6 && memcmp(pt_tok.start, "string", 6) == 0) pt = "char*";
                                else if (pt_tok.length == 5 && memcmp(pt_tok.start, "float", 5) == 0) pt = "double";
                                else if (pt_tok.length == 4 && memcmp(pt_tok.start, "bool", 4) == 0) pt = "bool";
                            }
                            int ptl = strlen(pt);
                            memcpy(params_buf + pb_len, pt, ptl); pb_len += ptl;
                            params_buf[pb_len] = '\0';
                        }
                        
                        // Generate function pointer type: ret_type (*param_name)(params)
                        snprintf(custom_type_buf, sizeof(custom_type_buf), "%s (*", ret_type);
                        param_type = custom_type_buf;
                        emit("%s", param_type);
                        emit("%.*s)(", stmt->fn.params[i].length, stmt->fn.params[i].start);
                        emit("%s", params_buf);
                        emit(")");
                        continue; // Skip the normal emit below
                    } else if (stmt->fn.param_types[i]->type == EXPR_IDENT) {
                        Token type_name = stmt->fn.param_types[i]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "long long";
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "array", 5) == 0) {
                            param_type = "WynArray";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            param_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            param_type = "WynHashSet*";
                        } else if (type_name.length == 3 && memcmp(type_name.start, "ptr", 3) == 0) {
                            param_type = "void*";   // FFI opaque pointer through a user fn
                        } else if (type_name.length == 4 && memcmp(type_name.start, "cstr", 4) == 0) {
                            param_type = "char*";   // raw C string
                        } else {
                            // Assume it's a custom struct type
                            token_to_cstr(custom_type_buf, sizeof(custom_type_buf), type_name);
                            param_type = custom_type_buf;
                        }
                    } else if (stmt->fn.param_types[i]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (stmt->fn.param_types[i]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Optional param. int?/string?/…/Struct? map to the
                        // concrete Option family; otherwise the generic WynOptional*.
                        Expr* _inr = stmt->fn.param_types[i]->optional_type.inner_type;
                        static char _opbuf2[128];
                        param_type = "WynOptional*";
                        if (_inr && _inr->type == EXPR_IDENT) {
                            Token t = _inr->token;
                            if (t.length == 3 && memcmp(t.start, "int", 3) == 0) param_type = "OptionInt";
                            else if (t.length == 6 && memcmp(t.start, "string", 6) == 0) param_type = "OptionString";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) param_type = "OptionFloat";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) param_type = "OptionBool";
                            else {
                                char _stn[96]; token_to_cstr(_stn, sizeof(_stn), t);
                                extern int is_known_struct(const char*);
                                if (is_known_struct(_stn)) { snprintf(_opbuf2, sizeof(_opbuf2), "Option%s", _stn); param_type = _opbuf2; }
                            }
                        }
                        // Register the param as an Option-family var so a match on it
                        // detects the family (mirrors how return-typed vars register).
                        if (strncmp(param_type, "Option", 6) == 0) {
                            char _pn[128]; token_to_cstr(_pn, sizeof(_pn), stmt->fn.params[i]);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_pn, param_type);
                        }
                    }
                }

                // Emit with pointer for mut params
                bool is_mut_p = stmt->fn.param_mutable && stmt->fn.param_mutable[i];
                char _pn[256]; token_to_cstr(_pn, sizeof(_pn), stmt->fn.params[i]);
                static const char* _c_kw[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                bool _ipk = false; for (int _k = 0; _c_kw[_k]; _k++) { if (strcmp(_pn, _c_kw[_k]) == 0) { _ipk = true; break; } }
                if (is_mut_p) {
                    emit("%s *%s%.*s", param_type, _ipk ? "_" : "", stmt->fn.params[i].length, stmt->fn.params[i].start);
                } else {
                    emit("%s %s%.*s", param_type, _ipk ? "_" : "", stmt->fn.params[i].length, stmt->fn.params[i].start);
                }
            }
            emit(") {\n");
            push_scope();  // Track allocations in this function
            
            // Register parameters for mut tracking
            clear_parameters();
            clear_local_variables();
            { extern void reset_shadow_vars(void); reset_shadow_vars(); }
            { extern void reset_string_vars(void); reset_string_vars(); }
            { extern void reset_array_scope(void); reset_array_scope(); } { extern void reset_hashmap_scope(void); reset_hashmap_scope(); } { extern void reset_closure_scope(void); reset_closure_scope(); }
            for (int i = 0; i < stmt->fn.param_count; i++) {
                char pname[256]; token_to_cstr(pname, sizeof(pname), stmt->fn.params[i]);
                bool is_mut_p = stmt->fn.param_mutable && stmt->fn.param_mutable[i];
                // Extract type name for trait dispatch
                const char* ptype = NULL;
                char ptbuf[64] = {0};
                if (stmt->fn.param_types[i] && stmt->fn.param_types[i]->type == EXPR_IDENT) {
                    token_to_cstr(ptbuf, sizeof(ptbuf), stmt->fn.param_types[i]->token);
                    ptype = ptbuf;
                }
                register_parameter_typed(pname, ptype, is_mut_p);
                // Track trait-typed params for call-site wrapping
                if (ptype && is_trait_type(ptype, strlen(ptype))) {
                    char fn_name_buf[128]; token_to_cstr(fn_name_buf, sizeof(fn_name_buf), stmt->fn.name);
                    register_fn_trait_param(fn_name_buf, ptype, i);
                }
            }
            
            // Set current function return kind for Ok/Err/Some/None resolution
            const char* prev_fn_return_kind = current_fn_return_kind;
            current_fn_return_kind = NULL;

            // Track whether this function's emitted C signature is non-void, so a
            // bare `return;` in its body lowers to `return 0;` (see STMT_RETURN).
            extern bool current_fn_c_nonvoid;
            bool prev_fn_c_nonvoid = current_fn_c_nonvoid;
            {
                const char* _rt_eff = (return_type_buf[0] != '\0') ? return_type_buf : return_type;
                current_fn_c_nonvoid = (strcmp(_rt_eff, "void") != 0);
            }
            if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_CALL &&
                stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
                Token rt = stmt->fn.return_type->call.callee->token;
                if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) {
                    // Result<T, E> - check T to determine ResultInt or ResultString
                    if (stmt->fn.return_type->call.arg_count > 0 &&
                        stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                        Token inner = stmt->fn.return_type->call.args[0]->token;
                        if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                            current_fn_return_kind = "ResultString";
                        else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                            current_fn_return_kind = "ResultFloat";
                        else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                            current_fn_return_kind = "ResultBool";
                        else
                            current_fn_return_kind = "ResultInt";
                    }
                } else if (rt.length == 6 && memcmp(rt.start, "Option", 6) == 0) {
                    if (stmt->fn.return_type->call.arg_count > 0 &&
                        stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                        Token inner = stmt->fn.return_type->call.args[0]->token;
                        if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                            current_fn_return_kind = "OptionString";
                        else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                            current_fn_return_kind = "OptionFloat";
                        else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                            current_fn_return_kind = "OptionBool";
                        else
                            current_fn_return_kind = "OptionInt";
                    }
                }
            } else if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_IDENT) {
                Token rt = stmt->fn.return_type->token;
                if (rt.length == 9 && memcmp(rt.start, "ResultInt", 9) == 0)
                    current_fn_return_kind = "ResultInt";
                else if (rt.length == 12 && memcmp(rt.start, "ResultString", 12) == 0)
                    current_fn_return_kind = "ResultString";
                else if (rt.length == 11 && memcmp(rt.start, "ResultFloat", 11) == 0)
                    current_fn_return_kind = "ResultFloat";
                else if (rt.length == 10 && memcmp(rt.start, "ResultBool", 10) == 0)
                    current_fn_return_kind = "ResultBool";
                else if (rt.length == 9 && memcmp(rt.start, "OptionInt", 9) == 0)
                    current_fn_return_kind = "OptionInt";
                else if (rt.length == 12 && memcmp(rt.start, "OptionString", 12) == 0)
                    current_fn_return_kind = "OptionString";
                else if (rt.length == 11 && memcmp(rt.start, "OptionFloat", 11) == 0)
                    current_fn_return_kind = "OptionFloat";
                else if (rt.length == 10 && memcmp(rt.start, "OptionBool", 10) == 0)
                    current_fn_return_kind = "OptionBool";
            } else if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                // `-> int?` / `-> string?` sugar → Option family, so Some/None
                // in the body resolve and the C return type is the Option struct.
                Expr* inner = stmt->fn.return_type->optional_type.inner_type;
                if (inner && inner->type == EXPR_IDENT && inner->token.length == 6 &&
                    memcmp(inner->token.start, "string", 6) == 0)
                    current_fn_return_kind = "OptionString";
                else if (inner && inner->type == EXPR_IDENT && inner->token.length == 5 &&
                    memcmp(inner->token.start, "float", 5) == 0)
                    current_fn_return_kind = "OptionFloat";
                else if (inner && inner->type == EXPR_IDENT && inner->token.length == 4 &&
                    memcmp(inner->token.start, "bool", 4) == 0)
                    current_fn_return_kind = "OptionBool";
                else if (inner && inner->type == EXPR_IDENT) {
                    // `-> Struct?` -> Option<Struct> family so body Some/None resolve.
                    char _stn[96]; token_to_cstr(_stn, sizeof(_stn), inner->token);
                    extern int is_known_struct(const char*);
                    if (is_known_struct(_stn)) {
                        static char _osrk[128];
                        snprintf(_osrk, sizeof(_osrk), "Option%s", _stn);
                        current_fn_return_kind = _osrk;
                    } else current_fn_return_kind = "OptionInt";
                } else
                    current_fn_return_kind = "OptionInt";
            } else if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_TUPLE) {
                static char _tup_rk[128];
                snprintf(_tup_rk, sizeof(_tup_rk), "_wyn_tup_%.*s", stmt->fn.name.length, stmt->fn.name.start);
                current_fn_return_kind = _tup_rk;
            }

            // Function body
            if (is_generator) {
                // L3: Generator wrapper - allocate args, create iterator, return
                char gn[256]; token_to_cstr(gn, sizeof(gn), stmt->fn.name);
                if (stmt->fn.param_count > 0) {
                    emit("    typedef struct { ");
                    for (int i = 0; i < stmt->fn.param_count; i++) {
                        const char* pt = "long long";
                        if (stmt->fn.param_types[i] && stmt->fn.param_types[i]->type == EXPR_IDENT) {
                            Token t = stmt->fn.param_types[i]->token;
                            if (t.length == 6 && memcmp(t.start, "string", 6) == 0) pt = "char*";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) pt = "double";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) pt = "bool";
                        }
                        emit("%s _p%d; ", pt, i);
                    }
                    emit("} _gen_args_%s;\n", gn);
                    emit("    _gen_args_%s* _a = malloc(sizeof(_gen_args_%s));\n", gn, gn);
                    for (int i = 0; i < stmt->fn.param_count; i++)
                        emit("    _a->_p%d = %.*s;\n", i, stmt->fn.params[i].length, stmt->fn.params[i].start);
                    emit("    return wyn_iter_create(_gen_body_%s, _a);\n", gn);
                } else {
                    emit("    return wyn_iter_create(_gen_body_%s, NULL);\n", gn);
                }
                pop_scope(); current_fn_return_kind = prev_fn_return_kind;
                current_fn_c_nonvoid = prev_fn_c_nonvoid;
                emit("}\n\n"); break;
            }
            // TCO: detect tail-recursive calls and convert to goto loop
            bool _is_tco = false;
            char _tco_fn_name[256] = {0};
            if (stmt->fn.body && stmt->fn.body->type == STMT_BLOCK && stmt->fn.body->block.count > 0) {
                Stmt* last = stmt->fn.body->block.stmts[stmt->fn.body->block.count - 1];
                // Check: last stmt is return + call to self
                if (last->type == STMT_RETURN && last->ret.value &&
                    last->ret.value->type == EXPR_CALL &&
                    last->ret.value->call.callee->type == EXPR_IDENT) {
                    Token callee = last->ret.value->call.callee->token;
                    if (callee.length == stmt->fn.name.length &&
                        memcmp(callee.start, stmt->fn.name.start, callee.length) == 0) {
                        _is_tco = true;
                        token_to_cstr(_tco_fn_name, sizeof(_tco_fn_name), stmt->fn.name);
                    }
                }
                // Also check: last stmt is if/else where both branches return self
                if (!_is_tco && last->type == STMT_IF) {
                    // Check the else branch for tail call (common pattern: if base { return val } return self(...))
                    // This is handled by the simpler case above when the last stmt IS the return
                }
            }
            // Register enum-typed parameters so `match param { ... }` in the body
            // recognizes it as a data enum (drives correct tag-based lowering).
            // Params aren't otherwise tracked in codegen's enum-var map.
            for (int pi = 0; pi < stmt->fn.param_count; pi++) {
                if (stmt->fn.param_types[pi] && stmt->fn.param_types[pi]->type == EXPR_IDENT) {
                    char _pt[128]; token_to_cstr(_pt, sizeof(_pt), stmt->fn.param_types[pi]->token);
                    extern int is_enum_type(const char*);
                    if (is_enum_type(_pt)) {
                        char _pn[128]; token_to_cstr(_pn, sizeof(_pn), stmt->fn.params[pi]);
                        extern void register_enum_var(const char*, const char*);
                        register_enum_var(_pn, _pt);
                    }
                }
            }

            if (_is_tco) emit("    __tco_start: ;\n");
            {
                // If TCO, emit all statements except the last (which we'll convert)
                if (_is_tco && stmt->fn.body->type == STMT_BLOCK) {
                    for (int _s = 0; _s < stmt->fn.body->block.count - 1; _s++) {
                        codegen_stmt(stmt->fn.body->block.stmts[_s]);
                    }
                    // Emit the tail call as parameter reassignment + goto
                    Stmt* last = stmt->fn.body->block.stmts[stmt->fn.body->block.count - 1];
                    Expr* call = last->ret.value;
                    // Assign new values to temps first (avoid order-dependent issues)
                    for (int _a = 0; _a < call->call.arg_count && _a < stmt->fn.param_count; _a++) {
                        emit("    __auto_type __tco_%d = ", _a);
                        codegen_expr(call->call.args[_a]);
                        emit(";\n");
                    }
                    for (int _a = 0; _a < call->call.arg_count && _a < stmt->fn.param_count; _a++) {
                        emit("    %.*s = __tco_%d;\n", stmt->fn.params[_a].length, stmt->fn.params[_a].start, _a);
                    }
                    emit("    goto __tco_start;\n");
                } else {
                    codegen_stmt(stmt->fn.body);
                }
                // Emit deferred calls at function end (LIFO)
                {
                    extern int get_defer_count(); extern Expr* get_defer(int);
                    for (int _d = get_defer_count() - 1; _d >= 0; _d--) {
                        emit("    "); codegen_expr(get_defer(_d)); emit(";\n");
                    }
                }
                // Auto-insert return 0 at end of main if not already there
                bool is_main = (stmt->fn.name.length == 4 && 
                               memcmp(stmt->fn.name.start, "main", 4) == 0);
                if (is_main) {
                    // Check if last statement is already a return
                    bool has_return = false;
                    if (stmt->fn.body && stmt->fn.body->type == STMT_BLOCK && stmt->fn.body->block.count > 0) {
                        Stmt* last = stmt->fn.body->block.stmts[stmt->fn.body->block.count - 1];
                        if (last && last->type == STMT_RETURN) has_return = true;
                    }
                    if (!has_return) emit("    return 0;\n");
                }
            }
            
            pop_scope();   // Auto-cleanup before function end
            current_fn_return_kind = prev_fn_return_kind;
            current_fn_c_nonvoid = prev_fn_c_nonvoid;
            codegen_fn_returns_array = _prev_fn_returns_array;
            emit("}\n\n");
            break;
        }
        case STMT_EXTERN: {
            // If the symbol is already declared by a header the generated C
            // includes (libc via wyn_runtime.h's stdio/stdlib/etc., or a Wyn
            // runtime function), emitting our own prototype would conflict
            // ("conflicting types for 'printf'"). is_c_name_collision already
            // tracks that set. In that case skip the prototype entirely and call
            // the existing declaration - the checker still has the user-declared
            // signature for type-checking the call.
            {
                char _en[256]; token_to_cstr(_en, sizeof(_en), stmt->extern_fn.name);
                extern int is_c_name_collision(const char*);
                extern int is_std_header_symbol(const char*);
                if (is_c_name_collision(_en) || is_std_header_symbol(_en)) break;
            }
            // Generate the C prototype for an `extern fn`. The type map mirrors the
            // checker's extern_map_type: int->long long, float->double, bool->bool,
            // string->const char* (char* for a return), void->void; anything else is
            // treated as an opaque machine word (long long / void* for pointers).
            emit("extern %s %.*s(", extern_c_type(stmt->extern_fn.return_type, true),
                 stmt->extern_fn.name.length, stmt->extern_fn.name.start);

            for (int i = 0; i < stmt->extern_fn.param_count; i++) {
                if (i > 0) emit(", ");
                emit("%s", extern_c_type(stmt->extern_fn.param_types[i], false));
            }

            if (stmt->extern_fn.is_variadic) {
                if (stmt->extern_fn.param_count > 0) emit(", ");
                emit("...");
            }

            emit(");\n");
            break;
        }
        case STMT_STRUCT:
            // For generic structs, emit with type params resolved to default C types
            // Full monomorphization will generate specialized versions
            
            // T2.5.3: Enhanced struct system with ARC integration
            emit("typedef struct {\n");
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                // Convert Wyn type to C type
                const char* c_type = "long long"; // default
                if (stmt->struct_decl.field_types[i]) {
                    if (stmt->struct_decl.field_types[i]->type == EXPR_IDENT) {
                        Token type_name = stmt->struct_decl.field_types[i]->token;
                        // Check if this is a type parameter
                        bool is_type_param = false;
                        for (int tp = 0; tp < stmt->struct_decl.type_param_count; tp++) {
                            if (stmt->struct_decl.type_params[tp].length == type_name.length &&
                                memcmp(stmt->struct_decl.type_params[tp].start, type_name.start, type_name.length) == 0) {
                                is_type_param = true;
                                break;
                            }
                        }
                        if (is_type_param) {
                            // Determine concrete type from struct instantiations
                            c_type = "long long";
                            bool found_first = false;
                            bool type_conflict = false;
                            if (current_program) {
                                for (int si = 0; si < current_program->count && !type_conflict; si++) {
                                    Stmt* s = current_program->stmts[si];
                                    if (s->type != STMT_FN || !s->fn.body || s->fn.body->type != STMT_BLOCK) continue;
                                    for (int bi = 0; bi < s->fn.body->block.count && !type_conflict; bi++) {
                                        Stmt* bs = s->fn.body->block.stmts[bi];
                                        if (bs->type != STMT_VAR || !bs->var.init || bs->var.init->type != EXPR_STRUCT_INIT) continue;
                                        Expr* init = bs->var.init;
                                        if (init->struct_init.type_name.length != stmt->struct_decl.name.length ||
                                            memcmp(init->struct_init.type_name.start, stmt->struct_decl.name.start, stmt->struct_decl.name.length) != 0) continue;
                                        if (i < init->struct_init.field_count) {
                                            Expr* fv = init->struct_init.field_values[i];
                                            const char* this_type = "long long";
                                            if (fv && (fv->type == EXPR_STRING || fv->type == EXPR_STRING_INTERP ||
                                                (fv->expr_type && fv->expr_type->kind == TYPE_STRING))) {
                                                this_type = "const char*";
                                            } else if (fv && (fv->type == EXPR_FLOAT ||
                                                (fv->expr_type && fv->expr_type->kind == TYPE_FLOAT))) {
                                                this_type = "double";
                                            }
                                            if (!found_first) { c_type = this_type; found_first = true; }
                                            else if (strcmp(c_type, this_type) != 0) { type_conflict = true; }
                                        }
                                    }
                                }
                            }
                            if (type_conflict) {
                                fprintf(stderr, "Error: Generic struct '%.*s' used with conflicting types for field '%.*s'\n",
                                    stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                                    stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start);
                                fprintf(stderr, "  \033[34mHelp:\033[0m Use separate concrete structs for different field types\n");
                            }
                        } else if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            c_type = "long long";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            // Wyn `float` is C `double` (64-bit) everywhere - values,
                            // params, returns - so a struct field must be `double`
                            // too. Emitting `float` (32-bit) truncated precision on
                            // assignment and broke by-value FFI struct layout.
                            c_type = "double";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            c_type = "bool";
                        } else {
                            // Custom struct type - use the type name as-is
                            static char custom_type[64]; token_to_cstr(custom_type, sizeof(custom_type), type_name);
                            c_type = custom_type;
                        }
                    } else if (stmt->struct_decl.field_types[i]->type == EXPR_ARRAY) {
                        // Array field type
                        c_type = "WynArray";
                    } else if (stmt->struct_decl.field_types[i]->type == EXPR_OPTIONAL_TYPE) {
                        // Optional field `f: T?` - emit the Option<T> family type so
                        // Some(...)/None and match work (was defaulting to long long,
                        // which miscompiled struct construction + match on the field).
                        Expr* inner = stmt->struct_decl.field_types[i]->optional_type.inner_type;
                        if (inner && inner->type == EXPR_IDENT) {
                            Token t = inner->token;
                            if (t.length == 3 && memcmp(t.start, "int", 3) == 0) c_type = "OptionInt";
                            else if (t.length == 6 && memcmp(t.start, "string", 6) == 0) c_type = "OptionString";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) c_type = "OptionFloat";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) c_type = "OptionBool";
                            else {
                                char _stn[96]; token_to_cstr(_stn, sizeof(_stn), t);
                                extern int is_known_struct(const char*);
                                if (is_known_struct(_stn)) {
                                    // Ensure the Option<Struct> family typedef is emitted.
                                    extern void register_option_struct(const char*);
                                    register_option_struct(_stn);
                                    static char _osf[128];
                                    snprintf(_osf, sizeof(_osf), "Option%s", _stn);
                                    c_type = _osf;
                                } else {
                                    c_type = "WynOptional*";
                                }
                            }
                        } else {
                            c_type = "WynOptional*";
                        }
                    }
                }

                // Add ARC annotation comment for managed fields
                if (stmt->struct_decl.field_arc_managed[i]) {
                    emit("    %s %.*s; // ARC-managed\n", 
                         c_type,
                         stmt->struct_decl.fields[i].length,
                         stmt->struct_decl.fields[i].start);
                } else {
                    emit("    %s %.*s;\n", 
                         c_type,
                         stmt->struct_decl.fields[i].length,
                         stmt->struct_decl.fields[i].start);
                }
            }
            // Emit struct name with module prefix if in module context
            if (current_module_prefix) {
                emit("} %s_%.*s;\n\n", 
                     current_module_prefix,
                     stmt->struct_decl.name.length,
                     stmt->struct_decl.name.start);
                
                // T2.5.3: Generate ARC cleanup function for struct
                emit("void %s_%.*s_cleanup(%s_%.*s* obj) {\n",
                     current_module_prefix,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     current_module_prefix,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
            } else {
                emit("} %.*s;\n\n", 
                     stmt->struct_decl.name.length,
                     stmt->struct_decl.name.start);
                
                // T2.5.3: Generate ARC cleanup function for struct
                emit("void %.*s_cleanup(%.*s* obj) {\n",
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
            }
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                if (stmt->struct_decl.field_arc_managed[i]) {
                    emit("    if (obj->%.*s) wyn_arc_release(obj->%.*s);\n",
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start,
                         stmt->struct_decl.fields[i].length, stmt->struct_decl.fields[i].start);
                }
            }
            emit("}\n\n");
            
            // Generate methods defined in struct
            if (stmt->struct_decl.method_count > 0)
                emit("/* %d methods */\n", stmt->struct_decl.method_count);
            for (int i = 0; i < stmt->struct_decl.method_count; i++) {
                FnStmt* method = stmt->struct_decl.methods[i];
                // Generate as TypeName_methodname(TypeName self, ...)
                
                // Emit return type
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token type_name = method->return_type->token;
                    if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                        emit("int");
                    } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                        emit("double");
                    } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        emit("char*");
                    } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                        emit("bool");
                    } else {
                        emit("%.*s", type_name.length, type_name.start);
                    }
                } else {
                    emit("void");
                }
                
                emit(" %.*s_%.*s(%.*s self",
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start,
                     method->name.length, method->name.start,
                     stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                
                // Skip first param if it's 'self'
                int start_param = 0;
                if (method->param_count > 0 && 
                    method->params[0].length == 4 && 
                    memcmp(method->params[0].start, "self", 4) == 0) {
                    start_param = 1;
                }
                
                for (int j = start_param; j < method->param_count; j++) {
                    emit(", ");
                    // Emit parameter type
                    if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                        Token ptype = method->param_types[j]->token;
                        if (ptype.length == 3 && memcmp(ptype.start, "int", 3) == 0) {
                            emit("long long");
                        } else if (ptype.length == 5 && memcmp(ptype.start, "float", 5) == 0) {
                            emit("double");
                        } else if (ptype.length == 6 && memcmp(ptype.start, "string", 6) == 0) {
                            emit("const char*");
                        } else {
                            emit("%.*s", ptype.length, ptype.start);
                        }
                    }
                    emit(" %.*s", method->params[j].length, method->params[j].start);
                }
                emit(") ");

                // Emit method body. Register `self` as a struct var of the
                // receiver type for the duration of the body so that internal
                // `self.other()` calls dispatch to `Type_other(self)` (without
                // this, self's type is unknown → the call emitted nothing, e.g.
                // `return ( + 1)`). Save/restore the struct-var stack depth so the
                // binding doesn't leak into later functions.
                if (method->body) {
                    extern int wyn_struct_var_depth(void);
                    extern void wyn_struct_var_truncate(int);
                    extern void register_struct_var(const char*, const char*);
                    int _sv_depth = wyn_struct_var_depth();
                    char _self_ty[64];
                    snprintf(_self_ty, sizeof(_self_ty), "%.*s",
                             stmt->struct_decl.name.length, stmt->struct_decl.name.start);
                    register_struct_var("self", _self_ty);
                    emit("{\n");
                    emit("    /* body has %d statements */\n", method->body->block.count);
                    codegen_stmt(method->body);
                    emit("}\n");
                    wyn_struct_var_truncate(_sv_depth);
                } else {
                    emit("{ /* no body */ }\n");
                }
                emit("\n");
            }
            
            break;
        case STMT_ENUM:
            { extern void register_enum_type(const char*); char _en[128]; token_to_cstr(_en, sizeof(_en), stmt->enum_decl.name); register_enum_type(_en); }
            // Check if any variant has data
            bool has_data = false;
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                if (stmt->enum_decl.variant_type_counts[i] > 0) {
                    has_data = true;
                    break;
                }
            }
            {
                // Register all enum variants (simple and data)
                extern void register_enum_variant_type(const char*, const char*, const char*);
                char _den[128]; token_to_cstr(_den, sizeof(_den), stmt->enum_decl.name);
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    char _vn[128]; token_to_cstr(_vn, sizeof(_vn), stmt->enum_decl.variants[i]);
                    if (stmt->enum_decl.variant_type_counts[i] == 1) {
                        register_enum_variant_type(_den, _vn, c_type_from_expr(stmt->enum_decl.variant_types[i][0]));
                    } else {
                        register_enum_variant_type(_den, _vn, "void");
                    }
                }
            }
            if (has_data) {
                extern void register_data_enum_type(const char*);
                char _den[128]; token_to_cstr(_den, sizeof(_den), stmt->enum_decl.name);
                register_data_enum_type(_den);
            }
            
            if (has_data) {
                // Generate tagged union for enum with data
                emit("typedef struct {\n");
                emit("    enum { ");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("%.*s_%.*s_TAG",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    if (i < stmt->enum_decl.variant_count - 1) emit(", ");
                }
                emit(" } tag;\n");
                emit("    union {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variant_type_counts[i] > 0) {
                        emit("        ");
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" %.*s_value;\n",
                                 stmt->enum_decl.variants[i].length,
                                 stmt->enum_decl.variants[i].start);
                        } else {
                            // Multi-field variant: struct { type0 f0; type1 f1; ... }
                            emit("struct { ");
                            for (int j = 0; j < stmt->enum_decl.variant_type_counts[i]; j++) {
                                emit_type_from_expr(stmt->enum_decl.variant_types[i][j]);
                                emit(" f%d; ", j);
                                // Record each field's C type so match-arm extraction
                                // can bind it with the right type (not a guess).
                                { extern void register_enum_variant_field_type(const char*, const char*, int, const char*);
                                  char _en[128], _vn[128];
                                  token_to_cstr(_en, sizeof(_en), stmt->enum_decl.name);
                                  token_to_cstr(_vn, sizeof(_vn), stmt->enum_decl.variants[i]);
                                  register_enum_variant_field_type(_en, _vn, j, c_type_from_expr(stmt->enum_decl.variant_types[i][j])); }
                            }
                            emit("} %.*s_value;\n",
                                 stmt->enum_decl.variants[i].length,
                                 stmt->enum_decl.variants[i].start);
                        }
                    }
                }
                emit("    } data;\n");
                emit("} %.*s;\n\n",
                     stmt->enum_decl.name.length,
                     stmt->enum_decl.name.start);
            } else {
                // Simple enum without data
                emit("typedef enum {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("    %.*s", 
                         stmt->enum_decl.variants[i].length,
                         stmt->enum_decl.variants[i].start);
                    if (i < stmt->enum_decl.variant_count - 1) {
                        emit(",");
                    }
                    emit("\n");
                }
                emit("} %.*s;\n\n", 
                     stmt->enum_decl.name.length,
                     stmt->enum_decl.name.start);
            }
            
            // Generate qualified constants for EnumName.MEMBER access (only for simple enums)
            if (!has_data) {
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("#define %.*s_%.*s %d\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         i);
                }
            }
            emit("\n");
            
            // Generate constructor functions for enums with data
            if (has_data) {
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    // Constructor function for all variants (with or without data)
                    emit("%.*s %.*s_%.*s(",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    if (stmt->enum_decl.variant_type_counts[i] > 0) {
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" value");
                        } else {
                            // Multi-field: generate params f0, f1, ...
                            for (int j = 0; j < stmt->enum_decl.variant_type_counts[i]; j++) {
                                if (j > 0) emit(", ");
                                emit_type_from_expr(stmt->enum_decl.variant_types[i][j]);
                                emit(" f%d", j);
                            }
                        }
                    }
                    // else: zero-argument constructor
                    
                    emit(") {\n");
                    emit("    %.*s result;\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                    emit("    result.tag = %.*s_%.*s_TAG;\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    
                    if (stmt->enum_decl.variant_type_counts[i] == 1) {
                        emit("    result.data.%.*s_value = value;\n",
                             stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                    } else if (stmt->enum_decl.variant_type_counts[i] > 1) {
                        for (int j = 0; j < stmt->enum_decl.variant_type_counts[i]; j++) {
                            emit("    result.data.%.*s_value.f%d = f%d;\n",
                                 stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start, j, j);
                        }
                    }
                    
                    emit("    return result;\n");
                    emit("}\n\n");
                }
            }
            
            // Generate toString function
            emit("const char* %.*s_toString(%.*s val) {\n",
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                 stmt->enum_decl.name.length, stmt->enum_decl.name.start);
            if (has_data) {
                emit("    switch(val.tag) {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("        case %.*s_%.*s_TAG: return \"%.*s\";\n",
                         stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                }
                emit("    }\n");
            } else {
                emit("    switch(val) {\n");
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    emit("        case %.*s: return \"%.*s\";\n",
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start,
                         stmt->enum_decl.variants[i].length, stmt->enum_decl.variants[i].start);
                }
                emit("    }\n");
            }
            emit("    return \"Unknown\";\n");
            emit("}\n\n");
            
            // Generate unwrap function for Option enum
            if (stmt->enum_decl.name.length == 6 && memcmp(stmt->enum_decl.name.start, "Option", 6) == 0) {
                // Find the Some variant and its type
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variants[i].length == 4 && 
                        memcmp(stmt->enum_decl.variants[i].start, "Some", 4) == 0 &&
                        stmt->enum_decl.variant_type_counts[i] == 1) {
                        // Generate unwrap function
                        emit("int %.*s_unwrap(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Some_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Some_value;\n");
                        emit("    }\n");
                        emit("    fprintf(stderr, \"Error: unwrap() called on None\\n\");\n");
                        emit("    exit(1);\n");
                        emit("}\n\n");
                        
                        // Generate is_some function
                        emit("bool %.*s_is_some(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Some_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate is_none function
                        emit("bool %.*s_is_none(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_None_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate unwrap_or function
                        emit("int %.*s_unwrap_or(%.*s val, int default_val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Some_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Some_value;\n");
                        emit("    }\n");
                        emit("    return default_val;\n");
                        emit("}\n\n");
                        break;
                    }
                }
            }
            
            // Generate unwrap function for Result enum
            if (stmt->enum_decl.name.length == 6 && memcmp(stmt->enum_decl.name.start, "Result", 6) == 0) {
                // Find the Ok variant and its type
                for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                    if (stmt->enum_decl.variants[i].length == 2 && 
                        memcmp(stmt->enum_decl.variants[i].start, "Ok", 2) == 0 &&
                        stmt->enum_decl.variant_type_counts[i] == 1) {
                        // Generate unwrap function
                        emit("int %.*s_unwrap(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Ok_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Ok_value;\n");
                        emit("    }\n");
                        emit("    fprintf(stderr, \"Error: unwrap() called on Err\\n\");\n");
                        emit("    exit(1);\n");
                        emit("}\n\n");
                        
                        // Generate is_ok function
                        emit("bool %.*s_is_ok(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Ok_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate is_err function
                        emit("bool %.*s_is_err(%.*s val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    return val.tag == %.*s_Err_TAG;\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("}\n\n");
                        
                        // Generate unwrap_or function
                        emit("int %.*s_unwrap_or(%.*s val, int default_val) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start,
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("    if (val.tag == %.*s_Ok_TAG) {\n",
                             stmt->enum_decl.name.length, stmt->enum_decl.name.start);
                        emit("        return val.data.Ok_value;\n");
                        emit("    }\n");
                        emit("    return default_val;\n");
                        emit("}\n\n");
                        break;
                    }
                }
            }
            
            break;
        case STMT_TYPE_ALIAS: {
            const char* ct = "long long";
            Token t = stmt->type_alias.target;
            if (t.length == 6 && memcmp(t.start, "string", 6) == 0) ct = "char*";
            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) ct = "double";
            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) ct = "bool";
            else if (t.length == 3 && memcmp(t.start, "int", 3) == 0) ct = "long long";
            else { static char _tb[128]; token_to_cstr(_tb, sizeof(_tb), t); ct = _tb; }
            emit("typedef %s %.*s;\n\n", ct, stmt->type_alias.name.length, stmt->type_alias.name.start);
            break;
        }
        case STMT_IMPL:
            for (int i = 0; i < stmt->impl.method_count; i++) {
                FnStmt* method = stmt->impl.methods[i];
                
                // Determine return type
                const char* return_type = "long long";
                if (method->return_type && method->return_type->type == EXPR_CALL &&
                    method->return_type->call.callee->type == EXPR_IDENT) {
                    Token rt = method->return_type->call.callee->token;
                    if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) return_type = "ResultInt";
                    else if (rt.length == 6 && memcmp(rt.start, "Option", 6) == 0) return_type = "OptionInt";
                } else if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token ret_type = method->return_type->token;
                    if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                        return_type = "long long";
                    } else if (ret_type.length == 5 && memcmp(ret_type.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (ret_type.length == 4 && memcmp(ret_type.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                        return_type = "const char*";
                    } else {
                        // Custom struct/enum return type
                        static char impl_ret_buf[128]; token_to_cstr(impl_ret_buf, sizeof(impl_ret_buf), ret_type);
                        return_type = impl_ret_buf;
                    }
                }
                
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int j = 0; j < method->param_count; j++) {
                    if (j > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "long long";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "long long";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            param_type = "const char*";
                        } else {
                            // Custom struct type
                            token_to_cstr(custom_type_buf, sizeof(custom_type_buf), type_name);
                            param_type = custom_type_buf;
                        }
                    }
                    
                    emit("%s %.*s", param_type, method->params[j].length, method->params[j].start);
                }
                emit(") {\n");
                push_scope();
                // Register `self` as the impl's receiver struct type so internal
                // `self.other()` calls dispatch correctly (see the struct-body
                // method emission for the same reasoning). Scoped to this body.
                extern int wyn_struct_var_depth(void);
                extern void wyn_struct_var_truncate(int);
                extern void register_struct_var(const char*, const char*);
                int _impl_sv_depth = wyn_struct_var_depth();
                {
                    char _impl_self_ty[64];
                    snprintf(_impl_self_ty, sizeof(_impl_self_ty), "%.*s",
                             stmt->impl.type_name.length, stmt->impl.type_name.start);
                    register_struct_var("self", _impl_self_ty);
                }
                codegen_stmt(method->body);
                wyn_struct_var_truncate(_impl_sv_depth);
                pop_scope();
                emit("}\n\n");
            }
            // Generate vtable wrappers and instance if this is a trait impl
            if (stmt->impl.trait_name.start && stmt->impl.trait_name.length > 0) {
                Token tname = stmt->impl.trait_name;
                Token itype = stmt->impl.type_name;
                register_trait_impl(itype.start, itype.length, tname.start, tname.length);
                for (int i = 0; i < stmt->impl.method_count; i++) {
                    FnStmt* method = stmt->impl.methods[i];
                    const char* ret = "long long";
                    if (method->return_type && method->return_type->type == EXPR_CALL &&
                        method->return_type->call.callee->type == EXPR_IDENT) {
                        Token rt = method->return_type->call.callee->token;
                        if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) ret = "ResultInt";
                    } else if (method->return_type && method->return_type->type == EXPR_IDENT) {
                        Token rt = method->return_type->token;
                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret = "const char*";
                        else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret = "double";
                    }
                    emit("%s %.*s_%.*s_%.*s_wrap(void* __d",
                         ret, tname.length, tname.start, method->name.length, method->name.start,
                         itype.length, itype.start);
                    // Extra params (skip self at index 0)
                    for (int j = 1; j < method->param_count; j++) {
                        const char* pt = "long long";
                        if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                            Token ptt = method->param_types[j]->token;
                            if (ptt.length == 6 && memcmp(ptt.start, "string", 6) == 0) pt = "const char*";
                            else if (ptt.length == 5 && memcmp(ptt.start, "float", 5) == 0) pt = "double";
                        }
                        emit(", %s __a%d", pt, j);
                    }
                    emit(") { return %.*s_%.*s(*(%.*s*)__d",
                         itype.length, itype.start,
                         method->name.length, method->name.start, itype.length, itype.start);
                    for (int j = 1; j < method->param_count; j++) emit(", __a%d", j);
                    emit("); }\n");
                }
                emit("%.*s_vtable %.*s_%.*s_vt = { ", tname.length, tname.start,
                     tname.length, tname.start, itype.length, itype.start);
                for (int i = 0; i < stmt->impl.method_count; i++) {
                    if (i > 0) emit(", ");
                    FnStmt* method = stmt->impl.methods[i];
                    emit("%.*s_%.*s_%.*s_wrap", tname.length, tname.start,
                         method->name.length, method->name.start, itype.length, itype.start);
                }
                emit(" };\n\n");
            }
            break;
        case STMT_TRAIT:
            // Generate trait vtable and trait object
            {
                Token tname = stmt->trait_decl.name;
                emit("// trait %.*s\n", tname.length, tname.start);
                
                // 1. Function pointer typedefs
                for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                    FnStmt* method = stmt->trait_decl.methods[i];
                    const char* ret = "long long";
                    if (method->return_type && method->return_type->type == EXPR_CALL &&
                        method->return_type->call.callee->type == EXPR_IDENT) {
                        Token rt = method->return_type->call.callee->token;
                        if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) ret = "ResultInt";
                    } else if (method->return_type && method->return_type->type == EXPR_IDENT) {
                        Token rt = method->return_type->token;
                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret = "const char*";
                        else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret = "double";
                    }
                    emit("typedef %s (*%.*s_%.*s_fn)(void*", ret,
                         tname.length, tname.start, method->name.length, method->name.start);
                    // Extra params (skip self at index 0)
                    for (int j = 1; j < method->param_count; j++) {
                        const char* pt = "long long";
                        if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                            Token ptt = method->param_types[j]->token;
                            if (ptt.length == 6 && memcmp(ptt.start, "string", 6) == 0) pt = "const char*";
                            else if (ptt.length == 5 && memcmp(ptt.start, "float", 5) == 0) pt = "double";
                        }
                        emit(", %s", pt);
                    }
                    emit(");\n");
                }
                
                // 2. Vtable struct
                emit("typedef struct {\n");
                for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                    FnStmt* method = stmt->trait_decl.methods[i];
                    emit("    %.*s_%.*s_fn %.*s;\n",
                         tname.length, tname.start, method->name.length, method->name.start,
                         method->name.length, method->name.start);
                }
                emit("} %.*s_vtable;\n", tname.length, tname.start);
                
                // 3. Trait object struct
                emit("typedef struct { void* data; %.*s_vtable* vtable; } %.*s;\n",
                     tname.length, tname.start, tname.length, tname.start);
            }
            break;
        case STMT_IF:
            // Dead code elimination: skip if(false) entirely, emit only else
            if (stmt->if_stmt.condition->type == EXPR_BOOL &&
                stmt->if_stmt.condition->token.length == 5 &&
                memcmp(stmt->if_stmt.condition->token.start, "false", 5) == 0) {
                if (stmt->if_stmt.else_branch) codegen_stmt(stmt->if_stmt.else_branch);
                break;
            }
            // Dead code elimination: skip else on if(true)
            if (stmt->if_stmt.condition->type == EXPR_BOOL &&
                stmt->if_stmt.condition->token.length == 4 &&
                memcmp(stmt->if_stmt.condition->token.start, "true", 4) == 0) {
                codegen_stmt(stmt->if_stmt.then_branch);
                break;
            }
            emit("if (");
            codegen_expr(stmt->if_stmt.condition);
            emit(") {\n");
            push_scope();
            codegen_stmt(stmt->if_stmt.then_branch);
            pop_scope();
            emit("    }");
            if (stmt->if_stmt.else_branch) {
                emit(" else ");
                if (stmt->if_stmt.else_branch->type == STMT_IF) {
                    codegen_stmt(stmt->if_stmt.else_branch);
                } else {
                    emit("{\n");
                    push_scope();
                    codegen_stmt(stmt->if_stmt.else_branch);
                    pop_scope();
                    emit("    }\n");
                }
            } else {
                emit("\n");
            }
            break;
        case STMT_WHILE:
            emit("while (");
            codegen_expr(stmt->while_stmt.condition);
            emit(") {\n");
            push_scope();
            codegen_stmt(stmt->while_stmt.body);
            pop_scope();
            emit("    }\n");
            break;
        case STMT_FOR:
            // Check if this is a for-in loop (array iteration)
            if (stmt->for_stmt.array_expr) {
                // Map iteration: `for k, v in m` (and `for k in m` -> keys).
                // Iterate the map's keys, binding k to the key (string) and, when
                // a value var is present, v to m[k] via the value-typed getter.
                if (stmt->for_stmt.array_expr->expr_type &&
                    stmt->for_stmt.array_expr->expr_type->kind == TYPE_MAP) {
                    Type* mvt = stmt->for_stmt.array_expr->expr_type->map_type.value_type;
                    const char* vget = "hashmap_get_string"; const char* vcty = "const char*";
                    if (mvt) {
                        switch (mvt->kind) {
                            case TYPE_INT:   vget = "hashmap_get_int";    vcty = "long long"; break;
                            case TYPE_BOOL:  vget = "hashmap_get_bool";   vcty = "bool"; break;
                            case TYPE_FLOAT: vget = "hashmap_get_float";  vcty = "double"; break;
                            default: break;
                        }
                    }
                    // With two vars the parser stores key in index_var, value in
                    // loop_var; with one var, the key is loop_var.
                    Token kvar = stmt->for_stmt.has_index ? stmt->for_stmt.index_var : stmt->for_stmt.loop_var;
                    emit("{\n"); push_scope();
                    emit("    WynHashMap* __for_map = "); codegen_expr(stmt->for_stmt.array_expr); emit(";\n");
                    emit("    WynArray __for_keys = hashmap_keys(__for_map);\n");
                    emit("    for (long long __ki = 0; __ki < __for_keys.count; __ki++) {\n");
                    emit("        const char* %.*s = array_get_str(__for_keys, __ki);\n", kvar.length, kvar.start);
                    // The key is a string; register it so the body types it right.
                    { char _kb[256]; token_to_cstr(_kb, sizeof(_kb), kvar);
                      extern void register_string_var(const char*); register_string_var(_kb); }
                    if (stmt->for_stmt.has_index) {
                        emit("        %s %.*s = %s(__for_map, %.*s);\n",
                             vcty, stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start,
                             vget, kvar.length, kvar.start);
                        // Register a string value binding so the body concats it correctly.
                        if (strcmp(vcty, "const char*") == 0) {
                            char _vb[256]; token_to_cstr(_vb, sizeof(_vb), stmt->for_stmt.loop_var);
                            extern void register_string_var(const char*); register_string_var(_vb);
                        }
                    }
                    if (stmt->for_stmt.body) codegen_stmt(stmt->for_stmt.body);
                    emit("    }\n");
                    pop_scope(); emit("}\n");
                    break;
                }
                // L3: Iterator-based for-in (generator functions)
                if (stmt->for_stmt.array_expr->type == EXPR_CALL &&
                    stmt->for_stmt.array_expr->call.callee->type == EXPR_IDENT) {
                    extern Program* current_program;
                    extern int fn_is_generator(Stmt*);
                    bool _is_gen_iter = false;
                    if (current_program) {
                        Token cn = stmt->for_stmt.array_expr->call.callee->token;
                        for (int _fi = 0; _fi < current_program->count; _fi++) {
                            Stmt* _s = current_program->stmts[_fi];
                            Stmt* _fs = (_s->type == STMT_EXPORT && _s->export.stmt) ? _s->export.stmt : _s;
                            if (fn_is_generator(_fs) && _fs->fn.name.length == cn.length &&
                                memcmp(_fs->fn.name.start, cn.start, cn.length) == 0) {
                                _is_gen_iter = true; break;
                            }
                        }
                    }
                    if (_is_gen_iter) {
                        emit("{\n"); push_scope();
                        emit("    WynIter* __iter = "); codegen_expr(stmt->for_stmt.array_expr); emit(";\n");
                        emit("    while (wyn_iter_next(__iter)) {\n");
                        emit("        long long %.*s = wyn_iter_value(__iter);\n", stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                        if (stmt->for_stmt.body) codegen_stmt(stmt->for_stmt.body);
                        emit("    }\n");
                        emit("    wyn_iter_destroy(__iter);\n");
                        pop_scope(); emit("}\n");
                        break;
                    }
                }
                // Check if iterating over a WynIntArray
                if (stmt->for_stmt.array_expr->type == EXPR_IDENT) {
                    char _ian[128]; token_to_cstr(_ian, sizeof(_ian), stmt->for_stmt.array_expr->token);
                    extern int is_int_array_var(const char*);
                    if (is_int_array_var(_ian)) {
                        emit("{\n"); push_scope();
                        emit("    WynIntArray __iter_iarr = "); codegen_expr(stmt->for_stmt.array_expr); emit(";\n");
                        emit("    for (long long __i = 0; __i < __iter_iarr.count; __i++) {\n");
                        emit("        long long %.*s = __iter_iarr.data[__i];\n", stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                        if (stmt->for_stmt.has_index) {
                            emit("        long long %.*s = __i;\n", stmt->for_stmt.index_var.length, stmt->for_stmt.index_var.start);
                        }
                        codegen_stmt(stmt->for_stmt.body);
                        emit("    }\n");
                        pop_scope(); emit("}\n");
                        break;
                    }
                }
                // Generate for-in loop: for item in array
                emit("{\n");
                push_scope();
                emit("    WynArray __iter_array = ");
                codegen_expr(stmt->for_stmt.array_expr);
                emit(";\n");
                emit("    for (long long __i = 0; __i < __iter_array.count; __i++) {\n");
                // Use a generic approach - check element type at runtime
                emit("        WynValue __elem = __iter_array.data[__i];\n");
                emit("        ");
                
                // Determine variable type based on array expression type
                bool is_string_array = false;
                bool is_struct_array = false;
                bool is_float_array = false;
                bool is_enum_array = false;
                bool is_array_array = false;   // element is itself an array (nested)
                char enum_arr_type[128] = "";
                Type* elem_type = NULL;

                // Check if array expression has type info
                if (stmt->for_stmt.array_expr->expr_type &&
                    stmt->for_stmt.array_expr->expr_type->kind == TYPE_ARRAY &&
                    stmt->for_stmt.array_expr->expr_type->array_type.element_type) {
                    elem_type = stmt->for_stmt.array_expr->expr_type->array_type.element_type;
                    if (elem_type->kind == TYPE_STRING) {
                        is_string_array = true;
                    } else if (elem_type->kind == TYPE_STRUCT) {
                        is_struct_array = true;
                    } else if (elem_type->kind == TYPE_FLOAT) {
                        is_float_array = true;
                    } else if (elem_type->kind == TYPE_ARRAY) {
                        is_array_array = true;
                    } else if (elem_type->kind == TYPE_ENUM && elem_type->name.length > 0) {
                        is_enum_array = true;
                        token_to_cstr(enum_arr_type, sizeof(enum_arr_type), elem_type->name);
                    }
                }
                // Array LITERAL whose first element is itself an array literal
                // (`[[1,2],[3,4]]`): the checker may not carry the nested element
                // type, so detect it directly here.
                if (!is_array_array && !is_struct_array && !is_enum_array &&
                    stmt->for_stmt.array_expr->type == EXPR_ARRAY &&
                    stmt->for_stmt.array_expr->array.count > 0 &&
                    stmt->for_stmt.array_expr->array.elements[0]->type == EXPR_ARRAY) {
                    is_array_array = true;
                }
                // Array literal whose first element is a data-enum constructor
                // (`[E.A(..), ...]`): the checker doesn't type these, so detect
                // the enum from the constructor's receiver.
                if (!is_enum_array && !is_struct_array &&
                    stmt->for_stmt.array_expr->type == EXPR_ARRAY &&
                    stmt->for_stmt.array_expr->array.count > 0) {
                    Expr* e0 = stmt->for_stmt.array_expr->array.elements[0];
                    if (e0->type == EXPR_METHOD_CALL && e0->method_call.object->type == EXPR_IDENT) {
                        char _en[128]; token_to_cstr(_en, sizeof(_en), e0->method_call.object->token);
                        extern int is_data_enum_type(const char*);
                        if (is_data_enum_type(_en)) { is_enum_array = true; snprintf(enum_arr_type, sizeof(enum_arr_type), "%s", _en); }
                    } else if (e0->expr_type && e0->expr_type->kind == TYPE_ENUM && e0->expr_type->name.length > 0) {
                        is_enum_array = true; token_to_cstr(enum_arr_type, sizeof(enum_arr_type), e0->expr_type->name);
                    }
                }
                // Array literal with a float first element -> float array.
                if (!is_string_array && !is_struct_array && !is_float_array &&
                    stmt->for_stmt.array_expr->type == EXPR_ARRAY &&
                    stmt->for_stmt.array_expr->array.count > 0 &&
                    stmt->for_stmt.array_expr->array.elements[0]->type == EXPR_FLOAT) {
                    is_float_array = true;
                }
                
                // Check if it's a method call that returns string array
                if (!is_string_array && !is_struct_array && stmt->for_stmt.array_expr->type == EXPR_METHOD_CALL) {
                    Token method = stmt->for_stmt.array_expr->method_call.method;
                    if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "lines", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "chars", 5) == 0) ||
                        (method.length == 4 && memcmp(method.start, "keys", 4) == 0) ||
                        (method.length == 6 && memcmp(method.start, "values", 6) == 0)) {
                        is_string_array = true;
                    }
                }
                
                // Fallback: check if array literal contains string elements
                if (!is_string_array && !is_struct_array && stmt->for_stmt.array_expr->type == EXPR_ARRAY) {
                    if (stmt->for_stmt.array_expr->array.count > 0 &&
                        stmt->for_stmt.array_expr->array.elements[0]->type == EXPR_STRING) {
                        is_string_array = true;
                    }
                }
                
                if (is_enum_array && enum_arr_type[0]) {
                    // Data-enum values are stored by value as structs; bind the
                    // loop var to the enum type and register it so `match x` /
                    // methods on it lower correctly.
                    emit("%s %.*s = *(%s*)__elem.data.struct_val;\n",
                         enum_arr_type,
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start,
                         enum_arr_type);
                    char _lv[128]; token_to_cstr(_lv, sizeof(_lv), stmt->for_stmt.loop_var);
                    extern void register_enum_var(const char*, const char*);
                    register_enum_var(_lv, enum_arr_type);
                } else if (is_struct_array && elem_type) {
                    Token type_name = elem_type->struct_type.name;
                    emit("%.*s %.*s = *(%.*s*)__elem.data.struct_val;\n",
                         type_name.length, type_name.start,
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start,
                         type_name.length, type_name.start);
                } else if (is_string_array) {
                    emit("const char* %.*s = (__elem.type == WYN_TYPE_STRING) ? __elem.data.string_val : \"\";\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                } else if (is_float_array) {
                    emit("double %.*s = (__elem.type == WYN_TYPE_FLOAT) ? __elem.data.float_val : (double)__elem.data.int_val;\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                } else if (is_array_array) {
                    // Nested array element: bind the loop var to the sub-array by
                    // value (deref the stored WynArray*), and register it so the body
                    // treats it as an array (indexing / re-iteration). Without this it
                    // was bound as `long long … : 0` → every row printed 0. (2026-07)
                    emit("WynArray %.*s = (__elem.type == WYN_TYPE_ARRAY && __elem.data.array_val) ? *__elem.data.array_val : array_new();\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                    { char _lv[256]; token_to_cstr(_lv, sizeof(_lv), stmt->for_stmt.loop_var);
                      extern void register_array_var(const char*); register_array_var(_lv); }
                } else {
                    emit("long long %.*s = (__elem.type == WYN_TYPE_INT) ? __elem.data.int_val : 0;\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                }
                // Emit index variable for indexed iteration: for i, v in arr
                if (stmt->for_stmt.has_index) {
                    emit("        long long %.*s = __i;\n",
                         stmt->for_stmt.index_var.length, stmt->for_stmt.index_var.start);
                }
                // Register loop var(s) as locals: inside a MODULE function,
                // EXPR_IDENT prefixes anything it doesn't know is a local - the
                // declaration above emitted the bare name but every use became
                // `mod_name` (undeclared identifier, bug M2 2026-07-18).
                {
                    char _lvn[256]; token_to_cstr(_lvn, sizeof(_lvn), stmt->for_stmt.loop_var);
                    register_local_variable(_lvn);
                    if (stmt->for_stmt.has_index) {
                        char _ivn[256]; token_to_cstr(_ivn, sizeof(_ivn), stmt->for_stmt.index_var);
                        register_local_variable(_ivn);
                    }
                }
                codegen_stmt(stmt->for_stmt.body);
                emit("    }\n");
                pop_scope();
                emit("}\n");
            } else {
                // Regular for loop
                emit("for (");
                if (stmt->for_stmt.init) {
                    emit("long long %.*s = ", stmt->for_stmt.init->var.name.length, stmt->for_stmt.init->var.name.start);
                    codegen_expr(stmt->for_stmt.init->var.init);
                }
                emit("; ");
                if (stmt->for_stmt.condition) {
                    codegen_expr(stmt->for_stmt.condition);
                }
                emit("; ");
                if (stmt->for_stmt.increment) {
                    codegen_expr(stmt->for_stmt.increment);
                }
                emit(") {\n");
                push_scope();
                codegen_stmt(stmt->for_stmt.body);
                pop_scope();
                emit("    }\n");
            }
            break;
        case STMT_IMPORT: {
            // Extract module name
            char module_name[256]; token_to_cstr(module_name, sizeof(module_name), stmt->import.module);
            
            // Handle selective imports: import { get, post } from module
            if (stmt->import.item_count > 0) {
                // Register each imported item with module prefix
                for (int i = 0; i < stmt->import.item_count; i++) {
                    char item_name[256]; token_to_cstr(item_name, sizeof(item_name), stmt->import.items[i]);
                    
                    // Map item_name -> module_name::item_name
                    char full_name[512];
                    const char* c_mod = module_to_c_ident(module_name);
                    snprintf(full_name, sizeof(full_name), "%s_%s", c_mod, item_name);
                    register_module_alias(item_name, full_name);
                }
            }
            
            // Register alias if present
            if (stmt->import.alias.start != NULL) {
                char alias_name[256]; token_to_cstr(alias_name, sizeof(alias_name), stmt->import.alias);
                register_module_alias(alias_name, module_name);
            }
            
            extern bool is_builtin_module(const char* name);
            extern bool is_module_loaded(const char* name);
            extern char* resolve_relative_module_name(const char* name);
            
            // Resolve relative paths (crate/config -> config)
            char* resolved_module_name = resolve_relative_module_name(module_name);
            const char* lookup_name = resolved_module_name ? resolved_module_name : module_name;
            
            // Priority: User modules > Built-ins
            // This allows community to override/extend built-in modules
            if (is_module_loaded(lookup_name)) {
                // User module exists - emit all loaded modules once per compilation
                if (!modules_emitted_this_compilation) {
                    modules_emitted_this_compilation = true;
                    
                    extern int get_module_count(void);
                    extern void* get_module_entry_at(int index);
                    int module_count = get_module_count();

                    for (int m = 0; m < module_count; m++) {
                        typedef struct { char* name; Program* ast; } ModuleEntry;
                        ModuleEntry* mod = (ModuleEntry*)get_module_entry_at(m);
                        
                        // Register short name mapping for nested modules
                        register_module_short_name(mod->name);
                        
                        // Register all functions in this module for internal call resolution
                        clear_module_functions();
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            if (s->type == STMT_FN) {
                                char func_name[256]; token_to_cstr(func_name, sizeof(func_name), s->fn.name);
                                register_module_function(func_name);
                            }
                        }
                        
                        // First: emit structs and enums with module prefix
                        const char* saved_prefix = current_module_prefix;
                        current_module_prefix = mod->name;
                        const char* c_mod_name = module_to_c_ident(mod->name);
                        
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            if ((s->type == STMT_STRUCT && s->struct_decl.is_public) ||
                                (s->type == STMT_ENUM && s->enum_decl.is_public)) {
                                codegen_stmt(s);
                                // Emit typedef alias: OrigName -> module_OrigName
                                if (s->type == STMT_ENUM) {
                                    emit("typedef %.*s %s_%.*s;\n",
                                         s->enum_decl.name.length, s->enum_decl.name.start,
                                         c_mod_name,
                                         s->enum_decl.name.length, s->enum_decl.name.start);
                                    // Alias constructors: module_Enum_Variant -> Enum_Variant
                                    for (int vi = 0; vi < s->enum_decl.variant_count; vi++) {
                                        emit("#define %s_%.*s_%.*s %.*s_%.*s\n",
                                             c_mod_name,
                                             s->enum_decl.name.length, s->enum_decl.name.start,
                                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start,
                                             s->enum_decl.name.length, s->enum_decl.name.start,
                                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start);
                                        // Also alias TAG names
                                        emit("#define %s_%.*s_%.*s_TAG %.*s_%.*s_TAG\n",
                                             c_mod_name,
                                             s->enum_decl.name.length, s->enum_decl.name.start,
                                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start,
                                             s->enum_decl.name.length, s->enum_decl.name.start,
                                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start);
                                    }
                                } else if (s->type == STMT_STRUCT) {
                                    emit("typedef %.*s %s_%.*s;\n",
                                         s->struct_decl.name.length, s->struct_decl.name.start,
                                         c_mod_name,
                                         s->struct_decl.name.length, s->struct_decl.name.start);
                                }
                            }
                        }
                        
                        current_module_prefix = saved_prefix;
                        
                        // Second: emit forward declarations for all functions
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            
                            if (s->type == STMT_FN) {
                                // Private functions are static
                                if (!s->fn.is_public) {
                                    emit("static ");
                                }
                                
                                // Determine return type - void when unannotated,
                                // matching the definition emitter (M4 fix: the
                                // prototype said long long while the definition
                                // said void → "conflicting types").
                                const char* return_type = s->fn.return_type ? "long long" : "void";
                                static char custom_ret_type[128];
                                if (s->fn.return_type) {
                                    if (s->fn.return_type->type == EXPR_ARRAY) {
                                        return_type = "WynArray";
                                    } else if (s->fn.return_type->type == EXPR_IDENT) {
                                        Token rt = s->fn.return_type->token;
                                        if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) {
                                            return_type = "const char*";
                                        } else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) {
                                            return_type = "double";
                                        } else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) {
                                            return_type = "bool";
                                        } else if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) {
                                            return_type = "long long";
                                        } else if (rt.length == 7 && memcmp(rt.start, "HashMap", 7) == 0) {
                                            return_type = "WynHashMap*";
                                        } else if (rt.length == 7 && memcmp(rt.start, "HashSet", 7) == 0) {
                                            return_type = "WynHashSet*";
                                        } else {
                                            // Custom struct type - add module prefix
                                            snprintf(custom_ret_type, 128, "%s_%.*s", c_mod_name, rt.length, rt.start);
                                            return_type = custom_ret_type;
                                        }
                                    }
                                }
                                
                                emit("%s %s_%.*s(", return_type, c_mod_name, s->fn.name.length, s->fn.name.start);
                                
                                // Emit parameters with types
                                for (int j = 0; j < s->fn.param_count; j++) {
                                    if (j > 0) emit(", ");
                                    
                                    // Determine parameter type
                                    const char* param_type = "long long";
                                    static char custom_param_type[128];
                                    if (s->fn.param_types[j]) {
                                        if (s->fn.param_types[j]->type == EXPR_IDENT) {
                                            Token pt = s->fn.param_types[j]->token;
                                            if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) {
                                                param_type = "const char*";
                                            } else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) {
                                                param_type = "double";
                                            } else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) {
                                                param_type = "bool";
                                            } else if (pt.length == 3 && memcmp(pt.start, "int", 3) == 0) {
                                                param_type = "long long";
                                            } else if (pt.length == 5 && memcmp(pt.start, "array", 5) == 0) {
                                                param_type = "WynArray";
                                            } else {
                                                // Custom struct type - add module prefix
                                                snprintf(custom_param_type, 128, "%s_%.*s", c_mod_name, pt.length, pt.start);
                                                param_type = custom_param_type;
                                            }
                                        }
                                    }
                                    
                                    emit("%s %.*s", param_type, s->fn.params[j].length, s->fn.params[j].start);
                                }
                                
                                emit(");\n");
                            }
                        }
                        
                        // Second: emit constants and variables
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            // Unwrap export for constants and variables
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                if (s->export.stmt->type == STMT_CONST || s->export.stmt->type == STMT_VAR) {
                                    s = s->export.stmt;
                                }
                            }
                            if (s->type == STMT_CONST) {
                                VarStmt* const_stmt = &s->const_stmt;
                                
                                // Determine type
                                if (const_stmt->init) {
                                    if (const_stmt->init->type == EXPR_STRING) {
                                        emit("const char* %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else if (const_stmt->init->type == EXPR_FLOAT) {
                                        emit("double %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else if (const_stmt->init->type == EXPR_BOOL) {
                                        emit("bool %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    } else {
                                        emit("int %s_%.*s = ", c_mod_name, const_stmt->name.length, const_stmt->name.start);
                                    }
                                    codegen_expr(const_stmt->init);
                                    emit(";\n");
                                }
                            } else if (s->type == STMT_VAR) {
                                // Module-level variables (mutable state)
                                VarStmt* var_stmt = &s->var;
                                
                                // Determine type from initializer
                                if (var_stmt->init) {
                                    if (var_stmt->init->type == EXPR_STRING) {
                                        emit("const char* %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                        codegen_expr(var_stmt->init);
                                        emit(";\n");
                                    } else if (var_stmt->init->type == EXPR_FLOAT) {
                                        emit("double %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                        codegen_expr(var_stmt->init);
                                        emit(";\n");
                                    } else if (var_stmt->init->type == EXPR_ARRAY) {
                                        emit("WynArray %s_%.*s;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else if (var_stmt->init->type == EXPR_CALL || var_stmt->init->type == EXPR_METHOD_CALL) {
                                        // HashMap.new() etc - defer init
                                        if (var_stmt->init->expr_type && var_stmt->init->expr_type->kind == TYPE_MAP) {
                                            emit("WynHashMap* %s_%.*s;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                        } else {
                                            emit("long long %s_%.*s;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                        }
                                    } else {
                                        emit("long long %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                        codegen_expr(var_stmt->init);
                                        emit(";\n");
                                    }
                                } else {
                                    emit("long long %s_%.*s = 0;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                }
                            }
                        }
                        
                        // Third: emit functions
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
                            
                            // Unwrap export statements
                            if (s->type == STMT_EXPORT && s->export.stmt) {
                                s = s->export.stmt;
                            }
                            
                            if (s->type == STMT_FN) {
                                emit_function_with_prefix(s, mod->name);
                            }
                        }
                    }
                }
            } else if (is_builtin_module(lookup_name)) {
                // Built-in module (only if no user override)
                static bool builtin_modules_emitted = false;
                if (!builtin_modules_emitted) {
                    builtin_modules_emitted = true;
                    
                    if (strcmp(lookup_name, "math") == 0) {
                        // Math module - useful functions only (use +, -, *, / operators for basic arithmetic)
                        emit("#include <math.h>\n");
                        emit("double math_pow(double base, double exp) { return pow(base, exp); }\n");
                        emit("double math_sqrt(double x) { return sqrt(x); }\n");
                        emit("double math_abs(double x) { return fabs(x); }\n");
                        emit("double math_floor(double x) { return floor(x); }\n");
                        emit("double math_ceil(double x) { return ceil(x); }\n");
                        emit("double math_round(double x) { return round(x); }\n");
                        emit("double math_sin(double x) { return sin(x); }\n");
                        emit("double math_cos(double x) { return cos(x); }\n");
                        emit("double math_tan(double x) { return tan(x); }\n");
                        emit("double math_log(double x) { return log(x); }\n");
                        emit("double math_exp(double x) { return exp(x); }\n");
                        emit("double math_min(double a, double b) { return a < b ? a : b; }\n");
                        emit("double math_max(double a, double b) { return a > b ? a : b; }\n");
                        emit("const double math_pi = 3.14159265358979323846;\n");
                        emit("const double math_e = 2.71828182845904523536;\n");
                    }
                }
            }
            
            if (resolved_module_name) {
                free(resolved_module_name);
            }
            break;
        }
        case STMT_EXPORT:
            // Generate the exported statement normally (comment already generated)
            codegen_stmt(stmt->export.stmt);
            break;
        case STMT_MATCH:  // T1.4.4: Control Flow Agent addition
            codegen_match_statement(stmt);
            break;
        case STMT_CONST: {
            // Module-level constants - emit as static const
            const char* c_type = "long long";
            bool type_has_const = false;
            (void)type_has_const;
            
            // Determine type from initializer
            if (stmt->const_stmt.init) {
                if (stmt->const_stmt.init->type == EXPR_STRING) {
                    c_type = "char*";  // Don't include const here, we'll add it below
                } else if (stmt->const_stmt.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (stmt->const_stmt.init->type == EXPR_BOOL) {
                    c_type = "bool";
                } else if (stmt->const_stmt.init->type == EXPR_INT) {
                    c_type = "long long";
                }
            }
            
            // Emit as static const with module prefix
            if (current_module_prefix) {
                emit("static const %s %s_%.*s = ", c_type, current_module_prefix, 
                     stmt->const_stmt.name.length, stmt->const_stmt.name.start);
            } else {
                emit("static const %s %.*s = ", c_type, 
                     stmt->const_stmt.name.length, stmt->const_stmt.name.start);
            }
            
            codegen_expr(stmt->const_stmt.init);
            emit(";\n");
            break;
        }
        default:
            break;
    }
}

// Forward scan for lambdas in expressions
