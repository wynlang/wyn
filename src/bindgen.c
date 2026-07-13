// C-header binding generator. See bindgen.h for the contract.
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "bindgen.h"

// ---- small string helpers -------------------------------------------------

static char* bg_trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

// Does `s` contain any character that makes it un-representable as a simple C
// declaration we handle? (pointers-to-pointers, arrays, function pointers, etc.)
static int has_char(const char* s, char c) { return strchr(s, c) != NULL; }

// ---- C type -> Wyn type ---------------------------------------------------

// Map a (trimmed) C type spelling to a Wyn FFI type keyword, or NULL if the
// type is outside the representable subset. `is_return` allows `void`.
// Pointer types (single `*`) collapse to `ptr`, except char*/const char* which
// map to Wyn `string`. Width/signedness of integer types collapses to `int`
// (all pass as machine words on LP64); floating types map to `float` (double).
static const char* map_c_type(const char* c, int is_return) {
    char t[256];
    strncpy(t, c, sizeof(t) - 1);
    t[sizeof(t) - 1] = '\0';
    char* s = bg_trim(t);
    if (!*s) return NULL;

    // Strip leading storage-class / qualifier keywords, repeatedly, so
    // "extern const unsigned int" -> "int", "static inline double" -> "double",
    // "struct Foo *" keeps the *. `unsigned`/`signed` also just collapse to the
    // base integer type.
    for (;;) {
        if      (strncmp(s, "extern ", 7) == 0)   s = bg_trim(s + 7);
        else if (strncmp(s, "static ", 7) == 0)   s = bg_trim(s + 7);
        else if (strncmp(s, "inline ", 7) == 0)   s = bg_trim(s + 7);
        else if (strncmp(s, "const ", 6) == 0)    s = bg_trim(s + 6);
        else if (strncmp(s, "volatile ", 9) == 0) s = bg_trim(s + 9);
        else if (strncmp(s, "register ", 9) == 0) s = bg_trim(s + 9);
        else if (strncmp(s, "unsigned ", 9) == 0) s = bg_trim(s + 9);
        else if (strncmp(s, "signed ", 7) == 0)   s = bg_trim(s + 7);
        else if (strncmp(s, "struct ", 7) == 0)   s = bg_trim(s + 7);
        else if (strncmp(s, "enum ", 5) == 0)     s = bg_trim(s + 5);
        else break;
    }

    // Pointer? (exactly one trailing '*', no '(' -> not a function pointer)
    int stars = 0;
    for (const char* p = s; *p; p++) if (*p == '*') stars++;
    if (stars >= 1) {
        if (has_char(s, '(') || has_char(s, '[')) return NULL; // fn-ptr / array
        // char* / const char* (const already stripped) -> Wyn string
        // Compare the base spelling with the stars removed.
        char base[256]; int bi = 0;
        for (const char* p = s; *p && bi < 255; p++) if (*p != '*' && !isspace((unsigned char)*p)) base[bi++] = *p;
        base[bi] = '\0';
        if (strcmp(base, "char") == 0 && stars == 1) return "string";
        return "ptr"; // any other single/multi pointer -> opaque
    }

    // Bare scalar spellings.
    if (strcmp(s, "void") == 0) return is_return ? "void" : NULL;
    if (strcmp(s, "bool") == 0 || strcmp(s, "_Bool") == 0) return "bool";
    if (strcmp(s, "float") == 0 || strcmp(s, "double") == 0 || strcmp(s, "long double") == 0)
        return "float";
    // Integer family (all widths/signedness collapse to Wyn int).
    if (strcmp(s, "int") == 0 || strcmp(s, "char") == 0 || strcmp(s, "short") == 0 ||
        strcmp(s, "long") == 0 || strcmp(s, "long long") == 0 ||
        strcmp(s, "size_t") == 0 || strcmp(s, "ssize_t") == 0 || strcmp(s, "intptr_t") == 0 ||
        strcmp(s, "uintptr_t") == 0 || strcmp(s, "int8_t") == 0 || strcmp(s, "uint8_t") == 0 ||
        strcmp(s, "int16_t") == 0 || strcmp(s, "uint16_t") == 0 || strcmp(s, "int32_t") == 0 ||
        strcmp(s, "uint32_t") == 0 || strcmp(s, "int64_t") == 0 || strcmp(s, "uint64_t") == 0)
        return "int";
    return NULL; // struct-by-value, unknown typedef, etc. — not representable
}

// ---- preprocess ------------------------------------------------------------

// Portable temp directory: honor TMPDIR/TMP/TEMP (Windows sets TMP/TEMP; `/tmp`
// does not exist there), falling back to "." (the cwd always exists).
static const char* temp_dir(void) {
    const char* d = getenv("TMPDIR");
    if (!d || !*d) d = getenv("TMP");
    if (!d || !*d) d = getenv("TEMP");
    if (!d || !*d) d = ".";
    return d;
}

// Run `<cc> -E <iflags> <header>` into a temp file; return the temp path
// (caller frees + unlinks) or NULL on failure. system() runs the platform shell
// (cmd.exe on Windows), so avoid shell-specific redirects: quote with '"', use
// the null device per-OS, and put the temp file in a real temp dir (not /tmp,
// which is absent on Windows).
static char* preprocess(const char* header, const char* cc, const char* iflags) {
    static int counter = 0;
    char* tmp = malloc(1024);
    snprintf(tmp, 1024, "%s/wyn_bind_%d.i", temp_dir(), counter++);
#ifdef _WIN32
    const char* devnull = "NUL";
#else
    const char* devnull = "/dev/null";
#endif
    // A system header (e.g. "math.h", "curl/curl.h") isn't a file in the cwd —
    // `cc -E math.h` would look for ./math.h and fail. If `header` isn't an
    // existing path, preprocess a generated stub `#include <header>` instead, so
    // the compiler resolves it on its system include path.
    char stub[1024] = "";
    const char* to_pp = header;
    if (access(header, R_OK) != 0) {
        snprintf(stub, sizeof(stub), "%s/wyn_bind_stub_%d.h", temp_dir(), counter++);
        FILE* sf = fopen(stub, "w");
        if (sf) { fprintf(sf, "#include <%s>\n", header); fclose(sf); to_pp = stub; }
    }
    char cmd[3072];
    snprintf(cmd, sizeof(cmd), "%s -E %s \"%s\" > \"%s\" 2>%s",
             cc, iflags ? iflags : "", to_pp, tmp, devnull);
    if (system(cmd) != 0) { free(tmp); return NULL; }
    return tmp;
}

// ---- #define constant emission --------------------------------------------

// From the `-dM` dump, emit Wyn `const NAME = value` for object-like macros
// whose body is a single integer/float/string literal. Skips function-like
// macros (`NAME(`), and anything non-literal.
static void emit_defines(FILE* dump, FILE* out, int* emitted_any) {
    char line[1024];
    while (fgets(line, sizeof(line), dump)) {
        char* s = bg_trim(line);
        if (strncmp(s, "#define ", 8) != 0) continue;
        s += 8;
        // name
        char* name = s;
        char* p = s;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
        if (p == name) continue;
        if (*p == '(') continue;             // function-like macro — skip
        char saved = *p; *p = '\0';
        // Skip compiler builtins (leading underscore double) and empty bodies.
        int builtin = (name[0] == '_' && name[1] == '_');
        char nm[128]; strncpy(nm, name, sizeof(nm) - 1); nm[sizeof(nm) - 1] = '\0';
        *p = saved;
        char* body = bg_trim(p);
        if (builtin || !*body) continue;
        // Accept a single int/float literal or a "double-quoted string".
        int ok = 0;
        if (body[0] == '"' && body[strlen(body) - 1] == '"') ok = 1;
        else {
            char* endp = NULL;
            strtod(body, &endp);
            if (endp && *bg_trim(endp) == '\0') ok = 1; // fully numeric
        }
        if (!ok) continue;
        fprintf(out, "const %s = %s\n", nm, body);
        *emitted_any = 1;
    }
}

// ---- declaration scanning --------------------------------------------------

// Emit `extern fn` for a single function prototype `<ret> <name>(<params>)`.
// `decl` is the text between statement boundaries, already known to contain a
// top-level `(`...`)` and end at `;`. Returns 1 if emitted, 0 if skipped.
static int emit_prototype(char* decl, FILE* out) {
    // Split at the first '(' — everything before is "<ret-and-quals> name".
    char* lp = strchr(decl, '(');
    if (!lp) return 0;
    char* rp = strrchr(decl, ')');
    if (!rp || rp < lp) return 0;

    // Header = ret type + name; params = between lp+1 and rp.
    *lp = '\0';
    char* head = bg_trim(decl);
    char params[1024];
    { size_t n = (size_t)(rp - (lp + 1)); if (n >= sizeof(params)) n = sizeof(params) - 1;
      memcpy(params, lp + 1, n); params[n] = '\0'; }

    // The function name is the last identifier token in `head`; the rest is ret.
    char* ne = head + strlen(head);
    while (ne > head && isspace((unsigned char)ne[-1])) ne--;
    char* ns = ne;
    while (ns > head && (isalnum((unsigned char)ns[-1]) || ns[-1] == '_')) ns--;
    if (ns == ne) return 0;
    // A '*' immediately before the name belongs to the return type (e.g. `char *foo`).
    char name[128]; { size_t n = (size_t)(ne - ns); if (n >= sizeof(name)) n = sizeof(name)-1; memcpy(name, ns, n); name[n] = '\0'; }
    char rettype[256]; { size_t n = (size_t)(ns - head); if (n >= sizeof(rettype)) n = sizeof(rettype)-1; memcpy(rettype, head, n); rettype[n] = '\0'; }
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return 0;
    // Skip compiler/library internal symbols (`__foo`, `_Foo`) — they're not part
    // of a library's public API and clutter the output with TODOs.
    if (name[0] == '_') return 0;

    const char* wyn_ret = map_c_type(rettype, 1);
    if (!wyn_ret) { fprintf(out, "// TODO: %s — unsupported return type '%s'\n", name, bg_trim(rettype)); return 0; }

    // Parse params: comma-split at top level (params here have no nested parens
    // in the representable subset; a '(' means a function-pointer param -> skip).
    char wyn_params[2048]; wyn_params[0] = '\0';
    int variadic = 0, pidx = 0, skip = 0;
    char* pstate = NULL;
    char* ptrim = bg_trim(params);
    if (strcmp(ptrim, "void") == 0 || *ptrim == '\0') {
        // no params
    } else if (has_char(ptrim, '(')) {
        fprintf(out, "// TODO: %s — function-pointer parameter not supported\n", name);
        return 0;
    } else {
        for (char* tok = strtok_r(ptrim, ",", &pstate); tok; tok = strtok_r(NULL, ",", &pstate)) {
            char* pt = bg_trim(tok);
            if (strcmp(pt, "...") == 0) { variadic = 1; continue; }
            // A param may be "<type> <name>" or just "<type>"; strip a trailing
            // identifier that is the param name (heuristic: if the last token is
            // a bare identifier AND there's a preceding type token).
            // Simplest robust approach: map the whole spelling, but first drop a
            // trailing identifier if the remainder still maps.
            const char* wt = map_c_type(pt, 0);
            if (!wt) {
                // try dropping a trailing param-name identifier
                char* e = pt + strlen(pt);
                while (e > pt && isspace((unsigned char)e[-1])) e--;
                char* s2 = e;
                while (s2 > pt && (isalnum((unsigned char)s2[-1]) || s2[-1] == '_')) s2--;
                if (s2 > pt && s2 != e) {
                    char just_type[256]; size_t n = (size_t)(s2 - pt);
                    if (n >= sizeof(just_type)) n = sizeof(just_type) - 1;
                    memcpy(just_type, pt, n); just_type[n] = '\0';
                    wt = map_c_type(just_type, 0);
                }
            }
            if (!wt) { skip = 1; break; }
            char one[128];
            snprintf(one, sizeof(one), "%sa%d: %s", pidx ? ", " : "", pidx, wt);
            strncat(wyn_params, one, sizeof(wyn_params) - strlen(wyn_params) - 1);
            pidx++;
        }
    }
    if (skip) { fprintf(out, "// TODO: %s — unsupported parameter type\n", name); return 0; }
    if (variadic) {
        char one[16]; snprintf(one, sizeof(one), "%s...", pidx ? ", " : "");
        strncat(wyn_params, one, sizeof(wyn_params) - strlen(wyn_params) - 1);
    }

    if (strcmp(wyn_ret, "void") == 0)
        fprintf(out, "extern fn %s(%s);\n", name, wyn_params);
    else
        fprintf(out, "extern fn %s(%s) -> %s;\n", name, wyn_params, wyn_ret);
    return 1;
}

int wyn_bindgen(const char* header_path, const char* cc, const char* extra_iflags, FILE* out) {
    char* pp = preprocess(header_path, cc, extra_iflags);
    if (!pp) {
        fprintf(stderr, "Error: could not preprocess '%s' (is the header path/-I correct?)\n", header_path);
        return 1;
    }
    FILE* f = fopen(pp, "r");
    if (!f) { unlink(pp); free(pp); return 1; }

    // Base name of the target header, to filter `# line "file"` markers to it.
    const char* base = strrchr(header_path, '/');
    base = base ? base + 1 : header_path;

    fprintf(out, "// Generated by `wyn bind %s` — review before use.\n", header_path);
    fprintf(out, "// Only functions/structs/#defines representable by Wyn's FFI type map are\n");
    fprintf(out, "// emitted; anything skipped is marked with a `// TODO:` note.\n\n");

    char line[4096];
    char decl[4096]; decl[0] = '\0';
    int in_target = 0;      // are we inside a region from the target header?
    int fn_count = 0, emitted_any = 0;

    while (fgets(line, sizeof(line), f)) {
        // Line marker: `# <num> "file" ...` — track whether "file" is our header.
        if (line[0] == '#') {
            char* q = strchr(line, '"');
            if (q) {
                char* q2 = strchr(q + 1, '"');
                if (q2) {
                    *q2 = '\0';
                    in_target = (strstr(q + 1, base) != NULL);
                }
            }
            decl[0] = '\0';
            continue;
        }
        if (!in_target) continue;

        // Accumulate until a ';' or '}' terminates a top-level declaration.
        strncat(decl, line, sizeof(decl) - strlen(decl) - 1);

        char* semi = strchr(decl, ';');
        if (!semi) {
            // A struct/enum/union body spans braces — if we see a '{' with no
            // matching handling, skip to its ';' (structs handled in a later pass).
            if (strlen(decl) > 3000) decl[0] = '\0'; // runaway guard
            continue;
        }
        *semi = '\0';
        char* d = bg_trim(decl);

        // Only handle plain function prototypes in this first bindgen: contains a
        // top-level '(' and does NOT start a struct/enum/typedef/union body.
        if (*d && strchr(d, '(') &&
            strncmp(d, "typedef", 7) != 0 && strncmp(d, "struct", 6) != 0 &&
            strncmp(d, "enum", 4) != 0 && strncmp(d, "union", 5) != 0 &&
            !has_char(d, '{') && !has_char(d, '=')) {
            char work[4096]; strncpy(work, d, sizeof(work) - 1); work[sizeof(work) - 1] = '\0';
            if (emit_prototype(work, out)) fn_count++;
            emitted_any = 1;
        }
        // Advance past the processed declaration.
        memmove(decl, semi + 1, strlen(semi + 1) + 1);
    }
    fclose(f);
    unlink(pp); free(pp);

    // Second pass: object-like #define constants. Read the ORIGINAL header
    // directly (not `-dM`, which dumps every compiler/system builtin macro and
    // has no way to attribute a macro to a file). This naturally scopes the
    // emitted constants to what the header itself declares.
    FILE* hf = fopen(header_path, "r");
    if (hf) { fprintf(out, "\n"); emit_defines(hf, out, &emitted_any); fclose(hf); }

    if (!emitted_any)
        fprintf(out, "// (no representable declarations found in %s)\n", base);
    fprintf(stderr, "Generated %d extern fn declaration(s) from %s\n", fn_count, base);
    return 0;
}
