// codegen_program.c - Program-level code generation
// Included from codegen.c - shares all statics

void codegen_program(Program* prog) {
    current_program = prog;
    bool has_main = false;
    bool has_math_import = false;
    
    // Reset module emission flag for this compilation
    modules_emitted_this_compilation = false;
    
    // Reset lambda collection
    lambda_count = 0;
    lambda_id_counter = 0;
    lambda_ref_counter = 0;
    
    // Reset spawn wrapper collection
    spawn_wrapper_count = 0;
    
    // PASS 1: Pre-scan to collect all lambdas
    // We need to emit lambda functions before they're used
    // So we do a quick scan to find and generate them first
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            // Scan function body for lambdas
            scan_for_lambdas(prog->stmts[i]->fn.body);
        }
    }
    
    // Check if math module is imported
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            ImportStmt* import = &prog->stmts[i]->import;
            if (import->module.length == 4 && memcmp(import->module.start, "math", 4) == 0) {
                has_math_import = true;
                break;
            }
        }
    }
    
    // Math functions are now handled by the module system
    
    // Collect generic instantiations (but don't generate yet)
    wyn_collect_generic_instantiations_from_program(prog);
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            if (prog->stmts[i]->fn.name.length == 4 &&
                memcmp(prog->stmts[i]->fn.name.start, "main", 4) == 0) {
                has_main = true;
            }
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            if (prog->stmts[i]->export.stmt->fn.name.length == 4 &&
                memcmp(prog->stmts[i]->export.stmt->fn.name.start, "main", 4) == 0) {
                has_main = true;
            }
        }
    }
    
    // Generate all structs, enums, and type aliases first
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT || prog->stmts[i]->type == STMT_ENUM || prog->stmts[i]->type == STMT_TYPE_ALIAS || prog->stmts[i]->type == STMT_TRAIT) {
            if (prog->stmts[i]->type == STMT_TRAIT) {
                register_trait_name(prog->stmts[i]->trait_decl.name.start, prog->stmts[i]->trait_decl.name.length);
            }
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate module-level constants (only if has main — script mode puts them in wyn_main)
    if (has_main) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_CONST) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    }
    
    // Generate global variables (only if has main)
    if (has_main) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_VAR) {
            Stmt* var_stmt = prog->stmts[i];
            // Determine C type from initializer
            const char* c_type = "long long";
            if (var_stmt->var.init) {
                if (var_stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                } else if (var_stmt->var.init->type == EXPR_FLOAT) {
                    c_type = "double";
                } else if (var_stmt->var.init->type == EXPR_BOOL) {
                    c_type = "long long";
                } else if (var_stmt->var.init->type == EXPR_ARRAY) {
                    c_type = "WynArray";
                } else if (var_stmt->var.init->type == EXPR_STRUCT_INIT) {
                    // Use struct type name
                    emit("\n");
                    Token sname = var_stmt->var.init->struct_init.type_name;
                    emit("%.*s %.*s = ", sname.length, sname.start,
                         var_stmt->var.name.length, var_stmt->var.name.start);
                    codegen_expr(var_stmt->var.init);
                    emit(";\n");
                    continue;
                } else if (var_stmt->var.init->type == EXPR_CALL) {
                    // Function call init — check for HashMap.new() etc.
                    c_type = "WynHashMap*";
                }
                // Check explicit type annotation
                if (var_stmt->var.type && var_stmt->var.type->type == EXPR_IDENT) {
                    Token tn = var_stmt->var.type->token;
                    if (tn.length == 6 && memcmp(tn.start, "string", 6) == 0) c_type = "const char*";
                    else if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) c_type = "double";
                    else if (tn.length == 4 && memcmp(tn.start, "bool", 4) == 0) c_type = "long long";
                }
            }
            emit("\n");
            emit("%s %.*s", c_type, var_stmt->var.name.length, var_stmt->var.name.start);
            if (var_stmt->var.init) {
                emit(" = ");
                codegen_expr(var_stmt->var.init);
            } else {
                emit(" = 0");
            }
            emit(";\n");
        }
    }
    } // end if (has_main) for global vars
    
    // Generate forward declarations for struct methods
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_STRUCT) {
            Stmt* stmt = prog->stmts[i];
            for (int j = 0; j < stmt->struct_decl.method_count; j++) {
                FnStmt* method = stmt->struct_decl.methods[j];
                
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
                
                int start_param = 0;
                if (method->param_count > 0 && 
                    method->params[0].length == 4 && 
                    memcmp(method->params[0].start, "self", 4) == 0) {
                    start_param = 1;
                }
                
                for (int k = start_param; k < method->param_count; k++) {
                    emit(", ");
                    if (method->param_types[k] && method->param_types[k]->type == EXPR_IDENT) {
                        Token ptype = method->param_types[k]->token;
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
                    emit(" %.*s", method->params[k].length, method->params[k].start);
                }
                emit(");\n");
            }
        }
    }
    emit("\n");
    
    // Generate extern declarations
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_EXTERN) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Generate macros
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_MACRO) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Emit lambda functions that were collected in pre-scan
    if (lambda_count > 0) {
        emit("// Lambda functions\n");
        for (int i = 0; i < lambda_count; i++) {
            emit("%s\n", lambda_functions[i].code);
        }
        emit("\n");
    }
    
    // Pre-declare lambda functions with generic signatures
    // Actual definitions will be emitted right after this
    emit("// Lambda functions (defined before use)\n");
    // Don't emit forward declarations - just emit definitions here
    // But we can't because lambdas aren't collected yet!
    
    // Generate monomorphic instances of generic functions (after structs are defined)
    wyn_generate_monomorphic_instances_for_codegen(prog);
    
    // Generate impl blocks (extension methods)
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPL) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    
    // Process import and export statements
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT) {
            codegen_stmt(prog->stmts[i]);
        } else if (prog->stmts[i]->type == STMT_EXPORT) {
            // Generate export comment only (the function will be generated later)
            emit("// export ");
            if (prog->stmts[i]->export.stmt && prog->stmts[i]->export.stmt->type == STMT_FN) {
                emit("%.*s\n", prog->stmts[i]->export.stmt->fn.name.length, prog->stmts[i]->export.stmt->fn.name.start);
            } else {
                emit("statement\n");
            }
        }
    }
    
    // Generate forward declarations for all functions
    for (int i = 0; i < prog->count; i++) {
        FnStmt* fn = NULL;
        
        if (prog->stmts[i]->type == STMT_FN) {
            fn = &prog->stmts[i]->fn;
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            fn = &prog->stmts[i]->export.stmt->fn;
        }
        
        if (fn) {
            // Skip generic functions - they will be handled by monomorphization
            if (fn->type_param_count > 0) {
                continue;
            }
            
            // Determine return type
            const char* return_type = fn->return_type ? "long long" : "void"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = fn->is_async;
            
            if (fn->return_type) {
                if (fn->return_type->type == EXPR_CALL) {
                    // Generic type instantiation: HashMap<K,V>, Option<T>, Result<T,E>
                    if (fn->return_type->call.callee->type == EXPR_IDENT) {
                        Token type_name = fn->return_type->call.callee->token;
                        if (type_name.length == 7 && memcmp(type_name.start, "HashMap", 7) == 0) {
                            return_type = "WynHashMap*";
                        } else if (type_name.length == 7 && memcmp(type_name.start, "HashSet", 7) == 0) {
                            return_type = "WynHashSet*";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Option", 6) == 0) {
                            // Resolve Option<int> -> OptionInt, Option<string> -> OptionString
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                                    return_type = "OptionInt";
                                else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "OptionString";
                                else return_type = "OptionInt";
                            } else return_type = "OptionInt";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            // Resolve Result<int, string> -> ResultInt, Result<string, string> -> ResultString
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)
                                    return_type = "ResultInt";
                                else if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "ResultString";
                                else return_type = "ResultInt";
                            } else return_type = "ResultInt";
                        }
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
                } else if (fn->return_type->type == EXPR_IDENT) {
                    Token type_name = fn->return_type->token;
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
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    return_type = "WynOptional*";
                } else if (fn->return_type->type == EXPR_FN_TYPE) {
                    // Function type: fn(int) -> int becomes WynClosure
                    return_type = "WynClosure";
                }
            }
            
            // Generate forward declaration
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (fn->name.length == 4 && 
                                   memcmp(fn->name.start, "main", 4) == 0);
            
            // Function forward declaration
            if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (fn->is_extension) {
                // Extension method: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     fn->receiver_type.length, fn->receiver_type.start,
                     fn->name.length, fn->name.start);
            } else {
                char _fn_name[256]; snprintf(_fn_name, sizeof(_fn_name), "%.*s", fn->name.length, fn->name.start);
                const char* _ckw[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                bool _is_ckw = false; for (int _k = 0; _ckw[_k]; _k++) { if (strcmp(_fn_name, _ckw[_k]) == 0) { _is_ckw = true; break; } }
                emit("%s %s%.*s(", return_type, _is_ckw ? "_" : "", fn->name.length, fn->name.start);
            }
            for (int j = 0; j < fn->param_count; j++) {
                if (j > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "long long"; // default
                char struct_type_name[256] = {0};
                bool is_struct_type = false;
                
                // Extension method self parameter: use receiver type
                if (fn->is_extension && j == 0 && !fn->param_types[j]) {
                    snprintf(struct_type_name, sizeof(struct_type_name), "%.*s",
                            fn->receiver_type.length, fn->receiver_type.start);
                    param_type = struct_type_name;
                    is_struct_type = true;
                } else if (fn->param_types[j]) {
                    if (fn->param_types[j]->type == EXPR_FN_TYPE) {
                        // Function type: fn(T) -> R becomes function pointer
                        FnTypeExpr* fn_type = &fn->param_types[j]->fn_type;
                        
                        // Build return type
                        const char* ret_type = "long long";
                        if (fn_type->return_type && fn_type->return_type->type == EXPR_IDENT) {
                            Token rt = fn_type->return_type->token;
                            if (rt.length == 3 && memcmp(rt.start, "int", 3) == 0) ret_type = "long long";
                            else if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) ret_type = "char*";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) ret_type = "double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) ret_type = "bool";
                        }
                        
                        // Build parameter types
                        char params_buf[256] = "";
                        for (int k = 0; k < fn_type->param_count; k++) {
                            if (k > 0) strcat(params_buf, ", ");
                            const char* pt = "long long";
                            if (fn_type->param_types[k] && fn_type->param_types[k]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[k]->token;
                                if (pt_tok.length == 3 && memcmp(pt_tok.start, "int", 3) == 0) pt = "long long";
                                else if (pt_tok.length == 6 && memcmp(pt_tok.start, "string", 6) == 0) pt = "char*";
                                else if (pt_tok.length == 5 && memcmp(pt_tok.start, "float", 5) == 0) pt = "double";
                                else if (pt_tok.length == 4 && memcmp(pt_tok.start, "bool", 4) == 0) pt = "bool";
                            }
                            strcat(params_buf, pt);
                        }
                        
                        // Generate function pointer type: ret_type (*param_name)(params)
                        emit("%s (*%.*s)(", ret_type, fn->params[j].length, fn->params[j].start);
                        emit("%s)", params_buf);
                        continue; // Skip the normal emit below
                    } else if (fn->param_types[j]->type == EXPR_IDENT) {
                        Token type_name = fn->param_types[j]->token;
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
                            // Assume it's a struct type
                            snprintf(struct_type_name, sizeof(struct_type_name), "%.*s", 
                                    type_name.length, type_name.start);
                            param_type = struct_type_name;
                            is_struct_type = true;
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Handle optional types - use WynOptional* for proper optional handling
                        param_type = "WynOptional*";
                    }
                }
                
                // Emit with pointer for mut params
                bool is_mut_param = fn->param_mutable && fn->param_mutable[j];
                if (is_mut_param) {
                    emit("%s *%.*s", param_type, fn->params[j].length, fn->params[j].start);
                } else {
                    emit("%s %.*s", param_type, fn->params[j].length, fn->params[j].start);
                }
            }
            emit(");\n");
        }
    }
    
    // Generate forward declarations for impl block methods
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPL) {
            Stmt* stmt = prog->stmts[i];
            for (int j = 0; j < stmt->impl.method_count; j++) {
                FnStmt* method = stmt->impl.methods[j];
                
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
                
                // Generate forward declaration: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int k = 0; k < method->param_count; k++) {
                    if (k > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "int";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[k] && method->param_types[k]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[k]->token;
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
                    
                    emit("%s %.*s", param_type, method->params[k].length, method->params[k].start);
                }
                emit(");\n");
            }
        }
    }
    emit("\n");
    
    // Vtable wrappers and instances are generated inline during main codegen
    // (after trait and impl statements have been processed)
    
    // Emit spawn wrapper functions (after forward declarations)
    if (spawn_wrapper_count > 0) {
        emit("// Spawn wrapper functions\n");
        for (int i = 0; i < spawn_wrapper_count; i++) {
            int ac = spawn_wrappers[i].arg_count;
            if (ac == 0) {
                emit("void* __spawn_wrapper_%s(void* arg) {\n", spawn_wrappers[i].func_name);
                emit("    long long* result = malloc(sizeof(long long));\n");
                emit("    *result = %s();\n", spawn_wrappers[i].func_name);
                emit("    return result;\n");
                emit("}\n\n");
            } else {
                emit("void* __spawn_wrapper_%s_%d(void* arg) {\n", spawn_wrappers[i].func_name, ac);
                emit("    struct { ");
                for (int j = 0; j < ac; j++) emit("long long a%d; ", j);
                emit("} *args = arg;\n");
                emit("    long long* result = malloc(sizeof(long long));\n");
                emit("    *result = (long long)%s(", spawn_wrappers[i].func_name);
                for (int j = 0; j < ac; j++) {
                    if (j > 0) emit(", ");
                    emit("args->a%d", j);
                }
                emit(");\n");
                emit("    free(args);\n");
                emit("    return result;\n");
                emit("}\n\n");
            }
        }
    }
    
    // Lambda functions will be emitted at the end of the program
    
    // Generate all functions
    for (int i = 0; i < prog->count; i++) {
        FnStmt* fn = NULL;
        
        if (prog->stmts[i]->type == STMT_FN) {
            fn = &prog->stmts[i]->fn;
        } else if (prog->stmts[i]->type == STMT_EXPORT && 
                   prog->stmts[i]->export.stmt && 
                   prog->stmts[i]->export.stmt->type == STMT_FN) {
            fn = &prog->stmts[i]->export.stmt->fn;
        }
        
        if (fn) {
            // Skip generic functions - they will be handled by monomorphization
            if (fn->type_param_count > 0) {
                continue;
            }
            
            if (prog->stmts[i]->type == STMT_EXPORT) {
                codegen_stmt(prog->stmts[i]->export.stmt);
            } else {
                codegen_stmt(prog->stmts[i]);
            }
        }
    }
    
    // If no main function, create one that executes all statements
    if (!has_main) {
        emit("long long wyn_main() {\n");
        
        // Special case: single expression should return its value
        if (prog->count == 1 && prog->stmts[0]->type == STMT_EXPR) {
            // Check if the expression is a function call that returns void
            Expr* expr = prog->stmts[0]->expr;
            if (expr->type == EXPR_CALL) {
                // Function calls are statements, not return values
                emit("    ");
                codegen_stmt(prog->stmts[0]);
                emit("    return 0;\n");
            } else {
                // Other expressions can be returned
                emit("    return ");
                codegen_expr(prog->stmts[0]->expr);
                emit(";\n");
            }
        } else {
            // Multiple statements or non-expression statements
            for (int i = 0; i < prog->count; i++) {
                if (prog->stmts[i]->type != STMT_FN && prog->stmts[i]->type != STMT_STRUCT && prog->stmts[i]->type != STMT_ENUM && prog->stmts[i]->type != STMT_TRAIT && prog->stmts[i]->type != STMT_IMPL && prog->stmts[i]->type != STMT_EXPORT) {
                    emit("    ");
                    codegen_stmt(prog->stmts[i]);
                }
            }
            emit("    return 0;\n");
        }
        emit("}\n");
    } else {
        // User defined main() is renamed to wyn_main during codegen
        // wyn_wrapper.c provides the actual main() that calls wyn_main()
    }
    
    // Note: main() wrapper is provided by wyn_wrapper.c, not generated here
}

// T1.4.4: Control Flow Code Generation - Control Flow Agent addition
void codegen_match_statement(Stmt* stmt) {
    if (!stmt || stmt->type != STMT_MATCH) return;
    
    // Check if this is a simple integer match that can use switch
    bool can_use_switch = true;
    bool has_wildcard = false;
    
    for (int i = 0; i < stmt->match_stmt.case_count; i++) {
        MatchCase* match_case = &stmt->match_stmt.cases[i];
        if (match_case->pattern->type == PATTERN_WILDCARD) {
            has_wildcard = true;
        } else if (match_case->pattern->type != PATTERN_LITERAL) {
            can_use_switch = false;
            break;
        } else if (match_case->pattern->literal.value.type != TOKEN_INT) {
            can_use_switch = false;
            break;
        }
        if (match_case->guard) {
            can_use_switch = false;
            break;
        }
    }
    
    if (can_use_switch) {
        // Generate C switch statement for simple integer matching
        emit("switch (");
        codegen_expr(stmt->match_stmt.value);
        emit(") {\n");
        
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            
            if (match_case->pattern->type == PATTERN_LITERAL) {
                emit("        case %.*s: ", 
                     match_case->pattern->literal.value.length,
                     match_case->pattern->literal.value.start);
                
                if (match_case->body) {
                    codegen_stmt(match_case->body);
                }
                emit(" break;\n");
            } else if (match_case->pattern->type == PATTERN_WILDCARD) {
                emit("        default: ");
                if (match_case->body) {
                    codegen_stmt(match_case->body);
                }
                emit(" break;\n");
            }
        }
        
        emit("    }\n");
    } else {
        // Generate if-else chain for complex patterns
        emit("{\n");
        
        // Determine the type of the match value
        bool is_enum_match = false;
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            if (match_case->pattern->type == PATTERN_OPTION && 
                match_case->pattern->option.variant_name.length > 0) {
                is_enum_match = true;
                break;
            }
        }
        
        if (is_enum_match) {
            // For enum matches, store the whole enum value
            emit("    __auto_type __match_val = ");
        } else {
            emit("    int __match_val = ");
        }
        codegen_expr(stmt->match_stmt.value);
        emit(";\n");
        
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* match_case = &stmt->match_stmt.cases[i];
            
            if (i == 0) {
                emit("    if (");
            } else {
                emit("    } else if (");
            }
            
            // Handle different pattern types
            if (match_case->pattern->type == PATTERN_LITERAL) {
                emit("__match_val == %.*s", 
                     match_case->pattern->literal.value.length,
                     match_case->pattern->literal.value.start);
            } else if (match_case->pattern->type == PATTERN_WILDCARD) {
                emit("1"); // Always true for wildcard
            } else if (match_case->pattern->type == PATTERN_OPTION) {
                if (match_case->pattern->option.variant_name.length > 0) {
                    // Check if this is a simple enum (no data) or tagged union
                    if (!match_case->pattern->option.is_some && 
                        match_case->pattern->option.enum_name.length > 0) {
                        // Simple enum variant: Color::Red -> Color_Red
                        emit("__match_val == %.*s_%.*s",
                             match_case->pattern->option.enum_name.length,
                             match_case->pattern->option.enum_name.start,
                             match_case->pattern->option.variant_name.length,
                             match_case->pattern->option.variant_name.start);
                    } else if (match_case->pattern->option.enum_name.length > 0) {
                        // Tagged union with full name: Result::Ok(x)
                        emit("__match_val.tag == %.*s_%.*s_TAG",
                             match_case->pattern->option.enum_name.length,
                             match_case->pattern->option.enum_name.start,
                             match_case->pattern->option.variant_name.length,
                             match_case->pattern->option.variant_name.start);
                    } else {
                        // Only variant name: Ok
                        emit("__match_val.tag == %.*s_TAG",
                             match_case->pattern->option.variant_name.length,
                             match_case->pattern->option.variant_name.start);
                    }
                } else if (match_case->pattern->option.is_some) {
                    emit("wyn_optional_is_some(__match_val)");
                } else {
                    emit("wyn_optional_is_none(__match_val)");
                }
            } else if (match_case->pattern->type == PATTERN_IDENT) {
                // Check if this looks like an enum variant (contains underscore)
                Token var_name = match_case->pattern->ident.name;
                bool is_enum_variant = false;
                for (int j = 0; j < var_name.length; j++) {
                    if (var_name.start[j] == '_') {
                        is_enum_variant = true;
                        break;
                    }
                }
                if (is_enum_variant) {
                    emit("__match_val == %.*s", var_name.length, var_name.start);
                } else {
                    emit("1"); // Variable binding always matches
                }
            } else {
                emit("0"); // Unsupported pattern
            }
            
            // Add guard clause if present
            if (match_case->guard) {
                emit(" && (");
                codegen_expr(match_case->guard);
                emit(")");
            }
            
            emit(") {\n");
            
            // Generate variable bindings for patterns that need them
            if (match_case->pattern->type == PATTERN_IDENT) {
                Token var_name = match_case->pattern->ident.name;
                // Only create binding if not an enum variant
                bool is_enum_variant = false;
                for (int j = 0; j < var_name.length; j++) {
                    if (var_name.start[j] == '_') {
                        is_enum_variant = true;
                        break;
                    }
                }
                if (!is_enum_variant) {
                    emit("        int %.*s = __match_val;\n", var_name.length, var_name.start);
                }
            } else if (match_case->pattern->type == PATTERN_OPTION && 
                       match_case->pattern->option.inner &&
                       match_case->pattern->option.inner->type == PATTERN_IDENT) {
                
                Token var_name = match_case->pattern->option.inner->ident.name;
                
                if (match_case->pattern->option.variant_name.length > 0) {
                    // Enum variant destructuring: extract from data union
                    emit("        __auto_type %.*s = __match_val.data.%.*s_value;\n", 
                         var_name.length, var_name.start,
                         match_case->pattern->option.variant_name.length,
                         match_case->pattern->option.variant_name.start);
                } else if (match_case->pattern->option.is_some) {
                    emit("        int %.*s = wyn_optional_unwrap(__match_val);\n", 
                         var_name.length, var_name.start);
                }
            }
            
            // Generate case body
            if (match_case->body) {
                emit("        ");
                codegen_stmt(match_case->body);
            }
        }
        
        // Close the if-else chain
        if (stmt->match_stmt.case_count > 0) {
            emit("    }\n");
        }
        
        emit("}\n");
    }
}
