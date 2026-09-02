// codegen_program.c - Program-level code generation
// Included from codegen.c - shares all statics

// Emit `len` bytes of `s` into a C string literal that will be used as a printf
// FORMAT string, doubling every `%` so it survives as a literal percent.
//
// Needed because a `test "..."` name is spliced straight into the emitted
// printf's format. A name like "a 30%-alpha stroke" made `%-a` a real conversion
// that consumed an argument nobody passed - undefined behaviour, and visibly
// wrong output ("a 300x1.08p-1044lpha stroke").
static void emit_percent_doubled(const char* s, int len) {
    for (int i = 0; i < len; i++) {
        if (s[i] == '%') emit("%%%%");   // one literal %% in the generated C
        else emit("%c", s[i]);
    }
}

// Track which Option<Struct> family typedefs have already been emitted this
// compilation, so the per-struct emission (below) and the standalone catch-all
// block don't emit the same family twice. Reset at the top of codegen_program.
static char** emitted_opt_struct = NULL;
static int emitted_opt_struct_count = 0;
static int emitted_opt_struct_cap = 0;
static int opt_struct_already_emitted(const char* s) {
    for (int i = 0; i < emitted_opt_struct_count; i++)
        if (strcmp(emitted_opt_struct[i], s) == 0) return 1;
    return 0;
}
// Emit the monomorphic Option<Struct> family (typedef + 6 inline members) for a
// user struct named `s`. Idempotent. The base struct `s` MUST already be emitted
// (the Option embeds an `s value` by value). Returns without emitting if already done.
static void emit_option_struct_family(const char* s) {
    if (opt_struct_already_emitted(s)) return;
    WYN_ENSURE_CAP(emitted_opt_struct, emitted_opt_struct_count, emitted_opt_struct_cap);
    emitted_opt_struct[emitted_opt_struct_count++] = strdup(s);
    emit("typedef struct { int tag; %s value; } Option%s;\n", s, s);
    emit("static inline Option%s Option%s_Some(%s value){ Option%s o; o.tag=1; o.value=value; return o; }\n", s, s, s, s);
    emit("static inline Option%s Option%s_None(void){ Option%s o; o.tag=0; return o; }\n", s, s, s);
    emit("static inline int Option%s_is_some(Option%s o){ return o.tag==1; }\n", s, s);
    emit("static inline int Option%s_is_none(Option%s o){ return o.tag==0; }\n", s, s);
    emit("static inline %s Option%s_unwrap(Option%s o){ if(o.tag==0){ fprintf(stderr, \"Error: unwrap() called on None\\n\"); exit(1); } return o.value; }\n", s, s, s);
    emit("static inline %s Option%s_unwrap_or(Option%s o, %s def){ return o.tag==1 ? o.value : def; }\n", s, s, s, s);
}

// Parallel tracking + emission for the monomorphic Result<Struct, string> family.
// Mirrors emit_option_struct_family: a `Result<Name>` typedef (tag + a union of an
// `s ok_value` struct payload and a `const char* err_value`) plus the 7 inline
// members (Ok/Err/is_ok/is_err/unwrap/unwrap_err/unwrap_or). The error type is
// always `const char*` (string), matching the builtin ResultInt/ResultString.
// Idempotent; the base struct `s` MUST already be emitted.
static char** emitted_res_struct = NULL;
static int emitted_res_struct_count = 0;
static int emitted_res_struct_cap = 0;
static int res_struct_already_emitted(const char* s) {
    for (int i = 0; i < emitted_res_struct_count; i++)
        if (strcmp(emitted_res_struct[i], s) == 0) return 1;
    return 0;
}
// Emit a monomorphic Result<Ok, Err> family. `fam` is the C family name (e.g.
// "ResultPoint" or "ResultPoint_Fail"); the concrete ok/err payload C types come
// from the registry (register_result_family), so the err payload is no longer
// pinned to `const char*` — it is whatever E lowered to (a struct, `long long`,
// `double`, `bool`, or `const char*`). The unwrap() diagnostic prints the err with
// a format chosen from err_is_str (a struct/scalar err has no printable %s).
static void emit_result_struct_family(const char* fam) {
    if (res_struct_already_emitted(fam)) return;
    extern int result_family_lookup(const char*, const char**, const char**, int*);
    const char* ok = NULL; const char* err = "const char*"; int err_is_str = 1;
    if (!result_family_lookup(fam, &ok, &err, &err_is_str) || !ok) return;
    WYN_ENSURE_CAP(emitted_res_struct, emitted_res_struct_count, emitted_res_struct_cap);
    emitted_res_struct[emitted_res_struct_count++] = strdup(fam);
    emit("typedef struct { int tag; union { %s ok_value; %s err_value; } data; } %s;\n", ok, err, fam);
    emit("static inline %s %s_Ok(%s value){ %s r; r.tag=0; r.data.ok_value=value; return r; }\n", fam, fam, ok, fam);
    emit("static inline %s %s_Err(%s msg){ %s r; r.tag=1; r.data.err_value=msg; return r; }\n", fam, fam, err, fam);
    emit("static inline int %s_is_ok(%s r){ return r.tag==0; }\n", fam, fam);
    emit("static inline int %s_is_err(%s r){ return r.tag==1; }\n", fam, fam);
    if (err_is_str)
        emit("static inline %s %s_unwrap(%s r){ if(r.tag==1){ fprintf(stderr, \"Error: unwrap() called on Err: %%s\\n\", r.data.err_value); exit(1); } return r.data.ok_value; }\n", ok, fam, fam);
    else
        emit("static inline %s %s_unwrap(%s r){ if(r.tag==1){ fprintf(stderr, \"Error: unwrap() called on Err\\n\"); exit(1); } return r.data.ok_value; }\n", ok, fam, fam);
    emit("static inline %s %s_unwrap_err(%s r){ if(r.tag==0){ fprintf(stderr, \"Error: unwrap_err() called on Ok\\n\"); exit(1); } return r.data.err_value; }\n", err, fam, fam);
    emit("static inline %s %s_unwrap_or(%s r, %s def){ return r.tag==0 ? r.data.ok_value : def; }\n", ok, fam, fam, ok);
}

// --- Field-wise struct equality helpers (`a == b` on struct values) ---
// The checker gates which structs are comparable; codegen emits one
// `static int __wyn_eq_<Name>(Name, Name)` per comparable struct so
// EXPR_BINARY == can call it instead of emitting C's illegal struct ==.
static StructStmt* cg_find_struct(Program* prog, Token name) {
    for (int i = 0; i < prog->count; i++) {
        Stmt* s = prog->stmts[i];
        if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
        if (s->type == STMT_STRUCT && s->struct_decl.name.length == name.length &&
            memcmp(s->struct_decl.name.start, name.start, name.length) == 0)
            return &s->struct_decl;
    }
    return NULL;
}
// Mirrors the checker's struct_fields_comparable: every field must be a
// scalar (int/float/bool), string, enum, or another comparable struct.
static int cg_struct_comparable(Program* prog, StructStmt* sd, int depth) {
    if (!sd || depth > 8) return 0;
    for (int i = 0; i < sd->field_count; i++) {
        Expr* ft = sd->field_types[i];
        if (!ft || ft->type != EXPR_IDENT) return 0;
        Token t = ft->token;
        if ((t.length == 3 && memcmp(t.start, "int", 3) == 0) ||
            (t.length == 5 && memcmp(t.start, "float", 5) == 0) ||
            (t.length == 4 && memcmp(t.start, "bool", 4) == 0) ||
            (t.length == 6 && memcmp(t.start, "string", 6) == 0)) continue;
        StructStmt* fsd = cg_find_struct(prog, t);
        if (fsd) { if (!cg_struct_comparable(prog, fsd, depth + 1)) return 0; continue; }
        int is_enum = 0;
        for (int j = 0; j < prog->count && !is_enum; j++) {
            Stmt* es = prog->stmts[j];
            if (es->type == STMT_EXPORT && es->export.stmt) es = es->export.stmt;
            if (es->type == STMT_ENUM && es->enum_decl.name.length == t.length &&
                memcmp(es->enum_decl.name.start, t.start, t.length) == 0) is_enum = 1;
        }
        if (!is_enum) return 0;
    }
    return 1;
}
// Data enums are tagged-union structs, so C `==` on them is a type error (this
// was an ICE and, for struct fields typed as an enum, a hard C error
// "__l.t != __r.t"). Emit a per-data-enum `__wyn_eq_enum_<Enum>` that compares
// the tag and, for equal tags, the variant payload (scalars directly, strings
// via strcmp, struct payloads via their own eq helper, nested/recursive enums
// recursively). Simple (payload-less) enums stay plain C enums and keep `==`.
// Emit the payload comparison for one enum field (accessed via `lhs`/`rhs`
// expressions already spelled by the caller). `tx` is the field's declared
// TYPE EXPR - the source truth for whether the payload is a data-enum / struct /
// string / scalar (the c_type registry stores "long long" for user types, which
// would wrongly pick raw `!=`). `boxed` = the field is heap-boxed (self-ref).
static void emit_enum_field_cmp(Program* prog, Expr* tx, int boxed,
                                const char* lhs, const char* rhs) {
    extern int is_data_enum_type(const char*);
    // Classify the field type from its type expr.
    char tn[128] = {0};
    int is_string = 0, is_data_enum = 0, is_struct = 0;
    if (tx && tx->type == EXPR_IDENT) {
        int l = tx->token.length < 127 ? tx->token.length : 127;
        memcpy(tn, tx->token.start, l); tn[l] = '\0';
        if (l == 6 && memcmp(tn, "string", 6) == 0) is_string = 1;
        else if (is_data_enum_type(tn)) is_data_enum = 1;
        else if (cg_find_struct(prog, tx->token)) is_struct = 1;
    }
    if (boxed) {
        // Heap-boxed recursive self-reference: deref and recurse. Guard NULLs.
        emit("        if ((%s) && (%s)) { if (!__wyn_eq_enum_%s(*(%s), *(%s))) return 0; } "
             "else if ((%s) != (%s)) return 0;\n",
             lhs, rhs, tn, lhs, rhs, lhs, rhs);
        return;
    }
    if (is_string) {
        emit("        if (strcmp((%s) ? (%s) : \"\", (%s) ? (%s) : \"\") != 0) return 0;\n",
             lhs, lhs, rhs, rhs);
    } else if (is_data_enum) {
        emit("        if (!__wyn_eq_enum_%s(%s, %s)) return 0;\n", tn, lhs, rhs);
    } else if (is_struct) {
        emit("        if (!__wyn_eq_%s(%s, %s)) return 0;\n", tn, lhs, rhs);
    } else {
        emit("        if ((%s) != (%s)) return 0;\n", lhs, rhs);
    }
}
static void emit_enum_eq_helpers(Program* prog) {
    extern int is_enum_field_boxed(const char*, const char*, int);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < prog->count; i++) {
            Stmt* s = prog->stmts[i];
            if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
            if (s->type != STMT_ENUM) continue;
            if (s->enum_decl.type_param_count > 0) continue;  // generic enum: no monomorphic type
            EnumStmt* ed = &s->enum_decl;
            int has_data = 0;
            for (int v = 0; v < ed->variant_count; v++)
                if (ed->variant_type_counts[v] > 0) { has_data = 1; break; }
            if (!has_data) continue;   // plain enum -> C `==` is fine
            char en[128]; token_to_cstr(en, sizeof(en), ed->name);
            if (pass == 0) {
                emit("static int __wyn_eq_enum_%s(%s __l, %s __r);\n", en, en, en);
                continue;
            }
            emit("static int __wyn_eq_enum_%s(%s __l, %s __r) {\n", en, en, en);
            emit("    if (__l.tag != __r.tag) return 0;\n");
            emit("    switch (__l.tag) {\n");
            for (int v = 0; v < ed->variant_count; v++) {
                char vn[128]; token_to_cstr(vn, sizeof(vn), ed->variants[v]);
                emit("      case %s_%s_TAG: {\n", en, vn);
                int nf = ed->variant_type_counts[v];
                if (nf == 1) {
                    int boxed = is_enum_field_boxed(en, vn, 0);
                    char lhs[256], rhs[256];
                    snprintf(lhs, sizeof(lhs), "__l.data.%s_value", vn);
                    snprintf(rhs, sizeof(rhs), "__r.data.%s_value", vn);
                    emit_enum_field_cmp(prog, ed->variant_types[v][0], boxed, lhs, rhs);
                } else if (nf > 1) {
                    for (int f = 0; f < nf; f++) {
                        int boxed = is_enum_field_boxed(en, vn, f);
                        char lhs[256], rhs[256];
                        snprintf(lhs, sizeof(lhs), "__l.data.%s_value.f%d", vn, f);
                        snprintf(rhs, sizeof(rhs), "__r.data.%s_value.f%d", vn, f);
                        emit_enum_field_cmp(prog, ed->variant_types[v][f], boxed, lhs, rhs);
                    }
                }
                emit("        break;\n      }\n");
            }
            emit("    }\n");
            emit("    return 1;\n}\n");
        }
    }
}
// Emitted after ALL struct typedefs; prototypes first so helpers for structs
// with struct-typed fields can call each other regardless of source order.
static void emit_struct_eq_helpers(Program* prog) {
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < prog->count; i++) {
            Stmt* s = prog->stmts[i];
            if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
            if (s->type != STMT_STRUCT) continue;
            StructStmt* sd = &s->struct_decl;
            if (sd->type_param_count > 0) continue;  // generics: no monomorphic type
            if (!cg_struct_comparable(prog, sd, 0)) continue;
            Token n = sd->name;
            if (pass == 0) {
                emit("static int __wyn_eq_%.*s(%.*s __l, %.*s __r);\n",
                     n.length, n.start, n.length, n.start, n.length, n.start);
                continue;
            }
            emit("static int __wyn_eq_%.*s(%.*s __l, %.*s __r) {\n",
                 n.length, n.start, n.length, n.start, n.length, n.start);
            for (int fi = 0; fi < sd->field_count; fi++) {
                Token fn = sd->fields[fi];
                Token t = sd->field_types[fi]->token;
                if (t.length == 6 && memcmp(t.start, "string", 6) == 0) {
                    emit("    if (strcmp(__l.%.*s ? __l.%.*s : \"\", __r.%.*s ? __r.%.*s : \"\") != 0) return 0;\n",
                         fn.length, fn.start, fn.length, fn.start, fn.length, fn.start, fn.length, fn.start);
                } else if (cg_find_struct(prog, t)) {
                    emit("    if (!__wyn_eq_%.*s(__l.%.*s, __r.%.*s)) return 0;\n",
                         t.length, t.start, fn.length, fn.start, fn.length, fn.start);
                } else if (({ char _ft[128]; token_to_cstr(_ft, sizeof(_ft), t);
                             extern int is_data_enum_type(const char*); is_data_enum_type(_ft); })) {
                    // Data-enum field is a tagged-union struct: `!=` is a C type
                    // error. Compare via the enum eq helper (tag + payload).
                    emit("    if (!__wyn_eq_enum_%.*s(__l.%.*s, __r.%.*s)) return 0;\n",
                         t.length, t.start, fn.length, fn.start, fn.length, fn.start);
                } else {
                    emit("    if (__l.%.*s != __r.%.*s) return 0;\n",
                         fn.length, fn.start, fn.length, fn.start);
                }
            }
            emit("    return 1;\n}\n");
        }
    }
}

// --- Per-struct stringifier (`"${p}"` on a struct value) ---
// `to_string` is a _Generic macro whose `default:` arm is int_to_string, so a
// struct argument was passed BY VALUE to a `long long` parameter: interpolation
// type-checked clean and then died in the generated C with "passing 'P' to
// parameter of incompatible type 'long long'" (PLAN_v1.21 S1). println(struct)
// already renders `P { x: 1, y: 2 }`, but it does so by emitting inline printf
// calls that print directly and yield no string, so interpolation - which needs
// a char* - cannot reuse it.
//
// Emit one `static char* __wyn_str_<Name>(<Name>)` per non-generic struct,
// alongside the existing __wyn_eq_<Name> / <Name>_cleanup helpers, returning a
// FRESH +1 RC string. The output deliberately matches println's format so the
// two spellings agree.
//
// Prototypes come first (pass 0) so a struct with a struct-typed field can call
// its field's helper regardless of source order - the same reason
// emit_struct_eq_helpers is two-pass.
//
// Field rendering, and the ownership rule for each:
//   string  - printed directly with %s; NOT via to_string, because
//             str_to_string returns its ARGUMENT unchanged, so releasing the
//             result would free the struct's own field. Quoted, as println does.
//   bool    - "true"/"false" inline; no allocation.
//   float   - float_to_string (fresh, released) so it goes through
//             wyn_format_float and keeps the v1.20.0 round-trip precision.
//   array   - array_to_string (fresh, released).
//   struct  - that struct's own __wyn_str_ helper (fresh, released).
//   other   - a self-describing `<Type>` placeholder. Data enums, maps, sets,
//             Json, optionals and fn fields have no string form yet; a
//             placeholder that names the type states plainly that no value is
//             being claimed, which is what keeps this from becoming the
//             silent-wrong class. Rendering them properly is a ROADMAP item.
// How one struct field is rendered by its __wyn_str_ helper. FK_OPAQUE is the
// "no string form yet" bucket (data enums, maps, sets, Json, fn fields) and
// prints a `<Type>` placeholder rather than inventing a value. FK_OPTION left
// that bucket once the Option families got renderers: `S { a: 1, b: <?> }` for
// a `b: int?` field now reads `S { a: 1, b: Some(2) }`.
enum { FK_OPAQUE = 0, FK_STR, FK_BOOL, FK_INT, FK_FLOAT, FK_STRUCT, FK_ENUM, FK_ARRAY, FK_OPTION };

// A PAYLOAD-LESS enum stays a plain C enum, so it renders with %lld exactly as
// println does. A DATA enum is a tagged-union struct and has no string form yet,
// so it takes the placeholder path.
static int cg_is_simple_enum(Program* prog, Token t) {
    for (int j = 0; j < prog->count; j++) {
        Stmt* es = prog->stmts[j];
        if (es->type == STMT_EXPORT && es->export.stmt) es = es->export.stmt;
        if (es->type == STMT_ENUM && es->enum_decl.name.length == t.length &&
            memcmp(es->enum_decl.name.start, t.start, t.length) == 0) {
            char nm[128]; token_to_cstr(nm, sizeof(nm), t);
            extern int is_data_enum_type(const char*);
            return !is_data_enum_type(nm);
        }
    }
    return 0;
}
// Does a __wyn_str_<name> helper exist for this struct? The interpolation site
// in codegen_expr.c asks before emitting a call, so the two stay in step: a
// generic struct has no monomorphic C type and therefore no helper.
int cg_struct_has_str_helper(Token name) {
    if (!current_program) return 0;
    StructStmt* sd = cg_find_struct(current_program, name);
    if (!sd || sd->type_param_count > 0) return 0;   // generic: no monomorphic type
    char nm[128]; token_to_cstr(nm, sizeof(nm), name);
    extern int is_interpolated_struct(const char*);
    return is_interpolated_struct(nm);
}
// A helper for an interpolated struct calls the helpers of its struct-typed
// FIELDS, which are usually never interpolated themselves. Register that closure
// before emitting, or `Outer { i: Inner }` would emit __wyn_str_Outer calling a
// __wyn_str_Inner that does not exist.
static void cg_close_interp_structs(Program* prog) {
    extern void register_interpolated_struct(const char*);
    for (int changed = 1, guard = 0; changed && guard < 64; guard++) {
        changed = 0;
        for (int i = 0; i < prog->count; i++) {
            Stmt* s = prog->stmts[i];
            if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
            if (s->type != STMT_STRUCT) continue;
            StructStmt* sd = &s->struct_decl;
            if (sd->type_param_count > 0) continue;
            if (!cg_struct_has_str_helper(sd->name)) continue;
            for (int fi = 0; fi < sd->field_count; fi++) {
                Expr* ft = sd->field_types[fi];
                if (!ft || ft->type != EXPR_IDENT) continue;
                StructStmt* fsd = cg_find_struct(prog, ft->token);
                if (!fsd || fsd->type_param_count > 0) continue;
                if (cg_struct_has_str_helper(ft->token)) continue;
                char fnm[128]; token_to_cstr(fnm, sizeof(fnm), ft->token);
                register_interpolated_struct(fnm);
                changed = 1;
            }
        }
    }
}
static void emit_struct_str_helpers(Program* prog) {
    cg_close_interp_structs(prog);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < prog->count; i++) {
            Stmt* s = prog->stmts[i];
            if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
            if (s->type != STMT_STRUCT) continue;
            StructStmt* sd = &s->struct_decl;
            if (sd->type_param_count > 0) continue;  // generics: no monomorphic type
            // Only structs actually interpolated (plus their struct-typed fields)
            // get a helper - see cg_struct_has_str_helper.
            if (!cg_struct_has_str_helper(sd->name)) continue;
            Token n = sd->name;
            if (pass == 0) {
                emit("static char* __wyn_str_%.*s(%.*s __v);\n",
                     n.length, n.start, n.length, n.start);
                continue;
            }
            emit("static char* __wyn_str_%.*s(%.*s __v) {\n",
                 n.length, n.start, n.length, n.start);
            // Classify each field ONCE. Four loops below (temps, format, args,
            // releases) must agree about every field, and an earlier revision
            // repeated the type tests in each one - a field added to three of
            // the four would emit a format spec with no argument, which is a
            // wild read rather than a compile error.
            int nf = sd->field_count > 0 ? sd->field_count : 1;
            int* kind = malloc(sizeof(int) * nf);
            // FK_OPTION needs the field's concrete family name, so it is resolved
            // once here alongside the kind rather than re-derived in each loop.
            char (*ofam)[96] = malloc(sizeof(char[96]) * nf);
            char _sdn[96]; token_to_cstr(_sdn, sizeof(_sdn), sd->name);
            for (int fi = 0; fi < sd->field_count; fi++) {
                Expr* ft = sd->field_types[fi];
                kind[fi] = FK_OPAQUE;
                ofam[fi][0] = '\0';
                if (!ft) continue;
                if (ft->type == EXPR_ARRAY) { kind[fi] = FK_ARRAY; continue; }
                // An `f: T?` field (or the `Option<T>` spelling) - ask the same
                // authority the field's own lowering used, so the renderer call
                // and the field's C type cannot disagree.
                {
                    char _fdn[96]; token_to_cstr(_fdn, sizeof(_fdn), sd->fields[fi]);
                    extern int get_struct_field_option_family(const char*, const char*, char*, size_t);
                    if (get_struct_field_option_family(_sdn, _fdn, ofam[fi], sizeof(ofam[fi]))) {
                        kind[fi] = FK_OPTION;
                        continue;
                    }
                }
                if (ft->type != EXPR_IDENT) continue;
                Token t = ft->token;
                if      (t.length == 6 && memcmp(t.start, "string", 6) == 0) kind[fi] = FK_STR;
                else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0)   kind[fi] = FK_BOOL;
                else if (t.length == 3 && memcmp(t.start, "int", 3) == 0)    kind[fi] = FK_INT;
                else if (t.length == 5 && memcmp(t.start, "float", 5) == 0)  kind[fi] = FK_FLOAT;
                else if (cg_find_struct(prog, t))                            kind[fi] = FK_STRUCT;
                else if (cg_is_simple_enum(prog, t))                         kind[fi] = FK_ENUM;
            }
            // Allocating fields are stringified first, so the format below is a
            // flat list of %s / %lld and every temp has a name to release.
            for (int fi = 0; fi < sd->field_count; fi++) {
                Token fn = sd->fields[fi];
                if (kind[fi] == FK_FLOAT)
                    emit("    char* __f%d = float_to_string(__v.%.*s);\n", fi, fn.length, fn.start);
                else if (kind[fi] == FK_ARRAY)
                    emit("    char* __f%d = array_to_string(__v.%.*s);\n", fi, fn.length, fn.start);
                else if (kind[fi] == FK_STRUCT) {
                    Token t = sd->field_types[fi]->token;
                    emit("    char* __f%d = __wyn_str_%.*s(__v.%.*s);\n",
                         fi, t.length, t.start, fn.length, fn.start);
                }
                else if (kind[fi] == FK_OPTION)
                    emit("    char* __f%d = %s_to_string(__v.%.*s);\n",
                         fi, ofam[fi], fn.length, fn.start);
            }
            // Two passes over the format: size probe, then the real write.
            for (int w = 0; w < 2; w++) {
                // A field-less struct renders `Name {}`; with the usual "{ " / " }"
                // pair it would come out as `Name {  }` with a doubled space.
                const char* open = sd->field_count > 0 ? "{ " : "{";
                if (w == 0) emit("    int __n = snprintf(NULL, 0, \"%.*s %s", n.length, n.start, open);
                else        emit("    char* __b = wyn_str_alloc(__n + 1);\n"
                                 "    snprintf(__b, __n + 1, \"%.*s %s", n.length, n.start, open);
                for (int fi = 0; fi < sd->field_count; fi++) {
                    Token fn = sd->fields[fi];
                    if (fi > 0) emit(", ");
                    emit("%.*s: ", fn.length, fn.start);
                    switch (kind[fi]) {
                        case FK_STR:    emit("\\\"%%s\\\""); break;
                        case FK_BOOL:   emit("%%s");   break;
                        case FK_INT: case FK_ENUM: emit("%%lld"); break;
                        case FK_FLOAT: case FK_STRUCT: case FK_ARRAY:
                        case FK_OPTION: emit("%%s"); break;
                        default: {
                            Expr* ft = sd->field_types[fi];
                            if (ft && ft->type == EXPR_IDENT)
                                emit("<%.*s>", ft->token.length, ft->token.start);
                            else emit("<?>");
                            break;
                        }
                    }
                }
                emit(" }\"");
                for (int fi = 0; fi < sd->field_count; fi++) {
                    Token fn = sd->fields[fi];
                    switch (kind[fi]) {
                        case FK_STR:
                            emit(", __v.%.*s ? __v.%.*s : \"\"",
                                 fn.length, fn.start, fn.length, fn.start); break;
                        case FK_BOOL:
                            emit(", __v.%.*s ? \"true\" : \"false\"", fn.length, fn.start); break;
                        case FK_INT: case FK_ENUM:
                            emit(", (long long)__v.%.*s", fn.length, fn.start); break;
                        case FK_FLOAT: case FK_STRUCT: case FK_ARRAY:
                        case FK_OPTION:
                            emit(", __f%d", fi); break;
                        default: break;   // placeholder: literal text, no argument
                    }
                }
                emit(");\n");
            }
            for (int fi = 0; fi < sd->field_count; fi++)
                if (kind[fi] == FK_FLOAT || kind[fi] == FK_STRUCT ||
                    kind[fi] == FK_ARRAY || kind[fi] == FK_OPTION)
                    emit("    wyn_rc_release(__f%d);\n", fi);
            free(kind);
            free(ofam);
            emit("    wyn_rc_set_length(__b, (unsigned int)__n);\n"
                 "    return __b;\n}\n");
        }
    }
}

// --- Monomorphic Option<Struct> stringifiers -------------------------------
//
// The eight builtin payload families render in wyn_runtime.h, but a family with
// a struct, data-enum or Option payload is emitted PER PROGRAM (it names a user
// type), so its renderer has to be emitted per program too. Without one,
// `print(Some(P { x: 1 }))` and `print(Some(Some(1)))` passed OptionP /
// OptionOptionInt by value to a `long long` parameter - the same defect the
// builtin families had, surviving in the one place a shared runtime cannot
// reach.
//
// Two passes, for the same reason the struct helpers use two: a struct with an
// `S?` FIELD calls OptionS_to_string from inside __wyn_str_<Struct>, and that
// helper is emitted before these definitions.

// Write the C expression that renders this family's PAYLOAD as a fresh char*,
// or return 0 if the payload has no string form. `payload` is the payload's own
// type name, which is also the key the family was registered under.
static int cg_optlike_payload_expr(Program* prog, const char* payload,
                                   char* out, size_t outsz) {
    static const char* builtin[] = {
        "OptionInt", "OptionString", "OptionFloat", "OptionBool",
        "ResultInt", "ResultString", "ResultFloat", "ResultBool",
    };
    // A nested Option: Some(Some(x)) registers the INNER family name as the
    // outer family's payload, so the recursion is just "does the payload have a
    // renderer" - true for a builtin, and true for another monomorphic family
    // because this same loop emits one for it.
    for (size_t i = 0; i < sizeof(builtin) / sizeof(builtin[0]); i++) {
        if (strcmp(payload, builtin[i]) == 0) {
            snprintf(out, outsz, "%s_to_string(__v.value)", payload);
            return 1;
        }
    }
    extern int is_registered_option_struct(const char*);
    if (strncmp(payload, "Option", 6) == 0 && is_registered_option_struct(payload + 6)) {
        snprintf(out, outsz, "%s_to_string(__v.value)", payload);
        return 1;
    }
    // A user struct renders through the helper interpolation already uses.
    Token pt = {TOKEN_IDENT, payload, (int)strlen(payload), 0};
    if (cg_find_struct(prog, pt) && cg_struct_has_str_helper(pt)) {
        snprintf(out, outsz, "__wyn_str_%s(__v.value)", payload);
        return 1;
    }
    return 0;
}

// Register every user-struct Option payload as interpolated, so
// emit_struct_str_helpers gives it a __wyn_str_ helper for the renderer to call.
// Must run BEFORE that function decides which structs get helpers.
static void cg_register_optlike_payloads(Program* prog) {
    extern int option_struct_count(void);
    extern const char* option_struct_name(int);
    extern void register_interpolated_struct(const char*);
    for (int i = 0; i < option_struct_count(); i++) {
        const char* p = option_struct_name(i);
        Token pt = {TOKEN_IDENT, p, (int)strlen(p), 0};
        StructStmt* sd = cg_find_struct(prog, pt);
        if (sd && sd->type_param_count == 0) register_interpolated_struct(p);
    }
}

// pass 0 = forward declarations, pass 1 = definitions.
static void emit_optlike_str_helpers(Program* prog, int pass) {
    extern int option_struct_count(void);
    extern const char* option_struct_name(int);
    for (int i = 0; i < option_struct_count(); i++) {
        const char* p = option_struct_name(i);
        char fam[160];
        snprintf(fam, sizeof(fam), "Option%s", p);
        if (pass == 0) {
            emit("static char* %s_to_string(%s __v);\n", fam, fam);
            continue;
        }
        char pexpr[192];
        int have = cg_optlike_payload_expr(prog, p, pexpr, sizeof(pexpr));
        emit("static char* %s_to_string(%s __v) {\n", fam, fam);
        // Sized by a probe, then written - the same no-truncation contract the
        // __wyn_str_ helpers and the runtime renderers use.
        emit("    if (__v.tag != 1) {\n"
             "        int __n = snprintf(NULL, 0, \"none\");\n"
             "        char* __b = wyn_str_alloc(__n + 1);\n"
             "        snprintf(__b, __n + 1, \"none\");\n"
             "        wyn_rc_set_length(__b, (unsigned int)__n);\n"
             "        return __b;\n"
             "    }\n");
        if (have) {
            emit("    char* __p = %s;\n", pexpr);
            emit("    int __n = snprintf(NULL, 0, \"Some(%%s)\", __p);\n"
                 "    char* __b = wyn_str_alloc(__n + 1);\n"
                 "    snprintf(__b, __n + 1, \"Some(%%s)\", __p);\n"
                 "    wyn_rc_set_length(__b, (unsigned int)__n);\n"
                 "    wyn_rc_release(__p);\n"
                 "    return __b;\n");
        } else {
            // No string form for this payload (a data enum, a generic
            // instantiation). Name the type rather than invent a value - the same
            // rule the struct helpers' <Type> placeholder follows.
            emit("    int __n = snprintf(NULL, 0, \"Some(<%s>)\");\n", p);
            emit("    char* __b = wyn_str_alloc(__n + 1);\n");
            emit("    snprintf(__b, __n + 1, \"Some(<%s>)\");\n", p);
            emit("    wyn_rc_set_length(__b, (unsigned int)__n);\n"
                 "    return __b;\n");
        }
        emit("}\n");
    }
}

// --- Typed array-element renderers ----------------------------------------
//
// A struct pushed into an array is heap-boxed as WYN_TYPE_STRUCT with no type
// name, so the runtime's three element formatters can only print `<struct>` for
// it. The element type IS known at the print site, so one renderer per printed
// element type is emitted here and the print sites call it instead of the
// generic array formatter.
//
// Emitted last: it calls __wyn_str_<T> for a user struct and <Fam>_to_string for
// an Option family, so both must already exist.
static void emit_array_elem_str_helpers(Program* prog) {
    extern int printed_array_elem_count(void);
    extern const char* printed_array_elem_name(int);
    for (int i = 0; i < printed_array_elem_count(); i++) {
        const char* el = printed_array_elem_name(i);
        // How one element renders. An Option/Result family has a _to_string; a
        // user struct has the interpolation helper. Anything else gets no
        // renderer at all and keeps the runtime's <struct> placeholder.
        char call[192];
        Token et = {TOKEN_IDENT, el, (int)strlen(el), 0};
        // codegen_expr.c is #included ahead of this file, so its guard is in
        // scope - and using the SAME guard the print sites use is what keeps the
        // emitted set and the called set from diverging.
        if (cg_optlike_has_renderer(el))
            snprintf(call, sizeof(call), "%s_to_string(*(%s*)__v.data.struct_val)", el, el);
        else if (cg_find_struct(prog, et) && cg_struct_has_str_helper(et))
            snprintf(call, sizeof(call), "__wyn_str_%s(*(%s*)__v.data.struct_val)", el, el);
        else
            continue;
        // Grown with realloc rather than sized by a probe: probing would mean
        // rendering every element twice, and each render allocates.
        emit("static char* __wyn_arrstr_%s(WynArray __a) {\n", el);
        emit("    size_t __cap = 64, __len = 0;\n"
             "    char* __t = (char*)malloc(__cap);\n"
             "    if (!__t) return wyn_str_alloc(1);\n"
             "    __t[__len++] = '[';\n"
             "    for (int __i = 0; __i < __a.count; __i++) {\n"
             "        WynValue __v = __a.data[__i];\n"
             "        char* __e = NULL;\n");
        // The array is a tagged container, so an element that is not a boxed
        // struct must not be cast - it keeps the runtime's placeholder.
        emit("        if (__v.type == WYN_TYPE_STRUCT && __v.data.struct_val) __e = %s;\n", call);
        emit("        const char* __s = __e ? __e : \"<struct>\";\n"
             "        size_t __el = strlen(__s);\n"
             "        size_t __need = __len + __el + 4;\n"
             "        if (__need > __cap) { while (__need > __cap) __cap *= 2;\n"
             "            char* __nt = (char*)realloc(__t, __cap);\n"
             "            if (!__nt) { free(__t); if (__e) wyn_rc_release(__e); return wyn_str_alloc(1); }\n"
             "            __t = __nt; }\n"
             "        if (__i > 0) { __t[__len++] = ','; __t[__len++] = ' '; }\n"
             "        memcpy(__t + __len, __s, __el); __len += __el;\n"
             "        if (__e) wyn_rc_release(__e);\n"
             "    }\n"
             "    __t[__len++] = ']';\n"
             "    char* __b = wyn_str_alloc(__len + 1);\n"
             "    memcpy(__b, __t, __len); __b[__len] = 0;\n"
             "    wyn_rc_set_length(__b, (unsigned int)__len);\n"
             "    free(__t);\n"
             "    return __b;\n}\n");
    }
}

void codegen_program(Program* prog) {
    current_program = prog;
    bool has_main = false;
    bool has_math_import = false;
    (void)has_math_import;
    
    // Reset module emission flag for this compilation
    modules_emitted_this_compilation = false;
    
    // Reset lambda collection
    lambda_count = 0;
    lambda_id_counter = 0;
    lambda_ref_counter = 0;

    // Reset Option<Struct> emission tracking for this compilation
    for (int _i = 0; _i < emitted_opt_struct_count; _i++) free(emitted_opt_struct[_i]);
    emitted_opt_struct_count = 0;
    // Reset Result<Struct> emission tracking for this compilation
    for (int _i = 0; _i < emitted_res_struct_count; _i++) free(emitted_res_struct[_i]);
    emitted_res_struct_count = 0;
    
    // Reset spawn wrapper collection
    spawn_wrapper_count = 0;
    
    // PASS 0: Pre-scan to register function return types (needed by spawn wrapper generation)
    extern void register_fn_return_type(const char*, const char*);
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            Stmt* fn_stmt = prog->stmts[i];
            {
                // No return type (even after checker inference) - the fn is
                // emitted as C `void`. Record it so `return f(...)` inside a
                // non-void C function (like wyn_main) lowers to `f(...); return 0;`
                // instead of the invalid `return <void expr>;` - the iOS-shim
                // pattern `fn main() { return wyn_ios_main(0, 0) }` hit this.
                // main (always long long) and generators (WynIter*) are excluded:
                // their C signatures are non-void despite the NULL return type.
                extern int fn_is_generator(Stmt*);
                bool _is_main = (fn_stmt->fn.name.length == 4 &&
                                 memcmp(fn_stmt->fn.name.start, "main", 4) == 0);
                if (!fn_stmt->fn.return_type && !fn_stmt->fn.is_extension &&
                    !_is_main && !fn_is_generator(fn_stmt)) {
                    char _vfn[128]; token_to_cstr(_vfn, sizeof(_vfn), fn_stmt->fn.name);
                    extern void register_void_fn(const char*);
                    register_void_fn(_vfn);
                }
            }
            if (fn_stmt->fn.return_type && fn_stmt->fn.return_type->type == EXPR_IDENT) {
                char _fn[128]; token_to_cstr(_fn, sizeof(_fn), fn_stmt->fn.name);
                char _rt[32]; token_to_cstr(_rt, sizeof(_rt), fn_stmt->fn.return_type->token);
                register_fn_return_type(_fn, _rt);
            } else if (fn_stmt->fn.return_type && fn_stmt->fn.return_type->type == EXPR_OPTIONAL_TYPE) {
                // `-> int?` / `-> string?` → OptionInt / OptionString family name,
                // so callers (var r = find(...)) get the right type and match detects it.
                char _fn[128]; token_to_cstr(_fn, sizeof(_fn), fn_stmt->fn.name);
                Expr* inr = fn_stmt->fn.return_type->optional_type.inner_type;
                const char* fam = "OptionInt";
                static char _osfam[128];
                if (inr && inr->type == EXPR_IDENT) {
                    // Recorded fn return family — same authority as the signature.
                    char _stn[96]; token_to_cstr(_stn, sizeof(_stn), inr->token);
                    extern const char* wyn_option_family(const char*, const char**, int*);
                    snprintf(_osfam, sizeof(_osfam), "%s", wyn_option_family(_stn, NULL, NULL));
                    fam = _osfam;
                }
                register_fn_return_type(_fn, fam);
            } else if (fn_stmt->fn.return_type && fn_stmt->fn.return_type->type == EXPR_CALL &&
                       fn_stmt->fn.return_type->call.callee->type == EXPR_IDENT) {
                Token gt = fn_stmt->fn.return_type->call.callee->token;
                const char* base = NULL;
                if (gt.length == 6 && memcmp(gt.start, "Option", 6) == 0) base = "Option";
                else if (gt.length == 6 && memcmp(gt.start, "Result", 6) == 0) base = "Result";
                if (base) {
                    char _fn[128]; token_to_cstr(_fn, sizeof(_fn), fn_stmt->fn.name);
                    const char* suf = "Int";
                    if (fn_stmt->fn.return_type->call.arg_count > 0 &&
                        fn_stmt->fn.return_type->call.args[0]->type == EXPR_IDENT) {
                        Token a0 = fn_stmt->fn.return_type->call.args[0]->token;
                        if (a0.length == 6 && memcmp(a0.start, "string", 6) == 0) suf = "String";
                        else if (a0.length == 5 && memcmp(a0.start, "float", 5) == 0) suf = "Float";
                        else if (a0.length == 4 && memcmp(a0.start, "bool", 4) == 0) suf = "Bool";
                    }
                    char _rt[32]; snprintf(_rt, sizeof(_rt), "%s%s", base, suf);
                    register_fn_return_type(_fn, _rt);
                }
            }
        }
    }
    
    // PASS 0: Decide which `[int]`-annotated vars may use the packed
    // WynIntArray representation. Must run before ANY emission, because the
    // STMT_VAR type decision consults the result. See the "int-array veto"
    // block in codegen.c.
    veto_scan_program(prog);

    // PASS 1: Pre-scan to collect all lambdas
    // We need to emit lambda functions before they're used
    // So we do a quick scan to find and generate them first

    // Module function bodies first, because that is the order they are EMITTED
    // in (the STMT_IMPORT case emits every loaded module's functions before the
    // main file's are generated). Scanning them here is what makes a lambda
    // inside an imported module work at all: a whole-module `import m` does not
    // merge module fns into prog->stmts, so this loop is the only chance to see
    // them, and without it `nums.filter((n) => n > 1)` in a module emitted a
    // reference to a body that was never generated. That withdrew the entire
    // higher-order toolkit (.map/.filter/...) from all multi-file Wyn code.
    //
    // Walk the registry in index order and unwrap STMT_EXPORT exactly as the
    // emitter does, so scan order and emission order cannot drift apart.
    {
        extern int get_module_count(void);
        extern void* get_module_entry_at(int index);
        int _mc = get_module_count();
        for (int m = 0; m < _mc; m++) {
            ModuleEntry* mod = (ModuleEntry*)get_module_entry_at(m);
            if (!mod || !mod->ast) continue;
            for (int i = 0; i < mod->ast->count; i++) {
                Stmt* s = mod->ast->stmts[i];
                if (s && s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
                if (!s) continue;
                if (s->type == STMT_FN) {
                    scan_for_lambdas(s->fn.body);
                } else {
                    scan_stmt_for_lambdas(s);
                }
            }
        }
    }

    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_FN) {
            scan_for_lambdas(prog->stmts[i]->fn.body);
        } else {
            // Also scan top-level statements (script mode, var decls, etc.)
            scan_stmt_for_lambdas(prog->stmts[i]);
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
    
    // Pre-register module aliases from imports (needed for struct/enum typedef generation)
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_IMPORT && prog->stmts[i]->import.item_count > 0) {
            char mod_name[256]; token_to_cstr(mod_name, sizeof(mod_name), prog->stmts[i]->import.module);
            const char* c_mod = module_to_c_ident(mod_name);
            for (int j = 0; j < prog->stmts[i]->import.item_count; j++) {
                char item[256], full[512];
                token_to_cstr(item, sizeof(item), prog->stmts[i]->import.items[j]);
                snprintf(full, sizeof(full), "%s_%s", c_mod, item);
                register_module_alias(item, full);
            }
        }
    }

    // Generate all structs, enums, and type aliases first.
    //
    // IN DEPENDENCY ORDER, not source order. A struct field of struct type embeds that
    // struct BY VALUE, so C needs the field's typedef to already exist:
    //
    //     struct A { b: B }      // emitted first, in source order
    //     struct B { v: int }    // ...so this typedef came too late
    //
    // gave `error: unknown type name 'B'` AFTER passing `wyn check` - the v1.21 soundness
    // rule (what checks must build), and the failure named a C type the user never wrote.
    // Declaring B first worked, so the language appeared to have a declare-before-use rule
    // that nothing documents and no diagnostic mentions.
    //
    // A topological order always exists: the checker already rejects a struct containing
    // its own type and any mutually-recursive cycle (both are infinite-size by value), so
    // the field graph is a DAG by the time we get here. `emit_order` is that order;
    // anything whose dependencies cannot be resolved (an unknown name, a non-struct field)
    // keeps its source position, so this can only ever REORDER what already worked.
    int* emit_order = malloc(sizeof(int) * (prog->count > 0 ? prog->count : 1));
    int emit_order_count = 0;
    {
        char* placed = calloc(prog->count > 0 ? prog->count : 1, 1);
        // Repeated passes: place a declaration once every struct-typed field it names has
        // been placed. O(n^2) on the number of type declarations, which is tiny, and it
        // terminates because the graph is acyclic - the final sweep below is the backstop
        // if that assumption is ever violated.
        for (int pass = 0; pass < prog->count + 1 && emit_order_count < prog->count; pass++) {
            int placed_this_pass = 0;
            for (int i = 0; i < prog->count; i++) {
                if (placed[i]) continue;
                Stmt* s = prog->stmts[i];
                if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
                bool ready = true;
                if (s->type == STMT_STRUCT) {
                    for (int f = 0; f < s->struct_decl.field_count && ready; f++) {
                        Expr* ft = s->struct_decl.field_types[f];
                        if (ft && ft->type == EXPR_OPTIONAL_TYPE) ft = ft->optional_type.inner_type;
                        if (!ft || ft->type != EXPR_IDENT) continue;
                        // Find a LATER struct declaration of this field's type. Only a
                        // not-yet-placed one can block us.
                        for (int j = 0; j < prog->count; j++) {
                            if (j == i || placed[j]) continue;
                            Stmt* o = prog->stmts[j];
                            if (o->type == STMT_EXPORT && o->export.stmt) o = o->export.stmt;
                            if (o->type != STMT_STRUCT) continue;
                            if (o->struct_decl.name.length == ft->token.length &&
                                memcmp(o->struct_decl.name.start, ft->token.start,
                                       ft->token.length) == 0) { ready = false; break; }
                        }
                    }
                }
                if (ready) { emit_order[emit_order_count++] = i; placed[i] = 1; placed_this_pass++; }
            }
            if (placed_this_pass == 0) break;   // no progress: fall through to the sweep
        }
        // Backstop: anything still unplaced keeps its source order, so a graph this pass
        // cannot order emits exactly as it did before rather than being dropped.
        for (int i = 0; i < prog->count; i++) if (!placed[i]) emit_order[emit_order_count++] = i;
        free(placed);
    }
    for (int oi = 0; oi < emit_order_count; oi++) {
        int i = emit_order[oi];
        Stmt* s = prog->stmts[i];
        // Unwrap export
        if (s->type == STMT_EXPORT && s->export.stmt) {
            int inner = s->export.stmt->type;
            if (inner == STMT_STRUCT || inner == STMT_ENUM || inner == STMT_TYPE_ALIAS) {
                s = s->export.stmt;
            }
        }
        if (s->type == STMT_STRUCT || s->type == STMT_ENUM || s->type == STMT_TYPE_ALIAS || s->type == STMT_TRAIT) {
            if (s->type == STMT_TRAIT) {
                register_trait_name(s->trait_decl.name.start, s->trait_decl.name.length);
            }
            codegen_stmt(s);
            // If this struct is used elsewhere as `S?`, emit its Option<S> family
            // right here - after S's typedef (Option embeds S by value) and before
            // any later struct that has an `S?` field can reference OptionS.
            if (s->type == STMT_STRUCT) {
                char _sn[96]; token_to_cstr(_sn, sizeof(_sn), s->struct_decl.name);
                extern int is_registered_option_struct(const char*);
                if (is_registered_option_struct(_sn)) emit_option_struct_family(_sn);
                extern int is_registered_result_struct(const char*);
                if (is_registered_result_struct(_sn)) emit_result_struct_family(_sn);
            }
            // A DATA-carrying enum needs the same hook: its Option<Enum> family names the
            // enum's own C struct, and a LATER struct may hold that family as a field
            // (`struct Holder { s: Shape? }` -> `OptionShape s;`). Without emitting here,
            // the family only appeared in the catch-all further below -- i.e. AFTER Holder --
            // and the C compile failed with "unknown type name 'OptionShape'".
            // emit_option_struct_family dedups, so the catch-all remains harmless.
            if (s->type == STMT_ENUM) {
                char _en[96]; token_to_cstr(_en, sizeof(_en), s->enum_decl.name);
                extern int is_registered_option_struct(const char*);
                if (is_registered_option_struct(_en)) emit_option_struct_family(_en);
                extern int is_registered_result_struct(const char*);
                if (is_registered_result_struct(_en)) emit_result_struct_family(_en);
            }
            // For imported enums, emit module-prefixed typedef and constructor aliases
            if (s->type == STMT_ENUM && prog->stmts[i]->type == STMT_EXPORT) {
                char ename[128]; token_to_cstr(ename, sizeof(ename), s->enum_decl.name);
                const char* resolved = resolve_module_alias(ename);
                if (resolved != ename && strcmp(resolved, ename) != 0) {
                    // resolved is "module_EnumName", ename is "EnumName"
                    emit("typedef %s %s;\n", ename, resolved);
                    for (int vi = 0; vi < s->enum_decl.variant_count; vi++) {
                        emit("#define %.*s_%.*s %s_%.*s\n",
                             (int)strlen(resolved), resolved,
                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start,
                             ename,
                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start);
                        emit("#define %.*s_%.*s_TAG %s_%.*s_TAG\n",
                             (int)strlen(resolved), resolved,
                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start,
                             ename,
                             s->enum_decl.variants[vi].length, s->enum_decl.variants[vi].start);
                    }
                }
            }
        }
    }
    free(emit_order);

    // Emit monomorphic Option<Struct> families for any user struct used as `S?`
    // (registered by the checker as it resolved the optionals). These can't live
    // in wyn_runtime.h because they name user structs defined just above.
    {
        extern int option_struct_count(void);
        extern const char* option_struct_name(int);
        // Catch-all for any registered family not already emitted next to its
        // base struct above (e.g. the base struct was defined after the use, so
        // the per-struct hook didn't fire - emit_option_struct_family dedups).
        for (int i = 0; i < option_struct_count(); i++) {
            emit_option_struct_family(option_struct_name(i));
        }
    }

    // Same catch-all for monomorphic Result<Struct, string> families.
    {
        extern int result_struct_count(void);
        extern const char* result_struct_name(int);
        for (int i = 0; i < result_struct_count(); i++) {
            emit_result_struct_family(result_struct_name(i));
        }
    }

    // Tag+payload == helpers for data enums (before struct helpers so a struct
    // with a data-enum field can call __wyn_eq_enum_<E>), then field-wise ==
    // helpers for user structs (after all typedefs exist).
    emit_enum_eq_helpers(prog);
    emit_struct_eq_helpers(prog);
    // Per-struct stringifiers, so `"${p}"` has a char* path instead of falling
    // through to_string's `default: int_to_string` arm (PLAN_v1.21 S1).
    //
    // Three steps, and the order is load-bearing. A struct with an `S?` field
    // calls OptionS_to_string, and OptionS_to_string calls __wyn_str_S, so each
    // needs the other declared: register the payload structs first (or they get
    // no helper), forward-declare the family renderers, then emit both bodies.
    cg_register_optlike_payloads(prog);
    emit_optlike_str_helpers(prog, 0);
    emit_struct_str_helpers(prog);
    emit_optlike_str_helpers(prog, 1);
    // Last: an element renderer calls one of the two above.
    emit_array_elem_str_helpers(prog);

    // Generate module-level constants (only if has main - script mode puts them in wyn_main)
    if (has_main) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_CONST) {
            codegen_stmt(prog->stmts[i]);
        }
    }
    }
    
    // Generate global variables
    // Emit declarations at file scope, initializations in wyn_main
    int deferred_init_count = 0;
    // Growable: one slot per top-level var stmt is enough
    int* deferred_init_indices = malloc((size_t)(prog->count > 0 ? prog->count : 1) * sizeof(int));
    if (!deferred_init_indices) {
        fprintf(stderr, "wyn: out of memory allocating deferred-init table\n");
        exit(1);
    }
    {
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_VAR) {
            Stmt* var_stmt = prog->stmts[i];
            const char* c_type = "long long";
            bool is_simple_init = false;  // Can be initialized at file scope
            if (var_stmt->var.init) {
                if (var_stmt->var.init->type == EXPR_STRING) {
                    c_type = "const char*";
                    is_simple_init = true;
                } else if (var_stmt->var.init->type == EXPR_FLOAT) {
                    c_type = "double";
                    is_simple_init = true;
                } else if (var_stmt->var.init->type == EXPR_BOOL) {
                    c_type = "long long";
                    is_simple_init = true;
                } else if (var_stmt->var.init->type == EXPR_INT) {
                    c_type = "long long";
                    is_simple_init = true;
                } else if (var_stmt->var.init->type == EXPR_ARRAY) {
                    c_type = "WynArray";
                } else if (var_stmt->var.init->type == EXPR_STRUCT_INIT) {
                    emit("\n");
                    Token sname = var_stmt->var.init->struct_init.type_name;
                    emit("%.*s %.*s = ", sname.length, sname.start,
                         var_stmt->var.name.length, var_stmt->var.name.start);
                    codegen_expr(var_stmt->var.init);
                    emit(";\n");
                    continue;
                } else if (var_stmt->var.init->type == EXPR_CALL) {
                    if (var_stmt->var.init->expr_type) {
                        if (var_stmt->var.init->expr_type->kind == TYPE_MAP) {
                            c_type = "WynHashMap*";
                        } else if (var_stmt->var.init->expr_type->kind == TYPE_ARRAY) {
                            c_type = "WynArray";
                        } else if (var_stmt->var.init->expr_type->kind == TYPE_STRING) {
                            c_type = "const char*";
                        }
                    } else {
                        c_type = "WynHashMap*";
                    }
                } else if (var_stmt->var.init->type == EXPR_HASHMAP_LITERAL) {
                    c_type = "WynHashMap*";
                } else if (var_stmt->var.init->type == EXPR_METHOD_CALL) {
                    if (var_stmt->var.init->expr_type) {
                        if (var_stmt->var.init->expr_type->kind == TYPE_ARRAY) {
                            c_type = "WynArray";
                        } else if (var_stmt->var.init->expr_type->kind == TYPE_MAP) {
                            c_type = "WynHashMap*";
                        } else if (var_stmt->var.init->expr_type->kind == TYPE_STRING) {
                            c_type = "const char*";
                        }
                    }
                } else if (var_stmt->var.init->type == EXPR_BINARY) {
                    // Same operator-keyed bool decision as the in-function var
                    // path (codegen_stmt.c STMT_VAR): logical ops/comparisons
                    // store a truth value, declare bool so it prints true/false.
                    WynTokenType _tbop = var_stmt->var.init->binary.op.type;
                    if (_tbop == TOKEN_AND || _tbop == TOKEN_OR ||
                        _tbop == TOKEN_AMPAMP || _tbop == TOKEN_PIPEPIPE ||
                        _tbop == TOKEN_EQEQ || _tbop == TOKEN_BANGEQ ||
                        _tbop == TOKEN_LT || _tbop == TOKEN_GT ||
                        _tbop == TOKEN_LTEQ || _tbop == TOKEN_GTEQ ||
                        _tbop == TOKEN_IN) {
                        c_type = "bool";
                    }
                }
                if (var_stmt->var.type && var_stmt->var.type->type == EXPR_IDENT) {
                    Token tn = var_stmt->var.type->token;
                    if (tn.length == 6 && memcmp(tn.start, "string", 6) == 0) c_type = "const char*";
                    else if (tn.length == 5 && memcmp(tn.start, "float", 5) == 0) c_type = "double";
                }
                // Explicit optional annotation (e.g. `int?`) -> hand-specialized Option type.
                if (var_stmt->var.type && var_stmt->var.type->type == EXPR_OPTIONAL_TYPE) {
                    Expr* inner = var_stmt->var.type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT &&
                        inner->token.length == 6 && memcmp(inner->token.start, "string", 6) == 0)
                        c_type = "OptionString";
                    else if (inner && inner->type == EXPR_IDENT &&
                        inner->token.length == 5 && memcmp(inner->token.start, "float", 5) == 0)
                        c_type = "OptionFloat";
                    else if (inner && inner->type == EXPR_IDENT &&
                        inner->token.length == 4 && memcmp(inner->token.start, "bool", 4) == 0)
                        c_type = "OptionBool";
                    else
                        c_type = "OptionInt";
                }
                // Fallback: if the AST-shape heuristics above left the default and the
                // checker inferred a concrete type, use it. This lets top-level (script
                // mode) declarations infer the same C types as ones inside a function
                // body - enums, comprehensions (arrays), optionals, match results, etc.
                if (strcmp(c_type, "long long") == 0) {
                    const char* inferred = codegen_c_type_from_type(var_stmt->var.init->expr_type);
                    if (inferred && strcmp(inferred, "long long") != 0) {
                        c_type = inferred;
                        // Register enum vars so `.to_string()` dispatches correctly.
                        if (var_stmt->var.init->expr_type->kind == TYPE_ENUM) {
                            char _vn[128]; token_to_cstr(_vn, sizeof(_vn), var_stmt->var.name);
                            extern void register_enum_var(const char*, const char*);
                            register_enum_var(_vn, c_type);
                        }
                    }
                }
            }
            emit("\n");
            // A top-level var whose name is a C keyword (long/short/double/...)
            // must be emitted with the collision prefix and registered so later
            // uses (EXPR_IDENT / assignment targets) prefix too - `long = ...`
            // used to emit `const char* long;` and ICE at the C stage.
            char _tvn[512]; token_to_cstr(_tvn, sizeof(_tvn), var_stmt->var.name);
            { extern int is_c_name_collision(const char*);
              extern void register_user_collision(const char*);
              if (is_c_name_collision(_tvn)) {
                  register_user_collision(_tvn);
                  memmove(_tvn + WYN_UFN_PFX_LEN, _tvn, strlen(_tvn) + 1);
                  memcpy(_tvn, WYN_UFN_PFX, WYN_UFN_PFX_LEN);
              } }
            // A module-level string global must be REGISTERED as one, or the assignment
            // path does not recognise it and emits a bare `g = concat(...)` where a
            // local gets the release-the-old-value form. That leaked the previous
            // string on every assignment: the same 300k-iteration loop peaked at
            // 29.1 MB writing to a global versus 1.5 MB writing to a local. It is a
            // memory CAP, not a slowdown -- a program accumulating into a global
            // cannot finish.
            if (strcmp(c_type, "const char*") == 0 || strcmp(c_type, "char*") == 0) {
                extern void register_string_global(const char*);
                register_string_global(_tvn);
            }
            if (is_simple_init) {
                // Simple literals can be initialized at file scope
                emit("%s %s", c_type, _tvn);
                if (var_stmt->var.init) { emit(" = "); codegen_expr(var_stmt->var.init); }
                else { emit(" = 0"); }
                emit(";\n");
            } else {
                // Declare at file scope, initialize in wyn_main
                emit("%s %s;\n", c_type, _tvn);
                deferred_init_indices[deferred_init_count++] = i;
            }
        }
    }
    } // end global vars block
    // For has_main mode, emit constructor for deferred inits
    // (script mode handles this in wyn_main sequentially)
    if (has_main && deferred_init_count > 0) {
        emit("\n__attribute__((constructor)) void __wyn_init_globals(void) {\n");
        for (int d = 0; d < deferred_init_count; d++) {
            Stmt* var_stmt = prog->stmts[deferred_init_indices[d]];
            char _dvn[512]; token_to_cstr(_dvn, sizeof(_dvn), var_stmt->var.name);
            { extern int is_c_name_collision(const char*);
              if (is_c_name_collision(_dvn)) {
                  memmove(_dvn + WYN_UFN_PFX_LEN, _dvn, strlen(_dvn) + 1);
                  memcpy(_dvn, WYN_UFN_PFX, WYN_UFN_PFX_LEN);
              } }
            emit("    %s = ", _dvn);
            codegen_expr(var_stmt->var.init);
            emit(";\n");
        }
        emit("}\n");
    }
    free(deferred_init_indices);
    deferred_init_indices = NULL;

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
                        // Param C types MUST match the body definition exactly
                        // (codegen_stmt emits `long long`/`const char*`), else the
                        // forward decl conflicts: "conflicting types for X".
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
    
    // Lambda PROTOTYPES here; the BODIES are emitted further down, after the
    // user-function forward declarations.
    //
    // Why split: a lambda body may CALL a user-defined function
    // (`var f = (v) => dbl(v)`), so emitting the body before `dbl`'s prototype
    // gave C an implicit declaration followed by a conflicting static one --
    //   error: call to undeclared function 'dbl'
    //   error: static declaration of 'dbl' follows non-static declaration
    // -- and ANY such lambda failed to build after passing `wyn check` cleanly.
    // But a MODULE function is emitted EARLIER than the forward declarations and
    // may reference a lambda (`pub fn f(xs) { xs.map((v) => v * 2) }`), so simply
    // moving the bodies late produced "use of undeclared identifier '__lambda_1'"
    // instead - caught by tests/module_tests/run_lambda_in_module_test.sh, 6 fail.
    // No single body position satisfies both orderings; a prototype does.
    // Same class of fix as #286 (struct typedefs in dependency order).
    if (lambda_count > 0) {
        emit("// Lambda prototypes (bodies follow the function declarations)\n");
        for (int i = 0; i < lambda_count; i++) {
            if (lambda_functions[i].ast) emit_lambda_prototype(&lambda_functions[i]);
        }
        emit("\n");
    }

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
            // Emit #define aliases for selectively imported functions
            if (prog->stmts[i]->import.item_count > 0) {
                for (int j = 0; j < prog->stmts[i]->import.item_count; j++) {
                    char item_name[256]; token_to_cstr(item_name, sizeof(item_name), prog->stmts[i]->import.items[j]);
                    const char* resolved = resolve_module_alias(item_name);
                    if (resolved != item_name && strcmp(resolved, item_name) != 0) {
                        // Skip enum types - they use typedef, not #define
                        extern int is_enum_type(const char*);
                        extern int is_data_enum_type(const char*);
                        if (!is_enum_type(item_name) && !is_data_enum_type(item_name)) {
                            emit("#define %s %s\n", item_name, resolved);
                        }
                    }
                }
            }
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
            
            // Skip imported functions (handled by module codegen + #define alias)
            {
                char fn_name_check[256]; token_to_cstr(fn_name_check, sizeof(fn_name_check), fn->name);
                const char* resolved_check = resolve_module_alias(fn_name_check);
                if (resolved_check != fn_name_check && strcmp(resolved_check, fn_name_check) != 0) {
                    continue;
                }
            }
            
            // Determine return type
            const char* return_type = fn->return_type ? "long long" : "void"; // default
            char return_type_buf[256] = {0};  // Buffer for custom return types
            bool is_async = fn->is_async;
            (void)is_async;
            
            // main() always returns long long (int), even without -> int
            bool is_main_fn = (fn->name.length == 4 && memcmp(fn->name.start, "main", 4) == 0);
            if (is_main_fn) return_type = "long long";
            
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
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)
                                    return_type = "OptionString";
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0)
                                    return_type = "OptionFloat";
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)
                                    return_type = "OptionBool";
                                else return_type = "OptionInt";
                            } else return_type = "OptionInt";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "Result", 6) == 0) {
                            // Resolve Result<int, string> -> ResultInt, Result<string, string> -> ResultString
                            if (fn->return_type->call.arg_count > 0 &&
                                fn->return_type->call.args[0]->type == EXPR_IDENT) {
                                Token inner = fn->return_type->call.args[0]->token;
                                extern const char* result_family_err_suffix(Expr*);
                                // This FORWARD DECLARATION must name the same family as
                                // the definition (codegen_stmt) — a primitive ok payload
                                // uses the builtin only for a string E, else its own
                                // `Result<Tag>_<ErrTag>` family. Disagreeing here emits
                                // "conflicting types for '<fn>'".
                                const char* _rsuf = result_family_err_suffix(fn->return_type);
                                const char* _rtag = NULL;
                                if (inner.length == 6 && memcmp(inner.start, "string", 6) == 0)     _rtag = "String";
                                else if (inner.length == 5 && memcmp(inner.start, "float", 5) == 0) _rtag = "Float";
                                else if (inner.length == 4 && memcmp(inner.start, "bool", 4) == 0)  _rtag = "Bool";
                                else if (inner.length == 3 && memcmp(inner.start, "int", 3) == 0)    _rtag = "Int";
                                if (_rtag) {
                                    snprintf(return_type_buf, sizeof(return_type_buf), "Result%s%s",
                                             _rtag, _rsuf);
                                    return_type = return_type_buf;
                                }
                                else {
                                    // `Result<Struct, E>` -> the monomorphic
                                    // Result<Struct,E> family (ResultPoint,
                                    // ResultPoint_Fail, …). The err suffix keeps this
                                    // C signature name in lockstep with the family.
                                    char _stn[96]; token_to_cstr(_stn, sizeof(_stn), inner);
                                    extern int is_known_struct(const char*);
                                    extern const char* result_family_err_suffix(Expr*);
                                    if (is_known_struct(_stn)) {
                                        snprintf(return_type_buf, sizeof(return_type_buf), "Result%s%s",
                                                 _stn, result_family_err_suffix(fn->return_type));
                                        return_type = return_type_buf;
                                    } else return_type = "ResultInt";
                                }
                            } else return_type = "ResultInt";
                        }
                    }
                } else if (fn->return_type->type == EXPR_ARRAY) {
                    // Array type like [int] or [string]
                    return_type = "WynArray";
                } else if (fn->return_type->type == EXPR_TUPLE) {
                    // Tuple return type: (int, string) -> generate typedef
                    static char _trt[256];
                    snprintf(_trt, sizeof(_trt), "_wyn_tup_%.*s", fn->name.length, fn->name.start);
                    // Emit typedef before the function
                    emit("typedef struct { ");
                    for (int _ti = 0; _ti < fn->return_type->tuple.count; _ti++) {
                        const char* et = "long long";
                        if (fn->return_type->tuple.elements[_ti]->type == EXPR_IDENT) {
                            Token tt = fn->return_type->tuple.elements[_ti]->token;
                            if (tt.length == 6 && memcmp(tt.start, "string", 6) == 0) et = "const char*";
                            else if (tt.length == 5 && memcmp(tt.start, "float", 5) == 0) et = "double";
                            else if (tt.length == 4 && memcmp(tt.start, "bool", 4) == 0) et = "bool";
                        }
                        emit("%s item%d; ", et, _ti);
                    }
                    emit("} %s;\n", _trt);
                    return_type = _trt;
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
                        token_to_cstr(return_type_buf, sizeof(return_type_buf), type_name);
                        return_type = return_type_buf;
                    }
                } else if (fn->return_type->type == EXPR_OPTIONAL_TYPE) {
                    Expr* inner = fn->return_type->optional_type.inner_type;
                    if (inner && inner->type == EXPR_IDENT) {
                        Token t = inner->token;
                        // Same authority as the DEFINITION in codegen_stmt — they must
                        // name the same family or C reports "conflicting types for '<fn>'".
                        {
                            char _ptn[96]; token_to_cstr(_ptn, sizeof(_ptn), t);
                            extern const char* wyn_option_family(const char*, const char**, int*);
                            static char _offd[128];
                            snprintf(_offd, sizeof(_offd), "%s", wyn_option_family(_ptn, NULL, NULL));
                            return_type = _offd;
                        }
                    } else {
                        return_type = "WynOptional*";
                    }
                } else if (fn->return_type->type == EXPR_FN_TYPE) {
                    // Function type: fn(int) -> int becomes WynClosure
                    return_type = "WynClosure";
                }
            }
            
            // L3: Generator - override return type
            extern int fn_is_generator(Stmt*);
            Stmt* _orig = prog->stmts[i]->type == STMT_FN ? prog->stmts[i] : prog->stmts[i]->export.stmt;
            if (fn_is_generator(_orig)) {
                return_type = "WynIter*";
            }
            
            // Generate forward declaration
            // Special handling for main function - rename to wyn_main
            bool is_main_function = (fn->name.length == 4 && 
                                   memcmp(fn->name.start, "main", 4) == 0);
            
            // Register default parameters
            if (fn->param_defaults) {
                char _fn[128]; token_to_cstr(_fn, sizeof(_fn), fn->name);
                extern void register_fn_defaults(const char*, Expr**, int);
                register_fn_defaults(_fn, fn->param_defaults, fn->param_count);
                extern void register_fn_param_names(const char*, Token*, int);
                register_fn_param_names(_fn, fn->params, fn->param_count);
            }
            
            // Register return type for spawn/await type dispatch
            if (fn->return_type && fn->return_type->type == EXPR_IDENT) {
                char _fn2[128]; token_to_cstr(_fn2, sizeof(_fn2), fn->name);
                char _rt[32]; token_to_cstr(_rt, sizeof(_rt), fn->return_type->token);
                extern void register_fn_return_type(const char*, const char*);
                register_fn_return_type(_fn2, _rt);
            }
            
            // Function forward declaration
            // Optimization: inline hint for small non-main functions
            // Don't inline spawned functions (called from worker threads)
            int _body_stmt_count = 0;
            if (fn->body && fn->body->type == STMT_BLOCK) _body_stmt_count = fn->body->block.count;
            bool _is_spawned_fn = false;
            { char _fnm[256]; token_to_cstr(_fnm, sizeof(_fnm), fn->name);
              for (int _si = 0; _si < spawn_wrapper_count; _si++) {
                  if (strcmp(spawn_wrappers[_si].func_name, _fnm) == 0) { _is_spawned_fn = true; break; }
              }
            }
            bool _emit_inline = (!is_main_function && !_is_spawned_fn && _body_stmt_count > 0 && _body_stmt_count <= 5);
            // Detect self-recursive functions for always_inline hint
            bool _is_recursive = false;
            if (!is_main_function && fn->body && fn->body->type == STMT_BLOCK) {
                for (int _s = 0; _s < fn->body->block.count && !_is_recursive; _s++) {
                    Stmt* _st = fn->body->block.stmts[_s];
                    if (_st->type == STMT_RETURN && _st->ret.value &&
                        _st->ret.value->type == EXPR_BINARY) {
                        // Check if either side of binary expr calls self
                        Expr* _l = _st->ret.value->binary.left;
                        Expr* _r = _st->ret.value->binary.right;
                        if ((_l && _l->type == EXPR_CALL && _l->call.callee->type == EXPR_IDENT &&
                             _l->call.callee->token.length == fn->name.length &&
                             memcmp(_l->call.callee->token.start, fn->name.start, fn->name.length) == 0) ||
                            (_r && _r->type == EXPR_CALL && _r->call.callee->type == EXPR_IDENT &&
                             _r->call.callee->token.length == fn->name.length &&
                             memcmp(_r->call.callee->token.start, fn->name.start, fn->name.length) == 0))
                            _is_recursive = true;
                    }
                }
            }
            // Same reason as in codegen_stmt.c: in library mode `static` would keep the
            // symbol out of the .dylib/.so, so a --python/--shared build must not
            // inline. Note this covers the RECURSIVE case too - `factorial` in the
            // Python guide is recursive AND under the size threshold, so it was hidden
            // by both conditions.
            extern bool codegen_in_library_mode(void);
            if ((_emit_inline || (_is_recursive && !_is_spawned_fn)) &&
                !codegen_in_library_mode()) emit("__attribute__((hot)) static inline ");
            else if (!is_main_function) emit("__attribute__((hot)) ");
            
            if (is_main_function) {
                emit("%s wyn_main(", return_type);
            } else if (fn->is_extension) {
                // Extension method: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     fn->receiver_type.length, fn->receiver_type.start,
                     fn->name.length, fn->name.start);
            } else {
                char _fn_name[256]; token_to_cstr(_fn_name, sizeof(_fn_name), fn->name);
                extern int is_c_name_collision(const char*);
                extern void register_user_collision(const char*);
                bool _is_ckw = is_c_name_collision(_fn_name);
                if (_is_ckw) register_user_collision(_fn_name);
                emit("%s %s%.*s(", return_type, _is_ckw ? WYN_UFN_PFX : "", fn->name.length, fn->name.start);
            }
            for (int j = 0; j < fn->param_count; j++) {
                if (j > 0) emit(", ");
                
                // Determine parameter type
                const char* param_type = "long long"; // default
                char struct_type_name[256] = {0};
                bool is_struct_type = false;
                (void)is_struct_type;
                
                // Extension method self parameter: use receiver type
                if (fn->is_extension && j == 0 && !fn->param_types[j]) {
                    token_to_cstr(struct_type_name, sizeof(struct_type_name), fn->receiver_type);
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
                        int pb_len = 0;
                        for (int k = 0; k < fn_type->param_count; k++) {
                            if (k > 0) { memcpy(params_buf + pb_len, ", ", 2); pb_len += 2; }
                            const char* pt = "long long";
                            if (fn_type->param_types[k] && fn_type->param_types[k]->type == EXPR_IDENT) {
                                Token pt_tok = fn_type->param_types[k]->token;
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
                        } else if (type_name.length == 3 && memcmp(type_name.start, "ptr", 3) == 0) {
                            // FFI opaque pointer - a user fn can pass one through.
                            param_type = "void*";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "cstr", 4) == 0) {
                            param_type = "char*";  // raw C string
                        } else {
                            // Assume it's a struct type
                            token_to_cstr(struct_type_name, sizeof(struct_type_name), type_name);
                            param_type = struct_type_name;
                            is_struct_type = true;
                        }
                    } else if (fn->param_types[j]->type == EXPR_ARRAY) {
                        // Handle array types [type] - pass as WynArray
                        param_type = "WynArray";
                    } else if (fn->param_types[j]->type == EXPR_CALL &&
                               fn->param_types[j]->call.callee &&
                               fn->param_types[j]->call.callee->type == EXPR_IDENT &&
                               fn->param_types[j]->call.callee->token.length == 7 &&
                               memcmp(fn->param_types[j]->call.callee->token.start, "HashMap", 7) == 0) {
                        // `m: {string: int}` (parser-desugared) / `HashMap<K, V>`:
                        // the forward declaration must agree with the definition
                        // (codegen_stmt.c), which now emits WynHashMap*.
                        param_type = "WynHashMap*";
                    } else if (fn->param_types[j]->type == EXPR_CALL &&
                               fn->param_types[j]->call.callee &&
                               fn->param_types[j]->call.callee->type == EXPR_IDENT &&
                               fn->param_types[j]->call.callee->token.length == 7 &&
                               memcmp(fn->param_types[j]->call.callee->token.start, "HashSet", 7) == 0) {
                        param_type = "WynHashSet*";
                    } else if (fn->param_types[j]->type == EXPR_OPTIONAL_TYPE) {
                        // T2.5.1: Optional param. int?/string?/…/Struct? map to the
                        // concrete Option family; otherwise the generic WynOptional*.
                        Expr* _inr = fn->param_types[j]->optional_type.inner_type;
                        static char _opbuf[128];
                        param_type = "WynOptional*";
                        if (_inr && _inr->type == EXPR_IDENT) {
                            Token t = _inr->token;
                            if (t.length == 3 && memcmp(t.start, "int", 3) == 0) param_type = "OptionInt";
                            else if (t.length == 6 && memcmp(t.start, "string", 6) == 0) param_type = "OptionString";
                            else if (t.length == 5 && memcmp(t.start, "float", 5) == 0) param_type = "OptionFloat";
                            else if (t.length == 4 && memcmp(t.start, "bool", 4) == 0) param_type = "OptionBool";
                            else {
                                // Same authority as the definition side.
                                char _stn[96]; token_to_cstr(_stn, sizeof(_stn), t);
                                extern const char* wyn_option_family(const char*, const char**, int*);
                                snprintf(_opbuf, sizeof(_opbuf), "%s", wyn_option_family(_stn, NULL, NULL));
                                param_type = _opbuf;
                            }
                        }
                    }
                }

                // Emit with pointer for mut params
                bool is_mut_param = fn->param_mutable && fn->param_mutable[j];
                // Check if param name is a C keyword
                char _pname[256]; token_to_cstr(_pname, sizeof(_pname), fn->params[j]);
                static const char* _c_kw[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                bool _is_pkw = false; for (int _k = 0; _c_kw[_k]; _k++) { if (strcmp(_pname, _c_kw[_k]) == 0) { _is_pkw = true; break; } }
                if (is_mut_param) {
                    emit("%s *%s%.*s", param_type, _is_pkw ? "_" : "", fn->params[j].length, fn->params[j].start);
                } else {
                    emit("%s %s%.*s", param_type, _is_pkw ? "_" : "", fn->params[j].length, fn->params[j].start);
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
                        static char impl_fwd_ret[128]; token_to_cstr(impl_fwd_ret, sizeof(impl_fwd_ret), ret_type);
                        return_type = impl_fwd_ret;
                    }
                }
                
                // Generate forward declaration: Type_method
                emit("%s %.*s_%.*s(", return_type,
                     stmt->impl.type_name.length, stmt->impl.type_name.start,
                     method->name.length, method->name.start);
                
                for (int k = 0; k < method->param_count; k++) {
                    if (k > 0) emit(", ");
                    
                    // Determine parameter type
                    const char* param_type = "long long";
                    char custom_type_buf[256] = {0};
                    if (method->param_types[k] && method->param_types[k]->type == EXPR_IDENT) {
                        Token type_name = method->param_types[k]->token;
                        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
                            param_type = "long long";
                        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
                            param_type = "const char*";
                        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
                            param_type = "double";
                        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
                            param_type = "bool";
                        } else {
                            // Custom struct type
                            token_to_cstr(custom_type_buf, sizeof(custom_type_buf), type_name);
                            param_type = custom_type_buf;
                        }
                    }
                    
                    // Must agree with the definition emitter in codegen_stmt.c:
                    // a `mut` param (including `mut self`) takes a pointer. A
                    // prototype that disagrees with its definition is a C error.
                    bool _fd_is_mut = method->param_mutable && method->param_mutable[k];
                    emit("%s %s%.*s", param_type, _fd_is_mut ? "*" : "",
                         method->params[k].length, method->params[k].start);
                }
                emit(");\n");
            }
        }
    }
    emit("\n");
    
    // Vtable wrappers and instances are generated inline during main codegen
    // (after trait and impl statements have been processed)
    
    // Lambda bodies, emitted HERE rather than before the forward declarations
    // above: a lambda body may call a user-defined function or an impl method, and
    // both are only declared by the loops above. See the note at the old site.
    // Bodies still precede wyn_main and every other function DEFINITION, so a
    // lambda referenced from any of them is still defined before use.
    if (lambda_count > 0) {
        emit("// Lambda functions\n");
        for (int i = 0; i < lambda_count; i++) {
            if (lambda_functions[i].ast) {
                emit_lambda_via_codegen(&lambda_functions[i]);
                emit("\n");
            }
        }
        emit("\n");
    }

    // Emit spawn wrapper functions (after forward declarations)
    if (spawn_wrapper_count > 0) {
        emit("\n// Spawn wrapper functions\n");
        for (int i = 0; i < spawn_wrapper_count; i++) {
            int ac = spawn_wrappers[i].arg_count;
            // A user function whose name collides with a C keyword or a libc/POSIX
            // symbol is EMITTED under the "wynfn_" prefix (PR #53), so the spawn
            // wrapper must CALL it by that prefixed name. It did not, so:
            //
            //   fn send(a: int, b: int, c: string) { ... }
            //   send(1, 2, "x")        // fine - builds and runs
            //   spawn send(1, 2, "x")  // wyn check: no errors
            //   -> error: too few arguments to function call, expected 4, have 3
            //
            // because the wrapper called POSIX send(sockfd, buf, len, flags). The
            // collision guard worked for every direct call and was bypassed by
            // spawn alone. `send`, `read`, `write`, `connect`, `accept`, `close`
            // are the natural names for request handlers, which is exactly the
            // code most likely to be spawned - it was found while making the
            // "REST API in 93 lines" post race-free, whose handler is named send.
            //
            // The WRAPPER's own name keeps the raw spelling: __spawn_wrapper_send
            // cannot collide, and the call sites already reference it that way.
            char _cbuf[WYN_UFN_PFX_LEN + 256];
            const char* callee = emit_c_var_name(_cbuf, sizeof(_cbuf), spawn_wrappers[i].func_name);
            if (ac == 0) {
                // Check if function has default parameters that need filling
                extern int get_fn_param_count(const char*);
                extern Expr* get_fn_default(const char*, int);
                int total_params = get_fn_param_count(spawn_wrappers[i].func_name);
                
                emit("void* __spawn_wrapper_%s(void* arg) {\n", spawn_wrappers[i].func_name);
                if (total_params > 0) {
                    // Function has params with defaults - fill them in
                    if (spawn_wrappers[i].returns_void) {
                        emit("    (void)arg; %s(", callee);
                        for (int di = 0; di < total_params; di++) {
                            if (di > 0) emit(", ");
                            Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                            if (def) { codegen_expr(def); } else { emit("0"); }
                        }
                        emit(");\n    return NULL;\n");
                    } else if (spawn_wrappers[i].return_type[0]) {
                        emit("    (void)arg; %s* __r = malloc(sizeof(%s)); *__r = %s(", spawn_wrappers[i].return_type, spawn_wrappers[i].return_type, callee);
                        for (int di = 0; di < total_params; di++) {
                            if (di > 0) emit(", ");
                            Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                            if (def) { codegen_expr(def); } else { emit("0"); }
                        }
                        emit(");\n    return __r;\n");
                    } else {
                        emit("    (void)arg; return (void*)(intptr_t)%s(", callee);
                        for (int di = 0; di < total_params; di++) {
                            if (di > 0) emit(", ");
                            Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                            if (def) { codegen_expr(def); } else { emit("0"); }
                        }
                        emit(");\n");
                    }
                } else {
                    if (spawn_wrappers[i].returns_void) {
                        emit("    (void)arg; %s();\n    return NULL;\n", callee);
                    } else if (spawn_wrappers[i].return_type[0]) {
                        emit("    (void)arg; %s* __r = malloc(sizeof(%s)); *__r = %s();\n    return __r;\n",
                             spawn_wrappers[i].return_type, spawn_wrappers[i].return_type, callee);
                    } else {
                        emit("    (void)arg; return (void*)(intptr_t)%s();\n", callee);
                    }
                }
                emit("}\n\n");
            } else if (ac == 1 && spawn_wrappers[i].boxed_arg1) {
                // Boxed single arg (float/struct/array): the call site mallocs
                // a one-field box; unpack with the REAL param type instead of
                // the truncating (void*)(intptr_t) word cast. The type comes
                // from spawn_param_c_type - the same helper the call sites use.
                const char* _bpt = spawn_param_c_type(spawn_wrappers[i].func_name, 0, NULL, NULL);
                emit("void* __spawn_wrapper_%s_1b(void* arg) {\n", spawn_wrappers[i].func_name);
                emit("    struct { %s a0; } *args = arg;\n", _bpt);
                if (spawn_wrappers[i].returns_void) {
                    emit("    %s(args->a0);\n    free(args);\n    return NULL;\n", callee);
                } else if (spawn_wrappers[i].return_type[0]) {
                    emit("    %s* __r = malloc(sizeof(%s));\n    *__r = %s(args->a0);\n    free(args);\n    return __r;\n",
                         spawn_wrappers[i].return_type, spawn_wrappers[i].return_type, callee);
                } else {
                    emit("    long long __r = (long long)%s(args->a0);\n    free(args);\n    return (void*)(intptr_t)__r;\n",
                         callee);
                }
                emit("}\n\n");
            } else if (ac == 1) {
                // Check if function has more params with defaults
                extern int get_fn_param_count(const char*);
                extern Expr* get_fn_default(const char*, int);
                int total_params = get_fn_param_count(spawn_wrappers[i].func_name);

                // Decode the word-sized arg with its REAL param type. A string
                // arg is decoded as const char* and RC-released here (the call
                // site retained it at spawn time so the spawning scope's own
                // release can't free it before this task runs).
                int _a1_str = 0;
                spawn_param_c_type(spawn_wrappers[i].func_name, 0, NULL, &_a1_str);
                const char* _a1_decode = _a1_str ? "__a0" : "(long long)(intptr_t)arg";
                emit("void* __spawn_wrapper_%s_1(void* arg) {\n", spawn_wrappers[i].func_name);
                if (_a1_str) emit("    const char* __a0 = (const char*)arg;\n");
                if (spawn_wrappers[i].returns_void) {
                    emit("    %s(%s", callee, _a1_decode);
                    for (int di = 1; di < total_params; di++) {
                        emit(", ");
                        Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                        if (def) { codegen_expr(def); } else { emit("0"); }
                    }
                    emit(");\n");
                    if (_a1_str) emit("    wyn_rc_release(__a0);\n");
                    emit("    return NULL;\n");
                } else if (spawn_wrappers[i].return_type[0]) {
                    emit("    %s* __r = malloc(sizeof(%s)); *__r = %s(%s", spawn_wrappers[i].return_type, spawn_wrappers[i].return_type, callee, _a1_decode);
                    for (int di = 1; di < total_params; di++) {
                        emit(", ");
                        Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                        if (def) { codegen_expr(def); } else { emit("0"); }
                    }
                    emit(");\n");
                    if (_a1_str) emit("    wyn_rc_release(__a0);\n");
                    emit("    return __r;\n");
                } else {
                    emit("    long long __r = (long long)%s(%s", callee, _a1_decode);
                    for (int di = 1; di < total_params; di++) {
                        emit(", ");
                        Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                        if (def) { codegen_expr(def); } else { emit("0"); }
                    }
                    emit(");\n");
                    if (_a1_str) emit("    wyn_rc_release(__a0);\n");
                    emit("    return (void*)(intptr_t)__r;\n");
                }
                emit("}\n\n");
            } else {
                // Multi-arg wrapper - also fill in defaults for remaining params
                extern int get_fn_param_count(const char*);
                extern Expr* get_fn_default(const char*, int);
                int total_params = get_fn_param_count(spawn_wrappers[i].func_name);
                
                emit("void* __spawn_wrapper_%s_%d(void* arg) {\n", spawn_wrappers[i].func_name, ac);
                emit("    struct { ");
                // Field types come from spawn_param_c_type - the same helper
                // the call sites' pack structs use, so they can never disagree.
                // String fields were RC-retained at spawn time; track them so
                // they are released after the call.
                int _str_field[64] = {0};
                for (int j = 0; j < ac; j++) {
                    int _isstr = 0;
                    const char* ptype = spawn_param_c_type(spawn_wrappers[i].func_name, j, NULL, &_isstr);
                    if (j < 64) _str_field[j] = _isstr;
                    emit("%s a%d; ", ptype, j);
                }
                emit("} *args = arg;\n");
                if (spawn_wrappers[i].returns_void) {
                    emit("    %s(", callee);
                    for (int j = 0; j < ac; j++) { if (j > 0) emit(", "); emit("args->a%d", j); }
                    for (int di = ac; di < total_params; di++) {
                        emit(", ");
                        Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                        if (def) { codegen_expr(def); } else { emit("0"); }
                    }
                    emit(");\n");
                    for (int j = 0; j < ac && j < 64; j++)
                        if (_str_field[j]) emit("    wyn_rc_release(args->a%d);\n", j);
                    emit("    free(args);\n    return NULL;\n");
                } else {
                    if (spawn_wrappers[i].return_type[0]) {
                        // Struct return: heap-allocate and return pointer
                        emit("    %s* __r = malloc(sizeof(%s));\n", spawn_wrappers[i].return_type, spawn_wrappers[i].return_type);
                        emit("    *__r = %s(", callee);
                        for (int j = 0; j < ac; j++) { if (j > 0) emit(", "); emit("args->a%d", j); }
                        for (int di = ac; di < total_params; di++) {
                            emit(", ");
                            Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                            if (def) { codegen_expr(def); } else { emit("0"); }
                        }
                        emit(");\n");
                        for (int j = 0; j < ac && j < 64; j++)
                            if (_str_field[j]) emit("    wyn_rc_release(args->a%d);\n", j);
                        emit("    free(args);\n    return __r;\n");
                    } else {
                        emit("    long long __r = (long long)%s(", callee);
                        for (int j = 0; j < ac; j++) { if (j > 0) emit(", "); emit("args->a%d", j); }
                        for (int di = ac; di < total_params; di++) {
                            emit(", ");
                            Expr* def = get_fn_default(spawn_wrappers[i].func_name, di);
                            if (def) { codegen_expr(def); } else { emit("0"); }
                        }
                        emit(");\n");
                        for (int j = 0; j < ac && j < 64; j++)
                            if (_str_field[j]) emit("    wyn_rc_release(args->a%d);\n", j);
                        emit("    free(args);\n    return (void*)(intptr_t)__r;\n");
                    }
                }
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
            
            // Skip functions that are imported from modules
            // Emit a #define alias to the prefixed version
            {
                char fn_name[256]; token_to_cstr(fn_name, sizeof(fn_name), fn->name);
                const char* resolved = resolve_module_alias(fn_name);
                if (resolved != fn_name && strcmp(resolved, fn_name) != 0) {
                    emit("#define %s %s\n", fn_name, resolved);
                    continue;
                }
            }
            
            if (prog->stmts[i]->type == STMT_EXPORT) {
                codegen_stmt(prog->stmts[i]->export.stmt);
            } else {
                codegen_stmt(prog->stmts[i]);
            }
        }
    }
    
    // If no main function, create one that executes all statements
    // Check for test blocks
    int test_count = 0;
    Stmt* before_each_body = NULL;
    Stmt* after_each_body = NULL;
    for (int i = 0; i < prog->count; i++) {
        if (prog->stmts[i]->type == STMT_TEST) test_count++;
    }
    
    // The synthesized wyn_main below (test runner / script mode) is non-void
    // (long long): a bare `return` in a test body or top-level statement must
    // lower to `return 0;` or C rejects it (see STMT_RETURN in codegen_stmt.c).
    extern bool current_fn_c_nonvoid;
    bool prev_fn_c_nonvoid = current_fn_c_nonvoid;
    if (!has_main) current_fn_c_nonvoid = true;

    if (!has_main && test_count > 0) {
        // File-level consts: script mode emits these inside wyn_main, but the
        // test runner synthesizes its own main, so emit them at file scope here.
        for (int i = 0; i < prog->count; i++) {
            if (prog->stmts[i]->type == STMT_CONST) {
                codegen_stmt(prog->stmts[i]);
            }
        }
        // Generate test runner main. File-level globals were already declared
        // at file scope above (simple literals initialized there, the rest
        // deferred) - re-emitting them here used to produce C "redefinition"
        // errors for any test file with a top-level var/const.
        emit("long long wyn_main() {\n");
        // Initialize deferred (non-literal) globals before running tests,
        // mirroring the script-mode branch below.
        for (int i = 0; i < prog->count; i++) {
            if (prog->stmts[i]->type != STMT_VAR) continue;
            Stmt* var_stmt = prog->stmts[i];
            if (!var_stmt->var.init) continue;
            bool is_simple = var_stmt->var.init->type == EXPR_STRING ||
                             var_stmt->var.init->type == EXPR_FLOAT ||
                             var_stmt->var.init->type == EXPR_BOOL ||
                             var_stmt->var.init->type == EXPR_INT ||
                             var_stmt->var.init->type == EXPR_STRUCT_INIT;
            if (is_simple) continue;
            char _tvn[512]; token_to_cstr(_tvn, sizeof(_tvn), var_stmt->var.name);
            { extern int is_c_name_collision(const char*);
              if (is_c_name_collision(_tvn)) {
                  memmove(_tvn + WYN_UFN_PFX_LEN, _tvn, strlen(_tvn) + 1);
                  memcpy(_tvn, WYN_UFN_PFX, WYN_UFN_PFX_LEN);
              } }
            emit("    %s = ", _tvn);
            codegen_expr(var_stmt->var.init);
            emit(";\n");
        }
        emit("    int __test_pass = 0, __test_fail = 0;\n");
        
        // Find before_each/after_each (parsed as test blocks with special names)
        for (int i = 0; i < prog->count; i++) {
            if (prog->stmts[i]->type == STMT_TEST) {
                Token name = prog->stmts[i]->test_stmt.name;
                if (name.length >= 11 && memcmp(name.start, "before_each", 11) == 0)
                    before_each_body = prog->stmts[i]->test_stmt.body;
                else if (name.length >= 10 && memcmp(name.start, "after_each", 10) == 0)
                    after_each_body = prog->stmts[i]->test_stmt.body;
            }
        }
        
        for (int i = 0; i < prog->count; i++) {
            if (prog->stmts[i]->type != STMT_TEST) continue;
            Token name = prog->stmts[i]->test_stmt.name;
            // Skip before_each/after_each
            if (name.length >= 10 && (memcmp(name.start, "before_each", 11) == 0 || memcmp(name.start, "after_each", 10) == 0))
                continue;
            
            // Strip quotes from string name
            const char* tname = name.start;
            int tname_len = name.length;
            if (tname_len >= 2 && tname[0] == '"') { tname++; tname_len -= 2; }
            
            emit("    { // test: %.*s\n", tname_len, tname);
            emit("        int __prev_fail = wyn_test_fail_count;\n");
            
            // before_each
            if (before_each_body) {
                emit("        // before_each\n");
                codegen_stmt(before_each_body);
            }
            
            // test body
            codegen_stmt(prog->stmts[i]->test_stmt.body);
            
            // after_each
            if (after_each_body) {
                emit("        // after_each\n");
                codegen_stmt(after_each_body);
            }
            
            // The test NAME lands inside the emitted printf's FORMAT string, so a
            // `%` in it becomes a live format specifier reading a nonexistent
            // argument: `test "a 30%-alpha stroke"` printed
            // "a 300x1.08p-1044lpha stroke", because `%-a` consumed a double that
            // was never passed. That is undefined behaviour rather than a cosmetic
            // mangling - a name containing `%s` or `%n` would read or write through
            // a wild pointer - so every `%` is doubled here. The other C-string
            // hazards in a name (a quote, a backslash) are handled where the name
            // is echoed elsewhere; this is the one site that treats it as a format.
            emit("        if (wyn_test_fail_count == __prev_fail) {\n");
            emit("            printf(\"  \\033[32m✓\\033[0m ");
            emit_percent_doubled(tname, tname_len);
            emit("\\n\");\n");
            emit("            __test_pass++;\n");
            emit("        } else {\n");
            emit("            printf(\"  \\033[31m✗\\033[0m ");
            emit_percent_doubled(tname, tname_len);
            emit("\\n\");\n");
            emit("            __test_fail++;\n");
            emit("        }\n");
            emit("    }\n");
        }
        
        emit("    printf(\"\\n\");\n");
        emit("    if (__test_fail == 0) {\n");
        emit("        printf(\"\\033[32m%%d tests passed\\033[0m\\n\", __test_pass);\n");
        emit("    } else {\n");
        emit("        printf(\"\\033[31m%%d passed, %%d failed\\033[0m\\n\", __test_pass, __test_fail);\n");
        emit("    }\n");
        emit("    return __test_fail > 0 ? 1 : 0;\n");
        emit("}\n");
    } else if (!has_main) {
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
                if (prog->stmts[i]->type == STMT_VAR) {
                    // Global var is declared at file scope; emit init here in order
                    Stmt* var_stmt = prog->stmts[i];
                    if (var_stmt->var.init) {
                        // Check if it was a simple init (already initialized at file scope)
                        bool is_simple = false;
                        if (var_stmt->var.init->type == EXPR_STRING ||
                            var_stmt->var.init->type == EXPR_FLOAT ||
                            var_stmt->var.init->type == EXPR_BOOL ||
                            var_stmt->var.init->type == EXPR_INT) {
                            is_simple = true;
                        }
                        if (!is_simple) {
                            // Prefix C-keyword names (registered at file-scope decl).
                            char _svn[512]; token_to_cstr(_svn, sizeof(_svn), var_stmt->var.name);
                            { extern int is_c_name_collision(const char*);
                              if (is_c_name_collision(_svn)) {
                                  memmove(_svn + WYN_UFN_PFX_LEN, _svn, strlen(_svn) + 1);
                                  memcpy(_svn, WYN_UFN_PFX, WYN_UFN_PFX_LEN);
                              } }
                            emit("    %s = ", _svn);
                            codegen_expr(var_stmt->var.init);
                            emit(";\n");
                        }
                    }
                } else if (prog->stmts[i]->type != STMT_FN && prog->stmts[i]->type != STMT_STRUCT && prog->stmts[i]->type != STMT_ENUM && prog->stmts[i]->type != STMT_TRAIT && prog->stmts[i]->type != STMT_IMPL && prog->stmts[i]->type != STMT_EXPORT) {
                    emit("    ");
                    codegen_stmt(prog->stmts[i]);
                }
            }
            emit("    return 0;\n");
        }
        emit("}\n");
    } else {
        // User defined main() is renamed to wyn_main during codegen
        // main() wrapper is in wyn_wrapper.c (compiled into runtime libraries)
    }
    current_fn_c_nonvoid = prev_fn_c_nonvoid;
}

// T1.4.4: Control Flow Code Generation - Control Flow Agent addition
void codegen_match_statement(Stmt* stmt) {
    if (!stmt || stmt->type != STMT_MATCH) return;
    
    // Check if this is a simple integer match that can use switch
    bool can_use_switch = true;
    bool has_wildcard = false;
    (void)has_wildcard;
    
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
        
        // Determine the type of the match value and get enum name
        bool is_enum_match = false;
        const char* match_enum_name = NULL;
        int match_enum_name_len = 0;
        
        // First check if match value is a known enum variable
        bool is_data_enum_match = false;
        if (stmt->match_stmt.value->type == EXPR_IDENT) {
            char _mv[128]; token_to_cstr(_mv, sizeof(_mv), stmt->match_stmt.value->token);
            extern const char* get_enum_var_type(const char*);
            const char* _et = get_enum_var_type(_mv);
            if (_et) {
                match_enum_name = _et; match_enum_name_len = strlen(_et); is_enum_match = true;
                extern int is_data_enum_type(const char*);
                if (is_data_enum_type(_et)) is_data_enum_match = true;
            }
        }
        
        if (!is_enum_match) {
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* match_case = &stmt->match_stmt.cases[i];
                if (match_case->pattern->type == PATTERN_OPTION && 
                    match_case->pattern->option.variant_name.length > 0) {
                    is_enum_match = true;
                    if (!match_enum_name && match_case->pattern->option.enum_name.length > 0) {
                        match_enum_name = match_case->pattern->option.enum_name.start;
                        match_enum_name_len = match_case->pattern->option.enum_name.length;
                    }
                    break;
                }
            }
        }
        // Check bare identifiers against known enum variants
        if (!is_enum_match) {
            extern const char* find_enum_for_variant(const char* variant);
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* match_case = &stmt->match_stmt.cases[i];
                if (match_case->pattern->type == PATTERN_IDENT) {
                    char _vn[128]; token_to_cstr(_vn, sizeof(_vn), match_case->pattern->ident.name);
                    const char* found = find_enum_for_variant(_vn);
                    if (found) {
                        is_enum_match = true;
                        match_enum_name = found;
                        match_enum_name_len = strlen(found);
                        break;
                    }
                }
            }
        }
        
        // A USER ENUM IS NEVER AN Option/Result, whether or not it carries
        // payloads. `Ok`, `Err`, `Some` and `None` are ordinary identifiers (the
        // lexer dropped TOKEN_OK/TOKEN_ERR precisely so they could be used as
        // names), so all four are legal - and natural - variant names:
        //
        //     enum Verdict { Ok, Over }
        //     match v { Verdict::Ok => ... }   // wyn check: no errors
        //     -> error: initializing 'ResultInt' with an expression of
        //        incompatible type 'Verdict'
        //
        // because the two flags below were decided from the arm's variant NAME
        // alone, never consulting the scrutinee, so the temp was declared
        // ResultInt and the arms tested `.tag`. The existing !is_data_enum_match
        // guard was this same bug found for PAYLOAD-CARRYING enums and fixed only
        // for them; a dataless enum still fell through. The bare-arm spelling
        // (`match v { Ok => .. }`) always worked, because a bare variant is a
        // PATTERN_IDENT rather than a PATTERN_OPTION with a variant_name - which
        // is why one spelling of the same match built and the other did not.
        //
        // Anything the scrutinee or the arms identify as a user enum is therefore
        // excluded here, not just the data-carrying subset.
        // The test is deliberately strict: the prefix must name an enum DECLARED IN
        // THIS PROGRAM, and that enum must actually declare the arm's variant.
        // `is_enum_type(prefix)` alone was not enough - a bare `Ok(v)` pattern
        // carries a prefix that satisfies it, so every genuine Result match took
        // the user-enum path and failed ("invalid operands to binary expression
        // ('ResultInt' and 'ResultInt (int)')"). Requiring the variant to belong to
        // a user-declared enum cannot be satisfied by the builtin families.
        bool arms_name_user_enum = false;
        for (int i = 0; i < stmt->match_stmt.case_count && !arms_name_user_enum; i++) {
            MatchCase* mc = &stmt->match_stmt.cases[i];
            if (!mc->pattern || mc->pattern->type != PATTERN_OPTION) continue;
            if (mc->pattern->option.enum_name.length == 0) continue;
            if (mc->pattern->option.variant_name.length == 0) continue;
            Token en = mc->pattern->option.enum_name;
            Token vn = mc->pattern->option.variant_name;
            for (int si = 0; si < current_program->count; si++) {
                Stmt* s = current_program->stmts[si];
                if (s->type == STMT_EXPORT && s->export.stmt) s = s->export.stmt;
                if (s->type != STMT_ENUM) continue;
                if (s->enum_decl.name.length != en.length ||
                    memcmp(s->enum_decl.name.start, en.start, en.length) != 0) continue;
                for (int vi = 0; vi < s->enum_decl.variant_count; vi++) {
                    if (s->enum_decl.variants[vi].length == vn.length &&
                        memcmp(s->enum_decl.variants[vi].start, vn.start, vn.length) == 0) {
                        arms_name_user_enum = true; break;
                    }
                }
                break;
            }
        }
        // NOTE: is_enum_match is deliberately NOT part of this. It is also set by
        // an arm-name fallback that resolves a bare `Ok(v)` to the builtin Result
        // enum, so including it made every GENUINE `match r { Ok(v) => .. }` take
        // the user-enum path and fail with "use of undeclared identifier 'Ok'".
        // Only an explicit `E::Ok` / `E.Ok` prefix naming a real enum is decisive,
        // and that is exactly the spelling that was broken.
        bool not_builtin_family = is_data_enum_match || arms_name_user_enum;

        // Check if this is a Result match (Ok/Err patterns).
        bool is_result_match = false;
        if (!not_builtin_family)
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* mc = &stmt->match_stmt.cases[i];
            if (mc->pattern->type == PATTERN_OPTION && mc->pattern->option.variant_name.length > 0) {
                if ((mc->pattern->option.variant_name.length == 2 && memcmp(mc->pattern->option.variant_name.start, "Ok", 2) == 0) ||
                    (mc->pattern->option.variant_name.length == 3 && memcmp(mc->pattern->option.variant_name.start, "Err", 3) == 0)) {
                    is_result_match = true;
                    break;
                }
            }
        }
        
        // Check if this is an Option match (Some/None patterns). Same guard as
        // above, and for the same reason: `enum Cache { None, Warm }` is a user
        // enum, not an Option.
        bool is_option_match = false;
        if (!not_builtin_family)
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* mc = &stmt->match_stmt.cases[i];
            if (mc->pattern->type == PATTERN_OPTION) {
                if (mc->pattern->option.is_some || 
                    (mc->pattern->option.variant_name.length == 4 && memcmp(mc->pattern->option.variant_name.start, "Some", 4) == 0) ||
                    (mc->pattern->option.variant_name.length == 4 && memcmp(mc->pattern->option.variant_name.start, "None", 4) == 0)) {
                    // Check it's not Ok/Err (which is Result, not Option)
                    if (!is_result_match) { is_option_match = true; break; }
                }
            }
        }
        
        // Check if this is a string match
        bool is_string_match = false;
        for (int i = 0; i < stmt->match_stmt.case_count; i++) {
            MatchCase* mc = &stmt->match_stmt.cases[i];
            if (mc->pattern->type == PATTERN_LITERAL && mc->pattern->literal.value.type == TOKEN_STRING) {
                is_string_match = true; break;
            }
        }
        
        if (is_string_match) {
            // Capture the id in a LOCAL: a nested match inside an arm body
            // re-enters this code and bumps the counter; reading the static
            // after that emitted references to the INNER match's (out-of-
            // scope) temp - an undeclared-variable ICE.
            static int _smid_ctr = 0; int _smid = ++_smid_ctr;
            emit("    const char* __match_str_%d = ", _smid);
            codegen_expr(stmt->match_stmt.value);
            emit(";\n");
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* mc = &stmt->match_stmt.cases[i];
                if (i > 0) emit(" else ");
                if (mc->pattern->type == PATTERN_LITERAL && mc->pattern->literal.value.type == TOKEN_STRING) {
                    emit("if (strcmp(__match_str_%d, %.*s) == 0) {\n", _smid,
                        mc->pattern->literal.value.length, mc->pattern->literal.value.start);
                } else if (mc->pattern->type == PATTERN_WILDCARD) {
                    emit("{\n");
                } else {
                    emit("if (1) {\n"); // fallback
                }
                emit("        ");
                if (mc->body) codegen_stmt(mc->body);
                emit("    }");
            }
            emit("\n");
        } else if (is_option_match) {
            // Local id - see the string-match comment above (nested match
            // inside an arm body must not clobber this match's temp name).
            static int _omid_ctr = 0; int _omid = ++_omid_ctr;
            // Resolve the concrete Option family from the checker-typed value, so
            // OptionFloat/OptionBool/OptionString lower with the right temp type
            // and payload binding (not the hardcoded OptionInt / long long).
            const char* _ofam = "OptionInt"; const char* _octy = "long long"; int _obind_str = 0;
            static char _ofam_buf[128]; static char _octy_buf[96];
            Type* _ovt = stmt->match_stmt.value ? stmt->match_stmt.value->expr_type : NULL;
            if (_ovt && _ovt->kind == TYPE_STRUCT && _ovt->struct_type.name.length > 0) {
                char _n[96]; token_to_cstr(_n, sizeof(_n), _ovt->struct_type.name);
                if (strcmp(_n, "OptionString") == 0) { _ofam = "OptionString"; _octy = "const char*"; _obind_str = 1; }
                else if (strcmp(_n, "OptionFloat") == 0) { _ofam = "OptionFloat"; _octy = "double"; }
                else if (strcmp(_n, "OptionBool") == 0) { _ofam = "OptionBool"; _octy = "bool"; }
                else if (strncmp(_n, "Option", 6) == 0 && strcmp(_n, "OptionInt") != 0) {
                    // Monomorphic Option<Name> (OptionUser, OptionShape): the payload is
                    // the struct value, bound by value. Ask THE authority for the payload
                    // C type rather than assuming it is the name minus "Option" — for a
                    // DATA-carrying enum the family is Option<Enum> while the payload C
                    // type is the enum's own struct typedef, and for a PLAIN enum the
                    // family collapses to OptionInt (so this branch is not even reached).
                    extern const char* wyn_option_family(const char*, const char**, int*);
                    const char* _pcty = NULL;
                    const char* _pfam = wyn_option_family(_n + 6, &_pcty, NULL);
                    snprintf(_ofam_buf, sizeof(_ofam_buf), "%s", _pfam); _ofam = _ofam_buf;
                    snprintf(_octy_buf, sizeof(_octy_buf), "%s", _pcty ? _pcty : _n + 6);
                    _octy = _octy_buf;
                }
            }
            emit("    %s __match_opt_%d = ", _ofam, _omid);
            codegen_expr(stmt->match_stmt.value);
            emit(";\n");
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* mc = &stmt->match_stmt.cases[i];
                if (i > 0) emit(" else ");
                if (mc->pattern->type == PATTERN_OPTION && mc->pattern->option.is_some) {
                    emit("if (__match_opt_%d.tag == 1) {\n", _omid);
                    if (mc->pattern->option.inner) {
                        emit("        %s %.*s = __match_opt_%d.value;\n", _octy,
                            mc->pattern->option.inner->ident.name.length,
                            mc->pattern->option.inner->ident.name.start, _omid);
                        if (_obind_str) { char _sv[256]; token_to_cstr(_sv, sizeof(_sv), mc->pattern->option.inner->ident.name);
                            extern void register_string_var(const char*); register_string_var(_sv); }
                        // If the payload is a DATA-carrying enum, record the binder's enum
                        // type for the arm body. A nested `match v { Circle(r) => ... }`
                        // resolves the matched value's enum via get_enum_var_type(); without
                        // this the binder is unknown, so the inner match is misclassified as
                        // an OPTION match (the parser marks any data-carrying variant pattern
                        // with option.is_some) and emitted `OptionInt __match_opt_2 = v;`
                        // against a plain Shape. Mirrors the string-var registration above.
                        {
                            extern int is_data_enum_type(const char*);
                            extern void register_enum_var(const char*, const char*);
                            if (is_data_enum_type(_octy)) {
                                char _ev[256]; token_to_cstr(_ev, sizeof(_ev), mc->pattern->option.inner->ident.name);
                                register_enum_var(_ev, _octy);
                            }
                        }
                    }
                    emit("        ");
                    if (mc->body) codegen_stmt(mc->body);
                    // Borrowed arm binder - unregister so the enclosing block's
                    // release pass doesn't free an out-of-scope name (see the Result
                    // arm below for the full rationale). (2026-07)
                    if (mc->pattern->option.inner &&
                        mc->pattern->option.inner->type == PATTERN_IDENT) {
                        char _ub[256]; token_to_cstr(_ub, sizeof(_ub), mc->pattern->option.inner->ident.name);
                        extern void unregister_string_var(const char*); unregister_string_var(_ub);
                        // Same scoping rule for the enum-var record: the binder does not
                        // exist outside this arm, so leaving it registered would make a
                        // later same-named variable of a different type resolve to this
                        // enum. (Dispatch-table leaks of exactly this kind were found by
                        // dogfooding WynJS -- see the str_array/int_array var-name leaks.)
                        extern void unregister_enum_var(const char*);
                        unregister_enum_var(_ub);
                    }
                    emit("    }");
                } else if (mc->pattern->type == PATTERN_OPTION && !mc->pattern->option.is_some) {
                    emit("if (__match_opt_%d.tag == 0) {\n", _omid);
                    emit("        ");
                    if (mc->body) codegen_stmt(mc->body);
                    emit("    }");
                } else if (mc->pattern->type == PATTERN_WILDCARD) {
                    emit("{\n        ");
                    if (mc->body) codegen_stmt(mc->body);
                    emit("    }");
                } else {
                    // Defensive: an arm pattern this lowering doesn't handle
                    // (e.g. a stray ident binding) must still emit a block -
                    // a bare ` else ` with no body is a C syntax error.
                    emit("{\n        ");
                    if (mc->body) codegen_stmt(mc->body);
                    emit("    }");
                }
            }
            emit("\n");
        } else if (is_result_match) {
            // Local id - see the string-match comment above.
            static int _rmid_ctr = 0; int _rmid = ++_rmid_ctr;
            // Resolve the concrete Result family from the checker-typed value, so
            // ResultFloat/ResultBool/ResultString lower with the right temp type
            // and Ok-payload binding (Err is always a string).
            const char* _rfam = "ResultInt"; const char* _rcty = "long long"; int _rok_str = 0;
            // Err payload C type/string-ness for the Err arm binding (defaults to
            // the #181 string err; overridden from the family registry for structs).
            const char* _rerr_cty = "const char*"; int _rerr_str = 1;
            static char _rfam_buf[192]; static char _rcty_buf[96];
            Type* _rvt = stmt->match_stmt.value ? stmt->match_stmt.value->expr_type : NULL;
            if (_rvt && _rvt->kind == TYPE_STRUCT && _rvt->struct_type.name.length > 0) {
                char _n[96]; token_to_cstr(_n, sizeof(_n), _rvt->struct_type.name);
                if (strcmp(_n, "ResultString") == 0) { _rfam = "ResultString"; _rcty = "const char*"; _rok_str = 1; }
                else if (strcmp(_n, "ResultFloat") == 0) { _rfam = "ResultFloat"; _rcty = "double"; }
                else if (strcmp(_n, "ResultBool") == 0) { _rfam = "ResultBool"; _rcty = "bool"; }
                else if (strncmp(_n, "Result", 6) == 0 && strcmp(_n, "ResultInt") != 0) {
                    // Monomorphic Result<Struct,E> (ResultPoint / ResultPoint_Fail):
                    // Ok payload is the struct, bound by value; the Err payload C type
                    // comes from the family registry (string/scalar/struct).
                    snprintf(_rfam_buf, sizeof(_rfam_buf), "%s", _n); _rfam = _rfam_buf;
                    extern int result_family_lookup(const char*, const char**, const char**, int*);
                    const char* _ro = NULL; const char* _re = NULL; int _reis = 1;
                    if (result_family_lookup(_n, &_ro, &_re, &_reis) && _ro) {
                        snprintf(_rcty_buf, sizeof(_rcty_buf), "%s", _ro); _rcty = _rcty_buf;
                        _rerr_cty = _re; _rerr_str = _reis;
                    } else {
                        snprintf(_rcty_buf, sizeof(_rcty_buf), "%s", _n + 6); _rcty = _rcty_buf;
                    }
                }
            }
            emit("    %s __match_val_%d = ", _rfam, _rmid);
            codegen_expr(stmt->match_stmt.value);
            emit(";\n");
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                MatchCase* mc = &stmt->match_stmt.cases[i];
                if (i > 0) emit(" else ");
                if (mc->pattern->type == PATTERN_OPTION) {
                    bool is_ok = (mc->pattern->option.variant_name.length == 2 && memcmp(mc->pattern->option.variant_name.start, "Ok", 2) == 0);
                    emit("if (__match_val_%d.tag == %d) {\n", _rmid, is_ok ? 0 : 1);
                    if (mc->pattern->option.inner) {
                        if (is_ok) {
                            emit("        %s %.*s = __match_val_%d.data.ok_value;\n", _rcty,
                                mc->pattern->option.inner->ident.name.length,
                                mc->pattern->option.inner->ident.name.start, _rmid);
                            if (_rok_str) { char _sv[256]; token_to_cstr(_sv, sizeof(_sv), mc->pattern->option.inner->ident.name);
                                extern void register_string_var(const char*); register_string_var(_sv); }
                        } else {
                            emit("        %s %.*s = __match_val_%d.data.err_value;\n", _rerr_cty,
                                mc->pattern->option.inner->ident.name.length,
                                mc->pattern->option.inner->ident.name.start, _rmid);
                            // If the Err payload is a string, register the binding so
                            // the arm body treats it as one (else `"..." + e` defaults
                            // to int and emits int_to_string(e) -> garbage). A struct/
                            // scalar err is NOT a string var.
                            if (_rerr_str) { char _ev[256]; token_to_cstr(_ev, sizeof(_ev), mc->pattern->option.inner->ident.name);
                              extern void register_string_var(const char*); register_string_var(_ev); }
                        }
                    }
                    emit("        ");
                    if (mc->body) codegen_stmt(mc->body);
                    // The payload binder is BORROWED from the Result (points into
                    // __match_val_%d.data), scoped to this arm, and must NOT be
                    // released here. Unregister it so the enclosing block's string-
                    // release pass doesn't emit `wyn_rc_release(<binder>)` AFTER the
                    // arm's closing brace - which referenced an out-of-scope name
                    // ("use of undeclared identifier 'e'") when the match sat inside
                    // a loop or other releasing block. (2026-07)
                    if (mc->pattern->option.inner &&
                        mc->pattern->option.inner->type == PATTERN_IDENT) {
                        char _ub[256]; token_to_cstr(_ub, sizeof(_ub), mc->pattern->option.inner->ident.name);
                        extern void unregister_string_var(const char*); unregister_string_var(_ub);
                    }
                    emit("    }");
                } else if (mc->pattern->type == PATTERN_WILDCARD) {
                    emit("{\n        ");
                    if (mc->body) codegen_stmt(mc->body);
                    emit("    }");
                }
            }
            emit("\n");
        } else {
        emit("{\n");
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
                        // Prefixed variant with no payload binder: Color.Red / Msg.Ping.
                        // For a DATA enum it's still a tagged union → compare .tag
                        // (comparing the struct value with `==` is a C type error).
                        if (is_data_enum_match) {
                            emit("__match_val.tag == %.*s_%.*s_TAG",
                                 match_case->pattern->option.enum_name.length,
                                 match_case->pattern->option.enum_name.start,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else {
                            emit("__match_val == %.*s_%.*s",
                                 match_case->pattern->option.enum_name.length,
                                 match_case->pattern->option.enum_name.start,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        }
                    } else if (match_case->pattern->option.is_some &&
                               match_case->pattern->option.enum_name.length > 0) {
                        // Variant with data pattern: Opt.Yep(v)
                        if (is_data_enum_match) {
                            emit("__match_val.tag == %.*s_%.*s_TAG",
                                 match_case->pattern->option.enum_name.length,
                                 match_case->pattern->option.enum_name.start,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else {
                            emit("__match_val == %.*s_%.*s",
                                 match_case->pattern->option.enum_name.length,
                                 match_case->pattern->option.enum_name.start,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        }
                    } else if (match_case->pattern->option.is_some) {
                        // Bare variant with data: Yep(v)
                        if (is_data_enum_match && match_enum_name) {
                            emit("__match_val.tag == %.*s_%.*s_TAG",
                                 match_enum_name_len, match_enum_name,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else if (match_enum_name) {
                            emit("__match_val == %.*s_%.*s",
                                 match_enum_name_len, match_enum_name,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else {
                            emit("__match_val == %.*s",
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        }
                    } else if (match_case->pattern->option.enum_name.length > 0) {
                        // Only variant name with no data: Nope
                        if (is_data_enum_match && match_enum_name) {
                            emit("__match_val.tag == %.*s_%.*s_TAG",
                                 match_enum_name_len, match_enum_name,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else if (match_enum_name) {
                            emit("__match_val == %.*s_%.*s",
                                 match_enum_name_len, match_enum_name,
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        } else {
                            emit("__match_val == %.*s",
                                 match_case->pattern->option.variant_name.length,
                                 match_case->pattern->option.variant_name.start);
                        }
                    }
                } else if (match_case->pattern->option.is_some) {
                    if (is_data_enum_match && match_enum_name) {
                        emit("__match_val.tag == %.*s_Some_TAG", match_enum_name_len, match_enum_name);
                    } else {
                        emit("wyn_optional_is_some(__match_val)");
                    }
                } else {
                    if (is_data_enum_match && match_enum_name) {
                        emit("__match_val.tag == %.*s_None_TAG", match_enum_name_len, match_enum_name);
                    } else {
                        emit("wyn_optional_is_none(__match_val)");
                    }
                }
            } else if (match_case->pattern->type == PATTERN_IDENT) {
                // Check if this looks like an enum variant
                Token var_name = match_case->pattern->ident.name;
                bool is_enum_variant = false;
                for (int j = 0; j < var_name.length; j++) {
                    if (var_name.start[j] == '_') {
                        is_enum_variant = true;
                        break;
                    }
                }
                // Also treat as enum variant if we know the match is on an enum
                if (!is_enum_variant && match_enum_name) {
                    is_enum_variant = true;
                }
                if (is_enum_variant) {
                    if (is_data_enum_match && match_enum_name) {
                        emit("__match_val.tag == %.*s_%.*s_TAG",
                             match_enum_name_len, match_enum_name,
                             var_name.length, var_name.start);
                    } else if (match_enum_name) {
                        emit("__match_val == %.*s_%.*s",
                             match_enum_name_len, match_enum_name,
                             var_name.length, var_name.start);
                    } else {
                        emit("__match_val == %.*s", var_name.length, var_name.start);
                    }
                } else {
                    emit("1"); // Variable binding always matches
                }
            } else if (match_case->pattern->type == PATTERN_RANGE) {
                // Range pattern in STATEMENT position - the expression form
                // always supported this; the statement form emitted `0`
                // (arm silently never matched).
                emit("(__match_val >= ");
                codegen_expr(match_case->pattern->range.start);
                emit(" && __match_val %s ", match_case->pattern->range.inclusive ? "<=" : "<");
                codegen_expr(match_case->pattern->range.end);
                emit(")");
            } else if (match_case->pattern->type == PATTERN_OR) {
                // Or pattern in STATEMENT position: 1 | 3 | 5. Was emitted as
                // constant 0 - `match n { 1 | 3 | 5 => ... }` NEVER matched.
                emit("(");
                for (int oi = 0; oi < match_case->pattern->or_pat.pattern_count; oi++) {
                    Pattern* sub = match_case->pattern->or_pat.patterns[oi];
                    if (oi > 0) emit(" || ");
                    if (sub->type == PATTERN_LITERAL) {
                        if (sub->literal.value.start[0] == '"') {
                            emit("strcmp(__match_val, %.*s) == 0",
                                 sub->literal.value.length, sub->literal.value.start);
                        } else {
                            emit("__match_val == %.*s",
                                 sub->literal.value.length, sub->literal.value.start);
                        }
                    } else if (sub->type == PATTERN_RANGE) {
                        emit("(__match_val >= ");
                        codegen_expr(sub->range.start);
                        emit(" && __match_val %s ", sub->range.inclusive ? "<=" : "<");
                        codegen_expr(sub->range.end);
                        emit(")");
                    } else {
                        emit("0");
                    }
                }
                emit(")");
            } else {
                emit("0"); // Unsupported pattern
            }
            
            // Add guard clause if present. A guard on a binding pattern
            // (`x if x > 2 =>`) references the binding INSIDE the condition,
            // but the binding declaration is emitted after it - so wrap the
            // guard in a statement-expression that declares the binding
            // locally. (Was emitted bare: C error "use of undeclared x".)
            if (match_case->guard) {
                bool _guard_binds = false;
                Token _bind_name = {0};
                if (match_case->pattern->type == PATTERN_IDENT) {
                    Token vn = match_case->pattern->ident.name;
                    bool _is_variant = false;
                    for (int j = 0; j < vn.length; j++)
                        if (vn.start[j] == '_') { _is_variant = true; break; }
                    if (!_is_variant && match_enum_name) _is_variant = true;
                    if (!_is_variant) { _guard_binds = true; _bind_name = vn; }
                }
                if (_guard_binds) {
                    emit(" && ({ long long %.*s = __match_val; (void)%.*s; (bool)(",
                         _bind_name.length, _bind_name.start,
                         _bind_name.length, _bind_name.start);
                    codegen_expr(match_case->guard);
                    emit("); })");
                } else {
                    emit(" && (");
                    codegen_expr(match_case->guard);
                    emit(")");
                }
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
                if (!is_enum_variant && match_enum_name) is_enum_variant = true;
                if (!is_enum_variant) {
                    emit("        int %.*s = __match_val;\n", var_name.length, var_name.start);
                }
            } else if (is_data_enum_match &&
                       match_case->pattern->type == PATTERN_OPTION &&
                       match_case->pattern->option.inner_count > 1 &&
                       match_case->pattern->option.inners) {
                // Multi-field variant: Rect(w, h) → bind each payload field with
                // its real type (.data.Rect_value.f0, .f1). Mirrors the working
                // expression-form match (codegen_expr EXPR_MATCH).
                const char* vn = match_case->pattern->option.variant_name.start;
                int vn_len = match_case->pattern->option.variant_name.length;
                char _en[128];
                if (match_case->pattern->option.enum_name.length > 0)
                    token_to_cstr(_en, sizeof(_en), match_case->pattern->option.enum_name);
                else { snprintf(_en, sizeof(_en), "%.*s", match_enum_name_len, match_enum_name ? match_enum_name : ""); }
                char _vn[128]; snprintf(_vn, sizeof(_vn), "%.*s", vn_len, vn);
                extern const char* get_enum_variant_field_type(const char*, const char*, int);
                extern int is_enum_field_boxed(const char*, const char*, int);
                for (int pi = 0; pi < match_case->pattern->option.inner_count; pi++) {
                    Pattern* ip = match_case->pattern->option.inners[pi];
                    if (!ip || ip->type != PATTERN_IDENT) continue;
                    const char* fty = get_enum_variant_field_type(_en, _vn, pi);
                    if (!fty) fty = "long long";
                    int _bx = is_enum_field_boxed(_en, _vn, pi);
                    emit("        %s %.*s = %s__match_val.data.%s_value.f%d;\n",
                         fty, ip->ident.name.length, ip->ident.name.start, _bx ? "*" : "", _vn, pi);
                    if (strcmp(fty, "const char*") == 0 || strcmp(fty, "char*") == 0) {
                        char _fb[128]; token_to_cstr(_fb, sizeof(_fb), ip->ident.name);
                        extern void register_string_var(const char*);
                        register_string_var(_fb);
                    }
                }
            } else if (match_case->pattern->type == PATTERN_OPTION &&
                       match_case->pattern->option.inner &&
                       match_case->pattern->option.inner->type == PATTERN_IDENT) {

                Token var_name = match_case->pattern->option.inner->ident.name;
                if (is_data_enum_match) {
                    // Extract data from tagged union
                    const char* vn = match_case->pattern->option.variant_name.start;
                    int vn_len = match_case->pattern->option.variant_name.length;
                    if (vn_len == 0 && match_case->pattern->option.is_some) { vn = "Some"; vn_len = 4; }
                    extern int is_enum_field_boxed(const char*, const char*, int);
                    char _sen[128], _svn[128];
                    if (match_case->pattern->option.enum_name.length > 0)
                        token_to_cstr(_sen, sizeof(_sen), match_case->pattern->option.enum_name);
                    else snprintf(_sen, sizeof(_sen), "%.*s", match_enum_name_len, match_enum_name ? match_enum_name : "");
                    snprintf(_svn, sizeof(_svn), "%.*s", vn_len, vn);
                    int _bx = is_enum_field_boxed(_sen, _svn, 0);
                    emit("        __auto_type %.*s = %s__match_val.data.%.*s_value;\n",
                         var_name.length, var_name.start, _bx ? "*" : "", vn_len, vn);
                } else {
                    // Simple enum - no data to extract
                    emit("        int %.*s = 0; (void)%.*s;\n",
                         var_name.length, var_name.start,
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
}
