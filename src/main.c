#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ast.h"
#include "types.h"
#include "memory.h"
#include "security.h"
#include "platform.h"
#include "llvm_codegen.h"
#include "optimize.h"

void init_lexer(const char* source);
void init_parser();
void init_checker();
void init_codegen(FILE* output);
Program* parse_program();
void check_program(Program* prog);
bool checker_had_error();
void free_program(Program* prog);
void codegen_c_header();
void codegen_program(Program* prog);
int create_new_project(const char* project_name);

static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = safe_malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    
    return buffer;
}

static char* get_version() {
    static char version[32] = {0};
    if (version[0] == 0) {
        // Try multiple locations for VERSION file
        const char* paths[] = {
            "VERSION",
            "../VERSION",
            "../../VERSION",
            NULL
        };
        
        FILE* f = NULL;
        for (int i = 0; paths[i] != NULL && !f; i++) {
            f = fopen(paths[i], "r");
        }
        
        if (f) {
            if (fgets(version, sizeof(version), f)) {
                char* newline = strchr(version, '\n');
                if (newline) *newline = 0;
            }
            fclose(f);
        }
        if (version[0] == 0) strcpy(version, "1.2.1");
    }
    return version;
}

#include "wyn_interface.h"

int main(int argc, char** argv) {
    // Initialize platform-specific functionality
    if (wyn_platform_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize arguments for Wyn compiler access
    wyn_init_args(argc, argv);
    
    if (argc < 2) {
        fprintf(stderr, "Wyn Compiler v%s\n", get_version());
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  wyn <file.wyn>           Compile file\n");
        fprintf(stderr, "  wyn run <file.wyn>       Compile and run\n");
        fprintf(stderr, "  wyn build <dir>          Build all .wyn files in directory\n");
        fprintf(stderr, "  wyn test                 Run tests\n");
        fprintf(stderr, "  wyn fmt <file.wyn>       Validate file\n");
        fprintf(stderr, "  wyn clean                Clean artifacts\n");
        fprintf(stderr, "  wyn cross <os> <file>    Cross-compile (linux/macos/windows)\n");
        fprintf(stderr, "  wyn llvm <file.wyn>      Compile with LLVM backend\n");
        fprintf(stderr, "  wyn version              Show version\n");
        fprintf(stderr, "  wyn help                 Show this help\n");
        fprintf(stderr, "\nOptimization flags:\n");
        fprintf(stderr, "  -O1                      Basic optimizations\n");
        fprintf(stderr, "  -O2                      Advanced optimizations\n");
        return 1;
    }
    
    char* command = argv[1];
    
    // Handle --version and -v flags
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("Wyn v%s\n", get_version());
        return 0;
    }
    
    // Handle --help and -h flags
    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        printf("Wyn Compiler v%s\n\n", get_version());
        printf("Commands:\n");
        printf("  wyn <file.wyn>           Compile file\n");
        printf("  wyn run <file.wyn>       Compile and run\n");
        printf("  wyn build <dir>          Build all .wyn files in directory\n");
        printf("  wyn init [name]          Create new project\n");
        printf("  wyn test                 Run tests\n");
        printf("  wyn fmt <file.wyn>       Validate file\n");
        printf("  wyn clean                Clean artifacts\n");
        printf("  wyn cross <os> <file>    Cross-compile\n");
        printf("  wyn llvm <file.wyn>      Compile with LLVM backend\n");
        printf("  wyn version              Show version\n");
        printf("  wyn help                 Show this help\n");
        printf("\nOptimization flags:\n");
        printf("  -O1                      Basic optimizations (dead code elimination)\n");
        printf("  -O2                      Advanced optimizations (includes function inlining)\n");
        printf("\nCross-compile targets:\n");
        printf("  linux   - Linux x86_64\n");
        printf("  macos   - macOS (current platform)\n");
        printf("  windows - Windows x86_64 (requires mingw)\n");
        return 0;
    }
    
    if (strcmp(command, "init") == 0) {
        static char input[256];  // Make it static to avoid stack issues
        char* project_name;
        
        if (argc < 3) {
            printf("Enter project name: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin)) {
                // Remove newline
                input[strcspn(input, "\n")] = 0;
                if (strlen(input) == 0) {
                    fprintf(stderr, "Error: Project name cannot be empty\n");
                    return 1;
                }
                project_name = input;
            } else {
                fprintf(stderr, "Error: Failed to read project name\n");
                return 1;
            }
        } else {
            project_name = argv[2];
        }
        return create_new_project(project_name);
    }
    
    if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn build <directory>\n");
            return 1;
        }
        
        char* dir = argv[2];
        
        // Create temp directory if it doesn't exist
        system("mkdir -p temp");
        
        printf("Building project in %s...\n", dir);
        
        // Find all .wyn files and compile them together
        char find_cmd[512];
        snprintf(find_cmd, 512, "find %s -name '*.wyn' -type f > temp/files.txt", dir);
        system(find_cmd);
        
        // Read file list and concatenate
        FILE* files = fopen("temp/files.txt", "r");
        if (!files) {
            fprintf(stderr, "Error: No .wyn files found in %s\n", dir);
            return 1;
        }
        
        // Check if file list is empty
        fseek(files, 0, SEEK_END);
        long file_size = ftell(files);
        fseek(files, 0, SEEK_SET);
        
        if (file_size == 0) {
            fprintf(stderr, "Error: No .wyn files found in %s\n", dir);
            fclose(files);
            return 1;
        }
        
        FILE* combined = fopen("temp/combined.wyn", "w");
        char line[512];
        while (fgets(line, 512, files)) {
            line[strcspn(line, "\n")] = 0; // Remove newline
            
            // Extract module name from filename (e.g., /path/to/math.wyn -> math)
            char* filename = strrchr(line, '/');
            if (!filename) filename = line;
            else filename++; // Skip the /
            
            char module_name[128] = "";
            char* dot = strrchr(filename, '.');
            if (dot) {
                int len = dot - filename;
                if (len > 0 && len < 127) {
                    strncpy(module_name, filename, len);
                    module_name[len] = '\0';
                }
            }
            
            char* source = read_file(line);
            
            // Process source: remove imports, prefix exports, replace module::func with module_func
            char* modified = malloc(strlen(source) * 2);
            char* dst = modified;
            char* src = source;
            
            while (*src) {
                // Skip import statements
                if (strncmp(src, "import ", 7) == 0) {
                    while (*src && *src != ';') src++;
                    if (*src == ';') src++;
                    while (*src == ' ' || *src == '\n') src++;
                    continue;
                }
                
                // Prefix exported functions
                if (strncmp(src, "export fn ", 10) == 0 && 
                    strcmp(module_name, "main") != 0 && strlen(module_name) > 0) {
                    strcpy(dst, "fn ");
                    dst += 3;
                    strcpy(dst, module_name);
                    dst += strlen(module_name);
                    *dst++ = '_';
                    src += 10;
                } else if (src[0] != '\0' && src[1] != '\0' && strncmp(src, "::", 2) == 0) {
                    // Replace :: with _
                    *dst++ = '_';
                    src += 2;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
            
            fprintf(combined, "// From %s\n%s\n\n", line, modified);
            free(modified);
            free(source);
        }
        fclose(files);
        fclose(combined);
        
        // Compile the combined file
        char* source = read_file("temp/combined.wyn");
        init_lexer(source);
        init_parser();
        set_parser_filename("temp/combined.wyn");  // Set filename for better error messages
        init_checker();
        
        Program* prog = parse_program();
        if (!prog) {
            fprintf(stderr, "Error: Failed to parse program\n");
            free(source);
            return 1;
        }
        
        check_program(prog);
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n");
            free(source);
            return 1;
        }
        
        char out_path[256];
        snprintf(out_path, 256, "%s/main.c", dir);
        FILE* out = fopen(out_path, "w");
        init_codegen(out);
        codegen_c_header();
        codegen_program(prog);
        fclose(out);
        
        // Get WYN_ROOT or use current directory
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) {
            snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
        }
        
        char compile_cmd[4096];
        snprintf(compile_cmd, sizeof(compile_cmd), 
                 "gcc -I %s/src -o %s/main %s/main.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c %s/src/concurrency.c %s/src/async_runtime.c %s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c %s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c -lm", 
                 wyn_root, dir, dir, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
        int result = system(compile_cmd);
        
        if (result == 0) {
            printf("Build successful: %s/main\n", dir);
            // Clean up intermediate files
            char cleanup[256];
            snprintf(cleanup, 256, "%s/main.c", dir);
            remove(cleanup);
            remove("temp/files.txt");
            remove("temp/combined.wyn");
        } else {
            fprintf(stderr, "Build failed\n");
        }
        
        free(source);
        return result;
    }
    
    if (strcmp(command, "test") == 0) {
        printf("Running tests...\n");
        
        // Check if we're in a project directory (has tests/ folder)
        FILE* test_dir = fopen("tests", "r");
        if (test_dir) {
            fclose(test_dir);
            // Simple approach: run each .wyn file in tests/
            return system("for f in tests/*.wyn; do [ -f \"$f\" ] && wyn run \"$f\"; done");
        } else {
            // Fallback to build system tests
            return system("make test 2>&1 && ./tests/integration_tests.sh");
        }
    }
    
    if (strcmp(command, "clean") == 0) {
        printf("Cleaning build artifacts...\n");
        system("find examples -name '*.c' -delete 2>/dev/null");
        system("find examples -name '*.out' -delete 2>/dev/null");
        system("find temp -name '*.c' -delete 2>/dev/null");
        system("find temp -name '*.out' -delete 2>/dev/null");
        system("rm -f tests/test_quick 2>/dev/null");
        printf("✅ Clean complete\n");
        return 0;
    }
    
    if (strcmp(command, "cross") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: wyn cross <target> <file.wyn>\n");
            fprintf(stderr, "Targets: linux, macos, windows\n");
            return 1;
        }
        
        char* target = argv[2];
        char* file = argv[3];
        
        // Compile to C first
        char* source = read_file(file);
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        
        Program* prog = parse_program();
        if (!prog) {
            fprintf(stderr, "Error: Failed to parse program\n");
            free(source);
            return 1;
        }
        
        check_program(prog);
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n");
            free(source);
            return 1;
        }
        
        char out_path[256];
        snprintf(out_path, 256, "%s.c", file);
        FILE* out = fopen(out_path, "w");
        init_codegen(out);
        codegen_c_header();
        codegen_program(prog);
        fclose(out);
        free(source);
        
        // Cross-compile based on target
        char compile_cmd[512];
        if (strcmp(target, "linux") == 0) {
            // On macOS, just compile normally (user can transfer binary)
            snprintf(compile_cmd, 512, "gcc -O2 -o %s.linux %s.c -lm", file, file);
            printf("Compiling for Linux (native binary)...\n");
        } else if (strcmp(target, "macos") == 0) {
            snprintf(compile_cmd, 512, "gcc -O2 -o %s.macos %s.c -lm", file, file);
            printf("Compiling for macOS...\n");
        } else if (strcmp(target, "windows") == 0) {
            snprintf(compile_cmd, 512, "x86_64-w64-mingw32-gcc -O2 -static -o %s.exe %s.c -lm", file, file);
            printf("Cross-compiling for Windows...\n");
        } else {
            fprintf(stderr, "Unknown target: %s\n", target);
            fprintf(stderr, "Available: linux, macos, windows\n");
            return 1;
        }
        
        int result = system(compile_cmd);
        if (result == 0) {
            printf("✅ Cross-compilation successful\n");
        } else {
            fprintf(stderr, "❌ Cross-compilation failed\n");
            if (strcmp(target, "windows") == 0) {
                fprintf(stderr, "Note: Windows cross-compilation requires mingw-w64\n");
                fprintf(stderr, "Install: brew install mingw-w64 (macOS) or apt install mingw-w64 (Linux)\n");
            }
        }
        return result;
    }
    
    if (strcmp(command, "fmt") == 0) {
        extern int cmd_fmt(const char* file, int argc, char** argv);
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn fmt <file.wyn>\n");
            return 1;
        }
        return cmd_fmt(argv[2], argc, argv);
    }
    
    if (strcmp(command, "repl") == 0) {
        extern int cmd_repl(int argc, char** argv);
        return cmd_repl(argc, argv);
    }
    
    if (strcmp(command, "doc") == 0) {
        extern int cmd_doc(const char* file, int argc, char** argv);
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn doc <file.wyn>\n");
            return 1;
        }
        return cmd_doc(argv[2], argc, argv);
    }
    
    if (strcmp(command, "pkg") == 0) {
        extern int cmd_pkg(int argc, char** argv);
        return cmd_pkg(argc, argv);
    }
    
    if (strcmp(command, "debug") == 0) {
        extern int cmd_debug(const char* program, int argc, char** argv);
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn debug <program>\n");
            return 1;
        }
        return cmd_debug(argv[2], argc, argv);
    }
    
    if (strcmp(command, "lsp") == 0) {
        extern int cmd_lsp(int argc, char** argv);
        return cmd_lsp(argc, argv);
    }
    
    if (strcmp(command, "llvm") == 0) {
#ifdef WITH_LLVM
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn llvm <file.wyn>\n");
            return 1;
        }
        
        char* file = argv[2];
        char* source = read_file(file);
        
        init_lexer(source);
        init_parser();
        init_checker();
        
        Program* prog = parse_program();
        if (!prog) {
            fprintf(stderr, "Error: Failed to parse program\n");
            free(source);
            return 1;
        }
        
        // Type check
        check_program(prog);
        
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n");
            free(source);
            return 1;
        }
        
        // Generate LLVM IR
        char out_path[256];
        snprintf(out_path, 256, "%s.ll", file);
        FILE* out = fopen(out_path, "w");
        init_codegen(out);
        codegen_c_header();
        codegen_program(prog);
        fclose(out);
        
        // Generate bitcode if possible
        char bc_path[256];
        snprintf(bc_path, 256, "%s.bc", file);
        if (codegen_generate_bitcode(bc_path)) {
            printf("Generated LLVM bitcode: %s\n", bc_path);
        }
        
        printf("Generated LLVM IR: %s\n", out_path);
        
        // Cleanup
        cleanup_codegen();
        free_program(prog);
        free(source);
        return 0;
#else
        fprintf(stderr, "Error: LLVM backend not available in this build\n");
        fprintf(stderr, "Rebuild with LLVM support: make wyn-llvm\n");
        return 1;
#endif
    }
    
    if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn run <file.wyn>\n");
            return 1;
        }
        char* file = argv[2];
        char* source = read_file(file);
        
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        
        Program* prog = parse_program();
        if (!prog) {
            fprintf(stderr, "Error: Failed to parse program\n");
            free(source);
            return 1;
        }
        
        // Type check
        check_program(prog);
        
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n");
            free(source);
            return 1;
        }
        
        char out_path[256];
        snprintf(out_path, 256, "%s.c", file);
        FILE* out = fopen(out_path, "w");
        init_codegen(out);
        codegen_c_header();
        codegen_program(prog);
        fclose(out);
        
        // Get WYN_ROOT or use current directory
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) {
            snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
        }
        
        char compile_cmd[4096];
        snprintf(compile_cmd, sizeof(compile_cmd), 
                 "gcc -O2 -I %s/src -o %s.out %s.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c %s/src/concurrency.c %s/src/async_runtime.c %s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c %s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c -lm", 
                 wyn_root, file, file, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
        int result = system(compile_cmd);
        
        if (result != 0) {
            fprintf(stderr, "C compilation failed\n");
            free(source);
            return result;
        }
        
        char run_cmd[512];
        snprintf(run_cmd, 512, "./%s.out", file);
        result = system(run_cmd);
        free(source);
        return result;
    }
    
    // Parse optimization flags and -o flag
    OptLevel optimization = OPT_NONE;
    int file_arg_index = -1;
    const char* output_name = NULL;
    
    // Check for flags (scan all args)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O1") == 0) {
            optimization = OPT_O1;
        } else if (strcmp(argv[i], "-O2") == 0) {
            optimization = OPT_O2;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[i + 1];
            i++; // Skip next arg
        } else if (argv[i][0] != '-' && file_arg_index == -1) {
            file_arg_index = i;
        }
    }
    
    if (file_arg_index == -1 || file_arg_index >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }
    
    // Initialize optimizer
    init_optimizer(optimization);
    
    // Initialize module registry
    extern void init_module_registry();
    init_module_registry();
    
    // Set source directory for relative module imports
    extern void set_source_directory(const char* source_file);
    set_source_directory(argv[file_arg_index]);
    
    char* source = read_file(argv[file_arg_index]);
    
    // Pre-load all imports before parsing
    extern void preload_imports(const char* source);
    preload_imports(source);
    
    init_lexer(source);
    init_parser();
    set_parser_filename(argv[file_arg_index]);  // Set filename for better error messages
    init_checker();
    
    Program* prog = parse_program();
    if (!prog) {
        fprintf(stderr, "Error: Failed to parse program\n");
        free(source);
        return 1;
    }
    
    // Type check
    check_program(prog);
    
    if (checker_had_error()) {
        fprintf(stderr, "Compilation failed due to errors\n");
        free(source);
        return 1;
    }
    
    // Apply optimizations
    if (optimization > OPT_NONE) {
        printf("Applying optimizations (level %d)...\n", optimization);
        eliminate_dead_code(prog);
        inline_small_functions(prog);
        
        // Apply constant folding to all expressions in the program
        for (int i = 0; i < prog->count; i++) {
            Stmt* stmt = prog->stmts[i];
            if (stmt && stmt->type == STMT_EXPR && stmt->expr) {
                stmt->expr = fold_constants(stmt->expr);
            }
        }
    }
    
    char out_path[256];
    snprintf(out_path, 256, "%s.c", argv[file_arg_index]);
    FILE* out = fopen(out_path, "w");
    init_codegen(out);
    codegen_c_header();
    codegen_program(prog);
    fclose(out);
    
    // Free AST
    free_program(prog);
    
    // Get WYN_ROOT or use current directory
    char wyn_root[1024] = ".";
    char* root_env = getenv("WYN_ROOT");
    if (root_env) {
        snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
    }
    
    char compile_cmd[2048];
    const char* opt_flag = (optimization == OPT_O2) ? "-O2" : (optimization == OPT_O1) ? "-O1" : "-O0";
    char output_bin[256];
    if (output_name) {
        snprintf(output_bin, 256, "%s", output_name);
    } else {
        snprintf(output_bin, 256, "%s.out", argv[file_arg_index]);
    }
    snprintf(compile_cmd, sizeof(compile_cmd), 
             "gcc %s -I %s/src -o %s %s.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c %s/src/concurrency.c %s/src/async_runtime.c %s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c %s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c -lm", 
             opt_flag, wyn_root, output_bin, argv[file_arg_index], wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
    int result = system(compile_cmd);
    
    // Check if output file was actually created
    FILE* check = fopen(output_bin, "r");
    if (check) {
        fclose(check);
        printf("Compiled successfully: %s\n", output_bin);
        // Keep intermediate .c file for debugging
        // char c_file[256];
        // snprintf(c_file, 256, "%s.c", argv[file_arg_index]);
        // remove(c_file);
    } else {
        fprintf(stderr, "C compilation failed - output file not created\n");
        fprintf(stderr, "Command: %s\n", compile_cmd);
        fprintf(stderr, "GCC exit code: %d\n", result);
        return 1;
    }
    
    safe_free(source);
    wyn_platform_cleanup();
    return result;
}

int create_new_project(const char* project_name) {
    char path[512];
    
    // Create directories
    snprintf(path, sizeof(path), "mkdir -p %s/src %s/tests", project_name, project_name);
    if (system(path) != 0) {
        fprintf(stderr, "Error: Failed to create project directories\n");
        return 1;
    }
    
    // Create main.wyn
    snprintf(path, sizeof(path), "%s/src/main.wyn", project_name);
    FILE* main_file = fopen(path, "w");
    if (!main_file) {
        fprintf(stderr, "Error: Failed to create main.wyn\n");
        return 1;
    }
    fprintf(main_file, "fn main() -> int {\n    print(\"Hello from %s!\")\n    return 0\n}\n", project_name);
    fclose(main_file);
    
    // Create test file
    snprintf(path, sizeof(path), "%s/tests/test_main.wyn", project_name);
    FILE* test_file = fopen(path, "w");
    if (!test_file) {
        fprintf(stderr, "Error: Failed to create test file\n");
        return 1;
    }
    fprintf(test_file, "// Tests for %s\n\nfn test_basic() -> int {\n    // Add your tests here\n    return 0\n}\n\nfn main() -> int {\n    test_basic()\n    print(\"All tests passed!\")\n    return 0\n}\n", project_name);
    fclose(test_file);
    
    // Create README.md
    snprintf(path, sizeof(path), "%s/README.md", project_name);
    FILE* readme_file = fopen(path, "w");
    if (readme_file) {
        fprintf(readme_file, "# %s\n\nA Wyn project.\n\n## Build\n\n```bash\nwyn run src/main.wyn\n```\n\n## Test\n\n```bash\nwyn run tests/test_main.wyn\n```\n", project_name);
        fclose(readme_file);
    }
    
    printf("Created new Wyn project: %s\n", project_name);
    printf("  %s/src/main.wyn\n", project_name);
    printf("  %s/tests/test_main.wyn\n", project_name);
    printf("  %s/README.md\n", project_name);
    printf("\nTo build and run:\n  cd %s\n  wyn run src/main.wyn\n", project_name);
    printf("\nTo run tests:\n  wyn run tests/test_main.wyn\n");
    
    return 0;
}
