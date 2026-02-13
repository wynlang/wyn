// codegen_expr.c - Expression code generation
// Included from codegen.c - shares all statics

void codegen_expr(Expr* expr) {
    if (!expr) return;
    
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
            memcpy(temp_ident, expr->token.start, expr->token.length);
            temp_ident[expr->token.length] = '\0';
            
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
                
                // Special handling for Time:: module - map to time_ prefix
                if (strcmp(temp_ident, "Time") == 0) {
                    snprintf(temp_ident, sizeof(temp_ident), "time_%s", function_part);
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
                const char* resolved = resolve_module_alias(temp_ident);
                // Rebuild identifier with resolved module name
                snprintf(temp_ident, sizeof(temp_ident), "%s.%s", resolved, function_part);
            }
            
            // Check if this is a C keyword that needs prefix
            const char* c_keywords[] = {"double", "float", "int", "char", "void", "return", "if", "else", "while", "for", NULL};
            bool is_c_keyword = false;
            for (int i = 0; c_keywords[i] != NULL; i++) {
                if (strlen(temp_ident) == strlen(c_keywords[i]) && 
                    strcmp(temp_ident, c_keywords[i]) == 0) {
                    is_c_keyword = true;
                    ident[0] = '_';
                    offset = 1;
                    break;
                }
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
            emit("%.*s", expr->token.length, expr->token.start);
            break;
        case EXPR_UNARY:
            if (expr->unary.op.type == TOKEN_NOT) {
                emit("!");
            } else {
                emit("%.*s", expr->unary.op.length, expr->unary.op.start);
            }
            codegen_expr(expr->unary.operand);
            break;
        case EXPR_AWAIT:
            // Await: get result from future, handle NULL safely
            emit("({ void* __fr = future_get((Future*)(intptr_t)");
            codegen_expr(expr->await.expr);
            emit("); __fr ? *(int*)__fr : 0; })");
            break;
        case EXPR_BINARY:
            // Special handling for string concatenation with + operator
            if (expr->binary.op.type == TOKEN_PLUS) {
                // Check if either operand is actually a string type
                bool left_is_string = (expr->binary.left->type == EXPR_STRING) ||
                                     (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_STRING);
                bool right_is_string = (expr->binary.right->type == EXPR_STRING) ||
                                      (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_STRING);
                
                bool left_is_int = (expr->binary.left->expr_type && expr->binary.left->expr_type->kind == TYPE_INT);
                bool right_is_int = (expr->binary.right->expr_type && expr->binary.right->expr_type->kind == TYPE_INT);
                
                // Also check if it's an int literal
                if (!left_is_int && expr->binary.left->type == EXPR_INT) left_is_int = true;
                if (!right_is_int && expr->binary.right->type == EXPR_INT) right_is_int = true;
                
                // Check if variable name suggests string type
                if (!left_is_string && expr->binary.left->type == EXPR_IDENT) {
                    Token name = expr->binary.left->token;
                    if ((name.length == 4 && memcmp(name.start, "name", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "text", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "line", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "word", 4) == 0) ||
                        (name.length == 3 && memcmp(name.start, "str", 3) == 0) ||
                        (name.length == 1 && name.start[0] == 's')) {
                        left_is_string = true;
                        left_is_int = false;
                    }
                }
                if (!right_is_string && expr->binary.right->type == EXPR_IDENT) {
                    Token name = expr->binary.right->token;
                    if ((name.length == 4 && memcmp(name.start, "name", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "text", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "line", 4) == 0) ||
                        (name.length == 4 && memcmp(name.start, "word", 4) == 0) ||
                        (name.length == 3 && memcmp(name.start, "str", 3) == 0) ||
                        (name.length == 1 && name.start[0] == 's')) {
                        right_is_string = true;
                        right_is_int = false;
                    }
                }
                
                if (left_is_string || right_is_string) {
                    // Use ARC-managed string concatenation with automatic conversion
                    emit("wyn_string_concat_safe(");
                    
                    // Convert left operand to string if it's an int
                    if (left_is_int && !left_is_string) {
                        emit("int_to_string(");
                        codegen_expr(expr->binary.left);
                        emit(")");
                    } else {
                        codegen_expr(expr->binary.left);
                    }
                    
                    emit(", ");
                    
                    // Convert right operand to string if it's an int
                    if (right_is_int && !right_is_string) {
                        emit("int_to_string(");
                        codegen_expr(expr->binary.right);
                        emit(")");
                    } else {
                        codegen_expr(expr->binary.right);
                    }
                    
                    emit(")");
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
                
                if (left_is_string && right_is_string) {
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
                    // Generate: wyn_safe_div(left, right) or wyn_safe_mod(left, right)
                    if (expr->binary.op.type == TOKEN_SLASH) {
                        emit("wyn_safe_div(");
                    } else {
                        emit("wyn_safe_mod(");
                    }
                    codegen_expr(expr->binary.left);
                    emit(", ");
                    codegen_expr(expr->binary.right);
                    emit(")");
                } else {
                    emit("(");
                    codegen_expr(expr->binary.left);
                    if (expr->binary.op.type == TOKEN_AND || expr->binary.op.type == TOKEN_AMPAMP) {
                        emit(" && ");
                    } else if (expr->binary.op.type == TOKEN_OR || expr->binary.op.type == TOKEN_PIPEPIPE) {
                        emit(" || ");
                    } else {
                        emit(" %.*s ", expr->binary.op.length, expr->binary.op.start);
                    }
                    codegen_expr(expr->binary.right);
                    emit(")");
                }
            }
            break;
        case EXPR_CALL:
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
                    emit("wyn_closure_call_int(");
                    codegen_expr(expr->call.callee);
                    emit(", ");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
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
            
            // Special handling for print function
            if (expr->call.callee->type == EXPR_IDENT && 
                expr->call.callee->token.length == 5 &&
                memcmp(expr->call.callee->token.start, "print", 5) == 0) {
                
                if (expr->call.arg_count == 1) {
                    // Single argument - use the _Generic macro for type dispatch
                    emit("print(");
                    codegen_expr(expr->call.args[0]);
                    emit(")");
                } else if (expr->call.arg_count >= 2 && 
                           expr->call.args[0]->type == EXPR_STRING) {
                    // Format string with {} placeholders: print("Value: {}", x)
                    const char* fmt = expr->call.args[0]->token.start + 1; // Skip opening quote
                    int fmt_len = expr->call.args[0]->token.length - 2; // Skip quotes
                    
                    emit("printf(\"");
                    int arg_idx = 1;
                    for (int i = 0; i < fmt_len; i++) {
                        if (i < fmt_len - 1 && fmt[i] == '{' && fmt[i+1] == '}') {
                            // Replace {} with appropriate format specifier
                            if (arg_idx < expr->call.arg_count) {
                                Expr* arg = expr->call.args[arg_idx];
                                
                                // Determine format specifier based on argument type
                                if (arg->expr_type && arg->expr_type->kind == TYPE_STRING) {
                                    emit("%%s");
                                } else if (arg->expr_type && arg->expr_type->kind == TYPE_FLOAT) {
                                    emit("%%f");
                                } else if (arg->expr_type && arg->expr_type->kind == TYPE_BOOL) {
                                    emit("%%s");  // Will use ternary for true/false
                                } else if (arg->type == EXPR_STRING) {
                                    // String literal
                                    emit("%%s");
                                } else if (arg->type == EXPR_IDENT) {
                                    // Variable - need better type detection
                                    // For now, check if it's likely a string parameter
                                    // This is a heuristic - proper solution needs symbol table lookup
                                    const char* var_name = arg->token.start;
                                    int var_len = arg->token.length;
                                    if ((var_len >= 4 && strncmp(var_name, "name", 4) == 0) ||
                                        (var_len >= 3 && strncmp(var_name, "msg", 3) == 0) ||
                                        (var_len >= 3 && strncmp(var_name, "str", 3) == 0) ||
                                        (var_len >= 4 && strncmp(var_name, "text", 4) == 0)) {
                                        emit("%%s");
                                    } else {
                                        emit("%%d");  // Default to int for other variables
                                    }
                                } else if (arg->type == EXPR_FIELD_ACCESS) {
                                    // Struct field access - check field name for string hints
                                    Token field_name = arg->field_access.field;
                                    if ((field_name.length >= 4 && strncmp(field_name.start, "name", 4) == 0) ||
                                        (field_name.length >= 3 && strncmp(field_name.start, "str", 3) == 0) ||
                                        (field_name.length >= 4 && strncmp(field_name.start, "text", 4) == 0) ||
                                        (field_name.length >= 5 && strncmp(field_name.start, "title", 5) == 0)) {
                                        emit("%%s");
                                    } else {
                                        emit("%%d");  // Default to int for other fields
                                    }
                                } else {
                                    // Default to int for backward compatibility
                                    emit("%%d");
                                }
                            }
                            i++; // Skip the }
                            arg_idx++;
                        } else if (fmt[i] == '%') {
                            emit("%%%%"); // Escape %
                        } else if (fmt[i] == '\\' && i < fmt_len - 1) {
                            emit("\\%c", fmt[i+1]);
                            i++;
                        } else {
                            emit("%c", fmt[i]);
                        }
                    }
                    emit("\\n\"");
                    // Add arguments
                    for (int i = 1; i < expr->call.arg_count; i++) {
                        emit(", ");
                        Expr* arg = expr->call.args[i];
                        // Handle boolean arguments that need string conversion
                        if (arg->expr_type && arg->expr_type->kind == TYPE_BOOL) {
                            emit("(");
                            codegen_expr(arg);
                            emit(" ? \"true\" : \"false\")");
                        } else {
                            codegen_expr(arg);
                        }
                    }
                    emit(")");
                } else {
                    // Multiple arguments - use individual print calls without newlines
                    emit("({ ");
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        if (i > 0) emit("printf(\" \"); ");
                        emit("print_no_nl(");
                        codegen_expr(expr->call.args[i]);
                        emit("); ");
                    }
                    emit("printf(\"\\n\"); })");
                }
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
                        // T1.5.3: Use mangled name only for actually overloaded functions
                        if (expr->call.selected_overload) {
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
                                    snprintf(func_name, 256, "%.*s", expr->call.callee->token.length, expr->call.callee->token.start);
                                    is_internal_call = is_module_function(func_name);
                                }
                                
                                // Only prefix if NOT an internal call
                                if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                    emit("%s_", current_module_prefix);
                                }
                                codegen_expr(expr->call.callee);
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
                                snprintf(func_name, 256, "%.*s", expr->call.callee->token.length, expr->call.callee->token.start);
                                is_internal_call = is_module_function(func_name);
                            }
                            
                            // Only prefix if NOT an internal call
                            if (current_module_prefix && !is_module_qualified && !is_internal_call) {
                                emit("%s_", current_module_prefix);
                            }
                            codegen_expr(expr->call.callee);
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
                    snprintf(callee_name, 64, "%.*s", 
                            expr->call.callee->token.length, expr->call.callee->token.start);
                    for (int i = 0; i < lambda_var_count; i++) {
                        if (strcmp(lambda_var_info[i].var_name, callee_name) == 0) {
                            is_lambda_call = true;
                            lambda_var_idx = i;
                            break;
                        }
                    }
                }
                
                // Emit captured variables first
                if (is_lambda_call && lambda_var_idx >= 0) {
                    for (int i = 0; i < lambda_var_info[lambda_var_idx].capture_count; i++) {
                        if (i > 0) emit(", ");
                        emit("%s", lambda_var_info[lambda_var_idx].captured_vars[i]);
                    }
                }
                
                for (int i = 0; i < expr->call.arg_count; i++) {
                    if (i > 0 || (is_lambda_call && lambda_var_idx >= 0 && lambda_var_info[lambda_var_idx].capture_count > 0)) {
                        emit(", ");
                    }
                    
                    // For array_push, take address of first argument (the array)
                    // and cast second argument to void* for integers
                    // For array_pop, take address of first argument (the array)
                    if ((is_array_push || is_array_pop) && i == 0) {
                        emit("&");
                    } else if (is_array_push && i == 1) {
                        emit("(void*)(intptr_t)");
                    }
                    
                    // Check if this argument needs trait object wrapping
                    if (expr->call.callee->type == EXPR_IDENT) {
                        char callee_buf[64];
                        snprintf(callee_buf, 64, "%.*s", expr->call.callee->token.length, expr->call.callee->token.start);
                        const char* trait = get_fn_trait_param(callee_buf, i);
                        if (trait && expr->call.args[i]->type == EXPR_IDENT && expr->call.args[i]->expr_type &&
                            expr->call.args[i]->expr_type->kind == TYPE_STRUCT) {
                            Token sname = expr->call.args[i]->expr_type->struct_type.name;
                            char sbuf[64];
                            snprintf(sbuf, 64, "%.*s", sname.length, sname.start);
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
                emit(")");
            }
            break;
        case EXPR_METHOD_CALL: {
            Token method = expr->method_call.method;
            
            // Extension methods on struct types - CHECK THIS FIRST
            if (expr->method_call.object->expr_type && 
                expr->method_call.object->expr_type->kind == TYPE_STRUCT) {
                Token type_name = expr->method_call.object->expr_type->struct_type.name;
                
                // Check if this is a trait type — use vtable dispatch
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
                
                emit("%.*s_%.*s(", type_name.length, type_name.start, 
                     method.length, method.start);
                codegen_expr(expr->method_call.object);
                for (int i = 0; i < expr->method_call.arg_count; i++) {
                    emit(", ");
                    codegen_expr(expr->method_call.args[i]);
                }
                emit(")");
                break;
            }
            
            // Check if object is a parameter with trait type
            if (expr->method_call.object->type == EXPR_IDENT) {
                Token obj = expr->method_call.object->token;
                // Check current function params for trait-typed params
                for (int pi = 0; pi < current_param_count; pi++) {
                    if (current_function_params[pi] && 
                        strlen(current_function_params[pi]) == (size_t)obj.length &&
                        memcmp(current_function_params[pi], obj.start, obj.length) == 0) {
                        // Found the param — check if its type is a trait
                        // We stored param types during STMT_FN processing
                        extern char current_param_types[64][64];
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
                char module_name[256];
                int len = obj_name.length < 255 ? obj_name.length : 255;
                memcpy(module_name, obj_name.start, len);
                module_name[len] = '\0';
                
                // Treat as module if it's loaded OR if it's a built-in
                // BUT NOT if it's a local variable or parameter
                bool is_local = is_parameter(module_name) || is_local_variable(module_name);
                if (!is_local && (is_module_loaded(module_name) || is_builtin_module(module_name))) {
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
                        emit("hashmap_%.*s(", method.length, method.start);
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
                    } else {
                        emit("%.*s_%.*s(", obj_name.length, obj_name.start, method.length, method.start);
                    }
                    for (int i = 0; i < expr->method_call.arg_count; i++) {
                        if (i > 0) emit(", ");
                        codegen_expr(expr->method_call.args[i]);
                    }
                    emit(")");
                    break;
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
                
                // arr.map(fn) -> wyn_array_map(arr, fn)
                if (method.length == 3 && memcmp(method.start, "map", 3) == 0 && expr->method_call.arg_count == 1) {
                    emit("wyn_array_map(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // arr.filter(fn) -> wyn_array_filter(arr, fn)
                if (method.length == 6 && memcmp(method.start, "filter", 6) == 0 && expr->method_call.arg_count == 1) {
                    emit("wyn_array_filter(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
                    break;
                }
                // arr.reduce(fn, init) -> wyn_array_reduce(arr, fn, init)
                if (method.length == 6 && memcmp(method.start, "reduce", 6) == 0 && expr->method_call.arg_count == 2) {
                    emit("wyn_array_reduce(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(", ");
                    codegen_expr(expr->method_call.args[1]);
                    emit(")");
                    break;
                }
                // arr.sort() -> arr_sort(arr.data, arr.count) (in-place)
                if (method.length == 4 && memcmp(method.start, "sort", 4) == 0 && expr->method_call.arg_count == 0) {
                    emit("({ WynArray __sa = ");
                    codegen_expr(expr->method_call.object);
                    emit("; arr_sort((int*)__sa.data, __sa.count); __sa; })");
                    break;
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
                    emit("arr_find(");
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.object);
                    emit(".count, ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(")");
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
            
            const char* receiver_type = get_receiver_type_string(object_type);
            
            // Check if variable is a known array (from list comp or array literal)
            if (expr->method_call.object->type == EXPR_IDENT) {
                char vn[64];
                snprintf(vn, 64, "%.*s", expr->method_call.object->token.length, expr->method_call.object->token.start);
                extern int is_known_array_var(const char*);
                if (is_known_array_var(vn)) receiver_type = "array";
            }
            
            // Fallback: if no type info, try to infer from expression
            if (!receiver_type && expr->method_call.object->type == EXPR_IDENT) {
                char mname[64];
                int mlen = method.length < 63 ? method.length : 63;
                memcpy(mname, method.start, mlen);
                mname[mlen] = '\0';
                if (strcmp(mname, "len") == 0 || strcmp(mname, "upper") == 0 ||
                    strcmp(mname, "lower") == 0 || strcmp(mname, "trim") == 0 ||
                    strcmp(mname, "contains") == 0 || strcmp(mname, "starts_with") == 0 ||
                    strcmp(mname, "ends_with") == 0 || strcmp(mname, "replace") == 0 ||
                    strcmp(mname, "repeat") == 0 || strcmp(mname, "index_of") == 0 ||
                    strcmp(mname, "substring") == 0 || strcmp(mname, "split_at") == 0 ||
                    strcmp(mname, "split_count") == 0 || strcmp(mname, "to_int") == 0 ||
                    strcmp(mname, "to_float") == 0) {
                    receiver_type = "string";
                }
            }
            // Also override int type when method is clearly a string method
            if (receiver_type && strcmp(receiver_type, "int") == 0) {
                char mname[64];
                int mlen = method.length < 63 ? method.length : 63;
                memcpy(mname, method.start, mlen);
                mname[mlen] = '\0';
                if (strcmp(mname, "to_int") == 0 || strcmp(mname, "to_float") == 0 ||
                    strcmp(mname, "split_at") == 0 || strcmp(mname, "split_count") == 0 ||
                    strcmp(mname, "upper") == 0 || strcmp(mname, "lower") == 0 ||
                    strcmp(mname, "trim") == 0 || strcmp(mname, "contains") == 0 ||
                    strcmp(mname, "starts_with") == 0 || strcmp(mname, "ends_with") == 0 ||
                    strcmp(mname, "substring") == 0 || strcmp(mname, "replace") == 0) {
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
                char method_name[256];
                int len = method.length < 255 ? method.length : 255;
                memcpy(method_name, method.start, len);
                method_name[len] = '\0';
                
                if ((strcmp(method_name, "set") == 0 || strcmp(method_name, "insert") == 0) && 
                    expr->method_call.arg_count == 2) {
                    // Determine insert function based on value type
                    Expr* value_expr = expr->method_call.args[1];
                    const char* insert_func = "hashmap_insert_int";
                    if (value_expr->type == EXPR_STRING) {
                        insert_func = "hashmap_insert_string";
                    } else if (value_expr->type == EXPR_FLOAT) {
                        insert_func = "hashmap_insert_float";
                    } else if (value_expr->type == EXPR_BOOL) {
                        insert_func = "hashmap_insert_bool";
                    } else if (value_expr->expr_type) {
                        if (value_expr->expr_type->kind == TYPE_STRING) {
                            insert_func = "hashmap_insert_string";
                        } else if (value_expr->expr_type->kind == TYPE_FLOAT) {
                            insert_func = "hashmap_insert_float";
                        } else if (value_expr->expr_type->kind == TYPE_BOOL) {
                            insert_func = "hashmap_insert_bool";
                        }
                    }
                    emit("%s(", insert_func);
                    codegen_expr(expr->method_call.object);
                    emit(", ");
                    codegen_expr(expr->method_call.args[0]);
                    emit(", ");
                    codegen_expr(expr->method_call.args[1]);
                    emit(")");
                    break;
                }
                
                // For map.get(), we need to use hashmap_get_string if the result is used as string
                // For now, use a generic approach that returns the value
                if (strcmp(method_name, "get") == 0 && expr->method_call.arg_count == 1) {
                    emit("hashmap_get_string(");
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
                    emit("hashmap_values_string(");
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
                char method_name[256];
                int len = method.length < 255 ? method.length : 255;
                memcpy(method_name, method.start, len);
                method_name[len] = '\0';
                
                MethodDispatch dispatch;
                if (dispatch_method(receiver_type, method_name, expr->method_call.arg_count, &dispatch)) {
                    emit("%s(", dispatch.c_function);
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
                char method_name[256];
                int len = method.length < 255 ? method.length : 255;
                memcpy(method_name, method.start, len);
                method_name[len] = '\0';
                
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
                        emit("hashmap_values_string("); codegen_expr(expr->method_call.object); emit(")"); break;
                    }
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
                    fprintf(stderr, "Hint: Available array methods: .len(), .push(item), .pop(), .contains(item), .sort()\n");
                } else if (strcmp(receiver_type, "string") == 0) {
                    fprintf(stderr, "Hint: Available string methods: .len(), .upper(), .lower(), .trim(), .contains(substr)\n");
                }
            } else {
                fprintf(stderr, "Error: Unknown method '%.*s' (no type info)\n", 
                        method.length, method.start);
            }
            method_done:
            break;
        }
        case EXPR_ARRAY: {
            // Generate simple array creation
            static int arr_counter = 0;
            int arr_id = arr_counter++;
            emit("({ WynArray __arr_%d = array_new(); ", arr_id);
            for (int i = 0; i < expr->array.count; i++) {
                Expr* elem = expr->array.elements[i];
                if (elem->type == EXPR_STRING) {
                    emit("array_push_str(&__arr_%d, ", arr_id);
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
                } else {
                    emit("array_push_int(&__arr_%d, ", arr_id);
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
                    
                    // Determine insert function based on value type
                    const char* insert_func = "hashmap_insert_int";
                    if (value_expr->type == EXPR_FLOAT) {
                        insert_func = "hashmap_insert_float";
                    } else if (value_expr->type == EXPR_STRING) {
                        insert_func = "hashmap_insert_string";
                    } else if (value_expr->type == EXPR_BOOL) {
                        insert_func = "hashmap_insert_bool";
                    }
                    
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
                // Map indexing: map["key"] -> hashmap_get_string(map, "key")
                emit("hashmap_get_string(");
                codegen_expr(expr->index.array);
                emit(", ");
                codegen_expr(expr->index.index);
                emit(")");
            } else {
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
                    // Common variable names for string arrays
                    if ((var_name.length == 4 && memcmp(var_name.start, "args", 4) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "files", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "names", 5) == 0) ||
                        (var_name.length == 5 && memcmp(var_name.start, "parts", 5) == 0) ||
                        (var_name.length == 7 && memcmp(var_name.start, "entries", 7) == 0)) {
                        is_string_array = true;
                    }
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
                    Type* elem_type = NULL;
                    if (expr->index.array->expr_type && 
                        expr->index.array->expr_type->kind == TYPE_ARRAY) {
                        elem_type = expr->index.array->expr_type->array_type.element_type;
                        if (elem_type) {
                            if (elem_type->kind == TYPE_STRUCT) {
                                is_struct_array = true;
                            } else if (elem_type->kind == TYPE_STRING) {
                                is_string_array = true;
                            }
                        }
                    }
                    
                    if (is_struct_array) {
                        emit("array_get_struct(");
                        codegen_expr(expr->index.array);
                        emit(", ");
                        codegen_expr(expr->index.index);
                        emit(", ");
                        Token type_name = elem_type->struct_type.name;
                        emit("%.*s", type_name.length, type_name.start);
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
            // Check if we need to prefix the assignment target with module name
            char target_name[512];
            memcpy(target_name, expr->assign.name.start, expr->assign.name.length);
            target_name[expr->assign.name.length] = '\0';
            
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
                if (is_local_variable(target_name)) {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
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
                    emit("(*%.*s) = ", expr->assign.name.length, expr->assign.name.start);
                } else {
                    emit("%.*s = ", expr->assign.name.length, expr->assign.name.start);
                }
            }
            codegen_expr(expr->assign.value);
            break;
        }
        case EXPR_STRUCT_INIT: {
            // Check if type needs ARC management
            bool needs_arc = false;
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
                snprintf(temp_name, 128, "%.*s", type_name.length, type_name.start);
                
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
            
            // Simple heuristic: types starting with uppercase likely need ARC
            if (actual_type_name_len > 0 && actual_type_name[0] >= 'A' && actual_type_name[0] <= 'Z') {
                needs_arc = true;
            }
            
            if (needs_arc) {
                // Generate ARC-managed struct initialization with cast
                emit("*(%.*s*)wyn_arc_new(sizeof(%.*s), &(%.*s){", 
                     actual_type_name_len, actual_type_name,
                     actual_type_name_len, actual_type_name,
                     actual_type_name_len, actual_type_name);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    if (i > 0) emit(", ");
                    emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                    codegen_expr(expr->struct_init.field_values[i]);
                }
                emit("})->data");
            } else {
                // Generate simple struct initialization
                emit("(%.*s){", actual_type_name_len, actual_type_name);
                for (int i = 0; i < expr->struct_init.field_count; i++) {
                    if (i > 0) emit(", ");
                    emit(".%.*s = ", expr->struct_init.field_names[i].length, expr->struct_init.field_names[i].start);
                    codegen_expr(expr->struct_init.field_values[i]);
                }
                emit("}");
            }
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
                emit("%.*s_%.*s",
                     obj_name.length, obj_name.start,
                     field_name.length, field_name.start);
                return;
            }
            
            // Handle module.function calls
            // Check if this is a module call (resolve aliases)
            char module_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s", obj_name.length, obj_name.start);
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
            const char* type_name = "int";  // Default fallback
            int type_name_len = 3;
            
            if (match_type && match_type->kind == TYPE_ENUM && match_type->name.length > 0) {
                type_name = match_type->name.start;
                type_name_len = match_type->name.length;
            } else if (match_type && match_type->kind == TYPE_STRING) {
                type_name = "const char*";
                type_name_len = 12;
            }
            
            // Store match value in temp variable
            emit("%.*s __match_val_%d = ", type_name_len, type_name, match_id);
            codegen_expr(expr->match.value);
            emit("; ");
            
            // Determine result type from first arm's expression
            const char* result_type = "int";
            if (expr->match.arm_count > 0 && expr->match.arms[0].result) {
                Expr* first_result = expr->match.arms[0].result;
                if (first_result->type == EXPR_STRING) {
                    result_type = "const char*";
                } else if (first_result->expr_type) {
                    if (first_result->expr_type->kind == TYPE_STRING) {
                        result_type = "const char*";
                    } else if (first_result->expr_type->kind == TYPE_FLOAT) {
                        result_type = "double";
                    } else if (first_result->expr_type->kind == TYPE_BOOL) {
                        result_type = "bool";
                    }
                }
            }
            
            // Generate result variable with correct type
            emit("%s __match_result_%d; ", result_type, match_id);
            
            // Generate if-else chain for each arm
            for (int i = 0; i < expr->match.arm_count; i++) {
                Pattern* pat = expr->match.arms[i].pattern;
                
                if (i > 0) emit("else ");
                
                // Check pattern type
                if (pat->type == PATTERN_WILDCARD) {
                    // Wildcard always matches
                    emit("{ ");
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
                    // Check if this looks like an enum variant (contains underscore)
                    bool is_enum_variant = false;
                    for (int j = 0; j < pat->ident.name.length; j++) {
                        if (pat->ident.name.start[j] == '_') {
                            is_enum_variant = true;
                            break;
                        }
                    }
                    
                    if (is_enum_variant) {
                        // Enum variant - generate comparison
                        emit("if (__match_val_%d == %.*s) { ",
                             match_id,
                             pat->ident.name.length,
                             pat->ident.name.start);
                    } else {
                        // Variable binding - always matches, bind variable
                        emit("{ %.*s %.*s = __match_val_%d; ",
                             type_name_len, type_name,
                             pat->ident.name.length,
                             pat->ident.name.start,
                             match_id);
                    }
                } else if (pat->type == PATTERN_OPTION && !pat->option.is_some) {
                    // Simple enum variant without data: Color::Red
                    // Generate: EnumName_VariantName
                    emit("if (__match_val_%d == %.*s_%.*s) { ",
                         match_id,
                         pat->option.enum_name.length,
                         pat->option.enum_name.start,
                         pat->option.variant_name.length,
                         pat->option.variant_name.start);
                } else if (pat->type == PATTERN_OPTION && pat->option.is_some) {
                    // Enum variant with data: Some(x), Ok(x), etc.
                    // Check tag matches variant
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
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "Option", 6) == 0) {
                emit("%s_Some(", current_fn_return_kind);
            } else {
                emit("OptionInt_Some(");
            }
            if (expr->option.value) codegen_expr(expr->option.value);
            emit(")");
            break;
        }
        case EXPR_NONE: {
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "Option", 6) == 0) {
                emit("%s_None()", current_fn_return_kind);
            } else {
                emit("OptionInt_None()");
            }
            break;
        }
        case EXPR_OK: {
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "Result", 6) == 0) {
                emit("%s_Ok(", current_fn_return_kind);
            } else {
                emit("ResultInt_Ok(");
            }
            if (expr->option.value) codegen_expr(expr->option.value);
            emit(")");
            break;
        }
        case EXPR_ERR: {
            if (current_fn_return_kind && strncmp(current_fn_return_kind, "Result", 6) == 0) {
                emit("%s_Err(", current_fn_return_kind);
            } else {
                emit("ResultInt_Err(");
            }
            if (expr->option.value) codegen_expr(expr->option.value);
            emit(")");
            break;
        }
        case EXPR_TRY: {
            // ? operator: unwrap Result or return early on error
            // var val = risky_call()?
            // Expands to: ({ ResultInt _r = risky_call(); if (_r.tag == 1) return _r; _r.data.ok_value; })
            emit("({ ResultInt __try_r = ");
            codegen_expr(expr->try_expr.value);
            emit("; if (__try_r.tag == 1) return __try_r; __try_r.data.ok_value; })");
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
        case EXPR_PIPELINE: {
            // Generate nested function calls: f(g(h(x)))
            // For x |> f |> g |> h, generate h(g(f(x)))
            
            // Start from the rightmost function and work backwards
            for (int i = expr->pipeline.stage_count - 1; i >= 1; i--) {
                if (expr->pipeline.stages[i]->type == EXPR_IDENT) {
                    // Simple function call
                    emit("%.*s(", 
                         expr->pipeline.stages[i]->token.length,
                         expr->pipeline.stages[i]->token.start);
                } else if (expr->pipeline.stages[i]->type == EXPR_METHOD_CALL) {
                    // Method call - need to handle specially
                    // For now, just emit the method name as a function
                    emit("%.*s(", 
                         expr->pipeline.stages[i]->method_call.method.length,
                         expr->pipeline.stages[i]->method_call.method.start);
                }
            }
            
            // Emit the first stage (the value)
            codegen_expr(expr->pipeline.stages[0]);
            
            // Close all the function calls
            for (int i = 1; i < expr->pipeline.stage_count; i++) {
                emit(")");
            }
            break;
        }
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
            emit("({ char __buf[8192]; sprintf(__buf, \"");
            
            // Build format string - use %s for everything and convert with _Generic
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.parts[i]) {
                    // String literal part
                    const char* part = expr->string_interp.parts[i];
                    while (*part) {
                        if (*part == '%') emit("%%"); // Escape % for sprintf
                        else emit("%c", *part);
                        part++;
                    }
                } else {
                    // Expression part - use %s and convert with to_string
                    emit("%%s");
                }
            }
            
            emit("\"");
            
            // Add arguments for expressions with type conversion
            for (int i = 0; i < expr->string_interp.count; i++) {
                if (expr->string_interp.expressions[i]) {
                    emit(", to_string(");
                    codegen_expr(expr->string_interp.expressions[i]);
                    emit(")");
                }
            }
            
            emit("); strdup(__buf); })");
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
        case EXPR_SPAWN: {
            // Spawn expression: spawn fn(args) returns a future
            if (expr->spawn.call && expr->spawn.call->type == EXPR_CALL &&
                expr->spawn.call->call.callee->type == EXPR_IDENT) {
                Expr* call = expr->spawn.call;
                Expr* callee = call->call.callee;
                char func_name[256];
                int len = callee->token.length < 255 ? callee->token.length : 255;
                memcpy(func_name, callee->token.start, len);
                func_name[len] = '\0';
                
                int arg_count = call->call.arg_count;
                
                if (arg_count == 0) {
                    emit("wyn_spawn_async((TaskFuncWithReturn)__spawn_wrapper_%s, NULL)", func_name);
                } else {
                    // Create args struct and pass to wrapper
                    spawn_id_counter++;
                    int sid = spawn_id_counter;
                    emit("({ struct __spawn_args_%d { ", sid);
                    for (int i = 0; i < arg_count; i++) {
                        emit("int a%d; ", i);
                    }
                    emit("} *__sa_%d = malloc(sizeof(struct __spawn_args_%d)); ", sid, sid);
                    for (int i = 0; i < arg_count; i++) {
                        emit("__sa_%d->a%d = ", sid, i);
                        codegen_expr(call->call.args[i]);
                        emit("; ");
                    }
                    emit("wyn_spawn_async((TaskFuncWithReturn)__spawn_wrapper_%s_%d, __sa_%d); })", 
                         func_name, arg_count, sid);
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
                    // Emit closure construction: allocate env, fill captured vars, return WynClosure
                    emit("({ __closure_env_%d* __env = malloc(sizeof(__closure_env_%d)); ", lid, lid);
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
                emit("__lambda_%d", lid);
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
            // Generate tuple as a struct literal (no compound statement)
            emit("(struct { ");
            for (int i = 0; i < expr->tuple.count; i++) {
                emit("int item%d; ", i);
            }
            emit("}){ ");
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
                // Determine insert function based on value type
                const char* insert_func = "hashmap_insert_int";
                if (expr->index_assign.value->type == EXPR_STRING) {
                    insert_func = "hashmap_insert_string";
                } else if (expr->index_assign.value->type == EXPR_FLOAT) {
                    insert_func = "hashmap_insert_float";
                } else if (expr->index_assign.value->type == EXPR_BOOL) {
                    insert_func = "hashmap_insert_bool";
                } else if (expr->index_assign.value->expr_type) {
                    if (expr->index_assign.value->expr_type->kind == TYPE_STRING) {
                        insert_func = "hashmap_insert_string";
                    } else if (expr->index_assign.value->expr_type->kind == TYPE_FLOAT) {
                        insert_func = "hashmap_insert_float";
                    } else if (expr->index_assign.value->expr_type->kind == TYPE_BOOL) {
                        insert_func = "hashmap_insert_bool";
                    }
                }
                emit("%s(", insert_func);
                codegen_expr(expr->index_assign.object);
                emit(", ");
                codegen_expr(expr->index_assign.index);
                emit(", ");
                codegen_expr(expr->index_assign.value);
                emit(")");
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

