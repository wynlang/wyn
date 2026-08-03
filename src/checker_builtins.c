// checker_builtins.c - registration of every builtin type and function signature.
//
// Split out of checker.c, which was 10,319 lines. This is init_checker() and nothing
// else: ~1,500 lines of pure registration - the namespaces (File, Http, Json, Time,
// ...), the Option/Result families, and the builtin scalar types - with no
// type-checking logic in it at all. Keeping it beside check_expr() made the one file
// that decides what type an expression has 3x longer than it needed to be.
//
// #included into checker.c rather than compiled separately, the same single-
// translation-unit arrangement codegen.c uses for codegen_expr.c and friends: it
// needs checker.c's file-scope statics (global_scope, the builtin_* types, reg_fn),
// and threading those through a header would be a bigger change than the split is
// worth. The Makefile lists it as a prerequisite so editing it triggers a rebuild -
// without that, make sees no changed prerequisite and silently keeps a stale binary.

void init_checker() {
    global_scope = calloc(1, sizeof(SymbolTable));
    global_scope->capacity = 128;
    global_scope->symbols = calloc(128, sizeof(Symbol));
    had_error = false;
    
    // Initialize trait system
    wyn_traits_init();
    
    builtin_int = make_type(TYPE_INT);
    builtin_float = make_type(TYPE_FLOAT);
    builtin_string = make_type(TYPE_STRING);
    builtin_bool = make_type(TYPE_BOOL);
    builtin_void = make_type(TYPE_VOID);
    // Opaque C pointer type for FFI (`ptr`). A TYPE_STRUCT named "void*" so it
    // flows through codegen_c_type_from_type's struct path to the C type "void*"
    // and is treated as an opaque machine word (not an ARC-managed Wyn struct,
    // which is keyed off registered struct names - "void*" is never registered).
    builtin_ptr = make_type(TYPE_STRUCT);
    { Token _pn = {TOKEN_IDENT, "void*", 5, 0}; builtin_ptr->struct_type.name = _pn; }
    builtin_array = make_type(TYPE_ARRAY);
    // int? — the return type of Task.try_recv (non-blocking receive → Option).
    builtin_int_opt = make_type(TYPE_OPTIONAL);
    builtin_int_opt->optional_type.inner_type = builtin_int;

    // Register collection types
    Type* builtin_map = make_type(TYPE_MAP);
    Token map_tok = {TOKEN_IDENT, "HashMap", 7, 0};
    add_symbol(global_scope, map_tok, builtin_map, false);
    
    Type* builtin_set = make_type(TYPE_SET);
    Token set_tok = {TOKEN_IDENT, "HashSet", 7, 0};
    add_symbol(global_scope, set_tok, builtin_set, false);
    
    // Register Result types
    Type* result_int_type = make_type(TYPE_STRUCT);
    result_int_type->struct_type.name = (Token){TOKEN_IDENT, "ResultInt", 9, 0};
    Token result_int_tok = {TOKEN_IDENT, "ResultInt", 9, 0};
    add_symbol(global_scope, result_int_tok, result_int_type, false);
    
    Type* result_string_type = make_type(TYPE_STRUCT);
    result_string_type->struct_type.name = (Token){TOKEN_IDENT, "ResultString", 12, 0};
    Token result_string_tok = {TOKEN_IDENT, "ResultString", 12, 0};
    add_symbol(global_scope, result_string_tok, result_string_type, false);
    
    // Add built-in functions
    const char* stdlib_funcs[] = {
        "print", "print_float", "print_str", "print_bool", "print_hex", "print_bin", "println", "print_debug", "input", "input_float", "input_line", "printf_wyn", "string_format", "sin_approx", "cos_approx", "pi_const", "e_const",
        "str_len", "str_eq", "str_concat", "str_upper", "str_lower", "str_contains", "str_starts_with", "str_ends_with", "str_trim",
        "str_replace", "str_split", "str_join", "int_to_str", "str_to_int", "str_repeat", "str_reverse", "str_parse_int", "str_parse_int_failed", "str_parse_float", "str_free",
        "split_get", "split_count", "char_at", "is_numeric", "str_count", "str_contains_substr",
        "string_char_at", "string_length",
        "abs_val", "min", "max", "pow_int", "clamp", "sign", "gcd", "lcm", "is_even", "is_odd",
        "sqrt_int", "ceil_int", "floor_int", "round_int", "abs_float",
        "swap", "clamp_float", "lerp", "map_range",
        "bit_set", "bit_clear", "bit_toggle", "bit_check", "bit_count",
        "arr_sum", "arr_max", "arr_min", "arr_contains", "arr_find", "arr_reverse", "arr_sort", "arr_count", "arr_fill", "arr_all", "arr_join", "arr_map_double", "arr_map_square", "arr_filter_positive", "arr_filter_even", "arr_filter_greater_than_3", "arr_reduce_sum", "arr_reduce_product",
        "file_exists", "file_size", "file_delete", "file_append", "file_copy", "last_error_get",
        "file_move", "file_list_dir", "file_mkdir", "file_rmdir", "file_is_file", "file_is_dir",
        // NOTE: file_read, file_write, sys_exec are registered separately with proper types
        "random_int", "random_range", "random_float", "seed_random", "random_bool",
        "random_string", "random_hex", "random_uuid", "random_choice_int", "random_choice_str",
        "random_seed_auto",
        "time_now", "time_format",
        "range", "array_new", "array_push", "array_push_str", "array_pop", "array_length_dyn", "len",
        "assert_eq", "assert_true", "assert_false", "panic", "todo",
        "await_all", "await_any",
        "exit_program", "sleep_ms", "getenv_var", "setenv_var",
        "Error", "TypeError", "ValueError", "DivisionByZeroError", "print_error",
        "http_get", "http_post", "http_put", "http_delete", "http_set_header", "http_clear_headers", "http_status", "http_error",
        "https_get", "https_post",
        "hashmap_new", "hashmap_insert", "hashmap_get", "hashmap_has", "hashmap_remove", "hashmap_free",
        "hashmap_insert_int", "hashmap_insert_float", "hashmap_insert_string", "hashmap_insert_bool",
        "hashmap_get_int", "hashmap_get_float", "hashmap_get_string", "hashmap_get_bool",
        "wyn_hashmap_new", "wyn_hashmap_insert_int", "wyn_hashmap_get_int", "wyn_hashmap_has", "wyn_hashmap_len", "wyn_hashmap_free",
        "hashset_new", "hashset_add", "hashset_contains", "hashset_remove", "hashset_free",
        "set_len", "set_is_empty", "set_clear", "set_union", "set_intersection", "set_difference", "set_is_subset", "set_is_superset",
        "json_parse", "json_get_string", "json_get_int", "json_free",
        "json_get_str", "json_get_int", "json_get_bool", "json_has_key", "json_stringify_int", "json_stringify_str", "json_stringify_bool", "json_array_stringify", "json_array_length", "json_array_get",
        "url_encode", "url_decode", "base64_encode", "hash_string",
        // v1.3.0 Standard Library
        "wyn_string_len", "wyn_string_contains", "wyn_string_starts_with", "wyn_string_ends_with",
        "wyn_string_to_upper", "wyn_string_to_lower", "wyn_string_trim", "wyn_str_replace",
        "wyn_string_split", "wyn_string_join", "wyn_str_substring", "wyn_string_index_of",
        "wyn_string_last_index_of", "wyn_string_repeat", "wyn_string_reverse",
        "wyn_string_pad_left", "wyn_string_pad_right",
        "wyn_string_pad_left_safe", "wyn_string_pad_right_safe",
        "wyn_array_map", "wyn_array_filter", "wyn_array_reduce", "wyn_array_find",
        "wyn_array_find_index", "wyn_array_unique", "wyn_array_join",
        "wyn_array_first", "wyn_array_last", "wyn_array_is_empty",
        "wyn_array_any", "wyn_array_all", "wyn_array_reverse", "wyn_array_sort",
        "wyn_array_contains", "wyn_array_index_of", "wyn_array_last_index_of",
        "wyn_array_slice", "wyn_array_concat", "wyn_array_fill",
        "wyn_array_sum", "wyn_array_min", "wyn_array_max", "wyn_array_average",
        "wyn_time_now", "wyn_time_now_millis", "wyn_time_now_micros",
        "wyn_time_sleep", "wyn_time_sleep_millis", "wyn_time_sleep_micros",
        "wyn_time_format", "wyn_time_parse",
        "wyn_time_year", "wyn_time_month", "wyn_time_day",
        "wyn_time_hour", "wyn_time_minute", "wyn_time_second",
        "wyn_crypto_hash32", "wyn_crypto_hash64", "wyn_crypto_md5", "wyn_crypto_sha256",
        "wyn_crypto_base64_encode", "wyn_crypto_base64_decode",
        "wyn_crypto_random_bytes", "wyn_crypto_random_hex", "wyn_crypto_xor_cipher",
        "wyn_math_abs", "wyn_math_min", "wyn_math_max", "wyn_math_pow",
        "wyn_math_sqrt", "wyn_math_floor", "wyn_math_ceil", "wyn_math_round"
    };
    
    for (int i = 0; i < (int)(sizeof(stdlib_funcs)/sizeof(stdlib_funcs[0])); i++) {
        // await_all and the string-returning http builtins get real function
        // types below - find_symbol returns the FIRST match, so they must not
        // be added here.
        if (strcmp(stdlib_funcs[i], "await_all") == 0) continue;
        if (strncmp(stdlib_funcs[i], "http_", 5) == 0 ||
            strncmp(stdlib_funcs[i], "https_", 6) == 0) continue;
        // json_get_string returns char*; the blanket int registration made it
        // type `int` in return position (works only in print/interp where the
        // arg is coerced), so `s = json_get_string(..)` inferred int. The real
        // string-typed entry is added in the JSON stdlib loop below - find_symbol
        // returns the FIRST match, so this one must not shadow it.
        if (strcmp(stdlib_funcs[i], "json_get_string") == 0) continue;
        // input_line/input_float return char*/float from the runtime, not int.
        // The blanket int entry made `s = input_line()` infer int, so codegen
        // emitted int_to_string(s) on the returned char* and printed the line's
        // ADDRESS as a decimal number (silent wrong output at exit 0). Real
        // function types are registered just below; find_symbol returns the
        // FIRST match, so these must not be added here.
        if (strcmp(stdlib_funcs[i], "input_line") == 0) continue;
        if (strcmp(stdlib_funcs[i], "input_float") == 0) continue;
        Token tok = {TOKEN_IDENT, stdlib_funcs[i], (int)strlen(stdlib_funcs[i]), 0};
        add_symbol(global_scope, tok, builtin_int, false);
    }

    // stdin builtins with their real return types (see the skips above).
    // `input()` genuinely returns int, so it keeps the blanket entry.
    {
        struct { const char* name; Type* ret; } stdin_fns[] = {
            {"input_float", builtin_float},
            {"input_line", builtin_string},
        };
        for (int i = 0; i < (int)(sizeof(stdin_fns)/sizeof(stdin_fns[0])); i++) {
            Type* ft = make_type(TYPE_FUNCTION);
            ft->fn_type.param_count = 0;
            ft->fn_type.param_types = NULL;
            ft->fn_type.return_type = stdin_fns[i].ret;
            Token tok = {TOKEN_IDENT, stdin_fns[i].name, (int)strlen(stdin_fns[i].name), 0};
            add_symbol(global_scope, tok, ft, false);
        }
    }

    // Bare http builtins with real types. The runtime returns char* from
    // http_get/http_post/http_put/http_delete/https_get/https_post and
    // http_error; the blanket int registration above made them type int in
    // return position while Http.get/Http.post correctly typed string.
    {
        struct { const char* name; int pc; Type* ret; } http_fns[] = {
            {"http_get", 1, builtin_string},
            {"http_post", 2, builtin_string},
            {"http_put", 2, builtin_string},
            {"http_delete", 1, builtin_string},
            {"https_get", 1, builtin_string},
            {"https_post", 2, builtin_string},
            {"http_set_header", 2, builtin_void},
            {"http_clear_headers", 0, builtin_void},
            {"http_status", 0, builtin_int},
            {"http_error", 0, builtin_string},
        };
        for (int i = 0; i < (int)(sizeof(http_fns)/sizeof(http_fns[0])); i++) {
            Type* ft = make_type(TYPE_FUNCTION);
            ft->fn_type.param_count = http_fns[i].pc;
            ft->fn_type.param_types = http_fns[i].pc
                ? malloc(sizeof(Type*) * http_fns[i].pc) : NULL;
            for (int p = 0; p < http_fns[i].pc; p++)
                ft->fn_type.param_types[p] = builtin_string;
            ft->fn_type.return_type = http_fns[i].ret;
            Token tok = {TOKEN_IDENT, http_fns[i].name, (int)strlen(http_fns[i].name), 0};
            add_symbol(global_scope, tok, ft, false);
        }
    }
    
    // await_all returns the array of awaited results, not int - the blanket
    // int registration above made `println(await_all(futs))` emit
    // to_string(<WynArray>) and die at the C level (B3, 2026-07-18).
    // Element type is int (the only awaited payload today); re-registering
    // OVERRIDES the entry added by the stdlib_funcs loop.
    {
        Type* aa_ret = make_type(TYPE_ARRAY);
        aa_ret->array_type.element_type = builtin_int;
        Type* aa_type = make_type(TYPE_FUNCTION);
        aa_type->fn_type.param_count = 1;
        aa_type->fn_type.param_types = malloc(sizeof(Type*));
        aa_type->fn_type.param_types[0] = builtin_array;
        aa_type->fn_type.return_type = aa_ret;
        Token aa_tok = {TOKEN_IDENT, "await_all", 9, 0};
        add_symbol(global_scope, aa_tok, aa_type, false);
    }

    // Add C interface functions with correct function types
    Type* get_argc_type = make_type(TYPE_FUNCTION);
    get_argc_type->fn_type.param_count = 0;
    get_argc_type->fn_type.return_type = builtin_int;
    Token get_argc_tok = {TOKEN_IDENT, "get_argc", 8, 0};
    add_symbol(global_scope, get_argc_tok, get_argc_type, false);
    
    Type* get_argv_type = make_type(TYPE_FUNCTION);
    get_argv_type->fn_type.param_count = 1;
    get_argv_type->fn_type.param_types = malloc(sizeof(Type*));
    get_argv_type->fn_type.param_types[0] = builtin_int;
    get_argv_type->fn_type.return_type = builtin_string;
    Token get_argv_tok = {TOKEN_IDENT, "get_argv", 8, 0};
    add_symbol(global_scope, get_argv_tok, get_argv_type, false);
    
    Type* read_file_content_type = make_type(TYPE_FUNCTION);
    read_file_content_type->fn_type.param_count = 1;
    read_file_content_type->fn_type.param_types = malloc(sizeof(Type*));
    read_file_content_type->fn_type.param_types[0] = builtin_string;
    read_file_content_type->fn_type.return_type = builtin_string;
    Token read_file_content_tok = {TOKEN_IDENT, "read_file_content", 17, 0};
    add_symbol(global_scope, read_file_content_tok, read_file_content_type, false);
    
    Type* check_file_exists_type = make_type(TYPE_FUNCTION);
    check_file_exists_type->fn_type.param_count = 1;
    check_file_exists_type->fn_type.param_types = malloc(sizeof(Type*));
    check_file_exists_type->fn_type.param_types[0] = builtin_string;
    check_file_exists_type->fn_type.return_type = builtin_bool;
    Token check_file_exists_tok = {TOKEN_IDENT, "check_file_exists", 17, 0};
    add_symbol(global_scope, check_file_exists_tok, check_file_exists_type, false);
    
    Type* is_content_valid_type = make_type(TYPE_FUNCTION);
    is_content_valid_type->fn_type.param_count = 1;
    is_content_valid_type->fn_type.param_types = malloc(sizeof(Type*));
    is_content_valid_type->fn_type.param_types[0] = builtin_string;
    is_content_valid_type->fn_type.return_type = builtin_bool;
    Token is_content_valid_tok = {TOKEN_IDENT, "is_content_valid", 16, 0};
    add_symbol(global_scope, is_content_valid_tok, is_content_valid_type, false);
    
    // Add compiler interface functions
    Type* c_init_lexer_type = make_type(TYPE_FUNCTION);
    c_init_lexer_type->fn_type.param_count = 1;
    c_init_lexer_type->fn_type.param_types = malloc(sizeof(Type*));
    c_init_lexer_type->fn_type.param_types[0] = builtin_string;
    c_init_lexer_type->fn_type.return_type = builtin_bool;
    Token c_init_lexer_tok = {TOKEN_IDENT, "c_init_lexer", 12, 0};
    add_symbol(global_scope, c_init_lexer_tok, c_init_lexer_type, false);
    
    Type* c_init_parser_type = make_type(TYPE_FUNCTION);
    c_init_parser_type->fn_type.param_count = 0;
    c_init_parser_type->fn_type.return_type = builtin_int;
    Token c_init_parser_tok = {TOKEN_IDENT, "c_init_parser", 13, 0};
    add_symbol(global_scope, c_init_parser_tok, c_init_parser_type, false);
    
    Type* c_parse_program_type = make_type(TYPE_FUNCTION);
    c_parse_program_type->fn_type.param_count = 0;
    c_parse_program_type->fn_type.return_type = builtin_int;
    Token c_parse_program_tok = {TOKEN_IDENT, "c_parse_program", 15, 0};
    add_symbol(global_scope, c_parse_program_tok, c_parse_program_type, false);
    
    Type* c_init_checker_type = make_type(TYPE_FUNCTION);
    c_init_checker_type->fn_type.param_count = 0;
    c_init_checker_type->fn_type.return_type = builtin_int;
    Token c_init_checker_tok = {TOKEN_IDENT, "c_init_checker", 14, 0};
    add_symbol(global_scope, c_init_checker_tok, c_init_checker_type, false);
    
    Type* c_check_program_type = make_type(TYPE_FUNCTION);
    c_check_program_type->fn_type.param_count = 1;
    c_check_program_type->fn_type.param_types = malloc(sizeof(Type*));
    c_check_program_type->fn_type.param_types[0] = builtin_int;
    c_check_program_type->fn_type.return_type = builtin_int;
    Token c_check_program_tok = {TOKEN_IDENT, "c_check_program", 15, 0};
    add_symbol(global_scope, c_check_program_tok, c_check_program_type, false);
    
    Type* c_checker_had_error_type = make_type(TYPE_FUNCTION);
    c_checker_had_error_type->fn_type.param_count = 0;
    c_checker_had_error_type->fn_type.return_type = builtin_bool;
    Token c_checker_had_error_tok = {TOKEN_IDENT, "c_checker_had_error", 19, 0};
    add_symbol(global_scope, c_checker_had_error_tok, c_checker_had_error_type, false);
    
    Type* c_generate_code_type = make_type(TYPE_FUNCTION);
    c_generate_code_type->fn_type.param_count = 2;
    c_generate_code_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    c_generate_code_type->fn_type.param_types[0] = builtin_int;
    c_generate_code_type->fn_type.param_types[1] = builtin_string;
    c_generate_code_type->fn_type.return_type = builtin_bool;
    Token c_generate_code_tok = {TOKEN_IDENT, "c_generate_code", 15, 0};
    add_symbol(global_scope, c_generate_code_tok, c_generate_code_type, false);
    
    // Add wyn_math function types
    Type* wyn_math_abs_type = make_type(TYPE_FUNCTION);
    wyn_math_abs_type->fn_type.param_count = 1;
    wyn_math_abs_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_abs_type->fn_type.param_types[0] = builtin_float;
    wyn_math_abs_type->fn_type.return_type = builtin_float;
    Token wyn_math_abs_tok = {TOKEN_IDENT, "wyn_math_abs", 12, 0};
    add_symbol(global_scope, wyn_math_abs_tok, wyn_math_abs_type, false);
    
    Type* wyn_math_min_type = make_type(TYPE_FUNCTION);
    wyn_math_min_type->fn_type.param_count = 2;
    wyn_math_min_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_min_type->fn_type.param_types[0] = builtin_float;
    wyn_math_min_type->fn_type.param_types[1] = builtin_float;
    wyn_math_min_type->fn_type.return_type = builtin_float;
    Token wyn_math_min_tok = {TOKEN_IDENT, "wyn_math_min", 12, 0};
    add_symbol(global_scope, wyn_math_min_tok, wyn_math_min_type, false);
    
    Type* wyn_math_max_type = make_type(TYPE_FUNCTION);
    wyn_math_max_type->fn_type.param_count = 2;
    wyn_math_max_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_max_type->fn_type.param_types[0] = builtin_float;
    wyn_math_max_type->fn_type.param_types[1] = builtin_float;
    wyn_math_max_type->fn_type.return_type = builtin_float;
    Token wyn_math_max_tok = {TOKEN_IDENT, "wyn_math_max", 12, 0};
    add_symbol(global_scope, wyn_math_max_tok, wyn_math_max_type, false);
    
    Type* wyn_math_pow_type = make_type(TYPE_FUNCTION);
    wyn_math_pow_type->fn_type.param_count = 2;
    wyn_math_pow_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    wyn_math_pow_type->fn_type.param_types[0] = builtin_float;
    wyn_math_pow_type->fn_type.param_types[1] = builtin_float;
    wyn_math_pow_type->fn_type.return_type = builtin_float;
    Token wyn_math_pow_tok = {TOKEN_IDENT, "wyn_math_pow", 12, 0};
    add_symbol(global_scope, wyn_math_pow_tok, wyn_math_pow_type, false);
    
    Type* wyn_math_sqrt_type = make_type(TYPE_FUNCTION);
    wyn_math_sqrt_type->fn_type.param_count = 1;
    wyn_math_sqrt_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_sqrt_type->fn_type.param_types[0] = builtin_float;
    wyn_math_sqrt_type->fn_type.return_type = builtin_float;
    Token wyn_math_sqrt_tok = {TOKEN_IDENT, "wyn_math_sqrt", 13, 0};
    add_symbol(global_scope, wyn_math_sqrt_tok, wyn_math_sqrt_type, false);
    
    Type* wyn_math_floor_type = make_type(TYPE_FUNCTION);
    wyn_math_floor_type->fn_type.param_count = 1;
    wyn_math_floor_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_floor_type->fn_type.param_types[0] = builtin_float;
    wyn_math_floor_type->fn_type.return_type = builtin_float;
    Token wyn_math_floor_tok = {TOKEN_IDENT, "wyn_math_floor", 14, 0};
    add_symbol(global_scope, wyn_math_floor_tok, wyn_math_floor_type, false);
    
    Type* wyn_math_ceil_type = make_type(TYPE_FUNCTION);
    wyn_math_ceil_type->fn_type.param_count = 1;
    wyn_math_ceil_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_ceil_type->fn_type.param_types[0] = builtin_float;
    wyn_math_ceil_type->fn_type.return_type = builtin_float;
    Token wyn_math_ceil_tok = {TOKEN_IDENT, "wyn_math_ceil", 13, 0};
    add_symbol(global_scope, wyn_math_ceil_tok, wyn_math_ceil_type, false);
    
    Type* wyn_math_round_type = make_type(TYPE_FUNCTION);
    wyn_math_round_type->fn_type.param_count = 1;
    wyn_math_round_type->fn_type.param_types = malloc(sizeof(Type*));
    wyn_math_round_type->fn_type.param_types[0] = builtin_float;
    wyn_math_round_type->fn_type.return_type = builtin_float;
    Token wyn_math_round_tok = {TOKEN_IDENT, "wyn_math_round", 14, 0};
    add_symbol(global_scope, wyn_math_round_tok, wyn_math_round_type, false);
    
    Type* c_create_c_filename_type = make_type(TYPE_FUNCTION);
    c_create_c_filename_type->fn_type.param_count = 1;
    c_create_c_filename_type->fn_type.param_types = malloc(sizeof(Type*));
    c_create_c_filename_type->fn_type.param_types[0] = builtin_string;
    c_create_c_filename_type->fn_type.return_type = builtin_string;
    Token c_create_c_filename_tok = {TOKEN_IDENT, "c_create_c_filename", 19, 0};
    add_symbol(global_scope, c_create_c_filename_tok, c_create_c_filename_type, false);
    
    Type* c_compile_to_binary_type = make_type(TYPE_FUNCTION);
    c_compile_to_binary_type->fn_type.param_count = 2;
    c_compile_to_binary_type->fn_type.param_types = malloc(2 * sizeof(Type*));
    c_compile_to_binary_type->fn_type.param_types[0] = builtin_string;
    c_compile_to_binary_type->fn_type.param_types[1] = builtin_string;
    c_compile_to_binary_type->fn_type.return_type = builtin_bool;
    Token c_compile_to_binary_tok = {TOKEN_IDENT, "c_compile_to_binary", 19, 0};
    add_symbol(global_scope, c_compile_to_binary_tok, c_compile_to_binary_type, false);
    
    Type* c_remove_file_type = make_type(TYPE_FUNCTION);
    c_remove_file_type->fn_type.param_count = 1;
    c_remove_file_type->fn_type.param_types = malloc(sizeof(Type*));
    c_remove_file_type->fn_type.param_types[0] = builtin_string;
    c_remove_file_type->fn_type.return_type = builtin_bool;
    Token c_remove_file_tok = {TOKEN_IDENT, "c_remove_file", 13, 0};
    add_symbol(global_scope, c_remove_file_tok, c_remove_file_type, false);
    
    // Register Result functions
    Type* result_int_ok_type = make_type(TYPE_FUNCTION);
    result_int_ok_type->fn_type.param_count = 1;
    result_int_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_ok_type->fn_type.param_types[0] = builtin_int;
    result_int_ok_type->fn_type.return_type = result_int_type;
    Token result_int_ok_tok = {TOKEN_IDENT, "ResultInt_Ok", 12, 0};
    add_symbol(global_scope, result_int_ok_tok, result_int_ok_type, false);
    
    Type* result_int_err_type = make_type(TYPE_FUNCTION);
    result_int_err_type->fn_type.param_count = 1;
    result_int_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_err_type->fn_type.param_types[0] = builtin_string;
    result_int_err_type->fn_type.return_type = result_int_type;
    Token result_int_err_tok = {TOKEN_IDENT, "ResultInt_Err", 13, 0};
    add_symbol(global_scope, result_int_err_tok, result_int_err_type, false);
    
    Type* result_int_is_ok_type = make_type(TYPE_FUNCTION);
    result_int_is_ok_type->fn_type.param_count = 1;
    result_int_is_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_is_ok_type->fn_type.param_types[0] = result_int_type;
    result_int_is_ok_type->fn_type.return_type = builtin_int;
    Token result_int_is_ok_tok = {TOKEN_IDENT, "ResultInt_is_ok", 15, 0};
    add_symbol(global_scope, result_int_is_ok_tok, result_int_is_ok_type, false);
    
    Type* result_int_is_err_type = make_type(TYPE_FUNCTION);
    result_int_is_err_type->fn_type.param_count = 1;
    result_int_is_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_is_err_type->fn_type.param_types[0] = result_int_type;
    result_int_is_err_type->fn_type.return_type = builtin_int;
    Token result_int_is_err_tok = {TOKEN_IDENT, "ResultInt_is_err", 16, 0};
    add_symbol(global_scope, result_int_is_err_tok, result_int_is_err_type, false);
    
    Type* result_string_ok_type = make_type(TYPE_FUNCTION);
    result_string_ok_type->fn_type.param_count = 1;
    result_string_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_ok_type->fn_type.param_types[0] = builtin_string;
    result_string_ok_type->fn_type.return_type = result_string_type;
    Token result_string_ok_tok = {TOKEN_IDENT, "ResultString_Ok", 15, 0};
    add_symbol(global_scope, result_string_ok_tok, result_string_ok_type, false);
    
    Type* result_string_err_type = make_type(TYPE_FUNCTION);
    result_string_err_type->fn_type.param_count = 1;
    result_string_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_err_type->fn_type.param_types[0] = builtin_string;
    result_string_err_type->fn_type.return_type = result_string_type;
    Token result_string_err_tok = {TOKEN_IDENT, "ResultString_Err", 16, 0};
    add_symbol(global_scope, result_string_err_tok, result_string_err_type, false);
    
    Type* result_string_is_ok_type = make_type(TYPE_FUNCTION);
    result_string_is_ok_type->fn_type.param_count = 1;
    result_string_is_ok_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_is_ok_type->fn_type.param_types[0] = result_string_type;
    result_string_is_ok_type->fn_type.return_type = builtin_int;
    Token result_string_is_ok_tok = {TOKEN_IDENT, "ResultString_is_ok", 18, 0};
    add_symbol(global_scope, result_string_is_ok_tok, result_string_is_ok_type, false);
    
    Type* result_string_is_err_type = make_type(TYPE_FUNCTION);
    result_string_is_err_type->fn_type.param_count = 1;
    result_string_is_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_is_err_type->fn_type.param_types[0] = result_string_type;
    result_string_is_err_type->fn_type.return_type = builtin_int;
    Token result_string_is_err_tok = {TOKEN_IDENT, "ResultString_is_err", 19, 0};
    add_symbol(global_scope, result_string_is_err_tok, result_string_is_err_type, false);
    
    // Register Result unwrap functions
    Type* result_int_unwrap_type = make_type(TYPE_FUNCTION);
    result_int_unwrap_type->fn_type.param_count = 1;
    result_int_unwrap_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_unwrap_type->fn_type.param_types[0] = result_int_type;
    result_int_unwrap_type->fn_type.return_type = builtin_int;
    Token result_int_unwrap_tok = {TOKEN_IDENT, "ResultInt_unwrap", 16, 0};
    add_symbol(global_scope, result_int_unwrap_tok, result_int_unwrap_type, false);
    
    Type* result_int_unwrap_err_type = make_type(TYPE_FUNCTION);
    result_int_unwrap_err_type->fn_type.param_count = 1;
    result_int_unwrap_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_int_unwrap_err_type->fn_type.param_types[0] = result_int_type;
    result_int_unwrap_err_type->fn_type.return_type = builtin_string;
    Token result_int_unwrap_err_tok = {TOKEN_IDENT, "ResultInt_unwrap_err", 20, 0};
    add_symbol(global_scope, result_int_unwrap_err_tok, result_int_unwrap_err_type, false);
    
    Type* result_string_unwrap_type = make_type(TYPE_FUNCTION);
    result_string_unwrap_type->fn_type.param_count = 1;
    result_string_unwrap_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_unwrap_type->fn_type.param_types[0] = result_string_type;
    result_string_unwrap_type->fn_type.return_type = builtin_string;
    Token result_string_unwrap_tok = {TOKEN_IDENT, "ResultString_unwrap", 19, 0};
    add_symbol(global_scope, result_string_unwrap_tok, result_string_unwrap_type, false);
    
    Type* result_string_unwrap_err_type = make_type(TYPE_FUNCTION);
    result_string_unwrap_err_type->fn_type.param_count = 1;
    result_string_unwrap_err_type->fn_type.param_types = malloc(sizeof(Type*));
    result_string_unwrap_err_type->fn_type.param_types[0] = result_string_type;
    result_string_unwrap_err_type->fn_type.return_type = builtin_string;
    Token result_string_unwrap_err_tok = {TOKEN_IDENT, "ResultString_unwrap_err", 23, 0};
    add_symbol(global_scope, result_string_unwrap_err_tok, result_string_unwrap_err_type, false);

    // OptionInt type and functions
    Type* option_int_type = make_type(TYPE_STRUCT);
    Token option_int_name = {TOKEN_IDENT, "OptionInt", 9, 0};
    option_int_type->struct_type.name = option_int_name;
    add_symbol(global_scope, option_int_name, option_int_type, false);

    Type* oi_some_t = make_type(TYPE_FUNCTION);
    oi_some_t->fn_type.param_count = 1;
    oi_some_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_some_t->fn_type.param_types[0] = builtin_int;
    oi_some_t->fn_type.return_type = option_int_type;
    Token oi_some_tok = {TOKEN_IDENT, "OptionInt_Some", 14, 0};
    add_symbol(global_scope, oi_some_tok, oi_some_t, false);

    Type* oi_none_t = make_type(TYPE_FUNCTION);
    oi_none_t->fn_type.param_count = 0;
    oi_none_t->fn_type.param_types = NULL;
    oi_none_t->fn_type.return_type = option_int_type;
    Token oi_none_tok = {TOKEN_IDENT, "OptionInt_None", 14, 0};
    add_symbol(global_scope, oi_none_tok, oi_none_t, false);

    Type* oi_is_some_t = make_type(TYPE_FUNCTION);
    oi_is_some_t->fn_type.param_count = 1;
    oi_is_some_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_is_some_t->fn_type.param_types[0] = option_int_type;
    oi_is_some_t->fn_type.return_type = builtin_int;
    Token oi_is_some_tok = {TOKEN_IDENT, "OptionInt_is_some", 17, 0};
    add_symbol(global_scope, oi_is_some_tok, oi_is_some_t, false);

    Type* oi_is_none_t = make_type(TYPE_FUNCTION);
    oi_is_none_t->fn_type.param_count = 1;
    oi_is_none_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_is_none_t->fn_type.param_types[0] = option_int_type;
    oi_is_none_t->fn_type.return_type = builtin_int;
    Token oi_is_none_tok = {TOKEN_IDENT, "OptionInt_is_none", 17, 0};
    add_symbol(global_scope, oi_is_none_tok, oi_is_none_t, false);

    Type* oi_unwrap_t = make_type(TYPE_FUNCTION);
    oi_unwrap_t->fn_type.param_count = 1;
    oi_unwrap_t->fn_type.param_types = malloc(sizeof(Type*));
    oi_unwrap_t->fn_type.param_types[0] = option_int_type;
    oi_unwrap_t->fn_type.return_type = builtin_int;
    Token oi_unwrap_tok = {TOKEN_IDENT, "OptionInt_unwrap", 16, 0};
    add_symbol(global_scope, oi_unwrap_tok, oi_unwrap_t, false);

    Type* oi_unwrap_or_t = make_type(TYPE_FUNCTION);
    oi_unwrap_or_t->fn_type.param_count = 2;
    oi_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    oi_unwrap_or_t->fn_type.param_types[0] = option_int_type;
    oi_unwrap_or_t->fn_type.param_types[1] = builtin_int;
    oi_unwrap_or_t->fn_type.return_type = builtin_int;
    Token oi_unwrap_or_tok = {TOKEN_IDENT, "OptionInt_unwrap_or", 19, 0};
    add_symbol(global_scope, oi_unwrap_or_tok, oi_unwrap_or_t, false);

    // OptionString type and functions
    Type* option_string_type = make_type(TYPE_STRUCT);
    Token option_string_name = {TOKEN_IDENT, "OptionString", 12, 0};
    option_string_type->struct_type.name = option_string_name;
    add_symbol(global_scope, option_string_name, option_string_type, false);

    Type* os_some_t = make_type(TYPE_FUNCTION);
    os_some_t->fn_type.param_count = 1;
    os_some_t->fn_type.param_types = malloc(sizeof(Type*));
    os_some_t->fn_type.param_types[0] = builtin_string;
    os_some_t->fn_type.return_type = option_string_type;
    Token os_some_tok = {TOKEN_IDENT, "OptionString_Some", 17, 0};
    add_symbol(global_scope, os_some_tok, os_some_t, false);

    Type* os_none_t = make_type(TYPE_FUNCTION);
    os_none_t->fn_type.param_count = 0;
    os_none_t->fn_type.param_types = NULL;
    os_none_t->fn_type.return_type = option_string_type;
    Token os_none_tok = {TOKEN_IDENT, "OptionString_None", 17, 0};
    add_symbol(global_scope, os_none_tok, os_none_t, false);

    Type* os_is_some_t = make_type(TYPE_FUNCTION);
    os_is_some_t->fn_type.param_count = 1;
    os_is_some_t->fn_type.param_types = malloc(sizeof(Type*));
    os_is_some_t->fn_type.param_types[0] = option_string_type;
    os_is_some_t->fn_type.return_type = builtin_int;
    Token os_is_some_tok = {TOKEN_IDENT, "OptionString_is_some", 20, 0};
    add_symbol(global_scope, os_is_some_tok, os_is_some_t, false);

    Type* os_is_none_t = make_type(TYPE_FUNCTION);
    os_is_none_t->fn_type.param_count = 1;
    os_is_none_t->fn_type.param_types = malloc(sizeof(Type*));
    os_is_none_t->fn_type.param_types[0] = option_string_type;
    os_is_none_t->fn_type.return_type = builtin_int;
    Token os_is_none_tok = {TOKEN_IDENT, "OptionString_is_none", 20, 0};
    add_symbol(global_scope, os_is_none_tok, os_is_none_t, false);

    Type* os_unwrap_t = make_type(TYPE_FUNCTION);
    os_unwrap_t->fn_type.param_count = 1;
    os_unwrap_t->fn_type.param_types = malloc(sizeof(Type*));
    os_unwrap_t->fn_type.param_types[0] = option_string_type;
    os_unwrap_t->fn_type.return_type = builtin_string;
    Token os_unwrap_tok = {TOKEN_IDENT, "OptionString_unwrap", 19, 0};
    add_symbol(global_scope, os_unwrap_tok, os_unwrap_t, false);

    Type* os_unwrap_or_t = make_type(TYPE_FUNCTION);
    os_unwrap_or_t->fn_type.param_count = 2;
    os_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    os_unwrap_or_t->fn_type.param_types[0] = option_string_type;
    os_unwrap_or_t->fn_type.param_types[1] = builtin_string;
    os_unwrap_or_t->fn_type.return_type = builtin_string;
    Token os_unwrap_or_tok = {TOKEN_IDENT, "OptionString_unwrap_or", 22, 0};
    add_symbol(global_scope, os_unwrap_or_tok, os_unwrap_or_t, false);

    // OptionFloat type and functions
    Type* optionfloat_type = make_type(TYPE_STRUCT);
    Token OptionFloat_name = {TOKEN_IDENT, "OptionFloat", 11, 0};
    optionfloat_type->struct_type.name = OptionFloat_name;
    add_symbol(global_scope, OptionFloat_name, optionfloat_type, false);
    Type* optionfloat_Some_t = make_type(TYPE_FUNCTION);
    optionfloat_Some_t->fn_type.param_count = 1;
    optionfloat_Some_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionfloat_Some_t->fn_type.param_types[0] = builtin_float;
    optionfloat_Some_t->fn_type.return_type = optionfloat_type;
    Token optionfloat_Some_tok = {TOKEN_IDENT, "OptionFloat_Some", 16, 0};
    add_symbol(global_scope, optionfloat_Some_tok, optionfloat_Some_t, false);
    Type* optionfloat_None_t = make_type(TYPE_FUNCTION);
    optionfloat_None_t->fn_type.param_count = 0;
    optionfloat_None_t->fn_type.param_types = NULL;
    optionfloat_None_t->fn_type.return_type = optionfloat_type;
    Token optionfloat_None_tok = {TOKEN_IDENT, "OptionFloat_None", 16, 0};
    add_symbol(global_scope, optionfloat_None_tok, optionfloat_None_t, false);
    Type* optionfloat_is_some_t = make_type(TYPE_FUNCTION);
    optionfloat_is_some_t->fn_type.param_count = 1;
    optionfloat_is_some_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionfloat_is_some_t->fn_type.param_types[0] = optionfloat_type;
    optionfloat_is_some_t->fn_type.return_type = builtin_int;
    Token optionfloat_is_some_tok = {TOKEN_IDENT, "OptionFloat_is_some", 19, 0};
    add_symbol(global_scope, optionfloat_is_some_tok, optionfloat_is_some_t, false);
    Type* optionfloat_is_none_t = make_type(TYPE_FUNCTION);
    optionfloat_is_none_t->fn_type.param_count = 1;
    optionfloat_is_none_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionfloat_is_none_t->fn_type.param_types[0] = optionfloat_type;
    optionfloat_is_none_t->fn_type.return_type = builtin_int;
    Token optionfloat_is_none_tok = {TOKEN_IDENT, "OptionFloat_is_none", 19, 0};
    add_symbol(global_scope, optionfloat_is_none_tok, optionfloat_is_none_t, false);
    Type* optionfloat_unwrap_t = make_type(TYPE_FUNCTION);
    optionfloat_unwrap_t->fn_type.param_count = 1;
    optionfloat_unwrap_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionfloat_unwrap_t->fn_type.param_types[0] = optionfloat_type;
    optionfloat_unwrap_t->fn_type.return_type = builtin_float;
    Token optionfloat_unwrap_tok = {TOKEN_IDENT, "OptionFloat_unwrap", 18, 0};
    add_symbol(global_scope, optionfloat_unwrap_tok, optionfloat_unwrap_t, false);
    Type* optionfloat_unwrap_or_t = make_type(TYPE_FUNCTION);
    optionfloat_unwrap_or_t->fn_type.param_count = 2;
    optionfloat_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    optionfloat_unwrap_or_t->fn_type.param_types[0] = optionfloat_type;
    optionfloat_unwrap_or_t->fn_type.param_types[1] = builtin_float;
    optionfloat_unwrap_or_t->fn_type.return_type = builtin_float;
    Token optionfloat_unwrap_or_tok = {TOKEN_IDENT, "OptionFloat_unwrap_or", 21, 0};
    add_symbol(global_scope, optionfloat_unwrap_or_tok, optionfloat_unwrap_or_t, false);

    // OptionBool type and functions
    Type* optionbool_type = make_type(TYPE_STRUCT);
    Token OptionBool_name = {TOKEN_IDENT, "OptionBool", 10, 0};
    optionbool_type->struct_type.name = OptionBool_name;
    add_symbol(global_scope, OptionBool_name, optionbool_type, false);
    Type* optionbool_Some_t = make_type(TYPE_FUNCTION);
    optionbool_Some_t->fn_type.param_count = 1;
    optionbool_Some_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionbool_Some_t->fn_type.param_types[0] = builtin_bool;
    optionbool_Some_t->fn_type.return_type = optionbool_type;
    Token optionbool_Some_tok = {TOKEN_IDENT, "OptionBool_Some", 15, 0};
    add_symbol(global_scope, optionbool_Some_tok, optionbool_Some_t, false);
    Type* optionbool_None_t = make_type(TYPE_FUNCTION);
    optionbool_None_t->fn_type.param_count = 0;
    optionbool_None_t->fn_type.param_types = NULL;
    optionbool_None_t->fn_type.return_type = optionbool_type;
    Token optionbool_None_tok = {TOKEN_IDENT, "OptionBool_None", 15, 0};
    add_symbol(global_scope, optionbool_None_tok, optionbool_None_t, false);
    Type* optionbool_is_some_t = make_type(TYPE_FUNCTION);
    optionbool_is_some_t->fn_type.param_count = 1;
    optionbool_is_some_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionbool_is_some_t->fn_type.param_types[0] = optionbool_type;
    optionbool_is_some_t->fn_type.return_type = builtin_int;
    Token optionbool_is_some_tok = {TOKEN_IDENT, "OptionBool_is_some", 18, 0};
    add_symbol(global_scope, optionbool_is_some_tok, optionbool_is_some_t, false);
    Type* optionbool_is_none_t = make_type(TYPE_FUNCTION);
    optionbool_is_none_t->fn_type.param_count = 1;
    optionbool_is_none_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionbool_is_none_t->fn_type.param_types[0] = optionbool_type;
    optionbool_is_none_t->fn_type.return_type = builtin_int;
    Token optionbool_is_none_tok = {TOKEN_IDENT, "OptionBool_is_none", 18, 0};
    add_symbol(global_scope, optionbool_is_none_tok, optionbool_is_none_t, false);
    Type* optionbool_unwrap_t = make_type(TYPE_FUNCTION);
    optionbool_unwrap_t->fn_type.param_count = 1;
    optionbool_unwrap_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    optionbool_unwrap_t->fn_type.param_types[0] = optionbool_type;
    optionbool_unwrap_t->fn_type.return_type = builtin_bool;
    Token optionbool_unwrap_tok = {TOKEN_IDENT, "OptionBool_unwrap", 17, 0};
    add_symbol(global_scope, optionbool_unwrap_tok, optionbool_unwrap_t, false);
    Type* optionbool_unwrap_or_t = make_type(TYPE_FUNCTION);
    optionbool_unwrap_or_t->fn_type.param_count = 2;
    optionbool_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    optionbool_unwrap_or_t->fn_type.param_types[0] = optionbool_type;
    optionbool_unwrap_or_t->fn_type.param_types[1] = builtin_bool;
    optionbool_unwrap_or_t->fn_type.return_type = builtin_bool;
    Token optionbool_unwrap_or_tok = {TOKEN_IDENT, "OptionBool_unwrap_or", 20, 0};
    add_symbol(global_scope, optionbool_unwrap_or_tok, optionbool_unwrap_or_t, false);

    // ResultFloat type and functions
    Type* resultfloat_type = make_type(TYPE_STRUCT);
    Token ResultFloat_name = {TOKEN_IDENT, "ResultFloat", 11, 0};
    resultfloat_type->struct_type.name = ResultFloat_name;
    add_symbol(global_scope, ResultFloat_name, resultfloat_type, false);
    Type* resultfloat_Ok_t = make_type(TYPE_FUNCTION);
    resultfloat_Ok_t->fn_type.param_count = 1;
    resultfloat_Ok_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_Ok_t->fn_type.param_types[0] = builtin_float;
    resultfloat_Ok_t->fn_type.return_type = resultfloat_type;
    Token resultfloat_Ok_tok = {TOKEN_IDENT, "ResultFloat_Ok", 14, 0};
    add_symbol(global_scope, resultfloat_Ok_tok, resultfloat_Ok_t, false);
    Type* resultfloat_Err_t = make_type(TYPE_FUNCTION);
    resultfloat_Err_t->fn_type.param_count = 1;
    resultfloat_Err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_Err_t->fn_type.param_types[0] = builtin_string;
    resultfloat_Err_t->fn_type.return_type = resultfloat_type;
    Token resultfloat_Err_tok = {TOKEN_IDENT, "ResultFloat_Err", 15, 0};
    add_symbol(global_scope, resultfloat_Err_tok, resultfloat_Err_t, false);
    Type* resultfloat_is_ok_t = make_type(TYPE_FUNCTION);
    resultfloat_is_ok_t->fn_type.param_count = 1;
    resultfloat_is_ok_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_is_ok_t->fn_type.param_types[0] = resultfloat_type;
    resultfloat_is_ok_t->fn_type.return_type = builtin_int;
    Token resultfloat_is_ok_tok = {TOKEN_IDENT, "ResultFloat_is_ok", 17, 0};
    add_symbol(global_scope, resultfloat_is_ok_tok, resultfloat_is_ok_t, false);
    Type* resultfloat_is_err_t = make_type(TYPE_FUNCTION);
    resultfloat_is_err_t->fn_type.param_count = 1;
    resultfloat_is_err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_is_err_t->fn_type.param_types[0] = resultfloat_type;
    resultfloat_is_err_t->fn_type.return_type = builtin_int;
    Token resultfloat_is_err_tok = {TOKEN_IDENT, "ResultFloat_is_err", 18, 0};
    add_symbol(global_scope, resultfloat_is_err_tok, resultfloat_is_err_t, false);
    Type* resultfloat_unwrap_t = make_type(TYPE_FUNCTION);
    resultfloat_unwrap_t->fn_type.param_count = 1;
    resultfloat_unwrap_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_unwrap_t->fn_type.param_types[0] = resultfloat_type;
    resultfloat_unwrap_t->fn_type.return_type = builtin_float;
    Token resultfloat_unwrap_tok = {TOKEN_IDENT, "ResultFloat_unwrap", 18, 0};
    add_symbol(global_scope, resultfloat_unwrap_tok, resultfloat_unwrap_t, false);
    Type* resultfloat_unwrap_err_t = make_type(TYPE_FUNCTION);
    resultfloat_unwrap_err_t->fn_type.param_count = 1;
    resultfloat_unwrap_err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultfloat_unwrap_err_t->fn_type.param_types[0] = resultfloat_type;
    resultfloat_unwrap_err_t->fn_type.return_type = builtin_string;
    Token resultfloat_unwrap_err_tok = {TOKEN_IDENT, "ResultFloat_unwrap_err", 22, 0};
    add_symbol(global_scope, resultfloat_unwrap_err_tok, resultfloat_unwrap_err_t, false);
    Type* resultfloat_unwrap_or_t = make_type(TYPE_FUNCTION);
    resultfloat_unwrap_or_t->fn_type.param_count = 2;
    resultfloat_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    resultfloat_unwrap_or_t->fn_type.param_types[0] = resultfloat_type;
    resultfloat_unwrap_or_t->fn_type.param_types[1] = builtin_float;
    resultfloat_unwrap_or_t->fn_type.return_type = builtin_float;
    Token resultfloat_unwrap_or_tok = {TOKEN_IDENT, "ResultFloat_unwrap_or", 21, 0};
    add_symbol(global_scope, resultfloat_unwrap_or_tok, resultfloat_unwrap_or_t, false);

    // ResultBool type and functions
    Type* resultbool_type = make_type(TYPE_STRUCT);
    Token ResultBool_name = {TOKEN_IDENT, "ResultBool", 10, 0};
    resultbool_type->struct_type.name = ResultBool_name;
    add_symbol(global_scope, ResultBool_name, resultbool_type, false);
    Type* resultbool_Ok_t = make_type(TYPE_FUNCTION);
    resultbool_Ok_t->fn_type.param_count = 1;
    resultbool_Ok_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_Ok_t->fn_type.param_types[0] = builtin_bool;
    resultbool_Ok_t->fn_type.return_type = resultbool_type;
    Token resultbool_Ok_tok = {TOKEN_IDENT, "ResultBool_Ok", 13, 0};
    add_symbol(global_scope, resultbool_Ok_tok, resultbool_Ok_t, false);
    Type* resultbool_Err_t = make_type(TYPE_FUNCTION);
    resultbool_Err_t->fn_type.param_count = 1;
    resultbool_Err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_Err_t->fn_type.param_types[0] = builtin_string;
    resultbool_Err_t->fn_type.return_type = resultbool_type;
    Token resultbool_Err_tok = {TOKEN_IDENT, "ResultBool_Err", 14, 0};
    add_symbol(global_scope, resultbool_Err_tok, resultbool_Err_t, false);
    Type* resultbool_is_ok_t = make_type(TYPE_FUNCTION);
    resultbool_is_ok_t->fn_type.param_count = 1;
    resultbool_is_ok_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_is_ok_t->fn_type.param_types[0] = resultbool_type;
    resultbool_is_ok_t->fn_type.return_type = builtin_int;
    Token resultbool_is_ok_tok = {TOKEN_IDENT, "ResultBool_is_ok", 16, 0};
    add_symbol(global_scope, resultbool_is_ok_tok, resultbool_is_ok_t, false);
    Type* resultbool_is_err_t = make_type(TYPE_FUNCTION);
    resultbool_is_err_t->fn_type.param_count = 1;
    resultbool_is_err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_is_err_t->fn_type.param_types[0] = resultbool_type;
    resultbool_is_err_t->fn_type.return_type = builtin_int;
    Token resultbool_is_err_tok = {TOKEN_IDENT, "ResultBool_is_err", 17, 0};
    add_symbol(global_scope, resultbool_is_err_tok, resultbool_is_err_t, false);
    Type* resultbool_unwrap_t = make_type(TYPE_FUNCTION);
    resultbool_unwrap_t->fn_type.param_count = 1;
    resultbool_unwrap_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_unwrap_t->fn_type.param_types[0] = resultbool_type;
    resultbool_unwrap_t->fn_type.return_type = builtin_bool;
    Token resultbool_unwrap_tok = {TOKEN_IDENT, "ResultBool_unwrap", 17, 0};
    add_symbol(global_scope, resultbool_unwrap_tok, resultbool_unwrap_t, false);
    Type* resultbool_unwrap_err_t = make_type(TYPE_FUNCTION);
    resultbool_unwrap_err_t->fn_type.param_count = 1;
    resultbool_unwrap_err_t->fn_type.param_types = malloc(sizeof(Type*) * 1);
    resultbool_unwrap_err_t->fn_type.param_types[0] = resultbool_type;
    resultbool_unwrap_err_t->fn_type.return_type = builtin_string;
    Token resultbool_unwrap_err_tok = {TOKEN_IDENT, "ResultBool_unwrap_err", 21, 0};
    add_symbol(global_scope, resultbool_unwrap_err_tok, resultbool_unwrap_err_t, false);
    Type* resultbool_unwrap_or_t = make_type(TYPE_FUNCTION);
    resultbool_unwrap_or_t->fn_type.param_count = 2;
    resultbool_unwrap_or_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    resultbool_unwrap_or_t->fn_type.param_types[0] = resultbool_type;
    resultbool_unwrap_or_t->fn_type.param_types[1] = builtin_bool;
    resultbool_unwrap_or_t->fn_type.return_type = builtin_bool;
    Token resultbool_unwrap_or_tok = {TOKEN_IDENT, "ResultBool_unwrap_or", 20, 0};
    add_symbol(global_scope, resultbool_unwrap_or_tok, resultbool_unwrap_or_t, false);

    // System functions
    Type* sys_exec_t = make_type(TYPE_FUNCTION);
    sys_exec_t->fn_type.param_count = 1;
    sys_exec_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exec_t->fn_type.param_types[0] = builtin_string;
    sys_exec_t->fn_type.return_type = builtin_string;
    Token sys_exec_tok = {TOKEN_IDENT, "System_exec", 11, 0};
    add_symbol(global_scope, sys_exec_tok, sys_exec_t, false);

    Type* sys_exec_code_t = make_type(TYPE_FUNCTION);
    sys_exec_code_t->fn_type.param_count = 1;
    sys_exec_code_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exec_code_t->fn_type.param_types[0] = builtin_string;
    sys_exec_code_t->fn_type.return_type = builtin_int;
    Token sys_exec_code_tok = {TOKEN_IDENT, "System_exec_code", 16, 0};
    add_symbol(global_scope, sys_exec_code_tok, sys_exec_code_t, false);

    Type* sys_exit_t = make_type(TYPE_FUNCTION);
    sys_exit_t->fn_type.param_count = 1;
    sys_exit_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_exit_t->fn_type.param_types[0] = builtin_int;
    sys_exit_t->fn_type.return_type = builtin_void;
    Token sys_exit_tok = {TOKEN_IDENT, "System_exit", 11, 0};
    add_symbol(global_scope, sys_exit_tok, sys_exit_t, false);

    Type* sys_env_t = make_type(TYPE_FUNCTION);
    sys_env_t->fn_type.param_count = 1;
    sys_env_t->fn_type.param_types = malloc(sizeof(Type*));
    sys_env_t->fn_type.param_types[0] = builtin_string;
    sys_env_t->fn_type.return_type = builtin_string;
    Token sys_env_tok = {TOKEN_IDENT, "System_env", 10, 0};
    add_symbol(global_scope, sys_env_tok, sys_env_t, false);

    // Conversion functions
    Type* itos_t = make_type(TYPE_FUNCTION);
    itos_t->fn_type.param_count = 1;
    itos_t->fn_type.param_types = malloc(sizeof(Type*));
    itos_t->fn_type.param_types[0] = builtin_int;
    itos_t->fn_type.return_type = builtin_string;
    Token itos_tok = {TOKEN_IDENT, "int_to_string", 13, 0};
    add_symbol(global_scope, itos_tok, itos_t, false);

    Type* ftos_t = make_type(TYPE_FUNCTION);
    ftos_t->fn_type.param_count = 1;
    ftos_t->fn_type.param_types = malloc(sizeof(Type*));
    ftos_t->fn_type.param_types[0] = builtin_float;
    ftos_t->fn_type.return_type = builtin_string;
    Token ftos_tok = {TOKEN_IDENT, "float_to_string", 15, 0};
    add_symbol(global_scope, ftos_tok, ftos_t, false);

    // Math stdlib - register all Math_ functions
    struct { const char* name; int param_count; Type* p1; Type* p2; Type* ret; } reg_math_fns[] = {
        {"Math_abs", 1, builtin_float, NULL, builtin_float},
        {"Math_max", 2, builtin_float, builtin_float, builtin_float},
        {"Math_min", 2, builtin_float, builtin_float, builtin_float},
        {"Math_pow", 2, builtin_float, builtin_float, builtin_float},
        {"Math_sqrt", 1, builtin_float, NULL, builtin_float},
        {"Math_floor", 1, builtin_float, NULL, builtin_float},
        {"Math_ceil", 1, builtin_float, NULL, builtin_float},
        {"Math_round", 1, builtin_float, NULL, builtin_float},
        {"Math_sin", 1, builtin_float, NULL, builtin_float},
        {"Math_cos", 1, builtin_float, NULL, builtin_float},
        {"Math_tan", 1, builtin_float, NULL, builtin_float},
        {"Math_round_to", 2, builtin_float, builtin_int, builtin_float},
        {"Math_atan2", 2, builtin_float, builtin_float, builtin_float},
        {"Math_pi", 0, NULL, NULL, builtin_float},
        {"Math_e", 0, NULL, NULL, builtin_float},
        {"Math_random", 0, NULL, NULL, builtin_float},
    };
    for (int i = 0; i < (int)(sizeof(reg_math_fns)/sizeof(reg_math_fns[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = reg_math_fns[i].param_count;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        if (reg_math_fns[i].p1) ft->fn_type.param_types[0] = reg_math_fns[i].p1;
        if (reg_math_fns[i].p2) ft->fn_type.param_types[1] = reg_math_fns[i].p2;
        ft->fn_type.return_type = reg_math_fns[i].ret;
        Token tok = {TOKEN_IDENT, reg_math_fns[i].name, (int)strlen(reg_math_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // String stdlib - function-style str_ functions
    struct { const char* name; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } str_fns[] = {
        {"str_len", 1, builtin_string, NULL, NULL, builtin_int},
        {"str_upper", 1, builtin_string, NULL, NULL, builtin_string},
        {"str_lower", 1, builtin_string, NULL, NULL, builtin_string},
        {"str_trim", 1, builtin_string, NULL, NULL, builtin_string},
        {"str_contains", 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_starts_with", 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_ends_with", 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_index_of", 2, builtin_string, builtin_string, NULL, builtin_int},
        {"str_replace", 3, builtin_string, builtin_string, builtin_string, builtin_string},
        {"str_substring", 3, builtin_string, builtin_int, builtin_int, builtin_string},
        {"str_repeat", 2, builtin_string, builtin_int, NULL, builtin_string},
        {"str_concat", 2, builtin_string, builtin_string, NULL, builtin_string},
        {"str_to_int", 1, builtin_string, NULL, NULL, builtin_int},
    };
    for (int i = 0; i < (int)(sizeof(str_fns)/sizeof(str_fns[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = str_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (str_fns[i].p1) ft->fn_type.param_types[0] = str_fns[i].p1;
        if (str_fns[i].p2) ft->fn_type.param_types[1] = str_fns[i].p2;
        if (str_fns[i].p3) ft->fn_type.param_types[2] = str_fns[i].p3;
        ft->fn_type.return_type = str_fns[i].ret;
        Token tok = {TOKEN_IDENT, str_fns[i].name, (int)strlen(str_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Path stdlib
    struct { const char* name; } reg_path_fns[] = {
        {"Path_basename", 13}, {"Path_dirname", 12},
        {"Path_extension", 14}, {"Path_join", 9},
    };
    for (int i = 0; i < 4; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = (i == 3) ? 2 : 1; // Path_join takes 2
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = builtin_string;
        if (i == 3) ft->fn_type.param_types[1] = builtin_string;
        ft->fn_type.return_type = builtin_string;
        Token tok = {TOKEN_IDENT, reg_path_fns[i].name, (int)strlen(reg_path_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }
    
    // JSON stdlib
    Type* json_obj_type = make_type(TYPE_MAP);
    struct { const char* name; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } json_fns[] = {
        {"json_new", 0, NULL, NULL, NULL, json_obj_type},
        {"json_set_string", 3, json_obj_type, builtin_string, builtin_string, builtin_void},
        {"json_set_int", 3, json_obj_type, builtin_string, builtin_int, builtin_void},
        // param0 is the JSON handle. json_parse() is registered (blanket loop) as
        // a plain int handle, so accept int here (not json_obj_type) or a var
        // holding a parsed doc fails the arg check. Return type MUST be string so
        // `s = json_get_string(..)` infers string everywhere, not just in
        // print/interp position (json_get_int stays shadowed as unchecked int).
        {"json_get_string", 2, builtin_int, builtin_string, NULL, builtin_string},
        {"json_get_int", 2, json_obj_type, builtin_string, NULL, builtin_int},
        {"json_stringify", 1, json_obj_type, NULL, NULL, builtin_string},
        {"Regex_match", 2, builtin_string, builtin_string, NULL, builtin_bool},
        {"Regex_replace", 3, builtin_string, builtin_string, builtin_string, builtin_string},
    };
    for (int i = 0; i < 8; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = json_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (json_fns[i].p1) ft->fn_type.param_types[0] = json_fns[i].p1;
        if (json_fns[i].p2) ft->fn_type.param_types[1] = json_fns[i].p2;
        if (json_fns[i].p3) ft->fn_type.param_types[2] = json_fns[i].p3;
        ft->fn_type.return_type = json_fns[i].ret;
        Token tok = {TOKEN_IDENT, json_fns[i].name, (int)strlen(json_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Register namespace identifiers so checker doesn't reject File.read() etc.
    // Also register their methods with proper return types
    const char* namespaces[] = {"File", "Path", "DateTime", "Time", "Json", "Http", "HashMap", "HashSet", "Regex", "System", "Terminal", "Color", "Test", "Math", "Env", "Net", "Url", "Task", "Db", "Gui", "Audio", "StringBuilder", "Crypto", "Encoding", "Os", "Uuid", "Log", "Process", "Csv", "Template", "Socket", "Ws", "Args", "Base64", "Toml", "Bcrypt", "Random", "Web", "Smtp", "App", "Shared", "Ptr", NULL};
    for (int i = 0; namespaces[i]; i++) {
        Token ns_tok = {TOKEN_IDENT, namespaces[i], (int)strlen(namespaces[i]), 0};
        if (!find_symbol(global_scope, ns_tok)) {
            add_symbol(global_scope, ns_tok, builtin_int, false);
        }
    }
    
    // Register loaded user modules as namespace symbols
    {
        extern int get_module_count(void);
        extern void* get_module_entry_at(int index);
        int mc = get_module_count();
        for (int mi = 0; mi < mc; mi++) {
            typedef struct { char* name; void* ast; } ME;
            ME* mod = (ME*)get_module_entry_at(mi);
            // Register short name (last segment after /)
            char* slash = strrchr(mod->name, '/');
            const char* short_name = slash ? slash + 1 : mod->name;
            Token ns_tok = {TOKEN_IDENT, short_name, (int)strlen(short_name), 0};
            if (!find_symbol(global_scope, ns_tok)) {
                add_symbol(global_scope, ns_tok, builtin_int, false);
            }
        }
    }

    // File namespace methods
    struct { const char* name; int pc; Type* p1; Type* p2; Type* ret; } file_ns_fns[] = {
        {"File_read", 1, builtin_string, NULL, builtin_string},
        {"File_write", 2, builtin_string, builtin_string, builtin_int},
        {"File_exists", 1, builtin_string, NULL, builtin_int},
        {"File_delete", 1, builtin_string, NULL, builtin_int},
        {"File_copy", 2, builtin_string, builtin_string, builtin_int},
        {"File_move", 2, builtin_string, builtin_string, builtin_int},
        {"File_size", 1, builtin_string, NULL, builtin_int},
        {"File_is_dir", 1, builtin_string, NULL, builtin_int},
        {"File_is_file", 1, builtin_string, NULL, builtin_int},
        {"File_mkdir", 1, builtin_string, NULL, builtin_int},
        {"File_list_dir", 1, builtin_string, NULL, builtin_string},
        {"File_append", 2, builtin_string, builtin_string, builtin_int},
        {"File_cwd", 0, NULL, NULL, builtin_string},
        {"File_open", 2, builtin_string, builtin_string, builtin_int},
        {"File_read_line", 1, builtin_int, NULL, builtin_string},
        {"File_write_line", 2, builtin_int, builtin_string, builtin_int},
        {"File_eof", 1, builtin_int, NULL, builtin_int},
        {"File_close", 1, builtin_int, NULL, builtin_void},
    };
    for (int i = 0; i < (int)(sizeof(file_ns_fns)/sizeof(file_ns_fns[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = file_ns_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = file_ns_fns[i].p1;
        if (file_ns_fns[i].p2) ft->fn_type.param_types[1] = file_ns_fns[i].p2;
        ft->fn_type.return_type = file_ns_fns[i].ret;
        Token tok = {TOKEN_IDENT, file_ns_fns[i].name, (int)strlen(file_ns_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // File.read_lines(path) -> [string] (week-one stdlib, P5). Registered with
    // its real array return type so downstream len()/for/indexing all see
    // [string] instead of the old builtin_string placeholder.
    {
        Type* lines_ret = make_type(TYPE_ARRAY);
        lines_ret->array_type.element_type = builtin_string;
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 1;
        ft->fn_type.param_types = malloc(sizeof(Type*));
        ft->fn_type.param_types[0] = builtin_string;
        ft->fn_type.return_type = lines_ret;
        Token tok = {TOKEN_IDENT, "File_read_lines", 15, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Json.keys(j) -> [string] (real array return type, so for/len/index work).
    {
        Type* keys_ret = make_type(TYPE_ARRAY);
        keys_ret->array_type.element_type = builtin_string;
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 1;
        ft->fn_type.param_types = malloc(sizeof(Type*));
        ft->fn_type.param_types[0] = builtin_int;  // json handle
        ft->fn_type.return_type = keys_ret;
        Token tok = {TOKEN_IDENT, "Json_keys", 9, 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // HashMap namespace: HashMap.new() -> HashMap_new()
    Type* map_type = make_type(TYPE_MAP);
    reg_fn("HashMap_new", map_type, 0);

    // HashSet namespace: HashSet.new() -> HashSet_new()
    Type* set_type = make_type(TYPE_SET);
    reg_fn("HashSet_new", set_type, 0);

    // Json namespace
    Type* json_type = make_type(TYPE_MAP); // opaque pointer
    struct { const char* name; int pc; Type* p1; Type* p2; Type* p3; Type* ret; } json_ns_fns[] = {
        {"Json_new", 0, NULL, NULL, NULL, json_type},
        {"Json_set_string", 3, json_type, builtin_string, builtin_string, builtin_void},
        {"Json_set_int", 3, json_type, builtin_string, builtin_int, builtin_void},
        {"Json_set_bool", 3, json_type, builtin_string, builtin_int, builtin_void},
        // Json_get_string and Json_get_int moved to new_fns for flexible type checking
        // {"Json_get_string", 2, json_type, builtin_string, NULL, builtin_string},
        // {"Json_get_int", 2, json_type, builtin_string, NULL, builtin_int},
        {"Json_stringify", 1, json_type, NULL, NULL, builtin_string},
    };
    // Iterate the actual array length - two entries were commented out above, so
    // a hardcoded 6 read one past the end (ASan: stack-buffer-overflow here).
    int json_ns_fn_count = (int)(sizeof(json_ns_fns) / sizeof(json_ns_fns[0]));
    for (int i = 0; i < json_ns_fn_count; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = json_ns_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 3);
        if (json_ns_fns[i].p1) ft->fn_type.param_types[0] = json_ns_fns[i].p1;
        if (json_ns_fns[i].p2) ft->fn_type.param_types[1] = json_ns_fns[i].p2;
        if (json_ns_fns[i].p3) ft->fn_type.param_types[2] = json_ns_fns[i].p3;
        ft->fn_type.return_type = json_ns_fns[i].ret;
        Token tok = {TOKEN_IDENT, json_ns_fns[i].name, (int)strlen(json_ns_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Http namespace
    reg_fn("Http_get", builtin_string, 1, builtin_string);

    // Regex namespace
    reg_fn("Regex_match", builtin_int, 2, builtin_string, builtin_string);
    reg_fn("Regex_replace", builtin_string, 3, builtin_string, builtin_string, builtin_string);

    // Terminal namespace
    struct { const char* name; int pc; Type* p1; Type* p2; Type* ret; } term_fns[] = {
        {"Terminal_cols", 0, NULL, NULL, builtin_int},
        {"Terminal_rows", 0, NULL, NULL, builtin_int},
        {"Terminal_raw_mode", 0, NULL, NULL, builtin_void},
        {"Terminal_restore", 0, NULL, NULL, builtin_void},
        {"Terminal_read_key", 0, NULL, NULL, builtin_int},
        {"Terminal_clear", 0, NULL, NULL, builtin_void},
        {"Terminal_write", 1, builtin_string, NULL, builtin_void},
        {"Terminal_move", 2, builtin_int, builtin_int, builtin_void},
    };
    for (int i = 0; i < 8; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = term_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        if (term_fns[i].p1) ft->fn_type.param_types[0] = term_fns[i].p1;
        if (term_fns[i].p2) ft->fn_type.param_types[1] = term_fns[i].p2;
        ft->fn_type.return_type = term_fns[i].ret;
        Token tok = {TOKEN_IDENT, term_fns[i].name, (int)strlen(term_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // DateTime stdlib
    // Color namespace - string-returning color functions
    const char* color_fn_names[] = {
        "Color_red", "Color_green", "Color_yellow", "Color_blue",
        "Color_magenta", "Color_cyan", "Color_gray", "Color_bold",
        "Color_dim", "Color_underline"
    };
    for (int i = 0; i < (int)(sizeof(color_fn_names)/sizeof(color_fn_names[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = 1;
        ft->fn_type.param_types = malloc(sizeof(Type*));
        ft->fn_type.param_types[0] = builtin_string;
        ft->fn_type.return_type = builtin_string;
        Token tok = {TOKEN_IDENT, color_fn_names[i], strlen(color_fn_names[i]), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    Type* dt_now_t = make_type(TYPE_FUNCTION);
    dt_now_t->fn_type.param_count = 0;
    dt_now_t->fn_type.param_types = NULL;
    dt_now_t->fn_type.return_type = builtin_int;
    Token dt_now_tok = {TOKEN_IDENT, "DateTime_now", 12, 0};
    add_symbol(global_scope, dt_now_tok, dt_now_t, false);
    
    // DateTime.millis / DateTime.micros
    Type* dt_millis_t = make_type(TYPE_FUNCTION);
    dt_millis_t->fn_type.param_count = 0;
    dt_millis_t->fn_type.param_types = NULL;
    dt_millis_t->fn_type.return_type = builtin_int;
    Token dt_millis_tok = {TOKEN_IDENT, "DateTime_millis", 15, 0};
    add_symbol(global_scope, dt_millis_tok, dt_millis_t, false);
    Type* dt_micros_t = make_type(TYPE_FUNCTION);
    dt_micros_t->fn_type.param_count = 0;
    dt_micros_t->fn_type.param_types = NULL;
    dt_micros_t->fn_type.return_type = builtin_int;
    Token dt_micros_tok = {TOKEN_IDENT, "DateTime_micros", 15, 0};
    add_symbol(global_scope, dt_micros_tok, dt_micros_t, false);
    
    Type* dt_format_t = make_type(TYPE_FUNCTION);
    dt_format_t->fn_type.param_count = 2;
    dt_format_t->fn_type.param_types = malloc(sizeof(Type*) * 2);
    dt_format_t->fn_type.param_types[0] = builtin_int;
    dt_format_t->fn_type.param_types[1] = builtin_string;
    dt_format_t->fn_type.return_type = builtin_string;
    Token dt_format_tok = {TOKEN_IDENT, "DateTime_format", 15, 0};
    add_symbol(global_scope, dt_format_tok, dt_format_t, false);
    
    Type* dt_sleep_t = make_type(TYPE_FUNCTION);
    dt_sleep_t->fn_type.param_count = 1;
    dt_sleep_t->fn_type.param_types = malloc(sizeof(Type*));
    dt_sleep_t->fn_type.param_types[0] = builtin_int;
    dt_sleep_t->fn_type.return_type = builtin_void;
    Token dt_sleep_tok = {TOKEN_IDENT, "DateTime_sleep", 14, 0};
    add_symbol(global_scope, dt_sleep_tok, dt_sleep_t, false);

    // Http namespace - additional methods
    // Http.post(url, data) -> string
    reg_fn("Http_post", builtin_string, 2, builtin_string, builtin_string);
    reg_fn("Http_put", builtin_string, 2, builtin_string, builtin_string);
    reg_fn("Http_delete", builtin_string, 1, builtin_string);
    // Http.serve(port) -> int (server fd)
    reg_fn("Http_serve", builtin_int, 1, builtin_int);
    // Http.accept(server_fd) -> string (request data)
    reg_fn("Http_accept", builtin_string, 1, builtin_int);
    // Http.respond(client_fd, status, content_type, body)
    reg_fn("Http_respond", builtin_void, 4,
           builtin_int, builtin_int, builtin_string, builtin_string);
    // Http.respond_json(fd, status, json_string)
    reg_fn("Http_respond_json", builtin_void, 3, builtin_int, builtin_int, builtin_string);
    // Http.respond_html(fd, status, html_string)
    reg_fn("Http_respond_html", builtin_void, 3, builtin_int, builtin_int, builtin_string);
    reg_fn("Http_set_header", builtin_void, 2, builtin_string, builtin_string);

    // Url namespace
    reg_fn("Url_encode", builtin_string, 1, builtin_string);
    reg_fn("Url_decode", builtin_string, 1, builtin_string);

    // Path namespace - already registered above

    // System namespace
    reg_fn("System_exec", builtin_string, 1, builtin_string);
    reg_fn("System_exec_code", builtin_int, 1, builtin_string);
    reg_fn("System_env", builtin_string, 1, builtin_string);

    // Math namespace - already registered above

    // Net namespace
    reg_fn("Net_listen", builtin_int, 1, builtin_int);
    reg_fn("Net_connect", builtin_int, 2, builtin_string, builtin_int);
    reg_fn("Net_send", builtin_int, 2, builtin_int, builtin_string);
    reg_fn("Net_recv", builtin_string, 1, builtin_int);
    reg_fn("Net_close", builtin_int, 1, builtin_int);

    // Task namespace
    struct { const char* name; Type* ret; int pc; Type* p1; } reg_task_fns[] = {
        {"Task_value", builtin_int, 1, builtin_int},
        {"Task_get", builtin_int, 1, builtin_int},
        {"Task_set", builtin_void, 2, builtin_int},
        {"Task_add", builtin_void, 2, builtin_int},
        {"Task_channel", builtin_int, 1, builtin_int},
        {"Task_send", builtin_void, 2, builtin_int},
        {"Task_recv", builtin_int, 1, builtin_int},
        {"Task_close", builtin_void, 1, builtin_int},
        // Task.try_recv(ch) — non-blocking receive returning int? (Some/none).
        {"Task_try_recv", builtin_int_opt, 1, builtin_int},
        {"Task_select_2", builtin_int, 2, builtin_int},
        {"Task_select_3", builtin_int, 3, builtin_int},
        // S4 cooperative cancellation: Task.cancel(handle) requests cancellation
        // of an awaited spawn; Task.is_cancelled() lets a running task check if it
        // was cancelled (0 args - reads the current coroutine).
        {"Task_cancel", builtin_void, 1, builtin_int},
        {"Task_is_cancelled", builtin_bool, 0, builtin_int},
    };
    for (int i = 0; i < (int)(sizeof(reg_task_fns)/sizeof(reg_task_fns[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = reg_task_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = reg_task_fns[i].p1;
        ft->fn_type.param_types[1] = builtin_int;
        ft->fn_type.return_type = reg_task_fns[i].ret;
        Token tok = {TOKEN_IDENT, reg_task_fns[i].name, (int)strlen(reg_task_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Ptr namespace - FFI pointer-cell helpers for C out-parameters (`T**`).
    // Ptr.cell() -> ptr (a heap slot holding one pointer); Ptr.read(cell) -> ptr
    // (the pointer the callee stored); Ptr.write(cell, p); Ptr.free(cell).
    {
        struct { const char* name; Type* ret; int pc; Type* p0; Type* p1; } reg_ptr_fns[] = {
            {"Ptr_cell", builtin_ptr,  0, NULL,        NULL},
            {"Ptr_read", builtin_ptr,  1, builtin_ptr, NULL},
            {"Ptr_write", builtin_void, 2, builtin_ptr, builtin_ptr},
            {"Ptr_free", builtin_void, 1, builtin_ptr, NULL},
        };
        for (int i = 0; i < (int)(sizeof(reg_ptr_fns)/sizeof(reg_ptr_fns[0])); i++) {
            Type* ft = make_type(TYPE_FUNCTION);
            ft->fn_type.param_count = reg_ptr_fns[i].pc;
            ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
            ft->fn_type.param_types[0] = reg_ptr_fns[i].p0;
            ft->fn_type.param_types[1] = reg_ptr_fns[i].p1;
            ft->fn_type.return_type = reg_ptr_fns[i].ret;
            Token tok = {TOKEN_IDENT, reg_ptr_fns[i].name, (int)strlen(reg_ptr_fns[i].name), 0};
            add_symbol(global_scope, tok, ft, false);
        }
    }

    // Db namespace
    struct { const char* name; Type* ret; int pc; Type* p1; } reg_db_fns[] = {
        {"Db_open", builtin_int, 1, builtin_string},
        {"Db_exec", builtin_int, 2, builtin_int},
        {"Db_query", builtin_string, 2, builtin_int},
        {"Db_query_one", builtin_string, 2, builtin_int},
        {"Db_last_insert_id", builtin_int, 1, builtin_int},
        {"Db_error", builtin_string, 1, builtin_int},
        {"Db_close", builtin_void, 1, builtin_int},
    };
    for (int i = 0; i < 7; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = reg_db_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 2);
        ft->fn_type.param_types[0] = reg_db_fns[i].p1;

    // New module registrations
    struct { const char* name; int nparams; Type* ret; } new_fns[] = {
        {"Json_parse", 1, builtin_int},
        {"Json_stringify", 1, builtin_string},
        {"Json_get", 2, builtin_string},
        {"Json_get_string", 2, builtin_string},
        {"Json_get_int", 2, builtin_int},
        {"Json_has", 2, builtin_int},
        // Json_keys registered separately below with its real [string] return
        // type (find_symbol returns the first match, so no builtin_string entry
        // here may shadow it).
        {"Json_array_len", 1, builtin_int},
        {"Json_array_get", 2, builtin_int},
        {"Json_node_str", 1, builtin_string},
        {"Encoding_base64_encode", 1, builtin_string},
        {"Encoding_base64_decode", 1, builtin_string},
        {"Encoding_hex_encode", 1, builtin_string},
        {"Crypto_sha256", 1, builtin_string},
        {"Crypto_md5", 1, builtin_string},
        {"Os_platform", 0, builtin_string},
        {"Os_arch", 0, builtin_string},
        {"Os_hostname", 0, builtin_string},
        {"Os_pid", 0, builtin_int},
        {"Os_temp_dir", 0, builtin_string},
        {"Os_home_dir", 0, builtin_string},
        {"Uuid_generate", 0, builtin_string},
        {"Math_clamp", 3, builtin_int},
        {"Math_sign", 1, builtin_int},
        {"DateTime_diff", 2, builtin_int},
        {"DateTime_add_seconds", 2, builtin_int},
        {"DateTime_to_iso", 1, builtin_string},
        {"regex_find", 2, builtin_int},
        {"regex_find_all", 2, builtin_string},
        {"Net_resolve", 1, builtin_string},
        {"Db_escape", 1, builtin_string},
        {"Db_table_exists", 2, builtin_int},
        {"Log_debug", 1, builtin_void},
        {"Log_info", 1, builtin_void},
        {"Log_warn", 1, builtin_void},
        {"Log_error", 1, builtin_void},
        {"Log_set_level", 1, builtin_void},
        {"Process_exec_capture", 1, builtin_string},
        {"Process_exec_status", 1, builtin_int},
        // File_read_lines registered with its real [string] return type next to
        // the other File namespace fns (find_symbol returns the FIRST match, so
        // a builtin_string entry here would shadow the typed one).
        {"Http_timeout", 1, builtin_int},
        {"Http_listen", 1, builtin_int},
        {"Http_accept", 1, builtin_string},
        {"Http_accept_fd", 1, builtin_int},
        {"Http_read_request", 1, builtin_string},
        {"Http_method", 1, builtin_string},
        {"Http_path", 1, builtin_string},
        {"Http_body", 1, builtin_string},
        {"Http_req_body", 1, builtin_string},
        {"Http_fd", 1, builtin_int},
        {"Http_respond", 4, builtin_void},
        {"Http_respond_json", 3, builtin_void},
        {"Http_respond_with_header", 5, builtin_void},
        {"Http_close_client", 1, builtin_void},
        {"Http_route_match", 3, builtin_int},
        {"Ws_connect", 1, builtin_int},
        {"Ws_send", 2, builtin_int},
        {"Ws_recv", 1, builtin_string},
        {"Socket_connect", 1, builtin_int},
        {"Socket_send", 2, builtin_int},
        {"Socket_recv", 1, builtin_string},
        {"Crypto_sha1", 1, builtin_string},
        {"Crypto_sha1_base64", 1, builtin_string},
        {"Crypto_hmac_sha256", 2, builtin_string},
        {"Crypto_hmac_sha256_hex", 2, builtin_string},
        {"Crypto_random_bytes", 1, builtin_string},
        {"Json_to_pretty_string", 1, builtin_string},
        {"Csv_parse", 1, builtin_int},
        {"Csv_row_count", 1, builtin_int},
        {"Csv_get", 2, builtin_string},
        {"Csv_get_field", 3, builtin_string},
        {"Csv_col_count", 1, builtin_int},
        {"Csv_header", 1, builtin_string},
        {"Csv_header_count", 1, builtin_int},
        {"Http_get_json", 1, builtin_int},
        {"Http_post_json", 2, builtin_int},
        {"Json_get_float", 2, builtin_float},
        {"Json_get_bool", 2, builtin_int},
        {"Json_get_array", 2, builtin_int},
        {"Json_get_object", 2, builtin_int},
        {"File_glob", 1, builtin_string},
        {"File_walk_dir", 1, builtin_string},
        {"File_temp_file", 1, builtin_string},
        {"DateTime_format_duration", 1, builtin_string},
        {"DateTime_day_of_week", 1, builtin_int},
        {"DateTime_year", 1, builtin_int},
        {"DateTime_month", 1, builtin_int},
        {"DateTime_day", 1, builtin_int},
        {"DateTime_hour", 1, builtin_int},
        {"DateTime_minute", 1, builtin_int},
        {"DateTime_second", 1, builtin_int},
        {"regex_split", 2, builtin_string},
        {"Regex_split", 2, builtin_string},
        {"Regex_find", 2, builtin_int},
        {"Regex_find_all", 2, builtin_string},
        {"Encoding_hex_decode", 1, builtin_string},
        {"Encoding_csv_parse", 1, builtin_string},
        // Missing functions from audit
        {"Env_get", 1, builtin_string},
        {"Env_set", 2, builtin_int},
        {"File_rename", 2, builtin_int},
        {"Db_open", 1, builtin_int},
        {"Db_close", 1, builtin_void},
        {"Db_exec", 2, builtin_int},
        {"Db_exec_p", 3, builtin_int},
        {"Db_query", 2, builtin_string},
        {"Db_query_one", 2, builtin_string},
        {"Db_query_p", 3, builtin_string},
        {"Db_error", 1, builtin_string},
        {"Db_last_insert_id", 1, builtin_int},
        {"Http_body", 1, builtin_string},
        {"Http_header", 2, builtin_string},
        {"Http_status", 1, builtin_int},
        {"Http_ctx_fd", 1, builtin_int},
        {"Http_set_timeout", 1, builtin_void},
        {"Http_close_server", 1, builtin_void},
        {"Http_free", 1, builtin_void},
        {"Time_now_millis", 0, builtin_int},
        {"Time_format", 1, builtin_string},
        {"Time_sleep", 1, builtin_void},
        {"Task_channel", 0, builtin_int},
        {"Task_value", 1, builtin_int},
        {"Task_get", 1, builtin_int},
        {"Task_recv", 1, builtin_int},
        {"Task_try_recv", 1, builtin_int_opt},
        {"Task_select_2", 1, builtin_int},
        {"Task_select_3", 1, builtin_int},
        {"Task_free_value", 1, builtin_void},
        {"Math_abs", 1, builtin_int},
        {"Socket_set_timeout", 2, builtin_int},
        {"Socket_set_nonblocking", 2, builtin_int},
        {"Socket_poll_read", 2, builtin_int},
        {"Socket_read_line", 1, builtin_string},
        {"Socket_close", 1, builtin_void},
        {"Ws_close", 1, builtin_void},
        {"System_gc", 0, builtin_void},
        {"System_load_env", 0, builtin_void},
        {"System_set_env", 2, builtin_int},
        {"Data_save", 2, builtin_void},
        {"Template_render", 2, builtin_string},
        {"Template_render_string", 2, builtin_string},
        {"String_char_from_int", 1, builtin_string},
        {"String_char", 1, builtin_string},
        {"String_from_chars", 1, builtin_string},
        {"Fs_read_file", 1, builtin_string},
        {"Queue_push", 1, builtin_void},
        {"Queue_pop", 0, builtin_int},
        {"Queue_peek", 1, builtin_int},
        {"Queue_len", 1, builtin_int},
        {"Queue_is_empty", 1, builtin_int},
        {"Stack_push", 1, builtin_void},
        {"Stack_pop", 1, builtin_int},
        {"Stack_peek", 1, builtin_int},
        {"Stack_len", 1, builtin_int},
        {"Stack_is_empty", 1, builtin_int},
        {"Terminal_color", 1, builtin_void},
        {"Terminal_bg", 1, builtin_void},
        {"Terminal_bold", 1, builtin_void},
        {"Terminal_dim", 1, builtin_void},
        {"Terminal_underline", 1, builtin_void},
        {"Terminal_reset", 1, builtin_void},
        {"Terminal_hide_cursor", 1, builtin_void},
        {"Terminal_show_cursor", 1, builtin_void},
        {"Terminal_box", 1, builtin_void},
        {"Terminal_progress", 1, builtin_void},
        {"Terminal_print_color", 1, builtin_void},
        {"Test_init", 1, builtin_void},
        {"Test_assert", 1, builtin_void},
        {"Test_describe", 1, builtin_void},
        {"Test_skip", 1, builtin_void},
        {"Test_summary", 1, builtin_int},
        {"Json_set", 1, builtin_void},
    };
    int new_fns_count = sizeof(new_fns) / sizeof(new_fns[0]);
    for (int i = 0; i < new_fns_count; i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = new_fns[i].nparams;
        ft->fn_type.param_types = malloc(sizeof(Type*) * (new_fns[i].nparams + 1));
        // Use generic type for params - C compiler validates actual types
        for (int p = 0; p < new_fns[i].nparams; p++) ft->fn_type.param_types[p] = builtin_int;
        ft->fn_type.is_variadic = true; // Allow flexible arg types for builtins
        ft->fn_type.return_type = new_fns[i].ret;
        Token tok = {TOKEN_IDENT, new_fns[i].name, (int)strlen(new_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }

    // Gui namespace
    struct { const char* name; Type* ret; int pc; } reg_gui_fns[] = {
        {"Gui_create", builtin_int, 3},
        {"Gui_clear", builtin_void, 3},
        {"Gui_color", builtin_void, 3},
        {"Gui_rect", builtin_void, 4},
        {"Gui_line", builtin_void, 4},
        {"Gui_point", builtin_void, 2},
        {"Gui_present", builtin_void, 0},
        {"Gui_poll", builtin_string, 0},
        {"Gui_running", builtin_int, 0},
        {"Gui_delay", builtin_void, 1},
        {"Gui_width", builtin_int, 0},
        {"Gui_height", builtin_int, 0},
        {"Gui_destroy", builtin_void, 0},
        {"Gui_text", builtin_void, 4},
        {"Gui_text_input", builtin_void, 4},
        {"Gui_text_input_activate", builtin_void, 1},
        {"Gui_text_input_key", builtin_int, 1},
        {"Gui_text_input_value", builtin_string, 0},
        {"Gui_text_input_clear", builtin_void, 0},
        {"Gui_text_input_set", builtin_void, 1},
        {"Gui_button", builtin_void, 5},
        {"Gui_button_clicked", builtin_int, 6},
        {"Gui_panel", builtin_void, 4},
        {"Gui_progress", builtin_void, 5},
        {"Gui_circle", builtin_void, 3},
        {"Gui_label", builtin_void, 3},
        {"Gui_rect_outline", builtin_void, 4},
        {"Gui_key_pressed", builtin_int, 1},
        {"Gui_mouse_x", builtin_int, 0},
        {"Gui_mouse_y", builtin_int, 0},
        {"Gui_mouse_down", builtin_int, 0},
        {"Gui_ticks", builtin_int, 0},
        {"Gui_load_sprite", builtin_int, 1},
        {"Gui_draw_sprite", builtin_void, 3},
        {"Gui_draw_sprite_scaled", builtin_void, 5},
    };
    for (int i = 0; i < (int)(sizeof(reg_gui_fns)/sizeof(reg_gui_fns[0])); i++) {
        Type* ft = make_type(TYPE_FUNCTION);
        ft->fn_type.param_count = reg_gui_fns[i].pc;
        ft->fn_type.param_types = malloc(sizeof(Type*) * 4);
        for (int j = 0; j < 4; j++) ft->fn_type.param_types[j] = builtin_int;
        if (i == 0) ft->fn_type.param_types[0] = builtin_string; // create(title, w, h)
        if (i == 7) ft->fn_type.param_types[0] = NULL; // poll()
        if (i == 13) { ft->fn_type.param_types[2] = builtin_string; } // text(x, y, str, scale)
        ft->fn_type.return_type = reg_gui_fns[i].ret;
        Token tok = {TOKEN_IDENT, reg_gui_fns[i].name, (int)strlen(reg_gui_fns[i].name), 0};
        add_symbol(global_scope, tok, ft, false);
    }
    }
}
