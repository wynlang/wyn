// Git-URL dependency management — see package.h for the contract.
#define _POSIX_C_SOURCE 200809L
#include "package.h"
#include "pkgspec.h"
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ---- small shell/quoting helpers -------------------------------------------

// Reject characters that would let a wyn.toml value break out of the single-
// quoted shell argument we build. URLs, refs, and paths never legitimately
// contain these. Returns 1 if safe.
static int shell_safe(const char* s) {
    if (!s) return 0;
    for (const char* c = s; *c; c++) {
        if (*c == '\'' || *c == '`' || *c == '$' || *c == '\n' || *c == '\r') return 0;
    }
    return 1;
}

static void ensure_dir(const char* path) {
    if (!shell_safe(path)) return;
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

static int dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// git rev-parse HEAD of a checkout → `sha`.
static void get_head_sha(const char* dir, char* sha, size_t n) {
    sha[0] = '\0';
    if (!shell_safe(dir)) return;
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "git -C '%s' rev-parse HEAD 2>/dev/null", dir);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        if (fgets(sha, n, fp)) { char* nl = strchr(sha, '\n'); if (nl) *nl = '\0'; }
        pclose(fp);
    }
}

// ---- the global cache -------------------------------------------------------

// Ensure `spec` is cloned into the cache at its ref. On success writes the
// resolved commit sha into `sha_out` and the cache dir into `dir_out`.
// Idempotent: an existing, non-empty cache dir is reused (no re-clone).
static int clone_into_cache(const PkgSpec* spec, char* dir_out, size_t dir_n,
                            char* sha_out, size_t sha_n) {
    pkgspec_cache_dir(spec, dir_out, dir_n);

    if (dir_exists(dir_out)) {
        // Already cached — reuse. Report its pinned commit.
        get_head_sha(dir_out, sha_out, sha_n);
        return 0;
    }
    if (!shell_safe(spec->url) || !shell_safe(dir_out) || !shell_safe(spec->ref)) {
        fprintf(stderr, "  \033[31m✗\033[0m Refusing unsafe URL/ref for %s\n", spec->name);
        return 1;
    }

    // Make the parent dir, then clone into the leaf.
    char parent[600]; snprintf(parent, sizeof(parent), "%s", dir_out);
    char* last = strrchr(parent, '/'); if (last) { *last = '\0'; ensure_dir(parent); }

    char cmd[1600];
    if (spec->ref[0]) {
        // Try a shallow clone at the ref (tags/branches); fall back to a full
        // clone + checkout for commit SHAs which --branch can't take.
        snprintf(cmd, sizeof(cmd),
                 "git clone --depth 1 --branch '%s' '%s' '%s' 2>/dev/null "
                 "|| ( git clone '%s' '%s' 2>&1 && git -C '%s' checkout '%s' 2>&1 )",
                 spec->ref, spec->url, dir_out,
                 spec->url, dir_out, dir_out, spec->ref);
    } else {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 '%s' '%s' 2>&1", spec->url, dir_out);
    }
    printf("  Cloning %s%s%s...\n", spec->url, spec->ref[0] ? "@" : "", spec->ref);
    int rc = system(cmd);
    if (rc != 0 || !dir_exists(dir_out)) {
        fprintf(stderr, "  \033[31m✗\033[0m Failed to clone %s\n", spec->url);
        // Clean up a half-made dir so a retry starts fresh.
        if (shell_safe(dir_out)) { char rm[700]; snprintf(rm, sizeof(rm), "rm -rf '%s'", dir_out); system(rm); }
        return 1;
    }

    // Validate it looks like a Wyn package (has .wyn / .🐉, or .c/.h for a C binding).
    char check[1400];
    snprintf(check, sizeof(check),
             "find '%s' \\( -name '*.wyn' -o -name '*.🐉' -o -name '*.c' -o -name '*.h' \\) 2>/dev/null | head -1",
             dir_out);
    FILE* fp = popen(check, "r");
    char buf[512] = "";
    if (fp) { if (fgets(buf, sizeof(buf), fp)) {} pclose(fp); }
    if (buf[0] == '\0') {
        fprintf(stderr, "  \033[31m✗\033[0m Not a Wyn package (no .wyn/.c files): %s\n", spec->url);
        char rm[700]; snprintf(rm, sizeof(rm), "rm -rf '%s'", dir_out); system(rm);
        return 1;
    }
    get_head_sha(dir_out, sha_out, sha_n);
    return 0;
}

// ---- wyn.lock (one format: `name url ref sha`, whitespace-separated) --------

typedef struct { char name[128], url[512], ref[128], sha[64]; } LockEntry;

// Read wyn.lock into `out` (capacity `max`), returns count. Tolerates a missing
// ref column (legacy `name url sha`): then ref="" and the 3rd token is the sha.
static int lock_read(LockEntry* out, int max) {
    FILE* f = fopen("wyn.lock", "r");
    if (!f) return 0;
    char line[900]; int n = 0;
    while (fgets(line, sizeof(line), f) && n < max) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char a[512] = "", b[512] = "", c[512] = "", d[512] = "";
        int got = sscanf(line, "%127s %511s %127s %63s", a, b, c, d);
        if (got < 2) continue;
        LockEntry* e = &out[n++];
        snprintf(e->name, sizeof(e->name), "%s", a);
        snprintf(e->url,  sizeof(e->url),  "%s", b);
        if (got >= 4)      { snprintf(e->ref, sizeof(e->ref), "%s", c); snprintf(e->sha, sizeof(e->sha), "%s", d); }
        else if (got == 3) { e->ref[0] = '\0'; snprintf(e->sha, sizeof(e->sha), "%s", c); }
        else               { e->ref[0] = '\0'; e->sha[0] = '\0'; }
    }
    fclose(f);
    return n;
}

static void lock_write(const LockEntry* e, int n) {
    FILE* f = fopen("wyn.lock", "w");
    if (!f) return;
    fprintf(f, "# wyn.lock — resolved dependency graph (do not edit by hand)\n");
    for (int i = 0; i < n; i++)
        fprintf(f, "%s %s %s %s\n", e[i].name, e[i].url, e[i].ref[0] ? e[i].ref : "-", e[i].sha);
    fclose(f);
}

// Upsert one entry into wyn.lock.
static void lock_upsert(const char* name, const char* url, const char* ref, const char* sha) {
    LockEntry e[128]; int n = lock_read(e, 128);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(e[i].name, name) == 0) {
            snprintf(e[i].url, sizeof(e[i].url), "%s", url);
            snprintf(e[i].ref, sizeof(e[i].ref), "%s", ref[0] ? ref : "-");
            snprintf(e[i].sha, sizeof(e[i].sha), "%s", sha);
            found = 1; break;
        }
    }
    if (!found && n < 128) {
        snprintf(e[n].name, sizeof(e[n].name), "%s", name);
        snprintf(e[n].url,  sizeof(e[n].url),  "%s", url);
        snprintf(e[n].ref,  sizeof(e[n].ref),  "%s", ref[0] ? ref : "-");
        snprintf(e[n].sha,  sizeof(e[n].sha),  "%s", sha);
        n++;
    }
    lock_write(e, n);
}

static void lock_remove(const char* name) {
    LockEntry e[128]; int n = lock_read(e, 128);
    int w = 0;
    for (int i = 0; i < n; i++) if (strcmp(e[i].name, name) != 0) e[w++] = e[i];
    lock_write(e, w);
}

// ---- wyn.toml [dependencies] upsert/remove ---------------------------------
//
// We reuse the toml parser for reads; writes are line-oriented so we preserve
// the rest of the file. A dependency line is `name = "url@ref"` inside the
// [dependencies] section.

// Store `name = "value"` under [dependencies], creating the section if needed.
static int manifest_upsert_dep(const char* name, const char* value) {
    FILE* in = fopen("wyn.toml", "r");
    const char* out_path = "wyn.toml.tmp";
    FILE* out = fopen(out_path, "w");
    if (!out) { if (in) fclose(in); fprintf(stderr, "Error: cannot write wyn.toml\n"); return 1; }

    int in_deps = 0, wrote = 0, saw_deps = 0;
    if (in) {
        char line[1024];
        while (fgets(line, sizeof(line), in)) {
            // Detect section transitions on a trimmed copy.
            char t[1024]; snprintf(t, sizeof(t), "%s", line);
            char* s = t; while (*s == ' ' || *s == '\t') s++;
            char* e = s + strlen(s); while (e > s && (e[-1]=='\n'||e[-1]=='\r'||e[-1]==' '||e[-1]=='\t')) *--e = '\0';

            if (s[0] == '[') {
                // Leaving [dependencies] without having written our line → add it now.
                if (in_deps && !wrote) { fprintf(out, "%s = \"%s\"\n", name, value); wrote = 1; }
                in_deps = (strcmp(s, "[dependencies]") == 0);
                if (in_deps) saw_deps = 1;
                fputs(line, out);
                continue;
            }
            if (in_deps) {
                // Replace an existing `name = ...` line.
                char* eq = strchr(s, '=');
                if (eq) {
                    char key[128]; int kl = eq - s;
                    while (kl > 0 && (s[kl-1]==' '||s[kl-1]=='\t')) kl--;
                    if (kl > 0 && kl < (int)sizeof(key)) {
                        memcpy(key, s, kl); key[kl] = '\0';
                        if (strcmp(key, name) == 0) {
                            if (!wrote) { fprintf(out, "%s = \"%s\"\n", name, value); wrote = 1; }
                            continue;   // drop old line
                        }
                    }
                }
            }
            fputs(line, out);
        }
        // File ended while still in [dependencies].
        if (in_deps && !wrote) { fprintf(out, "%s = \"%s\"\n", name, value); wrote = 1; }
        fclose(in);
    }
    if (!saw_deps && !wrote) {
        if (!in) fprintf(out, "[project]\nname = \"app\"\nversion = \"0.1.0\"\nentry = \"src/main.wyn\"\n");
        fprintf(out, "\n[dependencies]\n%s = \"%s\"\n", name, value);
    }
    fclose(out);
    rename(out_path, "wyn.toml");
    return 0;
}

static int manifest_remove_dep(const char* name) {
    FILE* in = fopen("wyn.toml", "r");
    if (!in) return 0;
    FILE* out = fopen("wyn.toml.tmp", "w");
    if (!out) { fclose(in); return 1; }
    int in_deps = 0, removed = 0;
    char line[1024];
    while (fgets(line, sizeof(line), in)) {
        char t[1024]; snprintf(t, sizeof(t), "%s", line);
        char* s = t; while (*s == ' ' || *s == '\t') s++;
        char* e = s + strlen(s); while (e > s && (e[-1]=='\n'||e[-1]=='\r'||e[-1]==' '||e[-1]=='\t')) *--e = '\0';
        if (s[0] == '[') { in_deps = (strcmp(s, "[dependencies]") == 0); fputs(line, out); continue; }
        if (in_deps) {
            char* eq = strchr(s, '=');
            if (eq) {
                char key[128]; int kl = eq - s;
                while (kl > 0 && (s[kl-1]==' '||s[kl-1]=='\t')) kl--;
                if (kl > 0 && kl < (int)sizeof(key)) {
                    memcpy(key, s, kl); key[kl] = '\0';
                    if (strcmp(key, name) == 0) { removed = 1; continue; }
                }
            }
        }
        fputs(line, out);
    }
    fclose(in); fclose(out);
    rename("wyn.toml.tmp", "wyn.toml");
    return removed ? 0 : 1;
}

// Parse `name = "url@ref"` (or legacy `{ git = "url" }`) into a PkgSpec. The
// stored value already carries the ref, so we hand it straight to pkgspec_parse.
static int spec_from_dep_value(const char* name, const char* value, PkgSpec* spec) {
    // Legacy `{ git = "URL" }` form: pull the URL out.
    const char* v = value;
    char extracted[512];
    const char* g = strstr(value, "git");
    if (value[0] == '{' && g) {
        const char* q = strchr(g, '"');
        if (q) { q++; const char* q2 = strchr(q, '"');
            if (q2 && (size_t)(q2 - q) < sizeof(extracted)) { memcpy(extracted, q, q2 - q); extracted[q2-q] = '\0'; v = extracted; } }
    }
    if (pkgspec_parse(v, name, spec) != 0) return 1;
    return 0;
}

// ---- public API -------------------------------------------------------------

int wyn_pkg_add(const char* input, const char* override_name) {
    PkgSpec spec;
    if (pkgspec_parse(input, override_name, &spec) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m Not a valid package: '%s'\n", input);
        fprintf(stderr, "  Try a name (\033[1margs\033[0m → github.com/wynlang/args) or a URL "
                        "(\033[1mgithub.com/user/repo[@ref]\033[0m).\n");
        return 1;
    }

    char dir[600], sha[64];
    if (clone_into_cache(&spec, dir, sizeof(dir), sha, sizeof(sha)) != 0) return 1;

    // Store `name = "url[@ref]"` in [dependencies].
    char value[640];
    if (spec.ref[0]) snprintf(value, sizeof(value), "%s@%s", spec.url, spec.ref);
    else             snprintf(value, sizeof(value), "%s", spec.url);
    if (manifest_upsert_dep(spec.name, value) != 0) return 1;
    lock_upsert(spec.name, spec.url, spec.ref, sha);

    printf("\033[32m✓\033[0m Added \033[1m%s\033[0m → %s%s%s\n",
           spec.name, spec.url, spec.ref[0] ? "@" : "", spec.ref);
    printf("    cached: %s\n", dir);
    printf("    use it: \033[1mimport %s\033[0m\n", spec.name);
    return 0;
}

int wyn_pkg_install(void) {
    // Prefer wyn.lock (pinned). Fall back to the manifest for a fresh checkout.
    LockEntry le[128]; int ln = lock_read(le, 128);
    int ok = 0, fail = 0;

    if (ln > 0) {
        for (int i = 0; i < ln; i++) {
            PkgSpec spec;
            // Reconstruct spec from the locked url + ref (pin to the exact ref).
            char urlref[640];
            if (le[i].ref[0] && strcmp(le[i].ref, "-") != 0) snprintf(urlref, sizeof(urlref), "%s@%s", le[i].url, le[i].ref);
            else snprintf(urlref, sizeof(urlref), "%s", le[i].url);
            if (pkgspec_parse(urlref, le[i].name, &spec) != 0) { fail++; continue; }
            char dir[600], sha[64];
            if (clone_into_cache(&spec, dir, sizeof(dir), sha, sizeof(sha)) == 0) {
                printf("  \033[32m✓\033[0m %s\n", le[i].name); ok++;
            } else fail++;
        }
    } else {
        WynConfig* cfg = wyn_config_parse("wyn.toml");
        if (!cfg) { fprintf(stderr, "No wyn.toml / wyn.lock found.\n"); return 1; }
        for (int i = 0; i < cfg->dependency_count; i++) {
            PkgSpec spec;
            if (spec_from_dep_value(cfg->dependencies[i].name, cfg->dependencies[i].version, &spec) != 0) { fail++; continue; }
            char dir[600], sha[64];
            if (clone_into_cache(&spec, dir, sizeof(dir), sha, sizeof(sha)) == 0) {
                lock_upsert(spec.name, spec.url, spec.ref, sha);
                printf("  \033[32m✓\033[0m %s\n", spec.name); ok++;
            } else fail++;
        }
        wyn_config_free(cfg);
    }
    printf("\n%d installed%s\n", ok, fail ? ", some failed" : "");
    return fail ? 1 : 0;
}

int wyn_pkg_remove(const char* name) {
    int m = manifest_remove_dep(name);
    lock_remove(name);
    if (m == 0) { printf("\033[32m✓\033[0m Removed %s\n", name); return 0; }
    fprintf(stderr, "\033[33m⚠\033[0m %s was not in [dependencies]\n", name);
    return 1;
}

int wyn_pkg_list(void) {
    WynConfig* cfg = wyn_config_parse("wyn.toml");
    if (!cfg) { fprintf(stderr, "No wyn.toml in current directory.\n"); return 1; }
    printf("Dependencies:\n\n");
    if (cfg->dependency_count == 0) printf("  (none)\n");
    for (int i = 0; i < cfg->dependency_count; i++) {
        PkgSpec spec; char dir[600]; int present = 0;
        if (spec_from_dep_value(cfg->dependencies[i].name, cfg->dependencies[i].version, &spec) == 0) {
            pkgspec_cache_dir(&spec, dir, sizeof(dir));
            present = dir_exists(dir);
        }
        printf("  %-14s %s  %s\n", cfg->dependencies[i].name, cfg->dependencies[i].version,
               present ? "\033[32m✓\033[0m" : "\033[33m(not installed — run `wyn install`)\033[0m");
    }
    printf("\n%d dependenc%s\n", cfg->dependency_count, cfg->dependency_count == 1 ? "y" : "ies");
    wyn_config_free(cfg);
    return 0;
}

int wyn_dep_resolve(const char* import_name, char* dir_out, size_t n) {
    WynConfig* cfg = wyn_config_parse("wyn.toml");
    if (!cfg) return 0;
    int hit = 0;
    for (int i = 0; i < cfg->dependency_count; i++) {
        if (strcmp(cfg->dependencies[i].name, import_name) != 0) continue;
        PkgSpec spec;
        if (spec_from_dep_value(cfg->dependencies[i].name, cfg->dependencies[i].version, &spec) == 0) {
            pkgspec_cache_dir(&spec, dir_out, n);
            if (dir_exists(dir_out)) hit = 1;
        }
        break;
    }
    wyn_config_free(cfg);
    return hit;
}

void wyn_deps_ffi_flags(char* out, size_t out_size) {
    WynConfig* cfg = wyn_config_parse("wyn.toml");
    if (!cfg) return;
    for (int i = 0; i < cfg->dependency_count; i++) {
        PkgSpec spec; char dir[600];
        if (spec_from_dep_value(cfg->dependencies[i].name, cfg->dependencies[i].version, &spec) != 0) continue;
        pkgspec_cache_dir(&spec, dir, sizeof(dir));
        if (!dir_exists(dir)) continue;
        char toml[700]; snprintf(toml, sizeof(toml), "%s/wyn.toml", dir);
        WynConfig* dep = wyn_config_parse(toml);
        if (dep) {
            char frag[512]; frag[0] = '\0';
            wyn_config_ffi_flags(dep, frag, sizeof(frag));
            size_t used = strlen(out);
            if (frag[0] && used + strlen(frag) + 1 < out_size)
                snprintf(out + used, out_size - used, "%s", frag);
            wyn_config_free(dep);
        }
    }
    wyn_config_free(cfg);
}
