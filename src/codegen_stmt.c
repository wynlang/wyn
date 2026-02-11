// codegen_stmt.c - Statement code generation
// Included from codegen.c - shares all statics

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
    for (int i = 0; i < fn_stmt->fn.param_count; i++) {
        char param_name[256];
        snprintf(param_name, 256, "%.*s", fn_stmt->fn.params[i].length, fn_stmt->fn.params[i].start);
        bool is_mut = fn_stmt->fn.param_mutable && fn_stmt->fn.param_mutable[i];
        register_parameter_mut(param_name, is_mut);
    }
    
    // Emit function signature with module prefix
    // Private functions are static (not accessible from outside)
    if (!fn_stmt->fn.is_public) {
        emit("static ");
    }
    
    // Determine return type
    const char* return_type = "long long";
    static char custom_return_type[128];
    if (fn_stmt->fn.return_type) {
        if (fn_stmt->fn.return_type->type == EXPR_IDENT) {
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
                    snprintf(custom_return_type, 128, "%.*s", rt.length, rt.start);
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

void codegen_stmt(Stmt* stmt) {
    if (!stmt) return;
    
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
                codegen_expr(stmt->expr);
                emit(";\n");
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
                    // Handle optional type annotation like int?
                    c_type = "WynOptional*";
                    needs_arc_management = true;
                } else if (stmt->var.type->type == EXPR_ARRAY) {
                    // Handle typed array annotation like [TokenType]
                    c_type = "WynArray";
                    needs_arc_management = false;
                } else if (stmt->var.type->type == EXPR_CALL) {
                    // Handle generic type instantiation: HashMap<K,V>, Option<T>, etc.
                    if (stmt->var.type->call.callee->type == EXPR_IDENT) {
                        Token type_name = stmt->var.type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
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
                    } else {
                        // Custom struct/enum type - use the type name as-is
                        static char custom_type_buf[256];
                        int len = type_name.length < 255 ? type_name.length : 255;
                        memcpy(custom_type_buf, type_name.start, len);
                        custom_type_buf[len] = '\0';
                        c_type = custom_type_buf;
                    }
                }
            } else if (stmt->var.init) {
                // Infer type from initializer if no explicit type
                if (stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                    is_already_const = true;  // String literals already have const
                } else if (stmt->var.init->type == EXPR_STRING_INTERP) {
                    c_type = "char*";
                    needs_arc_management = true;
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
                } else if (stmt->var.init->type == EXPR_ARRAY) {
                    c_type = "WynArray";
                    needs_arc_management = false;  // Array expression already returns proper value
                } else if (stmt->var.init->type == EXPR_MAP) {
                    // Map type - use the typedef
                    c_type = "WynMap";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_METHOD_CALL) {
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
                                snprintf(method_struct_buf, sizeof(method_struct_buf), "%.*s", sn.length, sn.start);
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
                        
                        // Look up method return type
                        Token method = stmt->var.init->method_call.method;
                        char method_name[64];
                        snprintf(method_name, sizeof(method_name), "%.*s", method.length, method.start);
                        
                        const char* return_type = lookup_method_return_type(receiver_type, method_name);
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
                    // Infer type from match expression's first arm
                    if (stmt->var.init->match.arm_count > 0 && stmt->var.init->match.arms[0].result) {
                        Expr* first_result = stmt->var.init->match.arms[0].result;
                        if (first_result->type == EXPR_STRING) {
                            c_type = "const char*";
                            is_already_const = true;
                        } else if (first_result->type == EXPR_FLOAT) {
                            c_type = "double";
                        } else if (first_result->type == EXPR_BOOL) {
                            c_type = "bool";
                        } else if (first_result->expr_type) {
                            if (first_result->expr_type->kind == TYPE_STRING) {
                                c_type = "const char*";
                                is_already_const = true;
                            } else if (first_result->expr_type->kind == TYPE_FLOAT) {
                                c_type = "double";
                            }
                        }
                    }
                } else if (stmt->var.init->type == EXPR_SPAWN) {
                    // Spawn returns a future
                    c_type = "Future*";
                } else if (stmt->var.init->type == EXPR_STRUCT_INIT) {
                    // Use the struct type name (monomorphic if available)
                    static char struct_type[128];
                    if (stmt->var.init->struct_init.monomorphic_name) {
                        snprintf(struct_type, 128, "%s", stmt->var.init->struct_init.monomorphic_name);
                    } else {
                        // Check if type_name contains a module prefix (from member expression)
                        // e.g., point.Point should become point_Point
                        Token type_name = stmt->var.init->struct_init.type_name;
                        
                        char temp_name[128];
                        snprintf(temp_name, 128, "%.*s", type_name.length, type_name.start);
                        
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
                            snprintf(struct_type, 128, "%.*s", type_name.length, type_name.start);
                        }
                    }
                    c_type = struct_type;
                    needs_arc_management = false;
                } else if (stmt->var.init->type == EXPR_SOME || stmt->var.init->type == EXPR_NONE) {
                    // Optional type
                    c_type = "WynOptional*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_OK || stmt->var.init->type == EXPR_ERR) {
                    // TASK-026: Result type
                    c_type = "WynResult*";
                    needs_arc_management = true;
                } else if (stmt->var.init->type == EXPR_LAMBDA) {
                    // Lambda/closure type - function pointer
                    c_type = "int (*)(int, int)";  // Simplified: assume int params and return
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
                            case TYPE_STRUCT: {
                                // Use the struct type name
                                static char struct_type_buf[256];
                                int len = stmt->var.init->expr_type->struct_type.name.length < 255 ? 
                                         stmt->var.init->expr_type->struct_type.name.length : 255;
                                memcpy(struct_type_buf, stmt->var.init->expr_type->struct_type.name.start, len);
                                struct_type_buf[len] = '\0';
                                c_type = struct_type_buf;
                                break;
                            }
                            default:
                                c_type = "long long";
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
                            Token var_name = stmt->var.init->index.array->token;
                            if ((var_name.length == 4 && memcmp(var_name.start, "args", 4) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "files", 5) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "names", 5) == 0) ||
                                (var_name.length == 5 && memcmp(var_name.start, "parts", 5) == 0) ||
                                (var_name.length == 7 && memcmp(var_name.start, "entries", 7) == 0)) {
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
                    }
                } else if (stmt->var.init->type == EXPR_TUPLE) {
                    // Tuple type - use __auto_type (GCC/Clang extension)
                    c_type = "__auto_type";
                } else if (stmt->var.init->type == EXPR_CALL) {
                    // Function call - check expr_type first, then use __auto_type
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
                            default:
                                c_type = "__auto_type";
                        }
                    } else {
                        c_type = "__auto_type";
                    }
                    // Track if this variable holds a closure (from a function returning fn type)
                    if (stmt->var.init->call.callee->type == EXPR_IDENT && lambda_var_count < 256) {
                        Token call_name = stmt->var.init->call.callee->token;
                        if (current_program) {
                            for (int fi = 0; fi < current_program->count; fi++) {
                                Stmt* fs = current_program->stmts[fi];
                                if (fs->type == STMT_FN && 
                                    fs->fn.name.length == call_name.length &&
                                    memcmp(fs->fn.name.start, call_name.start, call_name.length) == 0 &&
                                    fs->fn.return_type && fs->fn.return_type->type == EXPR_FN_TYPE) {
                                    // This function returns a closure!
                                    snprintf(lambda_var_info[lambda_var_count].var_name, 64, "%.*s",
                                            stmt->var.name.length, stmt->var.name.start);
                                    snprintf(lambda_var_info[lambda_var_count].name, 64, "%.*s",
                                            stmt->var.name.length, stmt->var.name.start);
                                    lambda_var_info[lambda_var_count].name_len = stmt->var.name.length;
                                    lambda_var_info[lambda_var_count].is_closure = true;
                                    lambda_var_info[lambda_var_count].capture_count = 0;
                                    lambda_var_count++;
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
                            c_type = "__auto_type";
                        }
                    } else {
                        // Other binary operations (-, *, /, ==, etc.) - use __auto_type
                        c_type = "__auto_type";
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
                                c_type = "__auto_type";
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
                                // Use struct type name
                                static char struct_type_buf[128];
                                Token type_name = stmt->var.init->expr_type->struct_type.name;
                                snprintf(struct_type_buf, 128, "%.*s", type_name.length, type_name.start);
                                c_type = struct_type_buf;
                                break;
                            }
                            case TYPE_ENUM: {
                                // Use enum type name
                                static char enum_type_buf[128];
                                Token type_name = stmt->var.init->expr_type->name;
                                snprintf(enum_type_buf, 128, "%.*s", type_name.length, type_name.start);
                                c_type = enum_type_buf;
                                break;
                            }
                            case TYPE_ARRAY:
                                c_type = "WynArray";
                                break;
                            default:
                                c_type = "long long";
                        }
                    }
                }
                // ... rest of type determination logic
            }
            
            // Emit variable declaration - avoid double const
            // Special handling for function pointers (lambdas)
            if (stmt->var.init && stmt->var.init->type == EXPR_LAMBDA) {
                // Function pointer syntax: int (*name)(params...)
                // Check if name is a C keyword
                const char* c_keywords[] = {"double", "float", "int", "char", "void", "return", "if", "else", "while", "for", "switch", "case", NULL};
                bool is_c_keyword = false;
                for (int i = 0; c_keywords[i] != NULL; i++) {
                    if (stmt->var.name.length == strlen(c_keywords[i]) && 
                        memcmp(stmt->var.name.start, c_keywords[i], stmt->var.name.length) == 0) {
                        is_c_keyword = true;
                        break;
                    }
                }
                
                // Find this lambda in the lambda_functions array to get capture count
                int total_params = stmt->var.init->lambda.param_count;
                int lambda_idx = -1;
                for (int i = 0; i < lambda_count; i++) {
                    // Match by checking if this is the right lambda (use counter)
                    static int lambda_var_counter = 0;
                    if (i == lambda_var_counter) {
                        total_params += lambda_functions[i].capture_count;
                        lambda_idx = i;
                        lambda_var_counter++;
                        break;
                    }
                }
                
                // Store lambda variable info for call site injection
                if (lambda_idx >= 0 && lambda_var_count < 256) {
                    snprintf(lambda_var_info[lambda_var_count].var_name, 64, "%.*s", 
                            stmt->var.name.length, stmt->var.name.start);
                    lambda_var_info[lambda_var_count].capture_count = lambda_functions[lambda_idx].capture_count;
                    for (int i = 0; i < lambda_functions[lambda_idx].capture_count; i++) {
                        strcpy(lambda_var_info[lambda_var_count].captured_vars[i], 
                               lambda_functions[lambda_idx].captured_vars[i]);
                    }
                    lambda_var_count++;
                }
                
                emit("int (*%s%.*s)(", is_c_keyword ? "_" : "", stmt->var.name.length, stmt->var.name.start);
                for (int i = 0; i < total_params; i++) {
                    if (i > 0) emit(", ");
                    emit("int");
                }
                emit(") = ");
            } else if (stmt->var.is_const && !stmt->var.is_mutable && !is_already_const) {
                emit("const %s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            } else {
                emit("%s %.*s = ", c_type, stmt->var.name.length, stmt->var.name.start);
            }
            
            // Register local variable for scope tracking
            {
                char var_name[256];
                snprintf(var_name, 256, "%.*s", stmt->var.name.length, stmt->var.name.start);
                register_local_variable(var_name);
            }
            
            if (needs_arc_management) {
                emit("({ ");
                codegen_expr(stmt->var.init);
                emit("; /* ARC retain for %.*s */ })", stmt->var.name.length, stmt->var.name.start);
            } else {
                codegen_expr(stmt->var.init);
            }
            emit(";\n");
            
            // Track ARC-managed variables for automatic cleanup
            if (needs_arc_management && !stmt->var.is_const) {
                track_var_with_type(stmt->var.name.start, stmt->var.name.length, c_type);
            }
            break;
        }
        case STMT_RETURN:
            if (in_async_function) {
                emit("*temp = ");
                codegen_expr(stmt->ret.value);
                emit("; goto async_return;\n");
            } else {
                emit("return ");
                codegen_expr(stmt->ret.value);
                emit(";\n");
            }
            break;
        case STMT_BREAK:
            emit("break;\n");
            break;
        case STMT_CONTINUE:
            emit("continue;\n");
            break;
        case STMT_SPAWN: {
            // Spawn: lightweight tasks (not OS threads)
            // Wrapper functions are generated in pre-scan phase
            if (stmt->spawn.call->type == EXPR_CALL && 
                stmt->spawn.call->call.callee->type == EXPR_IDENT &&
                stmt->spawn.call->call.arg_count == 0) {
                
                // Extract function name
                Expr* callee = stmt->spawn.call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                // Generate spawn call
                emit("wyn_spawn(__spawn_wrapper_");
                emit(func_name);
                emit(", NULL);\n");
            } else {
                // Fallback: just call the function
                emit("/* spawn (fallback) */ ");
                codegen_expr(stmt->spawn.call);
                emit(";\n");
            }
            break;
        }
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                emit("    ");
                codegen_stmt(stmt->block.stmts[i]);
            }
            break;
        case STMT_UNSAFE:
            // Unsafe blocks are just regular blocks in C
            emit("/* unsafe */ {\n");
            for (int i = 0; i < stmt->block.count; i++) {
                emit("    ");
                codegen_stmt(stmt->block.stmts[i]);
            }
            emit("}\n");
            break;
        case STMT_FN: {
            // Determine return type
            const char* return_type = "long long"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = stmt->fn.is_async;
            
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
                                if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                                    return_type = "OptionInt";
                                else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "OptionString";
                                else return_type = "OptionInt";
                            } else return_type = "OptionInt";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            if (stmt->fn.return_type->call.arg_count > 0 &&
                                stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = stmt->fn.return_type->call.args[0]->token;
                                if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                                    return_type = "ResultInt";
                                else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "ResultString";
                                else return_type = "ResultInt";
                            } else return_type = "ResultInt";
                        }
                    }
                } else if (stmt->fn.return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
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
                        snprintf(return_type_buf, sizeof(return_type_buf), "%.*s", 
                               type_name.length, type_name.start);
                        return_type = return_type_buf;
                    }
                } else if (stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                    return_type = "WynOptional*";
                } else if (stmt->fn.return_type->type == EXPR_FN_TYPE) {
                    return_type = "WynClosure";
                }
            }
            
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (stmt->fn.name.length == 4 && 
                                   memcmp(stmt->fn.name.start, "main", 4) == 0);
            
            // For async functions, return WynFuture*
            if (is_async) {
                emit("WynFuture* %.*s(", stmt->fn.name.length, stmt->fn.name.start);
            } else if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (stmt->fn.is_extension) {
                // Extension method: fn Type.method() -> Type_method()
                emit("%s %.*s_%.*s(", return_type, 
                     stmt->fn.receiver_type.length, stmt->fn.receiver_type.start,
                     stmt->fn.name.length, stmt->fn.name.start);
            } else {
                emit("%s %.*s(", return_type, stmt->fn.name.length, stmt->fn.name.start);
            }
            for (int i = 0; i < stmt->fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "long long"; // default
                char custom_type_buf[256] = {0};  // Buffer for custom types
                
                // FIX: For extension methods, first parameter (self) gets receiver type
                if (stmt->fn.is_extension && i == 0) {
                    snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s", 
                           stmt->fn.receiver_type.length, stmt->fn.receiver_type.start);
                    param_type = custom_type_buf;
                } else if (stmt->fn.param_types[i]) {
                    if (stmt->fn.param_types[i]->type == EXPR_FN_TYPE) {
                        // Function type: fn(T) -> R becomes function pointer
                        FnTypeExpr* fn_type = &stmt->fn.param_types[i]->fn_type;
                        
                        // Build return type
                        const char* ret_type = "int";
                        if (fn_type->return_type && fn_type->return_type->type == EXPR_IDENT) {
                            Token rt = fn_type->return_type->token;
                            if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) ret_type = "int";
                            else if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret_type = "char*";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret_type = "double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) ret_type = "bool";
                        }
                        
                        // Build parameter types
                        char params_buf[256] = "";
                        for (int j = 0; j < fn_type->param_count; j++) {
                            if (j > 0) strcat(params_buf, ", ");
                            const char* pt = "int";
                            if (fn_type->param_types[j] && fn_type->param_types[j]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[j]->token;
                                if (pt_tok.length == 3 && memcmp(pt_tok.start, "int", 3) == 0) pt = "int";
                                else if (pt_tok.length == 6 && memcmp(pt_tok.start, "string", 6) == 0) pt = "char*";
                                else if (pt_tok.length == 5 && memcmp(pt_tok.start, "float", 5) == 0) pt = "double";
                                else if (pt_tok.length == 4 && memcmp(pt_tok.start, "bool", 4) == 0) pt = "bool";
                            }
                            strcat(params_buf, pt);
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
                        } else {
                            // Assume it's a custom struct type
                            snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s", 
                                   type_name.length, type_name.start);
                            param_type = custom_type_buf;
                        }
                    } else if (stmt->fn.param_types[i]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (stmt->fn.param_types[i]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - use WynOptional* for proper optional handling
                        param_type = "WynOptional*";
                    }
                }
                
                // Emit with pointer for mut params
                bool is_mut_p = stmt->fn.param_mutable && stmt->fn.param_mutable[i];
                if (is_mut_p) {
                    emit("%s *%.*s", param_type, stmt->fn.params[i].length, stmt->fn.params[i].start);
                } else {
                    emit("%s %.*s", param_type, stmt->fn.params[i].length, stmt->fn.params[i].start);
                }
            }
            emit(") {\n");
            push_scope();  // Track allocations in this function
            
            // Register parameters for mut tracking
            clear_parameters();
            clear_local_variables();
            for (int i = 0; i < stmt->fn.param_count; i++) {
                char pname[256];
                snprintf(pname, 256, "%.*s", stmt->fn.params[i].length, stmt->fn.params[i].start);
                bool is_mut_p = stmt->fn.param_mutable && stmt->fn.param_mutable[i];
                register_parameter_mut(pname, is_mut_p);
            }
            
            // Set current function return kind for Ok/Err/Some/None resolution
            const char* prev_fn_return_kind = current_fn_return_kind;
            current_fn_return_kind = NULL;
            if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_CALL &&
                stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
                Token rt = stmt->fn.return_type->call.callee->token;
                if (rt.length == 6 && memcmp(rt.start, "Result", 6) == 0) {
                    // Result<T, E> - check T to determine ResultInt or ResultString
                    if (stmt->fn.return_type->call.arg_count > 0 &&
                        stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                        Token inner = stmt->fn.return_type->call.args[0]->token;
                        if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                            current_fn_return_kind = "ResultInt";
                        else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                            current_fn_return_kind = "ResultString";
                    }
                } else if (rt.length == 6 && memcmp(rt.start, "Option", 6) == 0) {
                    if (stmt->fn.return_type->call.arg_count > 0 &&
                        stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                        Token inner = stmt->fn.return_type->call.args[0]->token;
                        if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                            current_fn_return_kind = "OptionInt";
                        else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                            current_fn_return_kind = "OptionString";
                    }
                }
            } else if (stmt->fn.return_type && stmt->fn.return_type->type == EXPR_IDENT) {
                Token rt = stmt->fn.return_type->token;
                if (rt.length == 9 && memcmp(rt.start, "ResultInt", 9) == 0)
                    current_fn_return_kind = "ResultInt";
                else if (rt.length == 12 && memcmp(rt.start, "ResultString", 12) == 0)
                    current_fn_return_kind = "ResultString";
                else if (rt.length == 9 && memcmp(rt.start, "OptionInt", 9) == 0)
                    current_fn_return_kind = "OptionInt";
                else if (rt.length == 12 && memcmp(rt.start, "OptionString", 12) == 0)
                    current_fn_return_kind = "OptionString";
            }

            // For async functions, wrap the body in a future
            if (is_async) {
                emit("    WynFuture* future = wyn_future_new();\n");
                emit("    %s* temp = malloc(sizeof(%s));\n", return_type, return_type);
                emit("    {\n");
                // Set async context for return statement handling
                bool prev_async = in_async_function;
                in_async_function = true;
                codegen_stmt(stmt->fn.body);
                in_async_function = prev_async;
                emit("    }\n");
                emit("async_return:\n");
                emit("    wyn_future_ready(future, temp);\n");
                emit("    return future;\n");
            } else {
                codegen_stmt(stmt->fn.body);
                // Ensure main function always returns 0 if no explicit return
                bool is_main = (stmt->fn.name.length == 4 && 
                               memcmp(stmt->fn.name.start, "main", 4) == 0);
                if (is_main && !stmt->fn.return_type) {
                    emit("    return 0;\n");
                }
            }
            
            pop_scope();   // Auto-cleanup before function end
            current_fn_return_kind = prev_fn_return_kind;
            emit("}\n\n");
            break;
        }
        case STMT_EXTERN: {
            // Generate extern function declaration
            const char* return_type = "long long"; // default
            if (stmt->extern_fn.return_type && stmt->extern_fn.return_type->type == EXPR_IDENT) {
                Token ret_type = stmt->extern_fn.return_type->token;
                if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                    return_type = "long long";
                } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                    return_type = "char*";
                } else if (ret_type.length == 4 && memcmp(ret_type.start, "void", 4) == 0) {
                    return_type = "void";
                }
            }
            
            emit("extern %s %.*s(", return_type, 
                 stmt->extern_fn.name.length, stmt->extern_fn.name.start);
            
            for (int i = 0; i < stmt->extern_fn.param_count; i++) {
                if (i > 0) emit(", ");
                
                const char* param_type = "int"; // default
                if (stmt->extern_fn.param_types[i] && stmt->extern_fn.param_types[i]->type == EXPR_IDENT) {
                    Token type_name = stmt->extern_fn.param_types[i]->token;
                    if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                        param_type = "const char*";
                    }
                }
                
                emit("%s", param_type);
            }
            
            if (stmt->extern_fn.is_variadic) {
                if (stmt->extern_fn.param_count > 0) emit(", ");
                emit("...");
            }
            
            emit(");\n");
            break;
        }
        case STMT_MACRO: {
            // Generate C macro
            emit("#define %.*s(", stmt->macro.name.length, stmt->macro.name.start);
            for (int i = 0; i < stmt->macro.param_count; i++) {
                if (i > 0) emit(", ");
                emit("%.*s", stmt->macro.params[i].length, stmt->macro.params[i].start);
            }
            emit(") %.*s\n", stmt->macro.body.length, stmt->macro.body.start);
            break;
        }
        case STMT_STRUCT:
            // Skip generic structs - they will be handled by monomorphization
            if (stmt->struct_decl.type_param_count > 0) {
                break;
            }
            
            // T2.5.3: Enhanced struct system with ARC integration
            emit("typedef struct {\n");
            for (int i = 0; i < stmt->struct_decl.field_count; i++) {
                // Convert Wyn type to C type
                const char* c_type = "long long"; // default
                if (stmt->struct_decl.field_types[i]) {
                    if (stmt->struct_decl.field_types[i]->type == EXPR_IDENT) {
                        Token type_name = stmt->struct_decl.field_types[i]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            c_type = "long long";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            c_type = "float";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 3 && memcmp(type_name.start, "str", 3) == 0) {
                            c_type = "const char*"; // Always use simple strings for now
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            c_type = "bool";
                        } else {
                            // Custom struct type - use the type name as-is
                            static char custom_type[64];
                            snprintf(custom_type, 64, "%.*s", type_name.length, type_name.start);
                            c_type = custom_type;
                        }
                    } else if (stmt->struct_decl.field_types[i]->type == EXPR_ARRAY) {
                        // Array field type
                        c_type = "WynArray";
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
            emit("/* Generating %d methods */\n", stmt->struct_decl.method_count);
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
                            emit("int");
                        } else if (ptype.length == 5 && memcmp(ptype.start, "float", 5) == 0) {
                            emit("double");
                        } else if (ptype.length == 6 && memcmp(ptype.start, "string", 6) == 0) {
                            emit("char*");
                        } else {
                            emit("%.*s", ptype.length, ptype.start);
                        }
                    }
                    emit(" %.*s", method->params[j].length, method->params[j].start);
                }
                emit(") ");
                
                // Emit method body
                if (method->body) {
                    emit("{\n");
                    emit("    /* body has %d statements */\n", method->body->block.count);
                    codegen_stmt(method->body);
                    emit("}\n");
                } else {
                    emit("{ /* no body */ }\n");
                }
                emit("\n");
            }
            
            break;
        case STMT_ENUM:
            // Check if any variant has data
            bool has_data = false;
            for (int i = 0; i < stmt->enum_decl.variant_count; i++) {
                if (stmt->enum_decl.variant_type_counts[i] > 0) {
                    has_data = true;
                    break;
                }
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
                        // For now, only handle single-field variants
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" %.*s_value;\n",
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
                        // Only handle single-field variants for now
                        if (stmt->enum_decl.variant_type_counts[i] == 1) {
                            Expr* type_expr = stmt->enum_decl.variant_types[i][0];
                            emit_type_from_expr(type_expr);
                            emit(" value");
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
        case STMT_TYPE_ALIAS:
            // typedef target_type alias_name;
            emit("typedef %.*s %.*s;\n\n",
                 stmt->type_alias.target.length,
                 stmt->type_alias.target.start,
                 stmt->type_alias.name.length,
                 stmt->type_alias.name.start);
            break;
        case STMT_IMPL:
            for (int i = 0; i < stmt->impl.method_count; i++) {
                FnStmt* method = stmt->impl.methods[i];
                
                // Determine return type
                const char* return_type = "long long";
                if (method->return_type && method->return_type->type == EXPR_IDENT) {
                    Token ret_type = method->return_type->token;
                    if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                        return_type = "long long";
                    } else if (ret_type.length == 5 && memcmp(ret_type.start, "float", 5) == 0) {
                        return_type = "double";
                    } else if (ret_type.length == 4 && memcmp(ret_type.start, "bool", 4) == 0) {
                        return_type = "bool";
                    } else if (ret_type.length == 6 && memcmp(ret_type.start, "string", 6) == 0) {
                        return_type = "const char*";
                    }
                }
                
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int j = 0; j < method->param_count; j++) {
                    if (j > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "int";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[j]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "int";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else {
                            // Custom struct type
                            snprintf(custom_type_buf, sizeof(custom_type_buf), "%.*s",
                                   type_name.length, type_name.start);
                            param_type = custom_type_buf;
                        }
                    }
                    
                    emit("%s %.*s", param_type, method->params[j].length, method->params[j].start);
                }
                emit(") {\n");
                push_scope();
                codegen_stmt(method->body);
                pop_scope();
                emit("}\n\n");
            }
            break;
        case STMT_TRAIT:
            // Generate trait definition as C interface
            emit("// trait %.*s\n", stmt->trait_decl.name.length, stmt->trait_decl.name.start);
            for (int i = 0; i < stmt->trait_decl.method_count; i++) {
                FnStmt* method = stmt->trait_decl.methods[i];
                if (!stmt->trait_decl.method_has_default[i]) {
                    // Generate function pointer typedef for trait method
                    emit("typedef ");
                    if (method->return_type) {
                        if (method->return_type->type == EXPR_IDENT) {
                            Token ret_type = method->return_type->token;
                            if (ret_type.length == 3 && memcmp(ret_type.start, "str", 3) == 0) {
                                emit("char*");
                            } else if (ret_type.length == 3 && memcmp(ret_type.start, "int", 3) == 0) {
                                emit("int");
                            } else {
                                emit("void*");
                            }
                        } else {
                            emit("void*");
                        }
                    } else {
                        emit("void");
                    }
                    emit(" (*%.*s_%.*s_fn)(void*", 
                         stmt->trait_decl.name.length, stmt->trait_decl.name.start,
                         method->name.length, method->name.start);
                    for (int j = 0; j < method->param_count; j++) {
                        emit(", ");
                        if (method->param_types && method->param_types[j] && method->param_types[j]->type == EXPR_IDENT) {
                            Token param_type = method->param_types[j]->token;
                            if (param_type.length == 3 && memcmp(param_type.start, "str", 3) == 0) {
                                emit("char*");
                            } else if (param_type.length == 3 && memcmp(param_type.start, "int", 3) == 0) {
                                emit("int");
                            } else {
                                emit("void*");
                            }
                        } else {
                            emit("void*");
                        }
                    }
                    emit(");\n");
                }
            }
            break;
        case STMT_IF:
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
                // Generate for-in loop: for item in array
                emit("{\n");
                push_scope();
                emit("    WynArray __iter_array = ");
                codegen_expr(stmt->for_stmt.array_expr);
                emit(";\n");
                emit("    for (int __i = 0; __i < __iter_array.count; __i++) {\n");
                // Use a generic approach - check element type at runtime
                emit("        WynValue __elem = __iter_array.data[__i];\n");
                emit("        ");
                
                // Determine variable type based on array expression type
                bool is_string_array = false;
                bool is_struct_array = false;
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
                    }
                }
                
                // Check if it's a method call that returns string array
                if (!is_string_array && !is_struct_array && stmt->for_stmt.array_expr->type == EXPR_METHOD_CALL) {
                    Token method = stmt->for_stmt.array_expr->method_call.method;
                    if ((method.length == 5 && memcmp(method.start, "split", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "lines", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "words", 5) == 0) ||
                        (method.length == 5 && memcmp(method.start, "chars", 5) == 0)) {
                        is_string_array = true;
                    }
                }
                
                // Fallback: check variable name heuristics
                if (!is_string_array && !is_struct_array) {
                    const char* var_name = stmt->for_stmt.loop_var.start;
                    int var_len = stmt->for_stmt.loop_var.length;
                    if ((var_len >= 4 && strncmp(var_name, "name", 4) == 0) ||
                        (var_len >= 3 && strncmp(var_name, "str", 3) == 0) ||
                        (var_len >= 4 && strncmp(var_name, "text", 4) == 0) ||
                        (var_len >= 4 && strncmp(var_name, "line", 4) == 0) ||
                        (var_len >= 4 && strncmp(var_name, "word", 4) == 0) ||
                        (var_len >= 4 && strncmp(var_name, "part", 4) == 0) ||
                        (var_len >= 4 && strncmp(var_name, "char", 4) == 0) ||
                        (var_len == 1 && var_name[0] == 's')) {
                        is_string_array = true;
                    }
                }
                
                if (is_struct_array && elem_type) {
                    Token type_name = elem_type->struct_type.name;
                    emit("%.*s %.*s = *(%.*s*)__elem.data.struct_val;\n",
                         type_name.length, type_name.start,
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start,
                         type_name.length, type_name.start);
                } else if (is_string_array) {
                    emit("const char* %.*s = (__elem.type == WYN_TYPE_STRING) ? __elem.data.string_val : \"\";\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
                } else {
                    emit("long long %.*s = (__elem.type == WYN_TYPE_INT) ? __elem.data.int_val : 0;\n",
                         stmt->for_stmt.loop_var.length, stmt->for_stmt.loop_var.start);
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
            char module_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s", 
                    stmt->import.module.length, stmt->import.module.start);
            
            // Handle selective imports: import { get, post } from module
            if (stmt->import.item_count > 0) {
                // Register each imported item with module prefix
                for (int i = 0; i < stmt->import.item_count; i++) {
                    char item_name[256];
                    snprintf(item_name, sizeof(item_name), "%.*s",
                            stmt->import.items[i].length, stmt->import.items[i].start);
                    
                    // Map item_name -> module_name::item_name
                    char full_name[512];
                    const char* c_mod = module_to_c_ident(module_name);
                    snprintf(full_name, sizeof(full_name), "%s_%s", c_mod, item_name);
                    register_module_alias(item_name, full_name);
                }
            }
            
            // Register alias if present
            if (stmt->import.alias.start != NULL) {
                char alias_name[256];
                snprintf(alias_name, sizeof(alias_name), "%.*s",
                        stmt->import.alias.length, stmt->import.alias.start);
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
                    
                    extern int get_all_modules_raw(void** out_modules, int max_count);
                    void* all_modules_raw[64];
                    int module_count = get_all_modules_raw(all_modules_raw, 64);
                    
                    for (int m = 0; m < module_count; m++) {
                        typedef struct { char* name; Program* ast; } ModuleEntry;
                        ModuleEntry* mod = (ModuleEntry*)all_modules_raw[m];
                        
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
                                char func_name[256];
                                snprintf(func_name, 256, "%.*s", s->fn.name.length, s->fn.name.start);
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
                                
                                // Determine return type
                                const char* return_type = "long long";
                                static char custom_ret_type[128];
                                if (s->fn.return_type) {
                                    if (s->fn.return_type->type == EXPR_IDENT) {
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
                                    const char* param_type = "int";
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
                                                param_type = "int";
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
                        
                        // Second: emit constants
                        for (int i = 0; i < mod->ast->count; i++) {
                            Stmt* s = mod->ast->stmts[i];
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
                                
                                // Determine type
                                if (var_stmt->init) {
                                    if (var_stmt->init->type == EXPR_STRING) {
                                        emit("const char* %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else if (var_stmt->init->type == EXPR_FLOAT) {
                                        emit("double %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else if (var_stmt->init->type == EXPR_BOOL) {
                                        emit("bool %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    } else {
                                        emit("int %s_%.*s = ", c_mod_name, var_stmt->name.length, var_stmt->name.start);
                                    }
                                    codegen_expr(var_stmt->init);
                                    emit(";\n");
                                } else {
                                    // No initializer - default to 0
                                    emit("int %s_%.*s = 0;\n", c_mod_name, var_stmt->name.length, var_stmt->name.start);
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
        case STMT_TRY: {
            // TASK-026: Enhanced try/catch implementation with multiple catch blocks
            emit("{\n");
            emit("    jmp_buf exception_buf;\n");
            emit("    const char* exception_msg = NULL;\n");
            emit("    int exception_type = 0;\n");
            emit("    if (setjmp(exception_buf) == 0) {\n");
            emit("        // Try block\n");
            emit("        current_exception_buf = &exception_buf;\n");
            emit("        current_exception_msg = &exception_msg;\n");
            codegen_stmt(stmt->try_stmt.try_block);
            emit("    } else {\n");
            emit("        // Catch blocks\n");
            
            // Generate multiple catch blocks
            for (int i = 0; i < stmt->try_stmt.catch_count; i++) {
                if (i > 0) emit("        } else ");
                emit("        if (exception_type == %d) {\n", i);
                emit("            const char* %.*s = exception_msg;\n", 
                     stmt->try_stmt.exception_vars[i].length, 
                     stmt->try_stmt.exception_vars[i].start);
                codegen_stmt(stmt->try_stmt.catch_blocks[i]);
            }
            
            if (stmt->try_stmt.catch_count > 0) {
                emit("        }\n");
            }
            
            emit("    }\n");
            
            // Finally block
            if (stmt->try_stmt.finally_block) {
                emit("    // Finally block\n");
                codegen_stmt(stmt->try_stmt.finally_block);
            }
            
            emit("}\n");
            break;
        }
        case STMT_CATCH: {
            // TASK-026: Standalone catch statement
            emit("// Catch block for %.*s %.*s\n",
                 stmt->catch_stmt.exception_type.length, stmt->catch_stmt.exception_type.start,
                 stmt->catch_stmt.exception_var.length, stmt->catch_stmt.exception_var.start);
            codegen_stmt(stmt->catch_stmt.body);
            break;
        }
        case STMT_THROW:
            // Proper throw implementation
            emit("if (current_exception_buf && current_exception_msg) {\n");
            emit("    *current_exception_msg = ");
            codegen_expr(stmt->throw_stmt.value);
            emit(";\n");
            emit("    longjmp(*current_exception_buf, 1);\n");
            emit("} else {\n");
            emit("    printf(\"Uncaught exception: %%s\\n\", ");
            codegen_expr(stmt->throw_stmt.value);
            emit(");\n");
            emit("    exit(1);\n");
            emit("}\n");
            break;
        case STMT_MATCH:  // T1.4.4: Control Flow Agent addition
            codegen_match_statement(stmt);
            break;
        case STMT_CONST: {
            // Module-level constants - emit as static const
            const char* c_type = "long long";
            bool type_has_const = false;
            
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
