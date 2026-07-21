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

// ---- typedef resolution -----------------------------------------------------
// Real headers name almost everything through typedefs (png_uint_32,
// png_structp, uv_file, …). During the scan we record every simple typedef -
// from ALL files in the preprocessed unit, since a library's typedefs usually
// live in a sub-header (pngconf.h) rather than the umbrella target header -
// and map_c_type resolves unknown spellings through this table.
#define TD_CAP 4096
static char td_names[TD_CAP][64];
static const char* td_types[TD_CAP];   // interned "int"/"float"/"bool"/"string"/"ptr"
static int td_count = 0;

static const char* td_lookup(const char* name) {
    for (int i = td_count - 1; i >= 0; i--)   // later defs win
        if (strcmp(td_names[i], name) == 0) return td_types[i];
    return NULL;
}

static void td_add(const char* name, const char* wyn_type) {
    if (td_count >= TD_CAP || !wyn_type) return;
    if (strlen(name) >= sizeof(td_names[0])) return;
    strcpy(td_names[td_count], name);
    td_types[td_count] = wyn_type;
    td_count++;
}

// Map a (trimmed) C type spelling to a Wyn FFI type keyword, or NULL if the
// type is outside the representable subset. `is_return` allows `void`.
// Pointer types (single `*`) collapse to `ptr`, except char*/const char* which
// map to Wyn `string`. Width/signedness of integer types collapses to `int`
// (all pass as machine words on LP64); floating types map to `float` (double).
// Unknown single-word spellings resolve through the typedef table.
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
        // Calling-convention keywords (MSVC / mingw declare e.g.
        // `double __cdecl sqrt(double)`) - strip so the base type parses.
        else if (strncmp(s, "__cdecl ", 8) == 0)    s = bg_trim(s + 8);
        else if (strncmp(s, "__stdcall ", 10) == 0) s = bg_trim(s + 10);
        else if (strncmp(s, "__fastcall ", 11) == 0)s = bg_trim(s + 11);
        else if (strncmp(s, "__thiscall ", 11) == 0)s = bg_trim(s + 11);
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
        // A typedef'd string pointer (png_const_charp = const char*) with one
        // extra star is still opaque; the plain typedef resolves below.
        return "ptr"; // any other single/multi pointer -> opaque
    }

    // Bare `unsigned` / `signed` mean `unsigned int` / `int` (C implicit-int). The
    // strip loop above only removes `unsigned `/`signed ` WITH a trailing base word
    // (e.g. `unsigned int`); a lone `unsigned` (as in zstd's `unsigned
    // ZSTD_versionNumber(void)`) survives here and must collapse to int too.
    if (strcmp(s, "unsigned") == 0 || strcmp(s, "signed") == 0) return "int";

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
    // Single identifier: resolve through the typedef table (png_uint_32 -> int,
    // png_structp -> ptr, uv_file -> int, ...).
    {
        int is_ident = isalpha((unsigned char)s[0]) || s[0] == '_';
        for (const char* p = s; *p && is_ident; p++)
            if (!isalnum((unsigned char)*p) && *p != '_') is_ident = 0;
        if (is_ident) {
            const char* td = td_lookup(s);
            if (td) {
                if (strcmp(td, "void") == 0) return is_return ? "void" : NULL;
                return td;
            }
        }
    }
    return NULL; // struct-by-value, unknown typedef, etc. - not representable
}

// Record `typedef <spelling> <name>;` into the table. Handles the simple
// one-line forms that cover real library headers:
//   typedef unsigned int png_uint_32;
//   typedef struct png_struct_def *png_structp;   (pointer -> ptr)
//   typedef const char * png_const_charp;         (char* -> string)
// Function-pointer/array/struct-body typedefs are skipped (not representable).
static void td_record(const char* decl) {
    const char* s = decl + 7;                       // past "typedef"
    while (*s == ' ' || *s == '\t') s++;
    if (has_char(s, '{') || has_char(s, '[')) return;
    if (has_char(s, '(')) return;                    // function-pointer typedef
    // Name = last identifier; spelling = everything before it.
    const char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    const char* ns = e;
    while (ns > s && (isalnum((unsigned char)ns[-1]) || ns[-1] == '_')) ns--;
    if (ns == e || ns == s) return;
    char name[64]; size_t nl = (size_t)(e - ns);
    if (nl >= sizeof(name)) return;
    memcpy(name, ns, nl); name[nl] = '\0';
    char spelling[256]; size_t sl = (size_t)(ns - s);
    if (sl >= sizeof(spelling)) return;
    memcpy(spelling, s, sl); spelling[sl] = '\0';
    // Map the spelling with the same rules as parameters (no `void` collapse).
    const char* wt = map_c_type(spelling, 1);
    if (wt) td_add(name, wt);
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
    // A system header (e.g. "math.h", "curl/curl.h") isn't a file in the cwd -
    // `cc -E math.h` would look for ./math.h and fail. If `header` isn't an
    // existing path, preprocess a generated stub `#include <header>` instead, so
    // the compiler resolves it on its system include path. (fopen is portable;
    // access()/R_OK need <unistd.h> which mingw lacks.)
    char stub[1024] = "";
    const char* to_pp = header;
    int header_exists = 0;
    { FILE* _hf = fopen(header, "r"); if (_hf) { header_exists = 1; fclose(_hf); } }
    if (!header_exists) {
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
        if (*p == '(') continue;             // function-like macro - skip
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
// Strip a leading GNU `__attribute__((...))` (with balanced parens), repeatedly.
// After `cc -E`, export macros like LZ4LIB_API / ZSTDLIB_API expand to
// `__attribute__ ((visibility ("default")))` in front of the return type; without
// removing it, the scanner mistook the attribute's own `(...)` for the parameter
// list and skipped every such function (all of lz4, zstd, and many real libraries).
static char* bg_strip_attributes(char* s) {
    for (;;) {
        s = bg_trim(s);
        // Storage-class words may precede the attribute (`extern
        // __attribute__((visibility(...))) int git_...`) - hop over them so the
        // attribute is found; the type mapper re-strips them later anyway.
        if (strncmp(s, "extern ", 7) == 0) { s += 7; continue; }
        if (strncmp(s, "static ", 7) == 0) { s += 7; continue; }
        if (strncmp(s, "__attribute__", 13) != 0) return s;
        char* p = s + 13;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '(') return s;            // malformed - leave it
        int depth = 0;
        while (*p) {
            if (*p == '(') depth++;
            else if (*p == ')') { depth--; if (depth == 0) { p++; break; } }
            p++;
        }
        if (depth != 0) return s;           // unbalanced - bail
        s = p;                              // continue: there may be another
    }
}

// Remove nullability/restrict annotations in place - macOS SDK headers wrap
// nearly every pointer in `_Nullable`/`_Nonnull`/`_Null_unspecified`, and many
// libraries use `restrict`/`__restrict`. They carry no FFI meaning; without
// stripping, the type mapper sees "pthread_t _Nullable *" and bails.
static void bg_strip_annotations(char* s) {
    const char* kw[] = {"_Nullable", "_Nonnull", "_Null_unspecified",
                        "__restrict", "restrict", NULL};
    for (int k = 0; kw[k]; k++) {
        size_t kl = strlen(kw[k]);
        char* pos = s;
        while ((pos = strstr(pos, kw[k])) != NULL) {
            // Only whole tokens (avoid eating identifiers containing the word).
            int lb = (pos == s) || (!isalnum((unsigned char)pos[-1]) && pos[-1] != '_');
            int rb = !isalnum((unsigned char)pos[kl]) && pos[kl] != '_';
            if (lb && rb) memmove(pos, pos + kl, strlen(pos + kl) + 1);
            else pos += kl;
        }
    }
}

static int emit_prototype(char* decl, FILE* out) {
    decl = bg_strip_attributes(decl);
    bg_strip_annotations(decl);
    // Unwrap a parenthesized function name: `extern T (name)(args)` - libpng
    // (PNG_FUNCTION) and other export-macro styles emit this. Detect a first
    // paren group that contains a single identifier and is followed by another
    // '(' - that group is the name, not the parameter list.
    {
        char* lp0 = strchr(decl, '(');
        if (lp0) {
            char* q = lp0 + 1;
            while (*q == ' ' || *q == '\t') q++;
            char* id = q;
            while (isalnum((unsigned char)*q) || *q == '_') q++;
            char* qe = q;
            while (*q == ' ' || *q == '\t') q++;
            if (qe > id && *q == ')') {
                char* after = q + 1;
                while (*after == ' ' || *after == '\t') after++;
                if (*after == '(') {
                    // Rewrite "(  name  )" -> " name " in place.
                    *lp0 = ' '; *q = ' ';
                }
            }
        }
    }
    // Split at the first '(' - everything before is "<ret-and-quals> name".
    // The parameter list ends at the MATCHING ')', not the last one in the
    // declaration: macOS/SDK prototypes carry trailing availability/__asm
    // attributes with their own parens (`pthread_t pthread_self(void)
    // __asm("_pthread_self")`) which strrchr used to swallow into the params.
    char* lp = strchr(decl, '(');
    if (!lp) return 0;
    char* rp = NULL;
    { int depth = 0;
      for (char* p = lp; *p; p++) {
          if (*p == '(') depth++;
          else if (*p == ')') { depth--; if (depth == 0) { rp = p; break; } }
      } }
    if (!rp) return 0;

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
    // Skip compiler/library internal symbols (`__foo`, `_Foo`) - they're not part
    // of a library's public API and clutter the output with TODOs.
    if (name[0] == '_') return 0;

    // A calling-convention keyword can sit between the return type and the name
    // (mingw: `double __cdecl sqrt(double)`), so it lands in `rettype`. Remove any
    // such token anywhere in the return-type spelling before mapping.
    { const char* cc_kw[] = {"__cdecl", "__stdcall", "__fastcall", "__thiscall", NULL};
      for (int k = 0; cc_kw[k]; k++) {
          char* pos;
          while ((pos = strstr(rettype, cc_kw[k])) != NULL) {
              size_t kl = strlen(cc_kw[k]);
              memmove(pos, pos + kl, strlen(pos + kl) + 1);
          }
      } }

    const char* wyn_ret = map_c_type(rettype, 1);
    if (!wyn_ret) { fprintf(out, "// TODO: %s - unsupported return type '%s'\n", name, bg_trim(rettype)); return 0; }

    // Parse params: comma-split at top level (params here have no nested parens
    // in the representable subset; a '(' means a function-pointer param -> skip).
    char wyn_params[2048]; wyn_params[0] = '\0';
    int variadic = 0, pidx = 0, skip = 0;
    char* pstate = NULL;
    char* ptrim = bg_trim(params);
    if (strcmp(ptrim, "void") == 0 || *ptrim == '\0') {
        // no params
    } else if (has_char(ptrim, '(')) {
        fprintf(out, "// TODO: %s - function-pointer parameter not supported\n", name);
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
    if (skip) { fprintf(out, "// TODO: %s - unsupported parameter type\n", name); return 0; }
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
    td_count = 0;   // fresh typedef table per header
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

    fprintf(out, "// Generated by `wyn bind %s` - review before use.\n", header_path);
    fprintf(out, "// Only functions/structs/#defines representable by Wyn's FFI type map are\n");
    fprintf(out, "// emitted; anything skipped is marked with a `// TODO:` note.\n\n");

    char line[4096];
    char decl[4096]; decl[0] = '\0';
    int in_target = 0;      // are we inside a region from the target header?
    int fn_count = 0, emitted_any = 0;

    while (fgets(line, sizeof(line), f)) {
        // Line marker: `# <num> "file" ...` - track whether "file" is our header.
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
        // Accumulate everywhere: typedefs from sub-headers (pngconf.h,
        // uv/unix.h, SDL_stdinc.h, …) feed the typedef table; prototypes are
        // only EMITTED from the target header.
        strncat(decl, line, sizeof(decl) - strlen(decl) - 1);

        // Drain EVERY ';'-terminated declaration in the buffer, not just the
        // first: macro-generated headers (pcre2.h emits its whole API per
        // code-unit width on a single physical line) pack dozens of decls per
        // fgets chunk - processing one per read silently truncated the rest.
        char* semi;
        while ((semi = strchr(decl, ';')) != NULL) {
            *semi = '\0';
            char* d = bg_trim(decl);

            // Strip leading attributes BEFORE the shape filters: macOS
            // availability attributes contain '='
            // (`availability(macos,introduced=10.4)`), which the
            // `!has_char(d, '=')` initializer-filter below would otherwise
            // reject - silently skipping every attributed prototype in SDK
            // headers (all of pthread.h, most of libgit2).
            d = bg_strip_attributes(d);

            if (strncmp(d, "typedef", 7) == 0 && !has_char(d, '{')) {
                td_record(d);
            } else if (in_target && *d && strchr(d, '(') &&
                strncmp(d, "struct", 6) != 0 &&
                strncmp(d, "enum", 4) != 0 && strncmp(d, "union", 5) != 0 &&
                !has_char(d, '{') && !has_char(d, '=')) {
                // Plain function prototype from the target header.
                char work[4096]; strncpy(work, d, sizeof(work) - 1); work[sizeof(work) - 1] = '\0';
                if (emit_prototype(work, out)) fn_count++;
                emitted_any = 1;
            }
            // Advance past the processed declaration.
            memmove(decl, semi + 1, strlen(semi + 1) + 1);
        }
        // No ';' left: a struct/enum/union body spans braces - drop a runaway
        // accumulation (struct bodies are handled in a later bindgen pass).
        if (strlen(decl) > 3000) decl[0] = '\0';
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
