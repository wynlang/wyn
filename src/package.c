// Git-URL dependency management - see package.h for the contract.
#define _POSIX_C_SOURCE 200809L
#include "package.h"
#include "pkgspec.h"
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

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
        // Already cached - reuse. Report its pinned commit.
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
    fprintf(f, "# wyn.lock - resolved dependency graph (do not edit by hand)\n");
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
               present ? "\033[32m✓\033[0m" : "\033[33m(not installed - run `wyn install`)\033[0m");
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

// ---- wyn pkg audit -----------------------------------------------------------
// Supply-chain verification with zero infrastructure: everything comes from
// wyn.lock + the git cache + `git ls-remote`. Design: SECURITY_DESIGN.md §3b.
// Output rules: few lines, every line actionable, exit code = worst finding
// (0 ok / 1 warning / 2 error). No advisory database - an empty one would be
// theater ("0 known vulnerabilities" implying coverage that doesn't exist).

// `git ls-remote <url> <ref>` → sha of what the ref points at NOW (or "" on
// failure/missing). Tags are also tried peeled (`ref^{}`) since an annotated
// tag's own sha differs from the commit it tags.
static void ls_remote_sha(const char* url, const char* ref, char* sha, size_t n) {
    sha[0] = '\0';
    if (!shell_safe(url) || !shell_safe(ref)) return;
    char cmd[1400];
    snprintf(cmd, sizeof(cmd),
             "git ls-remote '%s' '%s' '%s^{}' 2>/dev/null | tail -1 | cut -f1", url, ref, ref);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        if (fgets(sha, n, fp)) { char* nl = strchr(sha, '\n'); if (nl) *nl = '\0'; }
        pclose(fp);
    }
}

// Is `ref` a tag (vs branch) on the remote? 1=tag, 0=branch/unknown.
static int remote_ref_is_tag(const char* url, const char* ref) {
    if (!shell_safe(url) || !shell_safe(ref)) return 0;
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "git ls-remote --tags '%s' '%s' 2>/dev/null | head -1", url, ref);
    FILE* fp = popen(cmd, "r");
    char buf[256] = "";
    if (fp) { if (fgets(buf, sizeof(buf), fp)) {} pclose(fp); }
    return buf[0] != '\0';
}

// Does this ref LOOK like a full/abbrev commit sha? (all hex, >= 7 chars)
static int ref_is_sha(const char* ref) {
    size_t len = strlen(ref);
    if (len < 7 || len > 40) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = ref[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

int wyn_pkg_audit(int offline) {
    LockEntry locks[128];
    int n = lock_read(locks, 128);
    WynConfig* cfg = wyn_config_parse("wyn.toml");
    int dep_count = cfg ? cfg->dependency_count : 0;

    if (n == 0 && dep_count == 0) {
        printf("No dependencies (no wyn.lock, nothing in [dependencies]). Nothing to audit.\n");
        if (cfg) wyn_config_free(cfg);
        return 0;
    }
    printf("\033[1mAuditing %d dependenc%s\033[0m%s\n\n", n, n == 1 ? "y" : "ies",
           offline ? "  (offline: remote checks skipped)" : "");

    int worst = 0;  // 0 ok, 1 warn, 2 error
    int ffi_count = 0;

    for (int i = 0; i < n; i++) {
        LockEntry* e = &locks[i];
        const char* ref = (e->ref[0] && strcmp(e->ref, "-") != 0) ? e->ref : "";
        char short_sha[12]; snprintf(short_sha, sizeof(short_sha), "%.9s", e->sha);

        // Cache integrity: does the cached checkout still match the lock?
        // pkgspec_parse fills host/owner/repo, which cache_dir needs - a raw
        // field copy computes the wrong path.
        PkgSpec spec; memset(&spec, 0, sizeof(spec));
        char spec_input[700];
        snprintf(spec_input, sizeof(spec_input), "%s%s%s", e->url,
                 ref[0] ? "@" : "", ref);
        if (pkgspec_parse(spec_input, e->name, &spec) != 0) {
            printf("  \033[33m⚠\033[0m %-14s unparseable lock entry (%s)\n", e->name, e->url);
            if (worst < 1) worst = 1;
            continue;
        }
        char dir[600]; pkgspec_cache_dir(&spec, dir, sizeof(dir));
        char cache_sha[64] = "";
        int cached = dir_exists(dir);
        if (cached) get_head_sha(dir, cache_sha, sizeof(cache_sha));

        // FFI surface: does the dep's manifest declare linked C libs?
        char ffi_libs[256] = "";
        if (cached) {
            char toml[700]; snprintf(toml, sizeof(toml), "%s/wyn.toml", dir);
            WynConfig* dep = wyn_config_parse(toml);
            if (dep) {
                if (dep->ffi.libs && dep->ffi.libs[0])
                    snprintf(ffi_libs, sizeof(ffi_libs), "%s", dep->ffi.libs);
                wyn_config_free(dep);
            }
        }

        // Remote check: where does the pinned ref point NOW?
        char remote_sha[64] = "";
        int is_tag = 0;
        if (!offline && ref[0] && !ref_is_sha(ref)) {
            ls_remote_sha(e->url, ref, remote_sha, sizeof(remote_sha));
            is_tag = remote_ref_is_tag(e->url, ref);
        }

        // Verdict per dep, worst condition wins.
        if (!offline && ref[0] && !ref_is_sha(ref) && remote_sha[0] == '\0') {
            printf("  \033[33m⚠\033[0m %-14s %s@%s  %s  REF NOT FOUND on remote (deleted or renamed?)\n",
                   e->name, e->url, ref, short_sha);
            if (worst < 1) worst = 1;
        } else if (!offline && is_tag && remote_sha[0] && e->sha[0] &&
                   strncmp(remote_sha, e->sha, strlen(e->sha)) != 0) {
            printf("  \033[31m✗\033[0m %-14s %s@%s  %s  TAG MOVED - now %.9s\n",
                   e->name, e->url, ref, short_sha, remote_sha);
            printf("      a retagged (force-pushed) release can hide code changes. Inspect before updating:\n");
            printf("      git -C %s log --oneline -5\n", dir);
            worst = 2;
        } else if (cached && cache_sha[0] && e->sha[0] &&
                   strcmp(cache_sha, e->sha) != 0) {
            printf("  \033[31m✗\033[0m %-14s %s@%s  %s  CACHE MISMATCH - checkout is at %.9s\n",
                   e->name, e->url, ref[0] ? ref : "-", short_sha, cache_sha);
            printf("      the local cache was modified or moved. Re-install: rm -rf %s && wyn pkg install\n", dir);
            worst = 2;
        } else if (ref[0] == '\0' || (!ref_is_sha(ref) && !is_tag && !offline && remote_sha[0])) {
            // branch pin (or no ref at all): mutable - the same install command
            // gives different code tomorrow.
            const char* what = ref[0] ? "BRANCH PIN" : "NO REF";
            printf("  \033[33m⚠\033[0m %-14s %s@%s  %s  %s - mutable; pin a tag or sha\n",
                   e->name, e->url, ref[0] ? ref : "-", short_sha, what);
            if (!offline && remote_sha[0] && e->sha[0] &&
                strncmp(remote_sha, e->sha, strlen(e->sha)) != 0)
                printf("      '%s' has moved (now %.9s). To keep today's code: wyn pkg add %s@%s\n",
                       ref[0] ? ref : "HEAD", remote_sha, e->name, short_sha);
            if (worst < 1) worst = 1;
        } else {
            const char* kind = ref_is_sha(ref) ? "sha" : (is_tag || offline ? "tag" : "ref");
            printf("  \033[32m✓\033[0m %-14s %s@%s  %s  %s%s\n",
                   e->name, e->url, ref[0] ? ref : "-", short_sha,
                   kind, offline ? " (unverified)" : ", verified");
        }
        if (ffi_libs[0]) {
            ffi_count++;
            printf("      \033[33mffi\033[0m links native code (%s) - capability limits don't apply to C; review this package\n",
                   ffi_libs);
        }
    }

    // Deps declared but never locked/installed.
    for (int d = 0; d < dep_count; d++) {
        int found = 0;
        for (int i = 0; i < n; i++)
            if (strcmp(cfg->dependencies[d].name, locks[i].name) == 0) { found = 1; break; }
        if (!found) {
            printf("  \033[33m⚠\033[0m %-14s declared in wyn.toml but not in wyn.lock - run: wyn pkg install\n",
                   cfg->dependencies[d].name);
            if (worst < 1) worst = 1;
        }
    }
    if (cfg) wyn_config_free(cfg);

    printf("\n");
    if (ffi_count > 0)
        printf("%d ffi dependenc%s - these link arbitrary native code and carry full trust.\n",
               ffi_count, ffi_count == 1 ? "y" : "ies");
    if (worst == 0) printf("\033[32m✓ audit clean\033[0m\n");
    else printf("%s\n", worst == 2 ? "\033[31maudit found errors\033[0m (exit 2)"
                                   : "\033[33maudit found warnings\033[0m (exit 1)");
    return worst;
}

// ---- wyn search ---------------------------------------------------------------
// Zero-infrastructure discovery: the "registry" is GitHub itself.
//   1. Official packages: repos in the wynlang org.
//   2. Community packages: repos tagged with the `wyn-package` topic.
// Unauthenticated API (60 req/h rate limit is plenty for interactive search);
// the compiler already shells out to git/curl, so popen keeps the pattern.
// Output rule: every hit shows the exact `wyn pkg add ...` to run.

// Non-package repos in the org (tooling, docs, this compiler, editors, ...).
static int search_is_infra_repo(const char* name) {
    static const char* infra[] = {
        "wyn", "internal-docs", "site", "sample-apps", "awesome-wyn",
        "nvim-wyn", "vscode-wyn", "play", "book", "wyn_old", "assets",
        ".github", NULL
    };
    for (int i = 0; infra[i]; i++)
        if (strcmp(name, infra[i]) == 0) return 1;
    return 0;
}

// Fetch a URL with curl into a malloc'd buffer (NULL on failure).
static char* search_fetch(const char* url) {
    if (!shell_safe(url)) return NULL;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -s -m 10 -H 'Accept: application/vnd.github+json' '%s'", url);
    FILE* fp = popen(cmd, "r");
    if (!fp) return NULL;
    size_t cap = 65536, len = 0;
    char* buf = malloc(cap);
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, fp)) > 0) {
        len += n;
        if (cap - len < 4096) { cap *= 2; buf = realloc(buf, cap); }
    }
    pclose(fp);
    buf[len] = '\0';
    if (len == 0) { free(buf); return NULL; }
    return buf;
}

// Minimal scanner over the GitHub JSON array: pull "name" and "description"
// pairs in order. Full JSON parsing is overkill for two string fields; this
// tolerates description:null and keeps package.c dependency-free.
// Find `"key"` then skip `: "` with arbitrary whitespace; returns the value
// start or NULL. GitHub pretty-prints its JSON ("name": "web"), so fixed
// "key":"value" patterns don't match.
static const char* search_json_str(const char* from, const char* key) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* k = strstr(from, pat);
    if (!k) return NULL;
    k += strlen(pat);
    while (*k == ' ' || *k == '\t') k++;
    if (*k != ':') return NULL;
    k++;
    while (*k == ' ' || *k == '\t' || *k == '\n') k++;
    if (*k != '"') return NULL;  // null / non-string
    return k + 1;
}

static void search_print_repos(const char* json, const char* needle,
                               const char* add_prefix, int* shown) {
    const char* p = json;
    char full[256], name[128], desc[512];
    // Anchor on "full_name" ("owner/repo") - unique per repo object; a bare
    // "name" key also appears inside nested license/owner objects ("MIT
    // License" showed up as a package before this).
    while ((p = search_json_str(p, "full_name")) != NULL) {
        size_t i = 0;
        while (*p && *p != '"' && i < sizeof(full) - 1) full[i++] = *p++;
        full[i] = '\0';
        const char* slash = strrchr(full, '/');
        snprintf(name, sizeof(name), "%s", slash ? slash + 1 : full);
        // description follows full_name in the repo object
        desc[0] = '\0';
        const char* next_repo = strstr(p, "\"full_name\"");
        const char* d = search_json_str(p, "description");
        if (d && (!next_repo || d < next_repo)) {
            size_t j = 0;
            while (*d && *d != '"' && j < sizeof(desc) - 1) {
                if (*d == '\\' && d[1]) d++;  // unescape \" etc.
                desc[j++] = *d++;
            }
            desc[j] = '\0';
        }
        if (search_is_infra_repo(name)) continue;
        if (needle && needle[0]) {
            // case-insensitive substring match on name or description
            char lname[128], ldesc[512], lneedle[128];
            size_t k;
            for (k = 0; name[k] && k < 127; k++) lname[k] = (char)tolower((unsigned char)name[k]);
            lname[k] = 0;
            for (k = 0; desc[k] && k < 511; k++) ldesc[k] = (char)tolower((unsigned char)desc[k]);
            ldesc[k] = 0;
            for (k = 0; needle[k] && k < 127; k++) lneedle[k] = (char)tolower((unsigned char)needle[k]);
            lneedle[k] = 0;
            if (!strstr(lname, lneedle) && !strstr(ldesc, lneedle)) continue;
        }
        printf("  \033[1m%-14s\033[0m %s\n", name, desc[0] ? desc : "(no description)");
        printf("                 \033[2mwyn pkg add %s%s\033[0m\n", add_prefix, name);
        (*shown)++;
    }
}

int wyn_pkg_search(const char* query) {
    printf("\033[1mSearching packages\033[0m%s%s\n\n",
           query && query[0] ? " for: " : "", query ? query : "");

    int shown = 0;
    // Official: the wynlang org. Bare names resolve here.
    char* org = search_fetch("https://api.github.com/orgs/wynlang/repos?per_page=100&sort=updated");
    if (org) {
        search_print_repos(org, query, "", &shown);
        free(org);
    } else {
        fprintf(stderr, "  \033[33m⚠\033[0m could not reach api.github.com (offline?)\n");
        return 1;
    }

    // Community: anything tagged `wyn-package`, any owner. Install by full path.
    char topic_url[512];
    if (query && query[0] && shell_safe(query)) {
        snprintf(topic_url, sizeof(topic_url),
                 "https://api.github.com/search/repositories?q=topic:wyn-package+%s&per_page=30", query);
    } else {
        snprintf(topic_url, sizeof(topic_url),
                 "https://api.github.com/search/repositories?q=topic:wyn-package&per_page=30");
    }
    char* topic = search_fetch(topic_url);
    if (topic) {
        search_print_repos(topic, query, "github.com/", &shown);
        free(topic);
    }

    if (shown == 0) {
        printf("  no packages matched%s%s\n", query ? " " : "", query ? query : "");
        printf("\n  Publish your own: push a repo with a wyn.toml and tag it\n");
        printf("  (add the 'wyn-package' GitHub topic so search finds it).\n");
    } else {
        printf("\n%d package%s. Pin a version: wyn pkg add <name>@<tag>\n", shown, shown == 1 ? "" : "s");
    }
    return 0;
}
