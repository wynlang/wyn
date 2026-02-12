#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "module.h"
#include "package.h"

static char* module_paths[16];
static int module_path_count = 0;

// Circular import detection
static char* loading_stack[32];
static int loading_stack_count = 0;

void add_module_path(const char* path) {
    if (module_path_count < 16) {
        module_paths[module_path_count++] = strdup(path);
    }
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
    if (loading_stack_count < 32) {
        loading_stack[loading_stack_count++] = strdup(module_name);
    }
}

static void pop_loading_stack() {
    if (loading_stack_count > 0) {
        free(loading_stack[--loading_stack_count]);
    }
}

static void print_circular_import_error(const char* module_name) {
    fprintf(stderr, "Error: Circular import detected: ");
    for (int i = 0; i < loading_stack_count; i++) {
        fprintf(stderr, "%s -> ", loading_stack[i]);
    }
    fprintf(stderr, "%s\n", module_name);
}

// Check if module is built-in (has C implementation)
bool is_builtin_module(const char* module_name) {
    const char* builtins[] = {
        "math", "Math", "File", "System", "Path", "DateTime", 
        "Json", "Http", "Regex", "Random", "HashMap", "HashSet", "Terminal",
        "Test", "Env", "Net", "Url", "Task", "Db", "Gui", "Audio", "StringBuilder", "Crypto", "Encoding", "Os", "Uuid", "Log", "Process", NULL
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
            
            // Extract module name (including . for nested modules, convert to /)
            while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '.') && len < 255) {
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
                    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') p++;
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

char* resolve_module_path(const char* module_name) {
    char path[512];
    struct stat st;
    
    // 1. Source file directory
    snprintf(path, sizeof(path), "%s/%s.wyn", source_directory, module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 2. Parent directory of source file
    char parent_dir[512];
    strcpy(parent_dir, source_directory);
    char* last_slash = strrchr(parent_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        snprintf(path, sizeof(path), "%s/%s.wyn", parent_dir, module_name);
        if (stat(path, &st) == 0) return strdup(path);
    }
    
    // 3. Source file directory + modules/
    snprintf(path, sizeof(path), "%s/modules/%s.wyn", source_directory, module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 4. Current directory
    snprintf(path, sizeof(path), "%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 5. ./modules/ directory
    snprintf(path, sizeof(path), "./modules/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
    // 5. ./wyn_modules/ directory
    snprintf(path, sizeof(path), "./wyn_modules/%s.wyn", module_name);
    if (stat(path, &st) == 0) return strdup(path);
    
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
        free(resolved_name);
        return NULL;
    }
    
    // Add to loading stack
    push_loading_stack(resolved_name);
    
    char* path = resolve_module_path(resolved_name);
    if (!path) {
        fprintf(stderr, "Error: Module '%s' not found\n", resolved_name);
        pop_loading_stack();
        free(resolved_name);
        return NULL;
    }
    
    // Read module file
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open module '%s'\n", path);
        free(path);
        pop_loading_stack();
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    free(path);
    
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
        
        // Read package manifest if available
        extern PackageInfo* read_package_manifest(const char* module_path);
        PackageInfo* pkg = read_package_manifest(path);
        if (pkg) {
            // Package manifest found - could validate version, dependencies, etc.
            // For now, just acknowledge it exists
            extern void free_package_info(PackageInfo* info);
            free_package_info(pkg);
        }
    }
    
    // Remove from loading stack
    pop_loading_stack();
    
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
