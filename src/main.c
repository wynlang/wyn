#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "common.h"
#include "ast.h"
#include "types.h"
#include "memory.h"
#include "security.h"
#include "platform.h"
#include "banner.h"
#include "optimize.h"
#include "module.h"
#include "commands.h"

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
    
    // Handle empty files
    if (size <= 0) {
        fclose(f);
        char* buffer = malloc(1);
        buffer[0] = '\0';
        return buffer;
    }
    
    char* buffer = safe_malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    
    return buffer;
}

static char* get_version() {
    static char version[64] = {0};
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
        if (version[0] == 0) strcpy(version, "1.6.0");
        
        // Add LLVM backend indicator
        #ifdef WITH_LLVM
        strcat(version, " (LLVM backend)");
        #endif
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
        // Banner
        print_banner(get_version());
        fprintf(stderr, "\033[1mUsage:\033[0m wyn \033[33m<command>\033[0m [options]\n\n");
        
        fprintf(stderr, "\033[1mDevelop:\033[0m\n");
        fprintf(stderr, "  \033[32mrun\033[0m <file.wyn>          Compile and run\n");
        fprintf(stderr, "  \033[32mcheck\033[0m <file.wyn>        Type-check without compiling\n");
        fprintf(stderr, "  \033[32mfmt\033[0m <file.wyn>          Format source file\n");
        fprintf(stderr, "  \033[32mtest\033[0m                    Run project tests\n");
        fprintf(stderr, "  \033[32mwatch\033[0m <file.wyn>        Watch and auto-rebuild\n");
        fprintf(stderr, "  \033[32mrepl\033[0m                    Interactive REPL\n");
        fprintf(stderr, "  \033[32mbench\033[0m <file.wyn>        Benchmark with timing\n");
        fprintf(stderr, "  \033[32mdoc\033[0m <file.wyn>          Generate documentation\n");
        
        fprintf(stderr, "\n\033[1mBuild:\033[0m\n");
        fprintf(stderr, "  \033[32mbuild\033[0m <dir>             Build all .wyn files in directory\n");
        fprintf(stderr, "  \033[32mcross\033[0m <target> <file>   Cross-compile (linux/macos/windows/ios/android)\n");
        fprintf(stderr, "  \033[32mbuild-runtime\033[0m           Precompile runtime for fast builds\n");
        fprintf(stderr, "  \033[32mclean\033[0m                   Remove build artifacts\n");
        
        fprintf(stderr, "\n\033[1mPackages:\033[0m\n");
        fprintf(stderr, "  \033[32minit\033[0m [name]             Create new project\n");
        fprintf(stderr, "  \033[32mpkg install\033[0m <name>      Install a package\n");
        fprintf(stderr, "  \033[32mpkg list\033[0m                List installed packages\n");
        fprintf(stderr, "  \033[32mpkg uninstall\033[0m <name>    Uninstall a package\n");
        fprintf(stderr, "  \033[32mpkg search\033[0m <query>      Search package registry\n");
        
        fprintf(stderr, "\n\033[1mTools:\033[0m\n");
        fprintf(stderr, "  \033[32mlsp\033[0m                     Start language server (for editors)\n");
        fprintf(stderr, "  \033[32minstall\033[0m                 Install wyn to system PATH\n");
        fprintf(stderr, "  \033[32muninstall\033[0m               Remove wyn from system PATH\n");
        fprintf(stderr, "  \033[32mversion\033[0m                 Show version\n");
        fprintf(stderr, "  \033[32mhelp\033[0m                    Show this help\n");
        
        fprintf(stderr, "\n\033[1mFlags:\033[0m\n");
        fprintf(stderr, "  \033[33m--fast\033[0m                  Skip optimizations (fastest compile)\n");
        fprintf(stderr, "  \033[33m--release               Full optimizations (-O2)\n");
        fprintf(stderr, "  \033[33m--debug\033[0m                Keep .c and .out artifacts\n");
        
        fprintf(stderr, "\n\033[2mhttps://wynlang.com\033[0m\n");
        return 1;
    }
    
    char* command = argv[1];
    
    // Handle --version and -v flags
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("\033[36mWyn\033[0m v%s\n", get_version());
        return 0;
    }
    
    // Install/uninstall wyn to system PATH
    if (strcmp(command, "install") == 0) {
#ifdef _WIN32
        fprintf(stderr, "On Windows, use the installer or add wyn.exe to your PATH manually.\n");
        fprintf(stderr, "  1. Copy wyn.exe to a directory\n");
        fprintf(stderr, "  2. Add that directory to your PATH environment variable\n");
        return 1;
#else
        char exe_path[1024];
        #ifdef __APPLE__
        uint32_t size = sizeof(exe_path);
        _NSGetExecutablePath(exe_path, &size);
        #else
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
        if (len > 0) exe_path[len] = 0; else { strncpy(exe_path, argv[0], sizeof(exe_path)-1); }
        #endif
        char abs_path[1024];
        if (realpath(exe_path, abs_path) == NULL) strncpy(abs_path, exe_path, sizeof(abs_path));
        
        const char* install_dir = "/usr/local/bin";
        char install_path[1100];
        snprintf(install_path, sizeof(install_path), "%s/wyn", install_dir);
        
        if (access(install_path, X_OK) == 0) {
            char existing[1024] = {0};
            readlink(install_path, existing, sizeof(existing)-1);
            printf("\033[33mwyn\033[0m is already installed at %s\n", install_path);
            if (existing[0]) printf("  → %s\n", existing);
            printf("Reinstall? [y/N] ");
            fflush(stdout);
            char c = getchar();
            if (c != 'y' && c != 'Y') { printf("Cancelled.\n"); return 0; }
            unlink(install_path);
        }
        
        printf("Installing \033[32mwyn\033[0m to %s...\n", install_path);
        if (symlink(abs_path, install_path) == 0) {
            printf("\033[32m✓\033[0m Installed: %s → %s\n", install_path, abs_path);
            printf("  Run \033[1mwyn version\033[0m from anywhere to verify.\n");
        } else {
            fprintf(stderr, "\033[31m✗\033[0m Failed to create symlink. Try:\n");
            fprintf(stderr, "  sudo ln -sf %s %s\n", abs_path, install_path);
        }
        return 0;
#endif
    }
    
    if (strcmp(command, "uninstall") == 0) {
#ifdef _WIN32
        fprintf(stderr, "On Windows, remove wyn.exe from your PATH manually.\n");
        return 1;
#else
        const char* install_path = "/usr/local/bin/wyn";
        if (access(install_path, F_OK) != 0) {
            printf("wyn is not installed at %s\n", install_path);
            return 0;
        }
        printf("Removing \033[33m%s\033[0m...\n", install_path);
        if (unlink(install_path) == 0) {
            printf("\033[32m✓\033[0m Uninstalled wyn from %s\n", install_path);
        } else {
            fprintf(stderr, "\033[31m✗\033[0m Failed. Try: sudo rm %s\n", install_path);
        }
        return 0;
#endif
    }
    
    // Handle --help and -h flags
    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        // Reuse the no-args banner by faking argc
        // Print banner directly
        print_banner(get_version());
        fprintf(stderr, "\033[1mUsage:\033[0m wyn \033[33m<command>\033[0m [options]\n\n");
        fprintf(stderr, "\033[1mDevelop:\033[0m\n");
        fprintf(stderr, "  \033[32mrun\033[0m <file.wyn>         Compile and run\n");
        fprintf(stderr, "  \033[32mcheck\033[0m <file.wyn>       Type-check without compiling\n");
        fprintf(stderr, "  \033[32mfmt\033[0m <file.wyn>         Format source file\n");
        fprintf(stderr, "  \033[32mtest\033[0m                    Run project tests\n");
        fprintf(stderr, "  \033[32mwatch\033[0m <file.wyn>       Watch and auto-rebuild\n");
        fprintf(stderr, "  \033[32mrepl\033[0m                    Interactive REPL\n");
        fprintf(stderr, "  \033[32mbench\033[0m <file.wyn>       Benchmark with timing\n");
        fprintf(stderr, "  \033[32mdoc\033[0m <file.wyn>         Generate documentation\n");
        fprintf(stderr, "\n\033[1mBuild:\033[0m\n");
        fprintf(stderr, "  \033[32mbuild\033[0m <dir>             Build all .wyn files in directory\n");
        fprintf(stderr, "  \033[32mcross\033[0m <target> <file>   Cross-compile (linux/macos/windows/ios/android)\n");
        fprintf(stderr, "  \033[32mbuild-runtime\033[0m           Precompile runtime for fast builds\n");
        fprintf(stderr, "  \033[32mclean\033[0m                   Remove build artifacts\n");
        fprintf(stderr, "\n\033[1mPackages:\033[0m\n");
        fprintf(stderr, "  \033[32minit\033[0m [name]             Create new project\n");
        fprintf(stderr, "  \033[32mpkg install\033[0m <name>      Install a package\n");
        fprintf(stderr, "  \033[32mpkg list\033[0m                List installed packages\n");
        fprintf(stderr, "  \033[32mpkg uninstall\033[0m <name>    Uninstall a package\n");
        fprintf(stderr, "  \033[32mpkg search\033[0m <query>      Search package registry\n");
        fprintf(stderr, "\n\033[1mTools:\033[0m\n");
        fprintf(stderr, "  \033[32mlsp\033[0m                     Start language server (for editors)\n");
        fprintf(stderr, "  \033[32minstall\033[0m                 Install wyn to system PATH\n");
        fprintf(stderr, "  \033[32muninstall\033[0m               Remove wyn from system PATH\n");
        fprintf(stderr, "  \033[32mversion\033[0m                 Show version\n");
        fprintf(stderr, "  \033[32mhelp\033[0m                    Show this help\n");
        fprintf(stderr, "\n\033[1mFlags:\033[0m\n");
        fprintf(stderr, "  \033[33m--fast\033[0m                  Skip optimizations (fastest compile)\n");
        fprintf(stderr, "  \033[33m--release               Full optimizations (-O2)\n");
        fprintf(stderr, "  \033[33m--debug\033[0m                Keep .c and .out artifacts\n");
        fprintf(stderr, "\n\033[1mCross-compile targets:\033[0m\n");
        fprintf(stderr, "  linux, macos, windows, ios, android\n");
        fprintf(stderr, "\n\033[2mhttps://wynlang.com\033[0m\n");
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
    
    if (strcmp(command, "watch") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn watch <file.wyn>\n");
            return 1;
        }
        return cmd_watch(argv[2], argc - 3, argv + 3);
    }
    
    if (strcmp(command, "install") == 0) {
        extern int package_install(const char*);
        if (argc < 3) {
            return package_install(".");
        } else {
            return package_install(argv[2]);
        }
    }
    
    if (strcmp(command, "lsp") == 0) {
        // wyn lsp - start LSP server
        extern int lsp_server_start();
        return lsp_server_start();
    }
    
    if (strcmp(command, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn search <query>\n");
            return 1;
        }
        extern int registry_search(const char*);
        return registry_search(argv[2]);
    }
    
    if (strcmp(command, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn info <package>\n");
            return 1;
        }
        extern int registry_info(const char*);
        return registry_info(argv[2]);
    }
    
    if (strcmp(command, "versions") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn versions <package>\n");
            return 1;
        }
        extern int registry_versions(const char*);
        return registry_versions(argv[2]);
    }
    
    if (strcmp(command, "publish") == 0) {
        int dry_run = 0;
        if (argc >= 3 && strcmp(argv[2], "--dry-run") == 0) {
            dry_run = 1;
        }
        extern int registry_publish(int);
        return registry_publish(dry_run);
    }
    
    if (strcmp(command, "pkg") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn pkg <list|install|uninstall|search>\n");
            return 1;
        }
        extern int package_install(const char*);
        extern int package_list();
        
        if (strcmp(argv[2], "list") == 0) {
            return package_list();
        } else if (strcmp(argv[2], "install") == 0) {
            return package_install(".");
        } else if (strcmp(argv[2], "uninstall") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: wyn pkg uninstall <package-name>\n");
                return 1;
            }
            char pkg_dir[1024];
            snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages/%s", getenv("HOME"), argv[3]);
            struct stat st;
            if (stat(pkg_dir, &st) != 0) {
                fprintf(stderr, "\033[31m✗\033[0m Package '%s' is not installed.\n", argv[3]);
                return 1;
            }
            char cmd[1100];
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", pkg_dir);
            system(cmd);
            printf("\033[32m✓\033[0m Uninstalled %s\n", argv[3]);
            return 0;
        } else if (strcmp(argv[2], "search") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: wyn pkg search <query>\n");
                return 1;
            }
            // Search installed packages and registry
            char pkg_dir[512];
            snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages", getenv("HOME"));
            printf("\033[1mSearching for '%s'...\033[0m\n\n", argv[3]);
            
            // Search local packages
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "ls -1 %s 2>/dev/null | grep -i '%s'", pkg_dir, argv[3]);
            FILE* fp = popen(cmd, "r");
            int found = 0;
            if (fp) {
                char buf[256];
                while (fgets(buf, sizeof(buf), fp)) {
                    buf[strcspn(buf, "\n")] = 0;
                    printf("  \033[32m●\033[0m %s \033[2m(installed)\033[0m\n", buf);
                    found++;
                }
                pclose(fp);
            }
            if (!found) printf("  No packages found matching '%s'\n", argv[3]);
            return 0;
        } else {
            fprintf(stderr, "Unknown pkg command: %s\n", argv[2]);
            return 1;
        }
    }
    
    if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn build <directory>\n");
            return 1;
        }
        char* dir = argv[2];
        
        // Look for main.wyn or src/main.wyn
        char entry[512];
        struct stat st;
        snprintf(entry, sizeof(entry), "%s/main.wyn", dir);
        if (stat(entry, &st) != 0) {
            snprintf(entry, sizeof(entry), "%s/src/main.wyn", dir);
            if (stat(entry, &st) != 0) {
                fprintf(stderr, "\033[31m✗\033[0m No main.wyn found in %s or %s/src\n", dir, dir);
                return 1;
            }
        }
        
        printf("\033[1mBuilding\033[0m %s...\n", entry);
        // Compile only — keep binary, clean .c
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s %s", argv[0], entry);
        int result = system(cmd);
        if (result == 0) {
            // Remove .c, keep .out
            char c_path[512];
            snprintf(c_path, sizeof(c_path), "%s.c", entry);
            unlink(c_path);
            // Rename .out to clean name
            char out_path[512], bin_path[512];
            snprintf(out_path, sizeof(out_path), "%s.out", entry);
            // Extract name: dir/main.wyn -> dir/main
            snprintf(bin_path, sizeof(bin_path), "%s", entry);
            char* dot = strrchr(bin_path, '.');
            if (dot) *dot = 0;
            rename(out_path, bin_path);
            printf("\033[32m✓\033[0m Built: %s\n", bin_path);
        } else {
            fprintf(stderr, "\033[31m✗\033[0m Build failed\n");
        }
        return result == 0 ? 0 : 1;
    }
    
    if (strcmp(command, "test") == 0) {
        struct stat st;
        // Prefer run_tests.wyn if it exists
        if (stat("tests/run_tests.wyn", &st) == 0) {
            printf("\033[1mRunning tests...\033[0m\n\n");
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s run tests/run_tests.wyn", argv[0]);
            return system(cmd) == 0 ? 0 : 1;
        }
        if (stat("tests", &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "\033[33mNo tests/ directory found.\033[0m\n");
            fprintf(stderr, "Create tests/*.wyn files to get started.\n");
            return 1;
        }
        printf("\033[1mRunning tests...\033[0m\n\n");
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "pass=0; fail=0; "
            "for f in tests/test_*.wyn tests/*/test_*.wyn; do "
            "  [ -f \"$f\" ] || continue; "
            "  result=$(%s run \"$f\" 2>&1); "
            "  if [ $? -eq 0 ]; then "
            "    echo \"  \\033[32m✓\\033[0m $f\"; pass=$((pass+1)); "
            "  else "
            "    echo \"  \\033[31m✗\\033[0m $f\"; fail=$((fail+1)); "
            "    echo \"$result\" | tail -3 | sed 's/^/    /'; "
            "  fi; "
            "done; "
            "echo; echo \"\\033[1mResults:\\033[0m $pass passed, $fail failed\"; "
            "[ $fail -eq 0 ]",
            argv[0]);
        return system(cmd) == 0 ? 0 : 1;
    }
    
    if (strcmp(command, "build-runtime") == 0) {
        // Precompile runtime library for fast compilation
        printf("Building runtime library...\n");
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "cd %s && mkdir -p runtime/obj && "
            "for f in src/wyn_wrapper.c src/wyn_interface.c src/io.c src/optional.c src/result.c "
            "src/arc_runtime.c src/concurrency.c src/async_runtime.c src/safe_memory.c src/error.c "
            "src/string_runtime.c src/hashmap.c src/hashset.c src/json.c src/json_runtime.c "
            "src/stdlib_runtime.c src/hashmap_runtime.c src/stdlib_string.c src/stdlib_array.c "
            "src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/spawn.c src/spawn_fast.c "
            "src/future.c src/net.c src/net_runtime.c src/test_runtime.c src/net_advanced.c "
            "src/file_io_simple.c src/stdlib_enhanced.c; do "
            "gcc -std=c11 -O2 -w -I src -c $f -o runtime/obj/$(basename $f .c).o; done && "
            "ar rcs runtime/libwyn_rt.a runtime/obj/*.o",
            argv[0] && strchr(argv[0], '/') ? "" : ".");
        // Use wyn_root
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) strncpy(wyn_root, root_env, sizeof(wyn_root)-1);
        else {
            char exe_path[1024];
            strncpy(exe_path, argv[0], sizeof(exe_path)-1);
            char* last_slash = strrchr(exe_path, '/');
            if (last_slash) { *last_slash = 0; strncpy(wyn_root, exe_path, sizeof(wyn_root)-1); }
        }
        snprintf(cmd, sizeof(cmd),
            "mkdir -p %s/runtime/obj && cd %s && "
            "for f in src/wyn_wrapper.c src/wyn_interface.c src/io.c src/optional.c src/result.c "
            "src/arc_runtime.c src/concurrency.c src/async_runtime.c src/safe_memory.c src/error.c "
            "src/string_runtime.c src/hashmap.c src/hashset.c src/json.c src/json_runtime.c "
            "src/stdlib_runtime.c src/hashmap_runtime.c src/stdlib_string.c src/stdlib_array.c "
            "src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/spawn.c src/spawn_fast.c "
            "src/future.c src/net.c src/net_runtime.c src/test_runtime.c src/net_advanced.c "
            "src/file_io_simple.c src/stdlib_enhanced.c; do "
            "gcc -std=c11 -O2 -w -I src -c $f -o runtime/obj/$(basename $f .c).o 2>/dev/null; done && "
            "ar rcs runtime/libwyn_rt.a runtime/obj/*.o && "
            "echo 'Built runtime/libwyn_rt.a'",
            wyn_root, wyn_root);
        return system(cmd);
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
    
    // wyn repl — interactive REPL
    if (strcmp(command, "repl") == 0) {
        printf("\033[1mWyn REPL\033[0m v%s  (type 'exit' to quit)\n\n", get_version());
        char line[4096];
        char history[65536] = "";  // accumulate definitions
        while (1) {
            printf("\033[36mwyn>\033[0m ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) break;
            line[strcspn(line, "\n")] = 0;
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
            if (strlen(line) == 0) continue;
            
            // Check if it's a definition (fn, struct, etc.) — accumulate
            int is_def = (strncmp(line, "fn ", 3) == 0 || strncmp(line, "struct ", 7) == 0 ||
                         strncmp(line, "enum ", 5) == 0 || strncmp(line, "var ", 4) == 0 ||
                         strncmp(line, "const ", 6) == 0);
            
            // Build a temp file with accumulated history + this expression
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "/tmp/wyn_repl_%d.wyn", getpid());
            FILE* f = fopen(tmp, "w");
            fprintf(f, "%s\n", history);
            if (is_def) {
                fprintf(f, "%s\n", line);
                fprintf(f, "fn main() -> int { return 0 }\n");
                // Add to history
                strcat(history, line);
                strcat(history, "\n");
            } else {
                fprintf(f, "fn main() -> int {\n  %s\n  return 0\n}\n", line);
            }
            fclose(f);
            
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s run %s 2>&1", argv[0], tmp);
            FILE* p = popen(cmd, "r");
            if (p) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), p)) {
                    // Filter out compilation noise
                    if (strstr(buf, "Compiled in") || strstr(buf, "warning")) continue;
                    printf("%s", buf);
                }
                pclose(p);
            }
            unlink(tmp);
            char tmpc[140]; snprintf(tmpc, sizeof(tmpc), "%s.c", tmp); unlink(tmpc);
            char tmpo[140]; snprintf(tmpo, sizeof(tmpo), "%s.out", tmp); unlink(tmpo);
        }
        printf("\nBye!\n");
        return 0;
    }
    
    // wyn bench <file> — run with timing
    if (strcmp(command, "bench") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn bench <file.wyn>\n");
            return 1;
        }
        printf("\033[1mBenchmarking\033[0m %s\n\n", argv[2]);
        
        // Run 5 times and report
        char cmd[512];
        double times[5];
        for (int i = 0; i < 5; i++) {
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            snprintf(cmd, sizeof(cmd), "%s run %s > /dev/null 2>&1", argv[0], argv[2]);
            int r = system(cmd);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (r != 0) {
                fprintf(stderr, "\033[31m✗\033[0m Compilation/execution failed\n");
                return 1;
            }
            times[i] = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
            printf("  Run %d: \033[33m%.1fms\033[0m\n", i + 1, times[i]);
        }
        
        // Stats
        double sum = 0, min = times[0], max = times[0];
        for (int i = 0; i < 5; i++) {
            sum += times[i];
            if (times[i] < min) min = times[i];
            if (times[i] > max) max = times[i];
        }
        printf("\n\033[1mResults:\033[0m\n");
        printf("  avg: \033[32m%.1fms\033[0m  min: %.1fms  max: %.1fms\n", sum / 5, min, max);
        return 0;
    }
    
    // wyn doc <file> — generate docs from source
    if (strcmp(command, "doc") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn doc <file.wyn>\n");
            return 1;
        }
        char* source = read_file(argv[2]);
        if (!source) {
            fprintf(stderr, "\033[31m✗\033[0m Cannot read %s\n", argv[2]);
            return 1;
        }
        
        printf("\033[1m# %s\033[0m\n\n", argv[2]);
        
        // Extract functions, structs, enums, traits with their comments
        char* p = source;
        char prev_comment[4096] = "";
        while (*p) {
            // Capture comments
            if (p[0] == '/' && p[1] == '/') {
                char* start = p + 2;
                while (*start == ' ') start++;
                char* end = start;
                while (*end && *end != '\n') end++;
                int len = end - start;
                if (len > 0 && len < 4000) {
                    int plen = strlen(prev_comment);
                    if (plen > 0) { prev_comment[plen] = '\n'; plen++; }
                    memcpy(prev_comment + plen, start, len);
                    prev_comment[plen + len] = 0;
                }
                p = *end ? end + 1 : end;
                continue;
            }
            
            // Check for definitions
            int is_fn = (strncmp(p, "fn ", 3) == 0);
            int is_pub_fn = (strncmp(p, "pub fn ", 7) == 0);
            int is_struct = (strncmp(p, "struct ", 7) == 0);
            int is_enum = (strncmp(p, "enum ", 5) == 0);
            int is_trait = (strncmp(p, "trait ", 6) == 0);
            
            if (is_fn || is_pub_fn || is_struct || is_enum || is_trait) {
                // Extract the signature (up to { or newline)
                char* end = p;
                while (*end && *end != '{' && *end != '\n') end++;
                int len = end - p;
                char sig[512];
                if (len > 511) len = 511;
                memcpy(sig, p, len);
                sig[len] = 0;
                // Trim trailing whitespace
                while (len > 0 && (sig[len-1] == ' ' || sig[len-1] == '\t')) sig[--len] = 0;
                
                if (is_struct) printf("\033[33m## struct\033[0m ");
                else if (is_enum) printf("\033[33m## enum\033[0m ");
                else if (is_trait) printf("\033[33m## trait\033[0m ");
                else printf("\033[32m## fn\033[0m ");
                
                printf("\033[1m%s\033[0m\n", sig + (is_pub_fn ? 7 : is_fn ? 3 : is_struct ? 7 : is_enum ? 5 : 6));
                
                if (prev_comment[0]) {
                    printf("  %s\n", prev_comment);
                }
                printf("\n");
                prev_comment[0] = 0;
            } else if (*p != '\n' && *p != ' ' && *p != '\t') {
                prev_comment[0] = 0;  // Reset comment if non-definition line
            }
            
            // Advance to next line
            while (*p && *p != '\n') p++;
            if (*p) p++;
        }
        free(source);
        return 0;
    }
    
    if (strcmp(command, "cross") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: wyn cross <target> <file.wyn>\n");
            fprintf(stderr, "Targets: linux, macos, windows, ios, android\n");
            return 1;
        }
        
        char* target = argv[2];
        char* file = argv[3];
        
        // Find wyn root for includes
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) { snprintf(wyn_root, sizeof(wyn_root), "%s", root_env); }
        else {
            char ep[1024]; strncpy(ep, argv[0], sizeof(ep)-1); ep[sizeof(ep)-1]=0;
            char* ls = strrchr(ep, '/');
            if (ls) { *ls = 0; snprintf(wyn_root, sizeof(wyn_root), "%s", ep); }
        }
        
        // Compile to C first
        char* source = read_file(file);
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        check_all_modules();  // Type check loaded modules
        
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
        char compile_cmd[4096];
        if (strcmp(target, "linux") == 0) {
            // On macOS, just compile normally (user can transfer binary)
            snprintf(compile_cmd, 512, "gcc -std=c11 -O2 -w -I %s/src -o %s.linux %s.c %s/runtime/libwyn_rt.a -lpthread -lm", wyn_root, file, file, wyn_root);
            printf("Compiling for Linux (native binary)...\n");
        } else if (strcmp(target, "macos") == 0) {
            snprintf(compile_cmd, 512, "gcc -std=c11 -O2 -w -I %s/src -o %s.macos %s.c %s/runtime/libwyn_rt.a -lpthread -lm", wyn_root, file, file, wyn_root);
            printf("Compiling for macOS...\n");
        } else if (strcmp(target, "windows") == 0) {
            snprintf(compile_cmd, 512, "x86_64-w64-mingw32-gcc -std=c11 -O2 -w -static -I %s/src -o %s.exe %s.c -lm", wyn_root, file, file);
            printf("Cross-compiling for Windows...\n");
        } else if (strcmp(target, "ios") == 0) {
            // iOS cross-compilation via Xcode SDK
            char sdk_path[512] = {0};
            FILE* sdk_fp = popen("xcrun --sdk iphoneos --show-sdk-path 2>/dev/null", "r");
            if (sdk_fp) { fgets(sdk_path, sizeof(sdk_path), sdk_fp); pclose(sdk_fp); }
            // Trim newline
            char* nl = strchr(sdk_path, '\n'); if (nl) *nl = 0;
            if (sdk_path[0] == 0) {
                fprintf(stderr, "Error: Xcode iOS SDK not found. Install Xcode.\n");
                return 1;
            }
            snprintf(compile_cmd, sizeof(compile_cmd),
                "clang -std=c11 -O2 -w -arch arm64 -isysroot %s "
                "-miphoneos-version-min=15.0 "
                "-I %s/src -o %s.ios %s.c "
                "%s/src/wyn_wrapper.c %s/src/wyn_interface.c "
                "%s/src/hashmap.c %s/src/hashset.c %s/src/json.c "
                "%s/src/test_runtime.c %s/src/spawn.c %s/src/spawn_fast.c %s/src/future.c "
                "%s/src/net.c %s/src/net_advanced.c "
                "-lpthread -lm",
                sdk_path, wyn_root, file, file,
                wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root);
            printf("Cross-compiling for iOS (arm64)...\n");
            printf("SDK: %s\n", sdk_path);
        } else if (strcmp(target, "android") == 0) {
            // Android cross-compilation via NDK
            char ndk_path[512] = {0};
            // Check common NDK locations
            const char* ndk_dirs[] = {
                "~/Library/Android/sdk/ndk",
                "~/Android/Sdk/ndk",
                "/usr/local/lib/android/sdk/ndk",
                NULL
            };
            for (int i = 0; ndk_dirs[i]; i++) {
                char expanded[512];
                if (ndk_dirs[i][0] == '~') {
                    snprintf(expanded, 512, "%s%s", getenv("HOME"), ndk_dirs[i] + 1);
                } else {
                    snprintf(expanded, 512, "%s", ndk_dirs[i]);
                }
                // Find latest NDK version
                char find_cmd[512];
                snprintf(find_cmd, 512, "ls -d %s/*/toolchains/llvm/prebuilt/*/bin/aarch64-linux-android*-clang 2>/dev/null | tail -1", expanded);
                FILE* fp = popen(find_cmd, "r");
                if (fp) {
                    char clang_path[512] = {0};
                    fgets(clang_path, sizeof(clang_path), fp);
                    pclose(fp);
                    char* nl2 = strchr(clang_path, '\n'); if (nl2) *nl2 = 0;
                    if (clang_path[0]) {
                        snprintf(ndk_path, 512, "%s", clang_path);
                        break;
                    }
                }
            }
            if (ndk_path[0] == 0) {
                fprintf(stderr, "Error: Android NDK not found.\n");
                fprintf(stderr, "Install: Android Studio → SDK Manager → NDK\n");
                fprintf(stderr, "Or: sdkmanager --install 'ndk;latest'\n");
                return 1;
            }
            snprintf(compile_cmd, 512,
                "%s -std=c11 -O2 -w -I %s/src -o %s.android %s.c -lm -llog",
                ndk_path, wyn_root, file, file);
            printf("Cross-compiling for Android (arm64)...\n");
            printf("NDK: %s\n", ndk_path);
        } else {
            fprintf(stderr, "Unknown target: %s\n", target);
            fprintf(stderr, "Available: linux, macos, windows, ios, android\n");
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
    
    if (strcmp(command, "check") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: wyn check <file.wyn>\n"); return 1; }
        char* file = argv[2];
        char* source = read_file(file);
        if (!source) { fprintf(stderr, "Error: Cannot read %s\n", file); return 1; }
        extern void preload_imports(const char* source);
        preload_imports(source);
        init_lexer(source);
        init_parser();
        set_parser_filename(file);
        init_checker();
        check_all_modules();
        Program* prog = parse_program();
        if (!prog) { fprintf(stderr, "Parse error\n"); free(source); return 1; }
        check_program(prog);
        if (checker_had_error()) { free(source); return 1; }
        printf("✓ %s: no errors\n", file);
        free(source);
        return 0;
    }
    
    if (strcmp(command, "fmt") == 0) {
        extern int cmd_fmt(const char* file, int argc, char** argv);
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn fmt <file.wyn>\n");
        fprintf(stderr, "  wyn check <file.wyn>     Type-check without compiling\n");
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
        check_all_modules();  // Type check loaded modules
        
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
        
        // Generate code using LLVM backend
        char base_name[256];
        strncpy(base_name, file, sizeof(base_name) - 1);
        char* dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';
        
        char ll_path[300];
        char obj_path[300];
        char out_path[300];
        snprintf(ll_path, sizeof(ll_path), "%s.ll", base_name);
        snprintf(obj_path, sizeof(obj_path), "%s.o", base_name);
        snprintf(out_path, sizeof(out_path), "%s.out", base_name);
        
        // Initialize LLVM codegen (no C file output)
        init_codegen(NULL);
        codegen_program(prog);
        
        // Write LLVM IR to file
        extern bool llvm_write_ir_to_file(const char* filename);
        if (!llvm_write_ir_to_file(ll_path)) {
            fprintf(stderr, "Error: Failed to write LLVM IR\n");
            free(source);
            return 1;
        }
        printf("Generated LLVM IR: %s\n", ll_path);
        
        // Compile LLVM IR to object file
        extern bool llvm_compile_to_object(const char* ir_file, const char* obj_file);
        if (!llvm_compile_to_object(ll_path, obj_path)) {
            fprintf(stderr, "Error: Failed to compile LLVM IR to object file\n");
            free(source);
            return 1;
        }
        printf("Compiled to object: %s\n", obj_path);
        
        // Get WYN_ROOT
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) {
            snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
        } else {
            // Auto-detect: try multiple locations
            const char* test_paths[] = {
                "./src/wyn_wrapper.c",
                "../src/wyn_wrapper.c",
                "./wyn/src/wyn_wrapper.c",
                NULL
            };
            
            for (int i = 0; test_paths[i] != NULL; i++) {
                FILE* test = fopen(test_paths[i], "r");
                if (test) {
                    fclose(test);
                    if (i == 0) {
                        strcpy(wyn_root, ".");
                    } else if (i == 1) {
                        strcpy(wyn_root, "..");
                    } else if (i == 2) {
                        strcpy(wyn_root, "./wyn");
                    }
                    break;
                }
            }
        }
        
        // Link to binary
        extern bool llvm_link_binary(const char* obj_file, const char* output, const char* wyn_root);
        if (!llvm_link_binary(obj_path, out_path, wyn_root)) {
            fprintf(stderr, "Error: Failed to link binary\n");
            free(source);
            return 1;
        }
        printf("Linked binary: %s\n", out_path);
        printf("Compiled successfully\n");
        
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
        char* file = NULL;
        
        // Check for --debug flag and find file arg
        int keep_artifacts = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--debug") == 0) keep_artifacts = 1;
            else if (strcmp(argv[i], "--fast") == 0 || strcmp(argv[i], "--release") == 0) {}
            else if (!file) file = argv[i];
        }
        if (!file) {
            fprintf(stderr, "Usage: wyn run <file.wyn>\n");
            return 1;
        }
        
        // Incremental: skip recompilation if binary is newer than source
        {
            char out_path[512];
            snprintf(out_path, sizeof(out_path), "%s.out", file);
            struct stat src_st, out_st;
            if (stat(file, &src_st) == 0 && stat(out_path, &out_st) == 0) {
                if (out_st.st_mtime >= src_st.st_mtime) {
                    char run_cmd[2048];
                    if (out_path[0] == '/') {
                        snprintf(run_cmd, sizeof(run_cmd), "'%s'", out_path);
                    } else {
                        snprintf(run_cmd, sizeof(run_cmd), "'./%s'", out_path);
                    }
                    for (int i = 3; i < argc; i++) { strcat(run_cmd, " "); strcat(run_cmd, argv[i]); }
                    return system(run_cmd);
                }
            }
        }
        
        char* source = read_file(file);
        
        // Pre-load all imports before parsing
        extern void preload_imports(const char* source);
        preload_imports(source);
        
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        check_all_modules();  // Type check loaded modules
        
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
        
        // Get WYN_ROOT or auto-detect from executable path
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) {
            snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
        } else {
            // Derive from executable path
            char exe_path[1024];
            strncpy(exe_path, argv[0], sizeof(exe_path) - 1);
            exe_path[sizeof(exe_path) - 1] = '\0';
            char* last_slash = strrchr(exe_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                if (exe_path[0] != '\0') {
                    snprintf(wyn_root, sizeof(wyn_root), "%s", exe_path);
                }
            }
            // Verify
            char test_path[1100];
            snprintf(test_path, sizeof(test_path), "%s/src/wyn_wrapper.c", wyn_root);
            FILE* test = fopen(test_path, "r");
            if (!test) {
                test = fopen("./src/wyn_wrapper.c", "r");
                if (test) strcpy(wyn_root, ".");
                else {
                    test = fopen("./wyn/src/wyn_wrapper.c", "r");
                    if (test) strcpy(wyn_root, "./wyn");
                }
            }
            if (test) fclose(test);
        }
        
        // Detect optional dependencies
        const char* sqlite_flags = strstr(source, "Db.") ? " -DWYN_USE_SQLITE -lsqlite3" : "";
        const char* gui_flags = strstr(source, "Gui.") ? " -DWYN_USE_GUI $(pkg-config --cflags --libs sdl2 2>/dev/null || echo '-lSDL2') " : "";
        
        // Check for --fast flag (use -O0 for fastest compile)
        const char* opt_level = "-O1";
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--fast") == 0) { opt_level = "-O0"; break; }
            if (strcmp(argv[i], "--release") == 0) { opt_level = "-O2"; break; }
        }
        
        char compile_cmd[8192];
        // Use precompiled runtime library for fast compilation
        // Falls back to source compilation if libwyn_rt.a doesn't exist
        char rt_lib[512];
        snprintf(rt_lib, sizeof(rt_lib), "%s/runtime/libwyn_rt.a", wyn_root);
        FILE* rt_check = fopen(rt_lib, "r");
        if (rt_check) {
            fclose(rt_check);
            snprintf(compile_cmd, sizeof(compile_cmd),
                     "gcc -std=c11 %s -w -I %s/src -o '%s.out' '%s.c' %s/runtime/libwyn_rt.a %s/runtime/parser_lib/libwyn_c_parser.a -lpthread -lm 2>wyn_cc_err.txt",
                     opt_level, wyn_root, file, file, wyn_root, wyn_root);
        } else {
            // Fallback: compile from source
            snprintf(compile_cmd, sizeof(compile_cmd),
                     "gcc -std=c11 %s -w -I %s/src -o '%s.out' '%s.c' %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c %s/src/concurrency.c %s/src/async_runtime.c %s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c %s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/json_runtime.c %s/src/stdlib_runtime.c %s/src/hashmap_runtime.c %s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c %s/src/stdlib_math.c %s/src/spawn.c %s/src/spawn_fast.c %s/src/future.c %s/src/net.c %s/src/net_runtime.c %s/src/test_runtime.c %s/src/net_advanced.c %s/src/file_io_simple.c %s/src/stdlib_enhanced.c %s/runtime/libwyn_runtime.a %s/runtime/parser_lib/libwyn_c_parser.a -lpthread -lm 2>wyn_cc_err.txt",
                     opt_level, wyn_root, file, file, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
        }
        // Append optional flags before the redirect
        if (sqlite_flags[0] || gui_flags[0]) {
            char* redirect = strstr(compile_cmd, " 2>/tmp");
            if (redirect) {
                char tail[256];
                strncpy(tail, redirect, sizeof(tail)-1); tail[sizeof(tail)-1] = 0;
                snprintf(redirect, sizeof(compile_cmd) - (redirect - compile_cmd), "%s%s%s", sqlite_flags, gui_flags, tail);
            }
        }
        int result = system(compile_cmd);
        
        if (result != 0) {
            fprintf(stderr, "Error: compilation failed (internal codegen error)\n");
            fprintf(stderr, "Run with WYN_DEBUG=1 for details\n");
            if (getenv("WYN_DEBUG")) {
                // Show raw C compiler output only in debug mode
                FILE* err_file = fopen("wyn_cc_err.txt", "r");
                if (err_file) {
                    char line[1024];
                    while (fgets(line, sizeof(line), err_file)) {
                        fprintf(stderr, "  %s", line);
                    }
                    fclose(err_file);
                }
            }
            free(source);
            return 1;
        }
        
        char run_cmd[512];
        if (file[0] == '/') {
            snprintf(run_cmd, 512, "'%s.out'", file);
        } else {
            snprintf(run_cmd, 512, "'./%s.out'", file);
        }
        result = system(run_cmd);
        free(source);
        // Cleanup artifacts unless --debug
        if (!keep_artifacts) {
            char c_path[512], out_path2[512];
            snprintf(c_path, 512, "%s.c", file);
            snprintf(out_path2, 512, "%s.out", file);
            unlink(c_path);
            unlink(out_path2);
        }
        // Extract actual exit code from system() result
#ifdef _WIN32
        return result;
#else
        if (WIFEXITED(result)) {
            return WEXITSTATUS(result);
        }
        return result;
#endif
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
    check_all_modules();  // Type check loaded modules
    
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
    
    #ifdef WITH_LLVM
    // Use LLVM backend
    char base_name[256];
    strncpy(base_name, argv[file_arg_index], sizeof(base_name) - 1);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';
    
    char ll_path[300];
    char obj_path[300];
    char out_path[300];
    snprintf(ll_path, sizeof(ll_path), "%s.ll", base_name);
    snprintf(obj_path, sizeof(obj_path), "%s.o", base_name);
    
    if (output_name) {
        snprintf(out_path, sizeof(out_path), "%s", output_name);
    } else {
        snprintf(out_path, sizeof(out_path), "%s.out", base_name);
    }
    
    // Initialize LLVM codegen
    init_codegen(NULL);
    codegen_program(prog);
    
    // Write LLVM IR to file
    extern bool llvm_write_ir_to_file(const char* filename);
    if (!llvm_write_ir_to_file(ll_path)) {
        fprintf(stderr, "Error: Failed to write LLVM IR\n");
        free(source);
        return 1;
    }
    
    // Compile LLVM IR to object file
    extern bool llvm_compile_to_object(const char* ir_file, const char* obj_file);
    if (!llvm_compile_to_object(ll_path, obj_path)) {
        fprintf(stderr, "Error: Failed to compile LLVM IR to object file\n");
        free(source);
        return 1;
    }
    
    // Get WYN_ROOT
    char wyn_root[1024] = ".";
    char* root_env = getenv("WYN_ROOT");
    if (root_env) {
        snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
    } else {
        // Auto-detect: try multiple locations
        const char* test_paths[] = {
            "./src/wyn_wrapper.c",
            "../src/wyn_wrapper.c",
            "./wyn/src/wyn_wrapper.c",
            NULL
        };
        
        for (int i = 0; test_paths[i] != NULL; i++) {
            FILE* test = fopen(test_paths[i], "r");
            if (test) {
                fclose(test);
                if (i == 0) {
                    strcpy(wyn_root, ".");
                } else if (i == 1) {
                    strcpy(wyn_root, "..");
                } else if (i == 2) {
                    strcpy(wyn_root, "./wyn");
                }
                break;
            }
        }
    }
    
    // Link to binary
    extern bool llvm_link_binary(const char* obj_file, const char* output, const char* wyn_root);
    if (!llvm_link_binary(obj_path, out_path, wyn_root)) {
        fprintf(stderr, "Error: Failed to link binary\n");
        free(source);
        return 1;
    }
    
    printf("Compiled successfully\n");
    cleanup_codegen();
    free_program(prog);
    free(source);
    return 0;
    
    #else
    // Use C backend (fallback)
    char out_path[256];
    snprintf(out_path, 256, "%s.c", argv[file_arg_index]);
    FILE* out = fopen(out_path, "w");
    init_codegen(out);
    codegen_c_header();
    codegen_program(prog);
    fclose(out);
    
    // Free AST
    free_program(prog);
    
    // Get WYN_ROOT or auto-detect from executable path
    char wyn_root[1024] = ".";
    char* root_env = getenv("WYN_ROOT");
    if (root_env) {
        snprintf(wyn_root, sizeof(wyn_root), "%s", root_env);
    } else {
        // Derive from executable path
        char exe_path[1024];
        strncpy(exe_path, argv[0], sizeof(exe_path) - 1);
        exe_path[sizeof(exe_path) - 1] = '\0';
        char* last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (exe_path[0] != '\0') {
                snprintf(wyn_root, sizeof(wyn_root), "%s", exe_path);
            }
        }
        // Verify
        char test_path[1100];
        snprintf(test_path, sizeof(test_path), "%s/src/wyn_wrapper.c", wyn_root);
        FILE* test = fopen(test_path, "r");
        if (!test) {
            test = fopen("./src/wyn_wrapper.c", "r");
            if (test) strcpy(wyn_root, ".");
            else {
                test = fopen("./wyn/src/wyn_wrapper.c", "r");
                if (test) strcpy(wyn_root, "./wyn");
            }
        }
        if (test) fclose(test);
    }
    
    char compile_cmd[2048];
    const char* opt_flag = (optimization == OPT_O2) ? "-O2" : (optimization == OPT_O1) ? "-O1" : "-O0";
    char output_bin[256];
    if (output_name) {
        snprintf(output_bin, sizeof(output_bin), "%s", output_name);
    } else {
        snprintf(output_bin, sizeof(output_bin), "%s.out", argv[file_arg_index]);
    }
    
    // Try precompiled runtime first
    char rt_path[512];
    snprintf(rt_path, sizeof(rt_path), "%s/runtime/libwyn_rt.a", wyn_root);
    FILE* rt_check = fopen(rt_path, "r");
    if (rt_check) {
        fclose(rt_check);
        snprintf(compile_cmd, sizeof(compile_cmd),
                 "gcc %s -std=c11 -w -I %s/src -o %s %s.c %s/runtime/libwyn_rt.a %s/runtime/parser_lib/libwyn_c_parser.a -lpthread -lm",
                 opt_flag, wyn_root, output_bin, argv[file_arg_index], wyn_root, wyn_root);
    } else {
        snprintf(compile_cmd, sizeof(compile_cmd),
                 "gcc %s -std=c11 -w -I %s/src -o %s %s.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c %s/src/concurrency.c %s/src/async_runtime.c %s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c %s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/json_runtime.c %s/src/stdlib_runtime.c %s/src/hashmap_runtime.c %s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c %s/src/stdlib_math.c %s/src/spawn.c %s/src/spawn_fast.c %s/src/future.c %s/src/net.c %s/src/net_runtime.c %s/src/test_runtime.c %s/src/net_advanced.c %s/src/file_io_simple.c %s/src/stdlib_enhanced.c %s/runtime/libwyn_runtime.a %s/runtime/parser_lib/libwyn_c_parser.a -lpthread -lm", 
                 opt_flag, wyn_root, output_bin, argv[file_arg_index], wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
    }
    
    int result = system(compile_cmd);
    if (result != 0) {
        fprintf(stderr, "C compilation failed\n");
        free(source);
        return result;
    }
    
    printf("Compiled successfully\n");
    free(source);
    return 0;
    #endif
}

int create_new_project(const char* project_name) {
    char path[512];
    
    // Create directories
    snprintf(path, sizeof(path), "mkdir -p %s/src %s/tests", project_name, project_name);
    if (system(path) != 0) {
        fprintf(stderr, "Error: Failed to create project directories\n");
        return 1;
    }
    
    // Create wyn.toml
    snprintf(path, sizeof(path), "%s/wyn.toml", project_name);
    FILE* toml_file = fopen(path, "w");
    if (!toml_file) {
        fprintf(stderr, "Error: Failed to create wyn.toml\n");
        return 1;
    }
    fprintf(toml_file, "[project]\n");
    fprintf(toml_file, "name = \"%s\"\n", project_name);
    fprintf(toml_file, "version = \"0.1.0\"\n");
    fprintf(toml_file, "entry = \"src/main.wyn\"\n");
    fprintf(toml_file, "\n[dependencies]\n");
    fprintf(toml_file, "# Add dependencies here\n");
    fclose(toml_file);
    
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
    printf("  %s/wyn.toml\n", project_name);
    printf("  %s/src/main.wyn\n", project_name);
    printf("  %s/tests/test_main.wyn\n", project_name);
    printf("  %s/README.md\n", project_name);
    printf("\nTo build and run:\n  cd %s\n  wyn run src/main.wyn\n", project_name);
    printf("\nTo run tests:\n  wyn run tests/test_main.wyn\n");
    
    return 0;
}
