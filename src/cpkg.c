// C-package manager. See cpkg.h for the contract.
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>   // _mkdir
#endif
#include "cpkg.h"
#include "bindgen.h"

// Portable recursive mkdir (like `mkdir -p`) — no shelling out, so it works the
// same on every platform (cmd.exe has no `mkdir -p`). Uses '/' separators, which
// Windows accepts in CRT calls.
static int make_dirs(const char* path) {
    char tmp[600]; strncpy(tmp, path, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
    // Success if the directory now exists (mkdir returns -1/EEXIST if present).
    struct stat st;
    return (stat(path, &st) == 0) ? 0 : -1;
}

// ---- a single recipe from the registry ------------------------------------

typedef struct {
    char name[64];
    char description[256];
    char version[64];
    char libs[256];
    char lib_dirs[256];
    char include_dirs[512];
    char headers[512];
    int found;
} Recipe;

static char* cp_trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char* e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

// Strip surrounding double quotes from a value in place.
static char* cp_unquote(char* v) {
    v = cp_trim(v);
    size_t n = strlen(v);
    if (n >= 2 && v[0] == '"' && v[n - 1] == '"') { v[n - 1] = '\0'; return v + 1; }
    return v;
}

static void registry_path(char* buf, size_t sz, const char* wyn_root) {
    snprintf(buf, sz, "%s/registry/c-packages.toml", wyn_root);
}

// Parse the registry, filling `out` for section [want]. Returns 1 if found.
// If want == NULL, instead prints every section's name+description (list mode)
// and returns the count.
static int registry_scan(const char* wyn_root, const char* want, Recipe* out) {
    char path[1024]; registry_path(path, sizeof(path), wyn_root);
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: registry not found at %s\n", path);
        return 0;
    }
    char line[1024];
    char section[64] = "";
    int in_want = 0, listed = 0;
    char cur_desc[256] = "";
    if (out) { memset(out, 0, sizeof(*out)); strcpy(out->version, "system"); }

    while (fgets(line, sizeof(line), f)) {
        char* s = cp_trim(line);
        if (!*s || *s == '#') continue;
        if (*s == '[') {
            char* end = strchr(s, ']');
            if (!end) continue;
            *end = '\0';
            // list mode: print the previous section as we cross into the next.
            if (!want && section[0] && cur_desc[0]) { printf("  %-14s %s\n", section, cur_desc); listed++; }
            strncpy(section, s + 1, sizeof(section) - 1); section[sizeof(section)-1] = '\0';
            cur_desc[0] = '\0';
            in_want = (want && strcmp(section, want) == 0);
            if (in_want && out) { strncpy(out->name, section, sizeof(out->name)-1); out->found = 1; }
            continue;
        }
        char* eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = cp_trim(s);
        char* val = cp_unquote(eq + 1);
        if (!want && strcmp(key, "description") == 0) {
            strncpy(cur_desc, val, sizeof(cur_desc) - 1); cur_desc[sizeof(cur_desc)-1] = '\0';
        }
        if (!in_want || !out) continue;
        if      (strcmp(key, "description") == 0)  strncpy(out->description, val, sizeof(out->description)-1);
        else if (strcmp(key, "version") == 0)      strncpy(out->version, val, sizeof(out->version)-1);
        else if (strcmp(key, "libs") == 0)         strncpy(out->libs, val, sizeof(out->libs)-1);
        else if (strcmp(key, "lib_dirs") == 0)     strncpy(out->lib_dirs, val, sizeof(out->lib_dirs)-1);
        else if (strcmp(key, "include_dirs") == 0) strncpy(out->include_dirs, val, sizeof(out->include_dirs)-1);
        else if (strcmp(key, "headers") == 0)      strncpy(out->headers, val, sizeof(out->headers)-1);
    }
    // flush the last section in list mode
    if (!want && section[0] && cur_desc[0]) { printf("  %-14s %s\n", section, cur_desc); listed++; }
    fclose(f);
    return want ? (out && out->found) : listed;
}

int wyn_cpkg_list(const char* wyn_root) {
    printf("Available C packages (curated):\n");
    int n = registry_scan(wyn_root, NULL, NULL);
    if (n == 0) printf("  (registry empty or not found)\n");
    printf("\nAdd one with:  wyn add <name>\n");
    return 0;
}

// Return 1 if `name` matches a curated C-library recipe in c-packages.toml.
// Used by `wyn add <arg>` to decide between the curated C-library path (cpkg)
// and the git-URL dependency path. Non-destructive: just scans the registry.
int wyn_cpkg_has_recipe(const char* name, const char* wyn_root) {
    if (!name || !*name) return 0;
    Recipe r;
    return registry_scan(wyn_root, name, &r) ? 1 : 0;
}

// ---- interactive TUI (wyn add, no name, on a TTY) --------------------------

typedef struct { char name[64]; char desc[256]; } RecipeBrief;

// Collect every recipe's name+description into `out` (up to `max`); returns count.
static int registry_collect(const char* wyn_root, RecipeBrief* out, int max) {
    char path[1024]; registry_path(path, sizeof(path), wyn_root);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[1024], section[64] = "", desc[256] = "";
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char* s = cp_trim(line);
        if (!*s || *s == '#') continue;
        if (*s == '[') {
            char* end = strchr(s, ']'); if (!end) continue; *end = '\0';
            if (section[0] && count < max) { strncpy(out[count].name, section, 63); strncpy(out[count].desc, desc, 255); count++; }
            strncpy(section, s + 1, sizeof(section)-1); section[sizeof(section)-1]='\0';
            desc[0] = '\0';
            continue;
        }
        char* eq = strchr(s, '='); if (!eq) continue; *eq = '\0';
        if (strcmp(cp_trim(s), "description") == 0) { strncpy(desc, cp_unquote(eq+1), 255); desc[255]='\0'; }
    }
    if (section[0] && count < max) { strncpy(out[count].name, section, 63); strncpy(out[count].desc, desc, 255); count++; }
    fclose(f);
    return count;
}

// Interactive package picker: browse/filter the registry, preview a recipe, and
// confirm before pulling it. Runs only on a TTY (main gates this); everywhere
// else `wyn add` with no name falls back to the plain list. Line-oriented (no
// raw-mode/ncurses) so it works over pipes, CI, and every platform.
int wyn_cpkg_tui(const char* wyn_root, const char* cc) {
    RecipeBrief recipes[256];
    int total = registry_collect(wyn_root, recipes, 256);
    if (total == 0) { printf("No curated packages found.\n"); return 0; }

    char filter[64] = "";
    for (;;) {
        // Render the filtered list with 1-based indices.
        int idx[256], shown = 0;
        printf("\n\033[1mCurated C packages\033[0m");
        if (filter[0]) printf("  (filter: \"%s\")", filter);
        printf("\n");
        for (int i = 0; i < total; i++) {
            if (filter[0] && !strstr(recipes[i].name, filter) && !strstr(recipes[i].desc, filter)) continue;
            idx[shown++] = i;
            printf("  \033[36m%2d\033[0m  \033[1m%-12s\033[0m %s\n", shown, recipes[i].name, recipes[i].desc);
        }
        if (shown == 0) printf("  (no packages match \"%s\")\n", filter);
        printf("\nEnter a \033[1mnumber\033[0m to add, \033[1m/text\033[0m to filter, or \033[1mq\033[0m to quit: ");
        fflush(stdout);

        char inbuf[128];
        if (!fgets(inbuf, sizeof(inbuf), stdin)) { printf("\n"); return 0; }
        char* in = cp_trim(inbuf);
        if (!*in) continue;
        if (strcmp(in, "q") == 0 || strcmp(in, "quit") == 0) return 0;
        if (in[0] == '/') { strncpy(filter, in + 1, sizeof(filter)-1); filter[sizeof(filter)-1]='\0'; continue; }

        // A number selects a package from the currently-shown list.
        char* endp = NULL;
        long sel = strtol(in, &endp, 10);
        if (endp != in && (*endp == '\0') && sel >= 1 && sel <= shown) {
            RecipeBrief* r = &recipes[idx[sel - 1]];
            // Preview the full recipe.
            Recipe full;
            registry_scan(wyn_root, r->name, &full);
            printf("\n\033[1m%s\033[0m — %s\n", full.name, full.description[0] ? full.description : r->desc);
            if (full.version[0])      printf("  version:  %s\n", full.version);
            if (full.libs[0])         printf("  links:    %s\n", full.libs);
            if (full.headers[0])      printf("  bindings: %s\n", full.headers);
            if (full.include_dirs[0]) printf("  includes: %s\n", full.include_dirs);
            printf("\nAdd \033[1m%s\033[0m to this project? [y/N] ", full.name);
            fflush(stdout);
            char yn[16];
            if (fgets(yn, sizeof(yn), stdin)) {
                char* y = cp_trim(yn);
                if (y[0] == 'y' || y[0] == 'Y') {
                    printf("\n");
                    return wyn_cpkg_add(full.name, wyn_root, cc);
                }
            }
            printf("Cancelled.\n");
            continue;
        }
        // Not a number/filter/quit: treat as a direct package name.
        for (int i = 0; i < total; i++) {
            if (strcmp(recipes[i].name, in) == 0) return wyn_cpkg_add(in, wyn_root, cc);
        }
        printf("No package named \"%s\" (use a number or /filter).\n", in);
    }
}

// Run `pkg-config --cflags-only-I <name>` and capture the -I flags (macOS/Linux).
// Returns 1 and fills `out` on success, 0 otherwise. Cross-platform-safe: if
// pkg-config is absent (e.g. Windows), returns 0 and the caller proceeds without
// extra include dirs (system headers on the default search path still resolve).
static int pkgconfig_cflags(const char* name, char* out, size_t sz) {
    out[0] = '\0';
#ifdef _WIN32
    // pkg-config isn't standard on Windows; skip it (caller proceeds without the
    // extra -I dirs — system headers still resolve on the default search path).
    (void)name; (void)sz;
    return 0;
#else
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pkg-config --cflags-only-I %s 2>/dev/null", name);
    FILE* p = popen(cmd, "r");
    if (!p) return 0;
    size_t got = fread(out, 1, sz - 1, p);
    int rc = pclose(p);
    if (rc != 0 || got == 0) { out[0] = '\0'; return 0; }
    out[got] = '\0';
    // strip trailing newline
    char* nl = strchr(out, '\n'); if (nl) *nl = '\0';
    return out[0] ? 1 : 0;
#endif
}

// Append " key = \"val\"" style lines into the [ffi] section of ./wyn.toml,
// merging with any existing values. Minimal approach: rewrite the file with an
// [ffi] block reflecting the union of existing + new libs/lib_dirs/include_dirs.
// To keep it simple and robust we append a fresh [ffi] block if none exists, or
// append the new libs to the existing one. (The toml parser reads the last value
// per key, and the build path concatenates -l flags, so appended libs take effect.)
static int merge_ffi_into_manifest(const Recipe* r, const char* extra_includes) {
    // Build the additions.
    // If wyn.toml doesn't exist, create a minimal one.
    FILE* f = fopen("wyn.toml", "r");
    int had_ffi = 0;
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char* s = cp_trim(line);
            if (strncmp(s, "[ffi]", 5) == 0) { had_ffi = 1; break; }
        }
        fclose(f);
    }
    FILE* out = fopen("wyn.toml", f ? "a" : "w");
    if (!out) { fprintf(stderr, "Error: cannot write wyn.toml\n"); return 1; }
    if (!f) {
        fprintf(out, "[project]\nname = \"app\"\nversion = \"0.1.0\"\n\n");
    }
    // A per-package [ffi] append block. Repeated [ffi] sections + repeated keys
    // are fine: the build path accumulates -l flags from each libs value it sees,
    // and this keeps `wyn add` idempotent-friendly and simple.
    fprintf(out, "\n# added by `wyn add %s`\n[ffi]\n", r->name);
    if (r->libs[0])     fprintf(out, "libs = \"%s\"\n", r->libs);
    if (r->lib_dirs[0]) fprintf(out, "lib_dirs = \"%s\"\n", r->lib_dirs);
    // include dirs: recipe + anything pkg-config found (strip -I prefixes to bare dirs)
    char inc[1024]; inc[0] = '\0';
    if (r->include_dirs[0]) { strncpy(inc, r->include_dirs, sizeof(inc)-1); }
    if (extra_includes && extra_includes[0]) {
        // convert "-I/a -I/b" -> "/a /b"
        char tmp[1024]; strncpy(tmp, extra_includes, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
        char* st=NULL;
        for (char* t = strtok_r(tmp, " ", &st); t; t = strtok_r(NULL, " ", &st)) {
            const char* dir = (strncmp(t, "-I", 2) == 0) ? t + 2 : t;
            if (!*dir) continue;
            size_t n = strlen(inc);
            snprintf(inc + n, sizeof(inc) - n, "%s%s", n ? " " : "", dir);
        }
    }
    if (inc[0]) fprintf(out, "include_dirs = \"%s\"\n", inc);
    fclose(out);
    (void)had_ffi;
    return 0;
}

int wyn_cpkg_add(const char* name, const char* wyn_root, const char* cc) {
    Recipe r;
    if (!registry_scan(wyn_root, name, &r)) {
        fprintf(stderr, "Error: no curated C package named '%s'.\n", name);
        fprintf(stderr, "Run `wyn add` with no name to list available packages.\n");
        return 1;
    }
    printf("Adding C package '%s' — %s\n", r.name, r.description);

    // 1. Resolve include dirs (pkg-config, best-effort; absent on some platforms).
    char pc_includes[1024] = "";
    if (pkgconfig_cflags(r.name, pc_includes, sizeof(pc_includes)))
        printf("  pkg-config: %s\n", pc_includes);

    // 2. Create ./packages/<name>/ and generate bindings for each header.
    char pkgdir[512]; snprintf(pkgdir, sizeof(pkgdir), "packages/%s", r.name);
    if (make_dirs(pkgdir) != 0) { fprintf(stderr, "Error: cannot create %s\n", pkgdir); return 1; }

    char binding_path[600]; snprintf(binding_path, sizeof(binding_path), "%s/%s.wyn", pkgdir, r.name);
    FILE* bf = fopen(binding_path, "w");
    if (!bf) { fprintf(stderr, "Error: cannot write %s\n", binding_path); return 1; }
    fprintf(bf, "// Bindings for C package '%s' — generated by `wyn add`.\n\n", r.name);

    int total = 0, ok = 0;
    if (r.headers[0]) {
        char hdrs[512]; strncpy(hdrs, r.headers, sizeof(hdrs)-1); hdrs[sizeof(hdrs)-1]=0;
        char* st = NULL;
        for (char* h = strtok_r(hdrs, ", \t", &st); h; h = strtok_r(NULL, ", \t", &st)) {
            total++;
            fprintf(bf, "// --- from %s ---\n", h);
            // include-flag string for bindgen: recipe include_dirs + pkg-config
            char iflags[1200] = "";
            if (r.include_dirs[0]) {
                char tmp[512]; strncpy(tmp,r.include_dirs,sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
                char* s2=NULL; for (char* d=strtok_r(tmp,", \t",&s2); d; d=strtok_r(NULL,", \t",&s2)) {
                    size_t n=strlen(iflags); snprintf(iflags+n,sizeof(iflags)-n,"%s-I%s",n?" ":"",d); }
            }
            if (pc_includes[0]) { size_t n=strlen(iflags); snprintf(iflags+n,sizeof(iflags)-n,"%s%s",n?" ":"",pc_includes); }
            if (wyn_bindgen(h, cc, iflags, bf) == 0) ok++;
            fprintf(bf, "\n");
        }
    }
    fclose(bf);
    printf("  bound %d/%d header(s) → %s\n", ok, total, binding_path);

    // 3. Merge link flags into wyn.toml [ffi].
    if (merge_ffi_into_manifest(&r, pc_includes) != 0) return 1;
    printf("  linked via wyn.toml [ffi]: libs = \"%s\"\n", r.libs);

    printf("\n\033[32m✓\033[0m Added '%s'. Bindings in %s; `import` it or copy the extern fns.\n", r.name, binding_path);
    if (ok < total)
        printf("  Note: %d header(s) could not be found/preprocessed — install the dev package or set [ffi].include_dirs.\n", total - ok);
    return 0;
}
