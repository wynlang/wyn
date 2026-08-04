#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "module.h"
#include "package.h"
#include "growable.h"

static char** module_paths = NULL;
static int module_path_count = 0;
static int module_path_cap = 0;

// Circular import detection (growable)
static char** loading_stack = NULL;
static int loading_stack_count = 0;
static int loading_stack_cap = 0;
static bool circular_import_detected = false;

bool has_circular_import(void) {
    return circular_import_detected;
}

// An import that could not be resolved. Like circular_import_detected, this is a
// LATCH: load_module already prints a precise error ("Module 'x' not found" /
// "Package 'x' not installed") and then returns NULL, and every caller treats NULL
// as "carry on without it" - so the compiler printed an error, said "✓ no errors",
// exited 0, and emitted C with a hole in it (`long long ui = ;`, because the call
// on the missing module had no type). Three contradictory statements about one
// build. The flag lets the entry points stop before codegen, exactly as they
// already do for circular imports.
static bool unresolved_import_detected = false;

bool has_unresolved_import(void) {
    return unresolved_import_detected;
}

void add_module_path(const char* path) {
    WYN_ENSURE_CAP(module_paths, module_path_count, module_path_cap);
    module_paths[module_path_count++] = strdup(path);
}

// The mtime of the most recently modified module file actually LOADED this run.
//
// `wyn run` keeps <file>.out as an incremental cache and reused it whenever the cache
// was newer than the ENTRY file and the compiler. It never looked at the imports - so
// editing a module and re-running a program that imports it silently ran the OLD
// binary. That is a gate-integrity bug, not a nuisance: a test suite can report green
// on code it never compiled, and a mutation test can report "no failures" for a
// mutant that was never built.
//
// A single newest-mtime is enough for the cache decision and costs one stat per
// module, so it does not need a list of paths.
static time_t newest_module_mtime = 0;

time_t get_newest_module_mtime(void) {
    return newest_module_mtime;
}

static void note_module_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && st.st_mtime > newest_module_mtime) {
        newest_module_mtime = st.st_mtime;
    }
}

// Newest mtime among the modules `source` imports, WITHOUT loading them.
//
// The `wyn run` cache decision happens before preload_imports, so it cannot use the
// figure recorded during loading - by then the decision is already made. This walks
// the same import syntax preload_imports does and only stats what it resolves, which
// is cheap enough to run on every `wyn run`.
//
// Deliberately SHALLOW: it sees a program's direct imports, not their imports. A
// deeper change is transitive closure, and the shallow answer already fixes the
// reported bug (edit a module, re-run the program that imports it). A transitively
// stale cache remains possible and is called out in the test.
time_t scan_import_mtimes(const char* source) {
    time_t newest = 0;
    if (!source) return 0;
    const char* p = source;
    bool in_comment = false, in_line_comment = false;
    while (*p) {
        if (!in_comment && !in_line_comment && *p == '/' && *(p+1) == '/') { in_line_comment = true; p += 2; continue; }
        if (!in_comment && !in_line_comment && *p == '/' && *(p+1) == '*') { in_comment = true; p += 2; continue; }
        if (in_comment && *p == '*' && *(p+1) == '/') { in_comment = false; p += 2; continue; }
        if (in_line_comment && *p == '\n') { in_line_comment = false; p++; continue; }
        if (in_comment || in_line_comment) { p++; continue; }
        // Strings, for the same reason preload_imports skips them: `test "import x"`
        // is a test NAME, not an import.
        if (*p == '"' || *p == '\'') {
            char q = *p; p++;
            while (*p && *p != q) { if (*p == '\\' && *(p+1)) p++; p++; }
            if (*p) p++;
            continue;
        }
        if (strncmp(p, "import ", 7) == 0) {
            p += 7;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '{') {   // selective: import { a, b } from mod
                while (*p && strncmp(p, " from ", 6) != 0) p++;
                if (strncmp(p, " from ", 6) == 0) { p += 6; while (*p == ' ' || *p == '\t') p++; }
            }
            char name[256];
            int len = 0;
            while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '_' || *p == '.') && len < 255) {
                name[len++] = (*p == '.') ? '/' : *p;
                p++;
            }
            name[len] = '\0';
            if (len > 0 && !is_builtin_module(name)) {
                char* mp = resolve_module_path(name);
                if (mp) {
                    struct stat st;
                    if (stat(mp, &st) == 0 && st.st_mtime > newest) newest = st.st_mtime;
                    free(mp);
                }
            }
            continue;
        }
        p++;
    }
    return newest;
}

static bool is_in_loading_stack(const char* module_name) {
    for (int i = 0; i < loading_stack_count; i++) {
        if (strcmp(loading_stack[i], module_name) == 0) {
            return true;
        }
    }
    return false;
}

static void push_loading_stack(const char* module_name) {
    WYN_ENSURE_CAP(loading_stack, loading_stack_count, loading_stack_cap);
    loading_stack[loading_stack_count++] = strdup(module_name);
}

static void pop_loading_stack() {
    if (loading_stack_count > 0) {
        free(loading_stack[--loading_stack_count]);
    }
}

static void print_circular_import_error(const char* module_name) {
    fprintf(stderr, "\033[31mError:\033[0m Circular import detected: ");
    for (int i = 0; i < loading_stack_count; i++) {
        fprintf(stderr, "%s → ", loading_stack[i]);
    }
    fprintf(stderr, "%s\n", module_name);
    fprintf(stderr, "  Break the cycle by removing one of the imports or extracting shared code into a third module.\n");
}

// Check if module is built-in (has C implementation)
bool is_builtin_module(const char* module_name) {
    const char* builtins[] = {
        "math", "Math", "File", "System", "Path", "DateTime", "Time",
        "Json", "Http", "Regex", "Random", "HashMap", "HashSet", "Terminal", "Color",
        "Test", "Env", "Net", "Url", "Task", "Db", "Gui", "Audio", "StringBuilder", "Crypto", "Encoding", "Os", "Uuid", "Log", "Process", "Csv", "Template", "String", "Data", "Socket", "Ws", "Args", "Base64", "Toml", "Bcrypt", "Web", "Smtp", "App", "Shared", "Ptr", NULL
    };
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(module_name, builtins[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Pre-scan source for imports and load them
void preload_imports(const char* source) {
    // Easter egg: import wisdom
    if (strstr(source, "import wisdom")) {
        extern void print_flight_rules();
        print_flight_rules();
        exit(0);
    }
    const char* p = source;
    bool in_comment = false;
    bool in_line_comment = false;
    
    while (*p) {
        // Track comments
        if (!in_comment && !in_line_comment && *p == '/' && *(p+1) == '/') {
            in_line_comment = true;
            p += 2;
            continue;
        }
        if (!in_comment && !in_line_comment && *p == '/' && *(p+1) == '*') {
            in_comment = true;
            p += 2;
            continue;
        }
        if (in_comment && *p == '*' && *(p+1) == '/') {
            in_comment = false;
            p += 2;
            continue;
        }
        if (in_line_comment && *p == '\n') {
            in_line_comment = false;
            p++;
            continue;
        }
        
        // Skip if in comment
        if (in_comment || in_line_comment) {
            p++;
            continue;
        }

        // SKIP STRING LITERALS. This scanner tracked comments but not strings, so the
        // word "import" inside any string was read as a real import statement:
        //
        //   test "import finds src/mathx.wyn" { ... }   ->   import module "finds"
        //
        // It printed a spurious "Module 'finds' not found" and carried on, which is why
        // it went unnoticed - the error was cosmetic noise on an otherwise passing
        // build. It stops being cosmetic the moment an unresolved import is fatal, and
        // a test NAME describing what it tests is a completely reasonable thing to
        // write. Both quote forms, with backslash escapes honoured.
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p+1)) p++;   // skip the escaped char, not the quote
                p++;
            }
            if (*p) p++;                          // consume the closing quote
            continue;
        }

        // Look for "import " keyword
        if (strncmp(p, "import ", 7) == 0) {
            p += 7;
            // Skip whitespace
            while (*p == ' ' || *p == '\t') p++;
            
            // Check for selective import: import { ... } from module
            if (*p == '{') {
                // Skip to "from"
                while (*p && strncmp(p, " from ", 6) != 0) p++;
                if (strncmp(p, " from ", 6) == 0) {
                    p += 6;
                    while (*p == ' ' || *p == '\t') p++;
                }
            }
            
            // Check for relative imports
            char module_name[256];
            int len = 0;
            
            // Handle root::, self::
            if (strncmp(p, "root::", 6) == 0) {
                strcpy(module_name, "crate/");
                len = 6;
                p += 6;
            } else if (strncmp(p, "self::", 6) == 0) {
                strcpy(module_name, "self/");
                len = 5;
                p += 6;
            }
            
            // Extract module name (including . for nested modules, convert to /).
            // Digits are part of an identifier (e.g. `sqlite3`) - must match the
            // lexer's isalnum rule, else a trailing digit is dropped and the name
            // is truncated ("sqlite3" -> "sqlite"), breaking module resolution.
            while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '_' || *p == '.') && len < 255) {
                if (*p == '.') {
                    module_name[len++] = '/';  // Convert . to /
                } else {
                    module_name[len++] = *p;
                }
                p++;
            }
            module_name[len] = '\0';

            if (len > 0) {
                // Skip optional "as alias"
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, "as ", 3) == 0) {
                    p += 3;
                    // Skip alias name
                    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                           (*p >= '0' && *p <= '9') || *p == '_') p++;
                }
                
                // Load module
                load_module(module_name);
            }
        }
        p++;
    }
}

static char source_directory[512] = ".";
static char current_module_path[512] = "";

void set_current_module_path(const char* path) {
    if (path) {
        strncpy(current_module_path, path, 511);
        current_module_path[511] = '\0';
    } else {
        current_module_path[0] = '\0';
    }
}

// Resolve relative module paths
static char* resolve_relative_path(const char* module_name) {
    if (strncmp(module_name, "crate/", 6) == 0) {
        // Absolute from root - just remove crate/ prefix
        return strdup(module_name + 6);
        
    } else if (strncmp(module_name, "self/", 5) == 0) {
        // Same directory as current module
        if (current_module_path[0] == '\0') {
            return strdup(module_name + 5);
        }
        
        char* last_slash = strrchr(current_module_path, '/');
        if (!last_slash) {
            return strdup(module_name + 5);
        }
        
        char resolved[512];
        int dir_len = last_slash - current_module_path;
        snprintf(resolved, 512, "%.*s/%s", dir_len, current_module_path, module_name + 5);
        return strdup(resolved);
    }
    
    // Not relative
    return strdup(module_name);
}

// Public wrapper for codegen
char* resolve_relative_module_name(const char* module_name) {
    return resolve_relative_path(module_name);
}


void set_source_directory(const char* source_file) {
    // Extract directory from source file path
    const char* last_slash = strrchr(source_file, '/');
    if (last_slash) {
        int len = last_slash - source_file;
        if (len < 512) {
            memcpy(source_directory, source_file, len);
            source_directory[len] = '\0';
        }
    } else {
        strcpy(source_directory, ".");
    }
}

// Try .wyn then .🐉 extension
#define TRY_RESOLVE(fmt, ...) do { \
    snprintf(path, sizeof(path), fmt ".wyn", __VA_ARGS__); \
    if (stat(path, &st) == 0) return strdup(path); \
    snprintf(path, sizeof(path), fmt ".🐉", __VA_ARGS__); \
    if (stat(path, &st) == 0) return strdup(path); \
} while(0)

char* resolve_module_path(const char* module_name) {
    char path[512];
    struct stat st;
    
    // 1. Source file directory
    TRY_RESOLVE("%s/%s", source_directory, module_name);
    
    // 2. Parent directory of source file
    char parent_dir[512];
    strcpy(parent_dir, source_directory);
    char* last_slash = strrchr(parent_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        TRY_RESOLVE("%s/%s", parent_dir, module_name);
    }
    
    // 3. Source file directory + modules/
    TRY_RESOLVE("%s/modules/%s", source_directory, module_name);
    
    // 4. Current directory
    TRY_RESOLVE("%s", module_name);

    // 4b. ./src/ - the layout `wyn init --template lib` scaffolds.
    //
    // `wyn test` compiles each test file on its own (cmd_test.c shells out to
    // `wyn build <file>`), so source_directory is always tests/ and candidates
    // 1-3 cannot see a sibling src/. Without this the one layout the tooling
    // itself creates is unimportable from a test, and projects resort to
    // committing ./<name>.wyn -> src/<name>.wyn symlinks to satisfy candidate 4.
    //
    // Consistent with the git-dep branch below, which already tries
    // `<cache>/src/<name>.wyn`: a fetched dependency's src/ was searched while
    // the local project's was not.
    TRY_RESOLVE("./src/%s", module_name);

    // 5. ./modules/ directory
    TRY_RESOLVE("./modules/%s", module_name);
    
    // 5. ./wyn_modules/ directory
    TRY_RESOLVE("./wyn_modules/%s", module_name);

    // 5c. Git-URL dependency in the global cache. `wyn add <name|url>` records the
    // package in wyn.toml [dependencies] and clones it into ~/.wyn/pkg/…; resolve
    // the import name → its cache dir, then try the usual library layouts inside.
    {
        extern int wyn_dep_resolve(const char* import_name, char* dir_out, size_t n);
        char dep_dir[600];
        if (wyn_dep_resolve(module_name, dep_dir, sizeof(dep_dir))) {
            TRY_RESOLVE("%s/%s", dep_dir, module_name);      // <cache>/<name>.wyn
            TRY_RESOLVE("%s/src/%s", dep_dir, module_name);  // <cache>/src/<name>.wyn
            TRY_RESOLVE("%s/src/main", dep_dir);             // <cache>/src/main.wyn
        }
    }

    // 5d. Legacy project-local packages: ./packages/<name>/<name>.wyn - where the
    // curated C-package `wyn add <clib>` still places generated bindings, and
    // pre-git-deps checkouts kept pure-Wyn packages. Both the flat file and the
    // nested <name>/<name> layout are tried (relative to cwd and the source dir).
    TRY_RESOLVE("./packages/%s/%s", module_name, module_name);
    TRY_RESOLVE("%s/packages/%s/%s", source_directory, module_name, module_name);
    TRY_RESOLVE("./packages/%s", module_name);

    // 6. User packages: ~/.wyn/packages/module_name/module_name.wyn
    const char* home = getenv("HOME");
    if (home) {
        snprintf(path, sizeof(path), "%s/.wyn/packages/%s/%s.wyn", home, module_name, module_name);
        if (stat(path, &st) == 0) return strdup(path);
        
        // 7. User modules: ~/.wyn/modules/
        snprintf(path, sizeof(path), "%s/.wyn/modules/%s.wyn", home, module_name);
        if (stat(path, &st) == 0) return strdup(path);
    }
    
    // 8. System modules
    snprintf(path, sizeof(path), "/usr/local/lib/wyn/modules/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 9. Standard library (relative to compiler)
    snprintf(path, sizeof(path), "./stdlib/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    snprintf(path, sizeof(path), "../stdlib/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 10. Test directory (for development)
    snprintf(path, sizeof(path), "/tmp/wyn_modules/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 11. Custom module paths
    for (int i = 0; i < module_path_count; i++) {
        snprintf(path, sizeof(path), "%s/%s.wyn", module_paths[i], module_name);
        if (stat(path, &st) == 0) return strdup(path);
    }
    
    return NULL;
}

Program* load_module(const char* module_name) {
    // Resolve relative paths
    char* resolved_name = resolve_relative_path(module_name);
    if (!resolved_name) {
        return NULL;
    }
    
    // Check if already loaded
    extern bool is_module_loaded(const char* name);
    extern Program* get_module(const char* name);
    
    if (is_module_loaded(resolved_name)) {
        Program* prog = get_module(resolved_name);
        free(resolved_name);
        return prog;
    }
    
    // Check for circular import
    if (is_in_loading_stack(resolved_name)) {
        print_circular_import_error(resolved_name);
        circular_import_detected = true;
        free(resolved_name);
        return NULL;
    }
    
    // Add to loading stack
    push_loading_stack(resolved_name);
    
    char* path = resolve_module_path(resolved_name);
    if (!path) {
        // Built-in modules (math, Math, File, System, Http, …) have no .wyn file
        // - they're provided directly by codegen. `import math` must NOT print a
        // spurious "Module not found" error just because there's no source file.
        if (is_builtin_module(resolved_name)) {
            pop_loading_stack();
            free(resolved_name);
            return NULL;   // silent: not an error, resolved elsewhere
        }
        // Deduplicate: only print error once per module name
        static char _reported[32][64];
        static int _reported_count = 0;
        int _already = 0;
        for (int i = 0; i < _reported_count; i++) { if (strcmp(_reported[i], resolved_name) == 0) { _already = 1; break; } }
        if (!_already) {
            if (_reported_count < 32) { strncpy(_reported[_reported_count], resolved_name, 63); _reported_count++; }
            // Check if it's a known package name
            static const char* known_pkgs[] = {"redis","sqlite","pg","mysql","bcrypt","jwt","smtp","yaml","xml","csv","markdown","websocket","http-client","https","image","audio","gui","log","cache","cron","dotenv","retry","validate","web","tar","zip","table","color","args","base64","opengl","sdl","wgpu","raylib",NULL};
            int is_pkg = 0;
            for (int i = 0; known_pkgs[i]; i++) { if (strcmp(known_pkgs[i], resolved_name) == 0) { is_pkg = 1; break; } }
            if (is_pkg) {
                fprintf(stderr, "\033[31mError:\033[0m Package '%s' not installed\n", resolved_name);
                fprintf(stderr, "  Run: \033[1mwyn pkg install %s\033[0m\n", resolved_name);
            } else {
                fprintf(stderr, "\033[31mError:\033[0m Module '%s' not found\n", resolved_name);
            }
        }
        // Set OUTSIDE the dedup guard: the guard only suppresses repeat PRINTING, and
        // a second import of the same missing module must still fail the build.
        // Builtins returned above without reaching here, so this cannot fire for them.
        unresolved_import_detected = true;
        pop_loading_stack();
        free(resolved_name);
        return NULL;
    }
    
    // Read module file
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open module '%s'\n", path);
        // Resolved to a path that will not open (permissions, a dangling symlink, a
        // race). Same consequence as not finding it at all: the module's symbols are
        // absent, so codegen must not run.
        unresolved_import_detected = true;
        free(path);
        pop_loading_stack();
        return NULL;
    }
    // Recorded here, on the path that actually OPENED, so the `wyn run` cache
    // invalidates when any imported module is newer than the cached binary.
    note_module_mtime(path);
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    // `path` is freed at the end of this function (after the module is parsed +
    // registered); the AST tokens point into `source`, not `path`.

    // Save current module path before loading imports
    char saved_module_path[512];
    strncpy(saved_module_path, current_module_path, 511);
    saved_module_path[511] = '\0';
    
    // Recursively preload any imports in this module
    // Set current module path for relative imports
    set_current_module_path(resolved_name);
    preload_imports(source);
    
    // Restore module path
    set_current_module_path(saved_module_path);
    
    // Save parser state
    extern void save_parser_state();
    extern void restore_parser_state();
    
    save_parser_state();
    
    // Parse module with fresh state
    extern void init_lexer(const char* source);
    extern void init_parser();
    extern Program* parse_program();
    
    init_lexer(source);
    init_parser();
    Program* prog = parse_program();
    
    // Restore original parser state
    restore_parser_state();
    
    // DON'T free source - the AST tokens point to it!
    // It will be freed when the program exits
    
    // Register module
    if (prog) {
        extern void register_module(const char* name, Program* ast);
        register_module(resolved_name, prog);
    }

    // Remove from loading stack
    pop_loading_stack();

    free(path);
    free(resolved_name);
    return prog;
}

// Type check all loaded modules (call after init_checker)
void check_all_modules(void) {
    extern void check_program(Program* prog);
    extern int get_module_count();
    extern Program* get_module_at(int index);
    extern const char* get_module_name_by_ast(Program* ast);
    extern void set_current_module(const char* name);
    
    int count = get_module_count();
    for (int i = 0; i < count; i++) {
        Program* prog = get_module_at(i);
        if (prog) {
            // Set current module for visibility checking
            const char* module_name = get_module_name_by_ast(prog);
            set_current_module(module_name);
            
            // Run full check on each module
            check_program(prog);
        }
    }
    
    // Clear module context
    set_current_module(NULL);
}
