#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
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

// Single source of truth for runtime source files
const char* wyn_runtime_sources[] = {
    "src/wyn_arena.c", "src/wyn_wrapper.c", "src/wyn_interface.c",
    "src/io.c", "src/optional.c", "src/result.c",
    "src/arc_runtime.c", "src/concurrency.c", "src/async_runtime.c",
    "src/safe_memory.c", "src/error.c", "src/string_runtime.c",
    "src/hashmap.c", "src/hashset.c", "src/json.c", "src/json_runtime.c",
    "src/stdlib_runtime.c", "src/hashmap_runtime.c",
    "src/stdlib_string.c", "src/stdlib_array.c",
    "src/stdlib_time.c", "src/stdlib_crypto.c", "src/stdlib_math.c",
    "src/spawn.c", "src/spawn_fast.c", "src/io_loop.c",
    "src/coroutine.c", "src/future.c",
    "src/net.c", "src/net_runtime.c", "src/test_runtime.c",
    "src/net_advanced.c", "src/file_io_simple.c", "src/stdlib_enhanced.c",
    NULL
};

// Build a space-separated string of runtime sources with a prefix
void build_source_list(char* buf, int bufsize, const char* prefix) {
    buf[0] = 0;
    for (int i = 0; wyn_runtime_sources[i]; i++) {
        int len = strlen(buf);
        if (prefix && prefix[0]) {
            snprintf(buf + len, bufsize - len, "%s/%s ", prefix, wyn_runtime_sources[i]);
        } else {
            snprintf(buf + len, bufsize - len, "%s ", wyn_runtime_sources[i]);
        }
    }
}

void init_lexer(const char* source);
void init_parser();
void init_checker();
void init_codegen(FILE* output);
Program* parse_program();
bool parser_had_error();
void check_program(Program* prog);
bool checker_had_error();
void free_program(Program* prog);
void codegen_c_header();
void codegen_program(Program* prog);
int create_new_project(const char* project_name);
int create_new_project_with_template(const char* name, const char* template, const char* lib_target);

// Wynter encouragement on compilation failure
static const char* wynter_compile_tips[] = {
    "Check the error above — the line number points to the problem.",
    "Try 'wyn check' to see all type errors without compiling.",
    "Common fix: make sure all variables are declared before use.",
    "Stuck? Simplify the code, get it working, then add complexity.",
    "String interpolation uses ${expr} — make sure quotes match.",
};
static int wynter_compile_tip_idx = 0;
static void wynter_encourage(void) {
    fprintf(stderr, "  \033[36m🐉 Wynter:\033[0m %s\n",
            wynter_compile_tips[wynter_compile_tip_idx++ % 5]);
}

// Detect available C backend: WYN_CC env > cc > gcc > clang
static const char* detect_cc(void) {
    static char cc_path[256] = "";
    if (cc_path[0]) return cc_path;
    char* env_cc = getenv("WYN_CC");
    if (env_cc && env_cc[0]) { strncpy(cc_path, env_cc, 255); return cc_path; }
#ifdef _WIN32
    const char* devnull = "> NUL 2>&1";
#else
    const char* devnull = "> /dev/null 2>&1";
#endif
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "cc --version %s", devnull);
    if (system(cmd) == 0) { strcpy(cc_path, "cc"); return cc_path; }
    snprintf(cmd, sizeof(cmd), "gcc --version %s", devnull);
    if (system(cmd) == 0) { strcpy(cc_path, "gcc"); return cc_path; }
    snprintf(cmd, sizeof(cmd), "clang --version %s", devnull);
    if (system(cmd) == 0) { strcpy(cc_path, "clang"); return cc_path; }
    // No C compiler found
    fprintf(stderr, "\033[31mError:\033[0m No C compiler found.\n");
    fprintf(stderr, "  Wyn needs a C compiler backend. Install one of:\n");
    fprintf(stderr, "    macOS:  xcode-select --install\n");
    fprintf(stderr, "    Ubuntu: sudo apt install build-essential\n");
    fprintf(stderr, "    Fedora: sudo dnf install gcc\n");
    fprintf(stderr, "  Or set WYN_CC=/path/to/compiler\n");
    strcpy(cc_path, "cc");
    return cc_path;
}

// TCC backend declarations
extern int wyn_tcc_compile_to_exe(const char* c_source, const char* output_path,
                                   const char* wyn_root, const char* include_path);
extern int wyn_tcc_available(void);

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
        if (version[0] == 0) strcpy(version, "1.8.0");
        
        // Version string
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
        fprintf(stderr, "  \033[32mbuild\033[0m <dir>             Build project (--shared / --python)\n");
        fprintf(stderr, "  \033[32mcross\033[0m <target> <file>   Cross-compile (linux/macos/windows/ios/android)\n");
        fprintf(stderr, "  \033[32mbuild-runtime\033[0m           Precompile runtime for fast builds\n");
        fprintf(stderr, "  \033[32mclean\033[0m                   Remove build artifacts\n");
        
        fprintf(stderr, "\n\033[1mPackages:\033[0m\n");
        fprintf(stderr, "  \033[32minit\033[0m [name]             Create new project\n");
        fprintf(stderr, "  \033[32mpkg\033[0m <command>           Package manager (register, login, search, install, push)\n");
        
        fprintf(stderr, "\n\033[1mTools:\033[0m\n");
        fprintf(stderr, "  \033[32mlsp\033[0m                     Start language server (for editors)\n");
        fprintf(stderr, "  \033[32minstall\033[0m                 Install wyn to system PATH\n");
        fprintf(stderr, "  \033[32muninstall\033[0m               Remove wyn from system PATH\n");
        fprintf(stderr, "  \033[32mdoctor\033[0m                  Check your setup\n");
        fprintf(stderr, "  \033[32mupgrade\033[0m                 Update wyn to latest version\n");
        fprintf(stderr, "  \033[32mversion\033[0m                 Show version\n");
        fprintf(stderr, "  \033[32mhelp\033[0m                    Show this help\n");
        
        fprintf(stderr, "\n\033[1mFlags:\033[0m\n");
        fprintf(stderr, "  \033[33m--fast\033[0m                  Skip optimizations (fastest compile)\n");
        fprintf(stderr, "  \033[33m--release               Full optimizations (-O3)\n");
        fprintf(stderr, "  \033[33m--debug\033[0m                Keep .c and .out artifacts\n");
        
        fprintf(stderr, "\n\033[2mhttps://wynlang.com\033[0m\n");
        return 1;
    }
    
    char* command = argv[1];
    
    // Handle --version and -v flags
    // wyn deploy <target> — deploy to server via SSH
    if (strcmp(command, "deploy") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn deploy <target> [--dry-run]\n");
            fprintf(stderr, "  Reads [deploy.<target>] from wyn.toml\n");
            return 1;
        }
        const char* target = argv[2];
        int dry_run = 0;
        for (int i = 3; i < argc; i++) { if (strcmp(argv[i], "--dry-run") == 0) dry_run = 1; }
        
        // Read wyn.toml
        FILE* toml = fopen("wyn.toml", "r");
        if (!toml) { fprintf(stderr, "\033[31m✗\033[0m No wyn.toml found\n"); return 1; }
        char toml_buf[8192]; int toml_len = fread(toml_buf, 1, sizeof(toml_buf)-1, toml); toml_buf[toml_len] = 0; fclose(toml);
        
        // Find [deploy.<target>] section
        char section[128]; snprintf(section, sizeof(section), "[deploy.%s]", target);
        char* sec = strstr(toml_buf, section);
        if (!sec) { fprintf(stderr, "\033[31m✗\033[0m No %s section in wyn.toml\n", section); return 1; }
        
        // Parse key = "value" pairs from section
        char host[256]="", user[128]="", key[512]="", path[512]="", os_target[32]="linux", pre[512]="", post[512]="";
        char* line = sec + strlen(section);
        while (*line) {
            while (*line == '\n' || *line == '\r' || *line == ' ') line++;
            if (*line == '[') break; // next section
            if (*line == '#' || *line == 0) { while (*line && *line != '\n') line++; continue; }
            char k[64], v[512]; v[0] = 0;
            if (sscanf(line, "%63[a-z_] = \"%511[^\"]\"", k, v) == 2) {
                if (strcmp(k, "host") == 0) strncpy(host, v, 255);
                else if (strcmp(k, "user") == 0) strncpy(user, v, 127);
                else if (strcmp(k, "key") == 0) strncpy(key, v, 511);
                else if (strcmp(k, "path") == 0) strncpy(path, v, 511);
                else if (strcmp(k, "os") == 0) strncpy(os_target, v, 31);
                else if (strcmp(k, "pre") == 0) strncpy(pre, v, 511);
                else if (strcmp(k, "post") == 0) strncpy(post, v, 511);
            }
            while (*line && *line != '\n') line++;
        }
        
        if (!host[0] || !path[0]) { fprintf(stderr, "\033[31m✗\033[0m Missing host or path in %s\n", section); return 1; }
        
        // Find entry point
        char* entry_ptr = strstr(toml_buf, "entry = \"");
        char entry[256] = "src/main.wyn";
        if (entry_ptr) { sscanf(entry_ptr, "entry = \"%255[^\"]\"", entry); }
        
        // Derive binary name
        char* name_ptr = strstr(toml_buf, "name = \"");
        char bin_name[128] = "app";
        if (name_ptr) { sscanf(name_ptr, "name = \"%127[^\"]\"", bin_name); }
        
        char ssh_opts[1024] = "";
        if (key[0]) {
            // Expand ~ in key path
            char expanded_key[512];
            if (key[0] == '~') { snprintf(expanded_key, sizeof(expanded_key), "%s%s", getenv("HOME") ?: "", key+1); }
            else { strncpy(expanded_key, key, 511); }
            snprintf(ssh_opts, sizeof(ssh_opts), "-i %s -o StrictHostKeyChecking=no", expanded_key);
        }
        
        char ssh_target[384]; snprintf(ssh_target, sizeof(ssh_target), "%s%s%s", user[0] ? user : "", user[0] ? "@" : "", host);
        
        printf("\033[1mDeploying\033[0m to %s (%s)\n", target, ssh_target);
        printf("  Binary: %s → %s/%s\n", entry, path, bin_name);
        if (pre[0]) printf("  Pre:    %s\n", pre);
        if (post[0]) printf("  Post:   %s\n", post);
        printf("\n");
        
        if (dry_run) { printf("\033[33m--dry-run: no changes made\033[0m\n"); return 0; }
        
        // Step 1: Cross-compile
        printf("  \033[2m[1/4] Cross-compiling for %s...\033[0m\n", os_target);
        char build_cmd[1024];
        snprintf(build_cmd, sizeof(build_cmd), "%s cross %s %s 2>&1", argv[0], os_target, entry);
        if (system(build_cmd) != 0) { fprintf(stderr, "\033[31m✗\033[0m Cross-compilation failed\n"); return 1; }
        
        // Step 2: Pre-deploy command
        if (pre[0]) {
            printf("  \033[2m[2/4] Running pre-deploy...\033[0m\n");
            char pre_cmd[1024]; snprintf(pre_cmd, sizeof(pre_cmd), "ssh %s %s '%s' 2>&1", ssh_opts, ssh_target, pre);
            system(pre_cmd);
        }
        
        // Step 3: Upload binary
        printf("  \033[2m[3/4] Uploading binary...\033[0m\n");
        char scp_cmd[1024];
        char local_bin[256]; snprintf(local_bin, sizeof(local_bin), "%s.out", entry);
        snprintf(scp_cmd, sizeof(scp_cmd), "scp %s %s %s:%s/%s 2>&1", ssh_opts, local_bin, ssh_target, path, bin_name);
        if (system(scp_cmd) != 0) { fprintf(stderr, "\033[31m✗\033[0m Upload failed\n"); return 1; }
        
        // Step 4: Post-deploy command
        if (post[0]) {
            printf("  \033[2m[4/4] Running post-deploy...\033[0m\n");
            char post_cmd[1024]; snprintf(post_cmd, sizeof(post_cmd), "ssh %s %s '%s' 2>&1", ssh_opts, ssh_target, post);
            system(post_cmd);
        }
        
        // Cleanup local binary
        unlink(local_bin);
        
        printf("\n\033[32m✓\033[0m Deployed %s to %s:%s/%s\n", bin_name, host, path, bin_name);
        return 0;
    }
    
    // wyn logs <target> — tail remote logs
    if (strcmp(command, "logs") == 0 || strcmp(command, "ssh") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: wyn %s <target>\n", command); return 1; }
        FILE* toml = fopen("wyn.toml", "r");
        if (!toml) { fprintf(stderr, "\033[31m✗\033[0m No wyn.toml found\n"); return 1; }
        char tb[8192]; int tl = fread(tb, 1, sizeof(tb)-1, toml); tb[tl] = 0; fclose(toml);
        char sec[128]; snprintf(sec, sizeof(sec), "[deploy.%s]", argv[2]);
        char* sp = strstr(tb, sec);
        if (!sp) { fprintf(stderr, "\033[31m✗\033[0m No %s in wyn.toml\n", sec); return 1; }
        char host[256]="", user[128]="", key[512]="";
        char* ln = sp;
        while (*ln) {
            while (*ln == '\n' || *ln == '\r' || *ln == ' ') ln++;
            if (*ln == '[') break;
            char k2[64], v2[512]; v2[0] = 0;
            if (sscanf(ln, "%63[a-z_] = \"%511[^\"]\"", k2, v2) == 2) {
                if (strcmp(k2, "host") == 0) strncpy(host, v2, 255);
                else if (strcmp(k2, "user") == 0) strncpy(user, v2, 127);
                else if (strcmp(k2, "key") == 0) strncpy(key, v2, 511);
            }
            while (*ln && *ln != '\n') ln++;
        }
        char ssh_opts2[1024] = "";
        if (key[0]) {
            char ek[512]; if (key[0] == '~') snprintf(ek, sizeof(ek), "%s%s", getenv("HOME") ?: "", key+1); else strncpy(ek, key, 511);
            snprintf(ssh_opts2, sizeof(ssh_opts2), "-i %s -o StrictHostKeyChecking=no", ek);
        }
        char st[384]; snprintf(st, sizeof(st), "%s%s%s", user[0] ? user : "", user[0] ? "@" : "", host);
        char cmd2[1024];
        if (strcmp(command, "ssh") == 0) {
            snprintf(cmd2, sizeof(cmd2), "ssh %s %s", ssh_opts2, st);
        } else {
            snprintf(cmd2, sizeof(cmd2), "ssh %s %s 'journalctl -u %s -f --no-pager' 2>&1", ssh_opts2, st, argv[2]);
        }
        return system(cmd2) == 0 ? 0 : 1;
    }
    
    if (strcmp(command, "explain") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: wyn explain <error-code>\n"); return 1; }
        const char* code = argv[2];
        struct { const char* code; const char* title; const char* detail; } errors[] = {
            {"E001", "Undefined variable", "A variable was used before being declared.\n\nFix: Declare the variable with 'var' before using it.\n\n  var x = 42\n  println(x.to_string())"},
            {"E002", "Undefined function", "A function was called that doesn't exist.\n\nFix: Define the function before calling it, or check for typos.\n\n  fn add(a: int, b: int) -> int { return a + b }\n  println(add(2, 3).to_string())"},
            {"E003", "Type mismatch", "An expression has the wrong type for the context.\n\nFix: Ensure types match. Use .to_string() for int→string conversion.\n\n  var x: int = 42       // correct\n  var y: string = \"hi\"  // correct"},
            {"E004", "Missing return", "A function with a return type doesn't return a value on all paths.\n\nFix: Add a return statement to every code path.\n\n  fn abs(x: int) -> int {\n    if x < 0 { return -x }\n    return x  // don't forget this!\n  }"},
            {"E005", "Wrong argument count", "A function was called with the wrong number of arguments.\n\nFix: Check the function signature and pass the correct number of arguments."},
            {NULL, NULL, NULL}
        };
        for (int i = 0; errors[i].code; i++) {
            if (strcmp(code, errors[i].code) == 0) {
                printf("\033[1m%s: %s\033[0m\n\n%s\n", errors[i].code, errors[i].title, errors[i].detail);
                return 0;
            }
        }
        fprintf(stderr, "Unknown error code: %s\n", code);
        return 1;
    }
    
    if (strcmp(command, "wisdom") == 0) {
        extern void print_flight_rules();
        print_flight_rules();
        return 0;
    }
    
    if (strcmp(command, "doctor") == 0) {
        printf("\033[1mWyn Doctor\033[0m — checking your setup\n\n");
        int issues = 0;
#ifdef _WIN32
        const char* devnull = "> NUL 2>&1";
#else
        const char* devnull = "> /dev/null 2>&1";
#endif
        
        // Determine wyn root
        char doc_root[512];
        strncpy(doc_root, argv[0], sizeof(doc_root)-1);
        char* last_slash = strrchr(doc_root, '/');
        if (last_slash) *last_slash = 0; else strcpy(doc_root, ".");
        
        // Check wyn binary
        printf("  \033[32m✓\033[0m Wyn compiler v%s\n", get_version());
        
        // Check bundled TCC
        char tcc_path[512]; snprintf(tcc_path, sizeof(tcc_path), "%s/vendor/tcc/bin/tcc", doc_root);
        int has_tcc = (access(tcc_path, X_OK) == 0);
        printf("  %s Bundled TCC backend\n", has_tcc ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
        if (!has_tcc) { printf("    Missing: %s\n", tcc_path); issues++; }
        
        // Check TCC runtime
        char rt_tcc[512]; snprintf(rt_tcc, sizeof(rt_tcc), "%s/vendor/tcc/lib/libwyn_rt_tcc.a", doc_root);
        int has_rt_tcc = (access(rt_tcc, F_OK) == 0);
        printf("  %s TCC runtime\n", has_rt_tcc ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
        if (!has_rt_tcc) { printf("    Missing: %s\n", rt_tcc); issues++; }
        
        // Check system cc (for --release)
        const char* cc = detect_cc();
        char doctor_cmd[128];
        snprintf(doctor_cmd, sizeof(doctor_cmd), "cc --version %s", devnull);
        int has_cc = (system(doctor_cmd) == 0);
        if (!has_cc) { snprintf(doctor_cmd, sizeof(doctor_cmd), "gcc --version %s", devnull); has_cc = (system(doctor_cmd) == 0); }
        printf("  %s System C compiler for --release (%s)\n", has_cc ? "\033[32m✓\033[0m" : "\033[33m○\033[0m", cc);
        if (!has_cc) printf("    Optional: install for optimized builds (xcode-select --install)\n");
        
        // Check precompiled runtime (for system cc)
        char rt_path[512]; snprintf(rt_path, sizeof(rt_path), "%s/runtime/libwyn_rt.a", doc_root);
        int has_rt = (access(rt_path, F_OK) == 0);
        printf("  %s Precompiled runtime (system cc)\n", has_rt ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
        if (!has_rt) printf("    Optional: run 'wyn build-runtime' for faster --release builds\n");
        
        // Check PATH
#ifdef _WIN32
        snprintf(doctor_cmd, sizeof(doctor_cmd), "where wyn %s", devnull);
#else
        snprintf(doctor_cmd, sizeof(doctor_cmd), "which wyn %s", devnull);
#endif
        int in_path = (system(doctor_cmd) == 0);
        printf("  %s wyn in PATH\n", in_path ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
        if (!in_path) printf("    Run: wyn install\n");
        
        // Check git (for packages)
        snprintf(doctor_cmd, sizeof(doctor_cmd), "git --version %s", devnull);
        int has_git = (system(doctor_cmd) == 0);
        printf("  %s git (for packages)\n", has_git ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
        if (!has_git) printf("    Optional: install git for 'wyn pkg install'\n");
        
        // Check curl (for registry)
#ifdef _WIN32
        snprintf(doctor_cmd, sizeof(doctor_cmd), "where curl %s", devnull);
#else
        snprintf(doctor_cmd, sizeof(doctor_cmd), "command -v curl %s", devnull);
#endif
        int has_curl = (system(doctor_cmd) == 0);
        printf("  %s curl (for package registry)\n", has_curl ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
        if (!has_curl) printf("    Optional: install curl for 'wyn pkg search/install/push'\n");

        // Check registry connectivity
        if (has_curl) {
            snprintf(doctor_cmd, sizeof(doctor_cmd), "curl -sS --max-time 5 https://pkg.wynlang.com/api/packages %s", devnull);
            int has_registry = (system(doctor_cmd) == 0);
            printf("  %s pkg.wynlang.com (registry)\n", has_registry ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
            if (!has_registry) printf("    Cannot reach package registry — check your internet connection\n");
        }
        
        printf("\n");
        if (issues == 0) printf("\033[32m✓ All good! No external dependencies needed.\033[0m\n");
        else printf("\033[31m%d issue(s) found.\033[0m Fix them and run wyn doctor again.\n", issues);
        return issues > 0 ? 1 : 0;
    }
    
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("\033[36m"
            "    /\\_/\\\n"
            "   / o o \\   \033[0m\033[1mWyn\033[0m v%s\n"
            "\033[36m  (  >.<  )  \033[0m\033[2m1 language for everything\033[0m\n"
            "\033[36m   \\_^_/\n"
            "  /|     |\\\n"
            " (_|     |_)  \033[0m\033[2m🐉 Wynter the Wyvern\033[0m\n\n",
            get_version());
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
    
    if (strcmp(command, "upgrade") == 0 || strcmp(command, "update") == 0) {
        printf("\033[1mChecking for updates...\033[0m\n");
#ifdef __APPLE__
#ifdef __aarch64__
        const char* platform = "macos-arm64";
#else
        const char* platform = "macos-x64";
#endif
#elif _WIN32
        const char* platform = "windows-x64";
#else
        const char* platform = "linux-x64";
#endif

#ifdef _WIN32
        // Windows: use PowerShell
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
            "powershell -Command \""
            "$latest = (Invoke-RestMethod https://api.github.com/repos/wynlang/wyn/releases/latest).tag_name -replace 'v','';"
            "if (-not $latest) { Write-Host '✗ Could not check for updates' -ForegroundColor Red; exit 1 }"
            "$current = '%s';"
            "if ($latest -eq $current) { Write-Host '✓ Already on latest (v'$current')' -ForegroundColor Green; exit 0 }"
            "Write-Host 'Upgrading v'$current' → v'$latest'...';"
            "Invoke-WebRequest -Uri https://github.com/wynlang/wyn/releases/download/v$latest/wyn-%s.exe -OutFile $env:TEMP\\wyn_new.exe;"
            "Copy-Item $env:TEMP\\wyn_new.exe (Get-Command wyn).Source -Force;"
            "Write-Host '✓ Upgraded to v'$latest -ForegroundColor Green\"",
            get_version(), platform);
        return system(cmd) == 0 ? 0 : 1;
#else
        // Find where the current binary is installed
        char self_path[1024] = "";
#ifdef __APPLE__
        uint32_t sz = sizeof(self_path);
        _NSGetExecutablePath(self_path, &sz);
#else
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len > 0) self_path[len] = '\0';
#endif
        // Resolve to absolute path
        char abs_path[1024];
        if (realpath(self_path, abs_path)) strncpy(self_path, abs_path, sizeof(self_path));

        // Check if we need sudo (can't write to the binary's directory)
        char* last_slash = strrchr(self_path, '/');
        char dir[1024] = "/usr/local/bin";
        if (last_slash) { size_t dlen = last_slash - self_path; memcpy(dir, self_path, dlen); dir[dlen] = '\0'; }
        int need_sudo = (access(dir, W_OK) != 0);

        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
            "latest=$(curl -sL https://api.github.com/repos/wynlang/wyn/releases/latest | grep tag_name | head -1 | sed 's/.*\"v//' | sed 's/\".*//');"
            "if [ -z \"$latest\" ]; then echo '\\033[31m✗\\033[0m Could not check for updates'; exit 1; fi;"
            "current='%s';"
            "if [ \"$latest\" = \"$current\" ]; then echo '\\033[32m✓\\033[0m Already on latest (v'$current')'; exit 0; fi;"
            "echo 'Upgrading v'$current' → v'$latest'...';"
            "curl -sL https://github.com/wynlang/wyn/releases/download/v$latest/wyn-%s -o /tmp/wyn_new && "
            "chmod +x /tmp/wyn_new && "
            "%smv /tmp/wyn_new %s && "
            "echo '\\033[32m✓\\033[0m Upgraded to v'$latest",
            get_version(), platform, need_sudo ? "sudo " : "", self_path[0] ? self_path : "/usr/local/bin/wyn");
        return system(cmd) == 0 ? 0 : 1;
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
        fprintf(stderr, "  \033[32mrun <file.wyn>         Compile and run (-e for eval)\n");
        fprintf(stderr, "  \033[32mcheck\033[0m <file.wyn>       Type-check without compiling\n");
        fprintf(stderr, "  \033[32mfmt\033[0m <file.wyn>         Format source file\n");
        fprintf(stderr, "  \033[32mtest\033[0m                    Run project tests\n");
        fprintf(stderr, "  \033[32mwatch\033[0m <file.wyn>       Watch and auto-rebuild\n");
        fprintf(stderr, "  \033[32mrepl\033[0m                    Interactive REPL\n");
        fprintf(stderr, "  \033[32mbench\033[0m <file.wyn>       Benchmark with timing\n");
        fprintf(stderr, "  \033[32mdoc\033[0m <file.wyn>         Generate documentation\n");
        fprintf(stderr, "\n\033[1mBuild:\033[0m\n");
        fprintf(stderr, "  \033[32mbuild\033[0m <dir>             Build project (--shared / --python)\n");
        fprintf(stderr, "  \033[32mcross\033[0m <target> <file>   Cross-compile (linux/macos/windows/ios/android)\n");
        fprintf(stderr, "  \033[32mbuild-runtime\033[0m           Precompile runtime for fast builds\n");
        fprintf(stderr, "  \033[32mclean\033[0m                   Remove build artifacts\n");
        fprintf(stderr, "\n\033[1mPackages:\033[0m\n");
        fprintf(stderr, "  \033[32minit\033[0m [name]             Create new project\n");
        fprintf(stderr, "  \033[32mpkg\033[0m <command>           Package manager (register, login, search, install, push)\n");
        
        fprintf(stderr, "\n\033[1mTools:\033[0m\n");
        fprintf(stderr, "  \033[32mlsp\033[0m                     Start language server (for editors)\n");
        fprintf(stderr, "  \033[32minstall\033[0m                 Install wyn to system PATH\n");
        fprintf(stderr, "  \033[32muninstall\033[0m               Remove wyn from system PATH\n");
        fprintf(stderr, "  \033[32mdoctor\033[0m                  Check your setup\n");
        fprintf(stderr, "  \033[32mupgrade\033[0m                 Update wyn to latest version\n");
        fprintf(stderr, "  \033[32mversion\033[0m                 Show version\n");
        fprintf(stderr, "  \033[32mhelp\033[0m                    Show this help\n");
        fprintf(stderr, "\n\033[1mFlags:\033[0m\n");
        fprintf(stderr, "  \033[33m--fast\033[0m                  Skip optimizations (fastest compile)\n");
        fprintf(stderr, "  \033[33m--release               Full optimizations (-O3)\n");
        fprintf(stderr, "  \033[33m--debug\033[0m                Keep .c and .out artifacts\n");
        fprintf(stderr, "\n\033[1mCross-compile targets:\033[0m\n");
        fprintf(stderr, "  linux, macos, windows, ios, android\n");
        fprintf(stderr, "\n\033[2mhttps://wynlang.com\033[0m\n");
        return 0;
    }
    
    if (strcmp(command, "init") == 0 || strcmp(command, "new") == 0) {
        static char input[256];
        char* project_name = NULL;
        const char* template = "default";
        const char* lib_target = NULL;
        
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--web") == 0) template = "web";
            else if (strcmp(argv[i], "--api") == 0) template = "api";
            else if (strcmp(argv[i], "--cli") == 0) template = "cli";
            else if (strcmp(argv[i], "--lib") == 0) {
                template = "lib";
                if (i + 1 < argc && argv[i+1][0] != '-') {
                    lib_target = argv[++i];
                }
            }
            else if (!project_name) project_name = argv[i];
        }
        
        // --lib without target: show help
        if (strcmp(template, "lib") == 0 && !lib_target) {
            printf("Library target required. Options:\n\n");
            printf("  wyn init mylib --lib wyn      Wyn package (installable via wyn pkg install)\n");
            printf("  wyn init mylib --lib python    Python extension module\n");
            printf("  wyn init mylib --lib node      Node.js native addon (N-API)\n");
            printf("  wyn init mylib --lib c         C shared library with header\n");
            return 0;
        }
        
        if (!project_name) {
            printf("Enter project name: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = 0;
                if (strlen(input) == 0) { fprintf(stderr, "Error: Project name cannot be empty\n"); return 1; }
                project_name = input;
            } else { return 1; }
        }
        return create_new_project_with_template(project_name, template, lib_target);
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
            fprintf(stderr, "Usage: wyn pkg <command>\n\n");
            fprintf(stderr, "Commands:\n");
            fprintf(stderr, "  register              Create an account on pkg.wynlang.com\n");
            fprintf(stderr, "  login                 Log in and save API key\n");
            fprintf(stderr, "  whoami                Show current logged-in user\n");
            fprintf(stderr, "  search <query>        Search the package registry\n");
            fprintf(stderr, "  info <name>           Show package details\n");
            fprintf(stderr, "  install <name>        Install a package (latest)\n");
            fprintf(stderr, "  install <name>@<ver>  Install a specific version\n");
            fprintf(stderr, "  uninstall <name>      Remove an installed package\n");
            fprintf(stderr, "  list                  List installed packages\n");
            fprintf(stderr, "  push                  Publish current project to registry\n");
            return 1;
        }
        extern int package_install(const char*);
        extern int package_list();
        extern int registry_search(const char*);
        extern int registry_info(const char*);
        extern int registry_install(const char*);
        extern int registry_publish(int dry_run);

        // Helper: auth file path
        char auth_path[512];
        snprintf(auth_path, sizeof(auth_path), "%s/.wyn/auth", getenv("HOME") ? getenv("HOME") : "/tmp");

        // Helper: temp file path (PID-scoped to avoid races)
        char tmp_json[256];
        snprintf(tmp_json, sizeof(tmp_json), "/tmp/wyn-pkg-%d.json", getpid());

        // Helper: read API key from ~/.wyn/auth
        #define READ_API_KEY(key_buf) do { \
            FILE* _af = fopen(auth_path, "r"); \
            if (!_af) { fprintf(stderr, "\033[31m✗\033[0m Not logged in. Run: wyn pkg register\n"); return 1; } \
            char _auth[2048] = {0}; fread(_auth, 1, sizeof(_auth)-1, _af); fclose(_af); \
            char* _kp = strstr(_auth, "\"api_key\":\""); \
            if (!_kp) _kp = strstr(_auth, "\"api_key\": \""); \
            if (_kp) { _kp = strchr(_kp + 9, '"'); if (_kp) { _kp++; char* _ke = strchr(_kp, '"'); \
                if (_ke) { size_t _kl = _ke - _kp; if (_kl >= sizeof(key_buf)) _kl = sizeof(key_buf)-1; \
                    memcpy(key_buf, _kp, _kl); key_buf[_kl] = '\0'; } } } \
            if (!key_buf[0]) { fprintf(stderr, "\033[31m✗\033[0m Invalid auth. Run: wyn pkg register\n"); return 1; } \
        } while(0)

        // ── wyn pkg register ──
        if (strcmp(argv[2], "register") == 0) {
            char username[128], password[128], password2[128];
            printf("Username: "); fflush(stdout);
            if (!fgets(username, sizeof(username), stdin)) return 1;
            username[strcspn(username, "\n")] = 0;
            printf("Password: "); fflush(stdout);
            system("stty -echo 2>/dev/null");
            if (!fgets(password, sizeof(password), stdin)) { system("stty echo 2>/dev/null"); return 1; }
            password[strcspn(password, "\n")] = 0;
            system("stty echo 2>/dev/null");
            printf("\nConfirm password: "); fflush(stdout);
            system("stty -echo 2>/dev/null");
            if (!fgets(password2, sizeof(password2), stdin)) { system("stty echo 2>/dev/null"); return 1; }
            password2[strcspn(password2, "\n")] = 0;
            system("stty echo 2>/dev/null");
            printf("\n");

            if (strcmp(password, password2) != 0) {
                fprintf(stderr, "\033[31m✗\033[0m Passwords do not match\n");
                return 1;
            }
            if (strlen(password) < 8) {
                fprintf(stderr, "\033[31m✗\033[0m Password must be at least 8 characters\n");
                return 1;
            }

            // Write JSON to temp file to avoid shell injection
            FILE* jf = fopen(tmp_json, "w");
            if (!jf) { fprintf(stderr, "\033[31m✗\033[0m Cannot create temp file\n"); return 1; }
            fprintf(jf, "{\"username\":\"%s\",\"password\":", username);
            // JSON-escape the password
            fputc('"', jf);
            for (int i = 0; password[i]; i++) {
                if (password[i] == '"' || password[i] == '\\') fputc('\\', jf);
                fputc(password[i], jf);
            }
            fprintf(jf, "\"}");
            fclose(jf);

            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "curl -sS -X POST https://pkg.wynlang.com/api/register "
                "-H 'Content-Type: application/json' -d @%s 2>/dev/null", tmp_json);
            FILE* fp = popen(cmd, "r");
            char resp[2048] = {0};
            if (fp) { fread(resp, 1, sizeof(resp)-1, fp); pclose(fp); }
            unlink(tmp_json);

            if (strstr(resp, "\"registered\"")) {
                printf("\033[32m✓\033[0m Account created for '%s'\n", username);
                printf("\nRun \033[1mwyn pkg login\033[0m to log in and start publishing.\n");
                return 0;
            } else {
                char* ep = strstr(resp, "\"error\":\"");
                if (ep) { ep += 9; char* ee = strchr(ep, '"'); if (ee) *ee = 0; fprintf(stderr, "\033[31m✗\033[0m %s\n", ep); }
                else fprintf(stderr, "\033[31m✗\033[0m Registration failed\n");
                return 1;
            }
        }

        // ── wyn pkg login ──
        if (strcmp(argv[2], "login") == 0) {
            char username[128], password[128];
            printf("Username: "); fflush(stdout);
            if (!fgets(username, sizeof(username), stdin)) return 1;
            username[strcspn(username, "\n")] = 0;
            printf("Password: "); fflush(stdout);
            system("stty -echo 2>/dev/null");
            if (!fgets(password, sizeof(password), stdin)) { system("stty echo 2>/dev/null"); return 1; }
            password[strcspn(password, "\n")] = 0;
            system("stty echo 2>/dev/null");
            printf("\n");

            // Write JSON to temp file to avoid shell injection
            FILE* jf = fopen(tmp_json, "w");
            if (!jf) { fprintf(stderr, "\033[31m✗\033[0m Cannot create temp file\n"); return 1; }
            fprintf(jf, "{\"username\":\"%s\",\"password\":", username);
            fputc('"', jf);
            for (int i = 0; password[i]; i++) {
                if (password[i] == '"' || password[i] == '\\') fputc('\\', jf);
                fputc(password[i], jf);
            }
            fprintf(jf, "\"}");
            fclose(jf);

            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "curl -sS -X POST https://pkg.wynlang.com/api/login "
                "-H 'Content-Type: application/json' -d @%s 2>/dev/null", tmp_json);
            FILE* fp = popen(cmd, "r");
            char resp[2048] = {0};
            if (fp) { fread(resp, 1, sizeof(resp)-1, fp); pclose(fp); }
            unlink(tmp_json);

            if (strstr(resp, "\"api_key\"")) {
                char mkdir_cmd[600]; snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s/.wyn", getenv("HOME"));
                system(mkdir_cmd);
                FILE* af = fopen(auth_path, "w");
                if (af) { fprintf(af, "%s", resp); fclose(af);
#ifndef _WIN32
                    chmod(auth_path, 0600);
#endif
                }
                printf("\033[32m✓\033[0m Logged in as %s\n", username);
                return 0;
            } else {
                fprintf(stderr, "\033[31m✗\033[0m Invalid credentials\n");
                return 1;
            }
        }

        // ── wyn pkg whoami ──
        if (strcmp(argv[2], "whoami") == 0) {
            FILE* af = fopen(auth_path, "r");
            if (!af) { printf("Not logged in. Run: wyn pkg register\n"); return 1; }
            char auth[2048] = {0}; fread(auth, 1, sizeof(auth)-1, af); fclose(af);
            char* up = strstr(auth, "\"username\":\"");
            if (!up) up = strstr(auth, "\"username\": \"");
            if (up) {
                up = strchr(up + 11, '"'); if (up) { up++;
                    char* ue = strchr(up, '"');
                    if (ue) { *ue = 0; printf("%s\n", up); return 0; }
                }
            }
            printf("Not logged in\n");
            return 1;
        }

        // ── wyn pkg search ──
        if (strcmp(argv[2], "search") == 0) {
            if (argc < 4) { fprintf(stderr, "Usage: wyn pkg search <query>\n"); return 1; }
            printf("\033[1mSearching for '%s'...\033[0m\n\n", argv[3]);
            return registry_search(argv[3]);
        }

        // ── wyn pkg info ──
        if (strcmp(argv[2], "info") == 0) {
            if (argc < 4) { fprintf(stderr, "Usage: wyn pkg info <name>\n"); return 1; }
            return registry_info(argv[3]);
        }

        // ── wyn pkg install ──
        if (strcmp(argv[2], "install") == 0) {
            if (argc < 4) { return package_install("."); }
            // Try git URL first, then registry
            if (strstr(argv[3], "github.com") || strstr(argv[3], "/")) {
                return package_install(argv[3]);
            }
            return registry_install(argv[3]);
        }

        // ── wyn pkg uninstall ──
        if (strcmp(argv[2], "uninstall") == 0) {
            if (argc < 4) { fprintf(stderr, "Usage: wyn pkg uninstall <name>\n"); return 1; }
            // Validate name to prevent path traversal
            const char* pname = argv[3];
            for (int i = 0; pname[i]; i++) {
                if (pname[i] == '/' || pname[i] == '\\' || pname[i] == '.') {
                    fprintf(stderr, "\033[31m✗\033[0m Invalid package name\n");
                    return 1;
                }
            }
            char pkg_dir[1024];
            snprintf(pkg_dir, sizeof(pkg_dir), "packages/%s", pname);
            struct stat st;
            if (stat(pkg_dir, &st) != 0) {
                fprintf(stderr, "\033[31m✗\033[0m Package '%s' is not installed\n", pname);
                return 1;
            }
            char cmd[1100]; snprintf(cmd, sizeof(cmd), "rm -rf 'packages/%s'", pname);
            system(cmd);
            printf("\033[32m✓\033[0m Uninstalled %s\n", pname);
            return 0;
        }

        // ── wyn pkg list ──
        if (strcmp(argv[2], "list") == 0) {
            return package_list();
        }

        // ── wyn pkg push ──
        if (strcmp(argv[2], "push") == 0) {
            // Check login first
            FILE* af_check = fopen(auth_path, "r");
            if (!af_check) {
                fprintf(stderr, "\033[31m✗\033[0m You must be logged in to push packages.\n");
                fprintf(stderr, "\n  Run \033[1mwyn pkg login\033[0m first.\n");
                fprintf(stderr, "  Don't have an account? Run \033[1mwyn pkg register\033[0m\n");
                return 1;
            }
            fclose(af_check);

            FILE* toml = fopen("wyn.toml", "r");
            if (!toml) { fprintf(stderr, "\033[31m✗\033[0m No wyn.toml found in current directory\n"); return 1; }
            char tb[4096]; int tl = fread(tb, 1, sizeof(tb)-1, toml); tb[tl] = 0; fclose(toml);

            char name[128]="", version[32]="", desc[256]="";
            char* np = strstr(tb, "name = \""); if (np) sscanf(np, "name = \"%127[^\"]\"", name);
            char* vp = strstr(tb, "version = \""); if (vp) sscanf(vp, "version = \"%31[^\"]\"", version);
            char* dp = strstr(tb, "description = \""); if (dp) sscanf(dp, "description = \"%255[^\"]\"", desc);
            if (!name[0]) { fprintf(stderr, "\033[31m✗\033[0m No 'name' field in wyn.toml\n"); return 1; }
            if (!version[0]) { fprintf(stderr, "\033[31m✗\033[0m No 'version' field in wyn.toml\n"); return 1; }
            if (!desc[0]) { fprintf(stderr, "\033[31m✗\033[0m No 'description' field in wyn.toml (required for publishing)\n"); return 1; }

            char api_key[256] = "";
            READ_API_KEY(api_key);

            printf("Pushing %s v%s...\n", name, version);

            // Create tarball excluding junk
            char tmp_tar[256];
            snprintf(tmp_tar, sizeof(tmp_tar), "/tmp/wyn-pkg-%d.tar.gz", getpid());
            {
                char tar_cmd[512];
                snprintf(tar_cmd, sizeof(tar_cmd),
                    "tar czf %s "
                    "--exclude='.git' --exclude='build' --exclude='.wyn' "
                    "--exclude='node_modules' --exclude='.DS_Store' "
                    "--exclude='*.o' --exclude='*.out' "
                    "-C . . 2>/dev/null", tmp_tar);
                system(tar_cmd);
            }

            // Check size
            struct stat tar_st;
            if (stat(tmp_tar, &tar_st) != 0 || tar_st.st_size == 0) {
                fprintf(stderr, "\033[31m✗\033[0m Failed to create package tarball\n");
                return 1;
            }
            if (tar_st.st_size > 10 * 1024 * 1024) {
                fprintf(stderr, "\033[31m✗\033[0m Package too large (%lldMB, max 10MB)\n", (long long)(tar_st.st_size / 1024 / 1024));
                unlink(tmp_tar);
                return 1;
            }

            // Build JSON payload via temp file
            FILE* jf = fopen(tmp_json, "w");
            if (!jf) { unlink(tmp_tar); fprintf(stderr, "\033[31m✗\033[0m Cannot create temp file\n"); return 1; }
            // Read base64
            char b64_cmd[512];
#ifdef _WIN32
            snprintf(b64_cmd, sizeof(b64_cmd), "certutil -encode %s /tmp/wyn-b64-%d.txt >nul 2>&1 && type /tmp/wyn-b64-%d.txt", tmp_tar, getpid(), getpid());
#else
            snprintf(b64_cmd, sizeof(b64_cmd), "base64 < %s", tmp_tar);
#endif
            FILE* b64p = popen(b64_cmd, "r");
            char* b64 = malloc(tar_st.st_size * 2);
            size_t b64_len = 0;
            if (b64p && b64) {
                b64_len = fread(b64, 1, tar_st.st_size * 2 - 1, b64p);
                pclose(b64p);
                size_t j = 0;
                for (size_t i = 0; i < b64_len; i++) {
                    if (b64[i] != '\n' && b64[i] != '\r') b64[j++] = b64[i];
                }
                b64[j] = '\0'; b64_len = j;
            } else {
                if (b64p) pclose(b64p);
                if (b64) free(b64);
                fclose(jf); unlink(tmp_tar); unlink(tmp_json);
                fprintf(stderr, "\033[31m✗\033[0m Failed to encode package\n");
                return 1;
            }
            unlink(tmp_tar);

            // Write JSON with proper escaping for all fields
            fprintf(jf, "{\"api_key\":\"");
            for (int i = 0; api_key[i]; i++) { if (api_key[i]=='"'||api_key[i]=='\\') fputc('\\',jf); fputc(api_key[i],jf); }
            fprintf(jf, "\",\"name\":\"");
            for (int i = 0; name[i]; i++) { if (name[i]=='"'||name[i]=='\\') fputc('\\',jf); fputc(name[i],jf); }
            fprintf(jf, "\",\"version\":\"");
            for (int i = 0; version[i]; i++) { if (version[i]=='"'||version[i]=='\\') fputc('\\',jf); fputc(version[i],jf); }
            fprintf(jf, "\",\"description\":\"");
            for (int i = 0; desc[i]; i++) {
                if (desc[i] == '"' || desc[i] == '\\') fputc('\\', jf);
                else if (desc[i] == '\n') { fputc('\\', jf); fputc('n', jf); continue; }
                fputc(desc[i], jf);
            }
            fprintf(jf, "\",\"tarball\":\"");
            fwrite(b64, 1, b64_len, jf);
            free(b64);
            fprintf(jf, "\"}");
            fclose(jf);

            char pub_cmd[1024];
            snprintf(pub_cmd, sizeof(pub_cmd), "curl -sS -X POST https://pkg.wynlang.com/api/publish "
                "-H 'Content-Type: application/json' -d @%s 2>/dev/null", tmp_json);
            FILE* fp = popen(pub_cmd, "r");
            char resp[2048] = {0};
            if (fp) { fread(resp, 1, sizeof(resp)-1, fp); pclose(fp); }
            unlink(tmp_json);

            if (strstr(resp, "\"published\"")) {
                printf("\033[32m✓\033[0m Published %s v%s to pkg.wynlang.com\n", name, version);
                return 0;
            } else {
                char* ep = strstr(resp, "\"error\":\"");
                if (ep) {
                    ep += 9; char* ee = strchr(ep, '"'); if (ee) *ee = 0;
                    fprintf(stderr, "\033[31m✗\033[0m %s\n", ep);
                } else {
                    fprintf(stderr, "\033[31m✗\033[0m Push failed — could not reach pkg.wynlang.com\n");
                }
                return 1;
            }
        }

        fprintf(stderr, "Unknown command: wyn pkg %s\n", argv[2]);
        return 1;
    }
    
    if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            // Try wyn.toml, then src/main.wyn
            struct stat _bs;
            if (stat("wyn.toml", &_bs) == 0) {
                FILE* _tf = fopen("wyn.toml", "r");
                char _tb[4096]; int _tl = fread(_tb, 1, sizeof(_tb)-1, _tf); _tb[_tl] = 0; fclose(_tf);
                char* _ep = strstr(_tb, "entry = \"");
                if (_ep) { char _e[256]; if (sscanf(_ep, "entry = \"%255[^\"]\"", _e) == 1 && stat(_e, &_bs) == 0) { argc = 3; argv[2] = strdup(_e); } }
            }
            if (argc < 3 && stat("src/main.wyn", &_bs) == 0) { argc = 3; argv[2] = "src/main.wyn"; }
            if (argc < 3) {
                fprintf(stderr, "Usage: wyn build <file|dir> [--shared|--python]\n");
                return 1;
            }
        }
        // Detect flags
        char* dir = NULL;
        const char* build_flag = "";
        int build_release = 0;
        int build_pgo = 0;
        const char* output_name = NULL;
        const char* build_target = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--shared") == 0) build_flag = " --shared";
            else if (strcmp(argv[i], "--python") == 0) build_flag = " --python";
            else if (strcmp(argv[i], "--release") == 0) build_release = 1;
            else if (strcmp(argv[i], "--pgo") == 0) build_pgo = 1;
            else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { output_name = argv[++i]; }
            else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) { build_target = argv[++i]; }
            else if (!dir) dir = argv[i];
        }
        
        // If --target is specified, delegate to cross-compile
        if (build_target) {
            // Rewrite as: wyn cross <target> <file> [flags]
            // First resolve the entry file the same way build does
            if (!dir) dir = ".";
            char entry_resolve[512];
            struct stat st_resolve;
            if (stat(dir, &st_resolve) == 0 && S_ISREG(st_resolve.st_mode)) {
                snprintf(entry_resolve, sizeof(entry_resolve), "%s", dir);
            } else {
                char toml_path[512]; snprintf(toml_path, sizeof(toml_path), "%s/wyn.toml", dir);
                FILE* toml = fopen(toml_path, "r");
                int found = 0;
                if (toml) {
                    char tb[4096]; int tl = fread(tb, 1, sizeof(tb)-1, toml); tb[tl] = 0; fclose(toml);
                    char* ep = strstr(tb, "entry = \"");
                    if (ep) { char e[256]; if (sscanf(ep, "entry = \"%255[^\"]\"", e) == 1) { snprintf(entry_resolve, sizeof(entry_resolve), "%s/%s", dir, e); found = 1; } }
                }
                if (!found) {
                    snprintf(entry_resolve, sizeof(entry_resolve), "%s/main.wyn", dir);
                    if (stat(entry_resolve, &st_resolve) != 0) {
                        snprintf(entry_resolve, sizeof(entry_resolve), "%s/src/main.wyn", dir);
                    }
                }
            }
            // Normalize target aliases
            const char* target = build_target;
            if (strcmp(target, "linux-x64") == 0 || strcmp(target, "linux-amd64") == 0) target = "linux";
            else if (strcmp(target, "linux-arm64") == 0 || strcmp(target, "linux-aarch64") == 0) target = "linux-arm64";
            else if (strcmp(target, "macos-x64") == 0) target = "macos-x64";
            else if (strcmp(target, "macos-arm64") == 0) target = "macos-arm64";
            else if (strcmp(target, "windows-x64") == 0 || strcmp(target, "win64") == 0) target = "windows";
            
            // Build new argv for cross command
            char* cross_argv[8];
            cross_argv[0] = argv[0];
            cross_argv[1] = "cross";
            cross_argv[2] = (char*)target;
            cross_argv[3] = entry_resolve;
            // Re-enter main with cross command by falling through
            printf("\033[1mCross-compiling\033[0m %s → %s\n", entry_resolve, target);
            // Execute cross-compile inline
            char cross_cmd[1024];
            snprintf(cross_cmd, sizeof(cross_cmd), "%s cross %s %s", argv[0], target, entry_resolve);
            return system(cross_cmd) == 0 ? 0 : 1;
        }
        if (!dir) dir = ".";
        
        // Accept a .wyn file directly or a directory
        char entry[512];
        struct stat st;
        if (stat(dir, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(entry, sizeof(entry), "%s", dir);
        } else {
            // Try wyn.toml first
            char toml_path[512]; snprintf(toml_path, sizeof(toml_path), "%s/wyn.toml", dir);
            FILE* toml = fopen(toml_path, "r");
            int found_entry = 0;
            if (toml) {
                char tb[4096]; int tl = fread(tb, 1, sizeof(tb)-1, toml); tb[tl] = 0; fclose(toml);
                char* ep = strstr(tb, "entry = \"");
                if (ep) {
                    char e[256]; if (sscanf(ep, "entry = \"%255[^\"]\"", e) == 1) {
                        snprintf(entry, sizeof(entry), "%s/%s", dir, e);
                        if (stat(entry, &st) == 0) found_entry = 1;
                    }
                }
            }
            if (!found_entry) {
                snprintf(entry, sizeof(entry), "%s/main.wyn", dir);
                if (stat(entry, &st) != 0) {
                    snprintf(entry, sizeof(entry), "%s/src/main.wyn", dir);
                    if (stat(entry, &st) != 0) {
                        fprintf(stderr, "\033[31m✗\033[0m No main.wyn found in %s or %s/src\n", dir, dir);
                        return 1;
                    }
                }
            }
        }
        
        printf("\033[1mBuilding\033[0m %s%s...\n", entry, build_flag);
        
        // Compile only — don't run
        char* source = read_file(entry);
        if (!source) return 1;
        
        init_lexer(source);
        init_parser();
        set_parser_filename(entry);
        init_checker();
        
        // Load imports
        extern void preload_imports(const char* source);
        extern void check_all_modules(void);
        extern bool has_circular_import(void);
        preload_imports(source);
        if (has_circular_import()) { fprintf(stderr, "Compilation failed due to circular imports\n"); free(source); return 1; }
        check_all_modules();
        
        Program* prog = parse_program();
        if (!prog) { fprintf(stderr, "Error: Failed to parse\n"); free(source); return 1; }
        
        { extern void set_checker_source(const char*, const char*); set_checker_source(source, entry); }
        check_program(prog);
        if (checker_had_error()) { fprintf(stderr, "Compilation failed\n"); wynter_encourage(); free(source); return 1; }
        
        char out_c[256];
        snprintf(out_c, sizeof(out_c), "%s.c", entry);
        FILE* out_f = fopen(out_c, "w");
        init_codegen(out_f);
        { extern void codegen_set_slim_runtime(bool);
          extern void codegen_set_source_file(const char*);
          codegen_set_slim_runtime(false);
          codegen_set_source_file(entry); }
        codegen_c_header();
        codegen_program(prog);
        fclose(out_f);
        
        // Determine output binary name
        char bin_path[512];
        if (output_name) {
            snprintf(bin_path, sizeof(bin_path), "%s", output_name);
        } else {
            snprintf(bin_path, sizeof(bin_path), "%s", entry);
            char* dot = strrchr(bin_path, '.'); if (dot) *dot = 0;
        }
        
        // Determine wyn_root
        char wyn_root[1024] = ".";
        char exe_path[1024]; strncpy(exe_path, argv[0], sizeof(exe_path)-1);
        char* ls = strrchr(exe_path, '/'); if (ls) { *ls = 0; snprintf(wyn_root, sizeof(wyn_root), "%s", exe_path); }

        // Compile with TCC or system cc
        const char* cc = detect_cc();
        char tcc_bin[512]; snprintf(tcc_bin, sizeof(tcc_bin), "%s/vendor/tcc/bin/tcc", wyn_root);
        char rt_tcc[512]; snprintf(rt_tcc, sizeof(rt_tcc), "%s/vendor/tcc/lib/libwyn_rt_tcc.a", wyn_root);
        const char* sqlite_flags = "";
        const char* sqlite_src = "";
        if (strstr(source, "Db.")) {
            struct stat _ss;
            if (stat("./packages/sqlite/src/sqlite3.c", &_ss) == 0) {
                sqlite_flags = "-DWYN_USE_SQLITE -I ./packages/sqlite/src";
                sqlite_src = " ./packages/sqlite/src/sqlite3.c";
            } else {
                sqlite_flags = "-DWYN_USE_SQLITE -lsqlite3";
            }
        }
        
        char cmd[4096];
        int result = -1;
        char rt_lib[512]; snprintf(rt_lib, sizeof(rt_lib), "%s/runtime/libwyn_rt.a", wyn_root);

        // Detect App module for webview linking
        const char* app_link = "";
#ifdef __APPLE__
        if (strstr(source, "App.")) {
            static char _app_link_buf[1024];
            snprintf(_app_link_buf, sizeof(_app_link_buf), " %s/src/wyn_webview.o -framework WebKit -framework Cocoa", wyn_root);
            app_link = _app_link_buf;
        }
#endif

        if (build_flag[0] == 0 && !build_release && access(tcc_bin, X_OK) == 0 && access(rt_tcc, R_OK) == 0 && !strstr(source, "App.")) {
            int _p = 0;
            _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s -o %s -I %s/src -I %s/vendor/tcc/tcc_include -I %s/vendor/minicoro -L %s/vendor/tcc/lib -w -DMCO_NO_MULTITHREAD -DMCO_USE_UCONTEXT -D_XOPEN_SOURCE=600 %s ", tcc_bin, bin_path, wyn_root, wyn_root, wyn_root, wyn_root, sqlite_flags);
            _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s.c ", entry);
            const char* _srcs[] = {"wyn_arena","stdlib_string","stdlib_array","stdlib_time","stdlib_crypto","stdlib_math","wyn_wrapper","wyn_interface","coroutine","spawn_fast","spawn","future","io","io_loop","optional","result","arc_runtime","concurrency","async_runtime","safe_memory","error","string_runtime","hashmap","hashset","json","stdlib_runtime","hashmap_runtime","net","net_runtime","net_advanced","test_runtime","file_io_simple","stdlib_enhanced",NULL};
            for (int _si = 0; _srcs[_si]; _si++) _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s/src/%s.c ", wyn_root, _srcs[_si]);
            _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s%s -lpthread -lm 2>/dev/null", rt_tcc, sqlite_src);
            result = system(cmd);
            // Check for zero-byte binary (TCC silently fails sometimes)
            if (result == 0) {
                struct stat st;
                if (stat(bin_path, &st) == 0 && st.st_size == 0) {
                    result = 1; // Force fallback to system cc
                }
            }
        }
        if (result != 0) {
#ifdef __APPLE__
            const char* plibs = "-lpthread -lm";
            (void)plibs;
#else
            const char* plibs = "-lpthread -lm";
#endif
            // Check if precompiled runtime exists
            FILE* _rt_check = fopen(rt_lib, "r");
            if (_rt_check) {
                fclose(_rt_check);
                snprintf(cmd, sizeof(cmd),
#ifdef _WIN32
                    "%s -std=c11 -O3 -w -I %s/src -o %s %s %s.c %s%s -lpthread -lm 2>NUL",
#elif defined(__APPLE__)
                    "%s -std=c11 -O3 -w -Wno-int-conversion -I %s/src -Wl,-dead_strip -o %s %s %s.c %s%s%s -lpthread -lm 2>/tmp/wyn_cc_err.txt",
#else
                    "%s -std=c11 -O3 -w -I %s/src -Wl,--gc-sections -o %s %s %s.c %s%s -lpthread -lm 2>/dev/null",
#endif
                    cc, wyn_root, bin_path, sqlite_flags, entry, rt_lib, sqlite_src
#ifdef __APPLE__
                    , app_link
#endif
                    );
            } else {
                // No precompiled runtime — compile from source files
                int _p = 0;
                _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s -std=c11 -O2 -w -D_GNU_SOURCE -I %s/src -I %s/vendor/minicoro -o %s %s %s.c ", cc, wyn_root, wyn_root, bin_path, sqlite_flags, entry);
                const char* _srcs[] = {"wyn_arena","wyn_wrapper","wyn_interface","coroutine","spawn_fast","spawn","future","io","io_loop","optional","result","arc_runtime","concurrency","async_runtime","safe_memory","error","string_runtime","hashmap","hashset","json","stdlib_runtime","hashmap_runtime","stdlib_string","stdlib_array","stdlib_time","stdlib_crypto","stdlib_math","net","net_runtime","net_advanced","test_runtime",NULL};
                for (int _si = 0; _srcs[_si]; _si++) _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s/src/%s.c ", wyn_root, _srcs[_si]);
                _p += snprintf(cmd + _p, sizeof(cmd) - _p, "%s -lpthread -lm 2>&1", sqlite_src);
            }
            result = system(cmd);
        }
        
        // Don't unlink C file yet if PGO is requested
        if (!build_pgo) unlink(out_c);
        free(source);
        
        if (result == 0 && build_pgo && build_release) {
            // PGO: three-phase build using the generated C file
            printf("\033[1mPGO\033[0m Phase 1: instrumented build...\n");
            char pgo_gen[2048];
            snprintf(pgo_gen, sizeof(pgo_gen),
                "%s -std=c11 -O3 -w -fprofile-instr-generate -I %s/src -o %s.pgo_gen %s.c %s%s -lpthread -lm 2>/dev/null",
                cc, wyn_root, entry, entry, rt_lib, sqlite_src);
            if (system(pgo_gen) != 0) {
                fprintf(stderr, "PGO: instrumented build failed (compiler may not support PGO)\n");
                unlink(out_c);
                goto pgo_done;
            }
            
            // Phase 2: run instrumented binary to generate profile
            printf("\033[1mPGO\033[0m Phase 2: collecting profile...\n");
            char pgo_run[1024];
            snprintf(pgo_run, sizeof(pgo_run), "LLVM_PROFILE_FILE=%s.profraw %s.pgo_gen >/dev/null 2>&1", entry, entry);
            system(pgo_run);
            
            // Merge profile data
            char pgo_merge[1024];
            snprintf(pgo_merge, sizeof(pgo_merge),
                "xcrun llvm-profdata merge -output=%s.profdata %s.profraw 2>/dev/null || "
                "llvm-profdata merge -output=%s.profdata %s.profraw 2>/dev/null",
                entry, entry, entry, entry);
            if (system(pgo_merge) != 0) {
                fprintf(stderr, "PGO: profile merge failed (llvm-profdata not found)\n");
                goto pgo_cleanup;
            }
            
            // Phase 3: recompile with profile data
            printf("\033[1mPGO\033[0m Phase 3: optimized build...\n");
            char pgo_use[2048];
            snprintf(pgo_use, sizeof(pgo_use),
                "%s -std=c11 -O3 -w -fprofile-instr-use=%s.profdata -I %s/src -o %s %s.c %s%s -lpthread -lm 2>/dev/null",
                cc, entry, wyn_root, bin_path, entry, rt_lib, sqlite_src);
            result = system(pgo_use);
            
            pgo_cleanup:
            // Clean up PGO artifacts
            { char tmp[512];
              snprintf(tmp, sizeof(tmp), "%s.pgo_gen", entry); unlink(tmp);
              snprintf(tmp, sizeof(tmp), "%s.profraw", entry); unlink(tmp);
              snprintf(tmp, sizeof(tmp), "%s.profdata", entry); unlink(tmp);
            }
            unlink(out_c);
            pgo_done:
            if (result == 0) {
                printf("\033[32m✓\033[0m Built with PGO: %s\n", bin_path);
            }
        } else if (result == 0) {
            printf("\033[32m✓\033[0m Built: %s\n", bin_path);
        } else {
            fprintf(stderr, "\033[31m✗\033[0m Build failed\n");
            // Show compiler errors if available
            FILE* ef = fopen("/tmp/wyn_cc_err.txt", "r");
            if (!ef) ef = fopen("/tmp/wyn_tcc_err.txt", "r");
            if (ef) {
                char ebuf[2048];
                size_t n = fread(ebuf, 1, sizeof(ebuf)-1, ef);
                ebuf[n] = '\0';
                fclose(ef);
                if (n > 0) fprintf(stderr, "  Compiler output:\n%s\n", ebuf);
            }
        }
        return result == 0 ? 0 : 1;
    }
    
    if (strcmp(command, "test") == 0) {
        int parallel = 0;
        for (int i = 2; i < argc; i++) { if (strcmp(argv[i], "--parallel") == 0) parallel = 1; }
        struct stat st;
        // Prefer run_tests.wyn if it exists (unless --parallel)
        if (!parallel && stat("tests/run_tests.wyn", &st) == 0) {
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
            "for f in tests/test_*.wyn tests/*/test_*.wyn tests/test_*.🐉 tests/*/test_*.🐉; do "
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
        if (parallel) {
            // Parallel: run all test files concurrently
            snprintf(cmd, sizeof(cmd),
                "echo '\\033[1mRunning tests in parallel...\\033[0m\\n'; "
                "pass=0; fail=0; tmpdir=$(mktemp -d); "
                "for f in tests/test_*.wyn tests/*/test_*.wyn tests/test_*.🐉 tests/*/test_*.🐉; do "
                "  [ -f \"$f\" ] || continue; "
                "  ( %s run \"$f\" > /dev/null 2>&1; echo $? > \"$tmpdir/$(echo $f | tr / _)\" ) & "
                "done; wait; "
                "for r in \"$tmpdir\"/*; do "
                "  f=$(basename \"$r\" | tr _ /); "
                "  if [ \"$(cat $r)\" = \"0\" ]; then echo \"  \\033[32m✓\\033[0m $f\"; pass=$((pass+1)); "
                "  else echo \"  \\033[31m✗\\033[0m $f\"; fail=$((fail+1)); fi; "
                "done; rm -rf \"$tmpdir\"; "
                "echo; echo \"\\033[1mResults:\\033[0m $pass passed, $fail failed\"; "
                "[ $fail -eq 0 ]",
                argv[0]);
        }
        return system(cmd) == 0 ? 0 : 1;
    }
    
    if (strcmp(command, "build-runtime") == 0) {
        // Precompile runtime library for fast compilation
        printf("Building runtime library...\n");
        char cmd[8192];
        char wyn_root[1024] = ".";
        char* root_env = getenv("WYN_ROOT");
        if (root_env) strncpy(wyn_root, root_env, sizeof(wyn_root)-1);
        else {
            char exe_path[1024];
            strncpy(exe_path, argv[0], sizeof(exe_path)-1);
            char* last_slash = strrchr(exe_path, '/');
            if (last_slash) { *last_slash = 0; strncpy(wyn_root, exe_path, sizeof(wyn_root)-1); }
        }
        // Build for-loop command from unified source list
        char for_list[4096];
        build_source_list(for_list, sizeof(for_list), "");
        snprintf(cmd, sizeof(cmd),
            "mkdir -p %s/runtime/obj && cd %s && "
            "for f in %s; do "
            "gcc -std=c11 -O2 -w -I src -I vendor/minicoro -c $f -o runtime/obj/$(basename $f .c).o 2>/dev/null; done && "
            "ar rcs runtime/libwyn_rt.a runtime/obj/*.o && "
            "echo 'Built runtime/libwyn_rt.a'",
            wyn_root, wyn_root, for_list);
        int rt_result = system(cmd);
        if (rt_result == 0) {
            // Also build precompiled header for faster compilation
            char pch_cmd[1024];
            snprintf(pch_cmd, sizeof(pch_cmd), "gcc -std=c11 -O0 -w %s/src/wyn_runtime.h -o %s/src/wyn_runtime.h.gch", wyn_root, wyn_root);
            if (system(pch_cmd) == 0) {
                printf("Built precompiled header\n");
            }
        }
        return rt_result;
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
                fprintf(f, "fn main() {}\n");
                // Add to history
                strcat(history, line);
                strcat(history, "\n");
            } else {
                // Expression — wrap in println so result is visible
                // Detect if it looks like a statement (assignment, println, etc.)
                int is_stmt = (strncmp(line, "println", 7) == 0 || strncmp(line, "print(", 6) == 0 ||
                              strstr(line, " = ") != NULL || strncmp(line, "if ", 3) == 0 ||
                              strncmp(line, "for ", 4) == 0 || strncmp(line, "while ", 6) == 0 ||
                              strstr(line, ".insert") != NULL || strstr(line, ".push") != NULL ||
                              strstr(line, ".exec") != NULL || strstr(line, ".close") != NULL);
                if (is_stmt) {
                    fprintf(f, "fn main() {\n  %s\n}\n", line);
                } else {
                    fprintf(f, "fn main() {\n  println(%s)\n}\n", line);
                }
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
            fprintf(stderr, "Usage: wyn bench <file.wyn> [--iterations N] [--compare]\n");
            return 1;
        }
        
        const char* file = argv[2];
        int iterations = 10;
        int compare_only = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) { iterations = atoi(argv[++i]); }
            else if (strcmp(argv[i], "--compare") == 0) { compare_only = 1; }
        }
        if (iterations < 1) iterations = 1;
        if (iterations > 10000) iterations = 10000;
        
        // Compile once with --release
        printf("\033[1mBenchmarking\033[0m %s (%d iterations)\n\n", file, iterations);
        char build_cmd[1024];
        snprintf(build_cmd, sizeof(build_cmd), "%s build %s --release 2>/dev/null", argv[0], file);
        if (system(build_cmd) != 0) {
            fprintf(stderr, "\033[31m✗\033[0m Build failed\n");
            return 1;
        }
        
        // Determine binary path (strip .wyn extension)
        char bin[512];
        snprintf(bin, sizeof(bin), "%s", file);
        char* dot = strrchr(bin, '.'); if (dot) *dot = '\0';
        
        // Check previous results for --compare
        char bench_file[512];
        snprintf(bench_file, sizeof(bench_file), "%s.bench", file);
        if (compare_only) {
            FILE* prev = fopen(bench_file, "r");
            if (!prev) { fprintf(stderr, "No previous benchmark data: %s\n", bench_file); return 1; }
            double prev_avg, prev_min, prev_p99;
            if (fscanf(prev, "%lf %lf %lf", &prev_avg, &prev_min, &prev_p99) >= 1) {
                printf("  Previous: avg=%.1fms min=%.1fms p99=%.1fms\n", prev_avg, prev_min, prev_p99);
            }
            fclose(prev);
            printf("  Run again without --compare to update.\n");
            return 0;
        }
        
        // Run N iterations, time execution only
        double* times = malloc(sizeof(double) * iterations);
        for (int i = 0; i < iterations; i++) {
            struct timespec start, end;
            char run_cmd[512];
            snprintf(run_cmd, sizeof(run_cmd), "%s > /dev/null 2>&1", bin);
            clock_gettime(CLOCK_MONOTONIC, &start);
            int r = system(run_cmd);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (r != 0 && i == 0) {
                fprintf(stderr, "\033[31m✗\033[0m Execution failed\n");
                free(times); return 1;
            }
            times[i] = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
            if (iterations <= 20) printf("  Run %d: \033[33m%.1fms\033[0m\n", i + 1, times[i]);
        }
        
        // Sort for percentiles
        for (int i = 0; i < iterations - 1; i++)
            for (int j = i + 1; j < iterations; j++)
                if (times[j] < times[i]) { double t = times[i]; times[i] = times[j]; times[j] = t; }
        
        double sum = 0, min = times[0], max = times[iterations - 1];
        for (int i = 0; i < iterations; i++) sum += times[i];
        double avg = sum / iterations;
        double median = times[iterations / 2];
        int p99_idx = (int)(iterations * 0.99); if (p99_idx >= iterations) p99_idx = iterations - 1;
        double p99 = times[p99_idx];
        double ops_sec = (avg > 0) ? 1000.0 / avg : 0;
        
        printf("\n\033[1mResults:\033[0m\n");
        printf("  min:    \033[32m%.1fms\033[0m\n", min);
        printf("  avg:    %.1fms\n", avg);
        printf("  median: %.1fms\n", median);
        printf("  p99:    %.1fms\n", p99);
        printf("  max:    %.1fms\n", max);
        printf("  ops/s:  %.0f\n", ops_sec);
        
        // Compare with previous
        FILE* prev = fopen(bench_file, "r");
        if (prev) {
            double prev_avg, prev_min, prev_p99;
            if (fscanf(prev, "%lf %lf %lf", &prev_avg, &prev_min, &prev_p99) >= 1) {
                double delta = avg - prev_avg;
                double pct = (delta / prev_avg) * 100;
                if (delta < -1) printf("\n  \033[32m↓ %.1fms faster (%.1f%%)\033[0m vs previous\n", -delta, -pct);
                else if (delta > 1) printf("\n  \033[31m↑ %.1fms slower (%.1f%%)\033[0m vs previous\n", delta, pct);
                else printf("\n  \033[2m≈ same as previous\033[0m\n");
            }
            fclose(prev);
        }
        
        // Save results
        FILE* save = fopen(bench_file, "w");
        if (save) { fprintf(save, "%.2f %.2f %.2f\n", avg, min, p99); fclose(save); }
        
        // Cleanup binary
        unlink(bin);
        char c_file[512]; snprintf(c_file, sizeof(c_file), "%s.c", file); unlink(c_file);
        free(times);
        
        return 0;
    }
    
    // wyn doc <file> [--html] — generate docs from source
    if (strcmp(command, "doc") == 0) {
        extern int cmd_doc(const char* file, int argc, char** argv);
        extern int cmd_doc_project(int argc, char** argv);
        // If no file arg or file is --html, do project-level docs
        if (argc < 3 || (argc == 3 && strcmp(argv[2], "--html") == 0)) {
            return cmd_doc_project(argc, argv);
        }
        return cmd_doc(argv[2], argc, argv);
    }
    
    if (strcmp(command, "cross") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: wyn cross <target> <file.wyn>\n");
            fprintf(stderr, "Targets: linux, linux-x64, linux-arm64, macos, macos-x64, macos-arm64,\n");
            fprintf(stderr, "         windows, windows-x64, ios, android\n");
            return 1;
        }
        
        char* target = argv[2];
        char* file = argv[3];
        
        // Normalize target aliases
        char* arch = "x86_64";
        if (strcmp(target, "linux-x64") == 0 || strcmp(target, "linux-amd64") == 0) { target = "linux"; arch = "x86_64"; }
        else if (strcmp(target, "linux-arm64") == 0 || strcmp(target, "linux-aarch64") == 0) { target = "linux"; arch = "aarch64"; }
        else if (strcmp(target, "macos-x64") == 0) { target = "macos"; arch = "x86_64"; }
        else if (strcmp(target, "macos-arm64") == 0) { target = "macos"; arch = "arm64"; }
        else if (strcmp(target, "windows-x64") == 0 || strcmp(target, "win64") == 0) { target = "windows"; arch = "x86_64"; }
        else if (strcmp(target, "linux") == 0) { arch = "x86_64"; }  // default linux to x64
        else if (strcmp(target, "macos") == 0) {
#ifdef __aarch64__
            arch = "arm64";
#else
            arch = "x86_64";
#endif
        }
        
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
        { extern void set_source_directory(const char*); set_source_directory(file); }
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        check_all_modules();  // Type check loaded modules
        
        Program* prog = parse_program();
        if (!prog) {
            if (!parser_had_error()) { fprintf(stderr, "Error: Failed to parse program\n"); fprintf(stderr, "  Hint: Check for stray code outside functions or unmatched braces\n"); }
            free(source);
            return 1;
        }
        
        { extern void set_checker_source(const char*, const char*); set_checker_source(source, file); } check_program(prog);
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n"); wynter_encourage();
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
            snprintf(compile_cmd, sizeof(compile_cmd), "gcc -std=c11 -O2 -w -I %s/src -o %s.linux %s.c %s/runtime/libwyn_rt.a -lpthread -lm", wyn_root, file, file, wyn_root);
            printf("Compiling for Linux (%s)...\n", arch);
        } else if (strcmp(target, "macos") == 0) {
            snprintf(compile_cmd, sizeof(compile_cmd), "clang -std=c11 -O2 -w -arch %s -I %s/src -o %s.macos %s.c %s/runtime/libwyn_rt.a -lpthread -lm", arch, wyn_root, file, file, wyn_root);
            printf("Compiling for macOS (%s)...\n", arch);
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
                "-I %s/src -I %s/vendor/minicoro -o %s.ios %s.c "
                "%s/src/wyn_arena.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c "
                "%s/src/hashmap.c %s/src/hashset.c %s/src/json.c "
                "%s/src/string_runtime.c %s/src/stdlib_runtime.c %s/src/hashmap_runtime.c "
                "%s/src/test_runtime.c %s/src/spawn.c %s/src/spawn_fast.c %s/src/io_loop.c %s/src/coroutine.c %s/src/future.c "
                "%s/src/net.c %s/src/net_advanced.c %s/src/net_runtime.c "
                "%s/src/optional.c %s/src/result.c %s/src/arc_runtime.c "
                "%s/src/safe_memory.c %s/src/error.c %s/src/concurrency.c %s/src/async_runtime.c "
                "%s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c %s/src/stdlib_math.c "
                "-lpthread -lm",
                sdk_path, wyn_root, wyn_root, file, file,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root, wyn_root,
                wyn_root, wyn_root, wyn_root, wyn_root, wyn_root);
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
        { extern void set_source_directory(const char*); set_source_directory(file); }
        char* source = read_file(file);
        if (!source) { fprintf(stderr, "Error: Cannot read %s\n", file); return 1; }
        extern void preload_imports(const char* source);
        extern bool has_circular_import(void);
        preload_imports(source);
        if (has_circular_import()) { fprintf(stderr, "Compilation failed due to circular imports\n"); free(source); return 1; }
        init_lexer(source);
        init_parser();
        set_parser_filename(file);
        init_checker();
        check_all_modules();
        Program* prog = parse_program();
        if (!prog) { fprintf(stderr, "Parse error\n"); free(source); return 1; }
        { extern void set_checker_source(const char*, const char*); set_checker_source(source, file); } check_program(prog);
        if (checker_had_error()) { free(source); return 1; }
        printf("✓ %s: no errors\n", file);
        free(source);
        return 0;
    }
    
    if (strcmp(command, "fmt") == 0) {
        extern int cmd_fmt(const char* file, int argc, char** argv);
        if (argc < 3) {
            fprintf(stderr, "Usage: wyn fmt <file.wyn|directory> [--check]\n");
            return 1;
        }
        // Check for --check flag
        int check_only = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--check") == 0) check_only = 1;
        }
        // If argument is a directory, format all .wyn files recursively
        struct stat fmt_st;
        if (stat(argv[2], &fmt_st) == 0 && S_ISDIR(fmt_st.st_mode)) {
            char fmt_cmd[4096];
            if (check_only) {
                snprintf(fmt_cmd, sizeof(fmt_cmd),
                    "changed=0; for f in $(find %s -name '*.wyn' -not -path '*/.archive/*'); do "
                    "orig=$(cat \"$f\"); formatted=$(%s fmt \"$f\" 2>/dev/null && cat \"$f\"); "
                    "if [ \"$orig\" != \"$formatted\" ]; then echo \"  ✗ $f\"; changed=1; fi; done; "
                    "[ $changed -eq 0 ] && echo '✓ All files formatted' || exit 1",
                    argv[2], argv[0]);
            } else {
                snprintf(fmt_cmd, sizeof(fmt_cmd),
                    "count=0; for f in $(find %s -name '*.wyn' -not -path '*/.archive/*'); do "
                    "%s fmt \"$f\" 2>/dev/null && count=$((count+1)); done; "
                    "echo \"✓ Formatted $count files\"",
                    argv[2], argv[0]);
            }
            return system(fmt_cmd) == 0 ? 0 : 1;
        }
        return cmd_fmt(argv[2], argc, argv);
    }
    
    if (strcmp(command, "repl") == 0) {
        extern int cmd_repl(int argc, char** argv);
        return cmd_repl(argc, argv);
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
    
    if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            // Try to find main.wyn or read wyn.toml
            struct stat _rs;
            if (stat("wyn.toml", &_rs) == 0) {
                FILE* _tf = fopen("wyn.toml", "r");
                char _tb[4096]; int _tl = fread(_tb, 1, sizeof(_tb)-1, _tf); _tb[_tl] = 0; fclose(_tf);
                char* _ep = strstr(_tb, "entry = \"");
                if (_ep) { char _e[256]; if (sscanf(_ep, "entry = \"%255[^\"]\"", _e) == 1 && stat(_e, &_rs) == 0) { argc = 3; argv[2] = strdup(_e); } }
            }
            if (argc < 3 && stat("main.wyn", &_rs) == 0) { argc = 3; argv[2] = "main.wyn"; }
            if (argc < 3 && stat("src/main.wyn", &_rs) == 0) { argc = 3; argv[2] = "src/main.wyn"; }
            if (argc < 3) { fprintf(stderr, "Usage: wyn run <file.wyn>\n"); return 1; }
        }
        char* file = NULL;
        char* eval_code = NULL;
        
        // Check for --debug flag, -e eval, and find file arg
        int keep_artifacts = 0;
        int mem_stats = 0;
        int user_args_start = -1;  // index in argv where user args begin (after --)
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--") == 0) { user_args_start = i + 1; break; }
            if (strcmp(argv[i], "--debug") == 0) keep_artifacts = 1;
            else if (strcmp(argv[i], "--mem-stats") == 0) mem_stats = 1;
            else if (strcmp(argv[i], "--fast") == 0 || strcmp(argv[i], "--release") == 0 || strcmp(argv[i], "--shared") == 0 || strcmp(argv[i], "--python") == 0) {}
            else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) { eval_code = argv[++i]; }
            else if (!file) file = argv[i];
        }
        // Handle -e: write to temp file
        if (eval_code) {
            file = "/tmp/__wyn_eval.wyn";
            FILE* ef = fopen(file, "w");
            if (ef) { fprintf(ef, "%s\n", eval_code); fclose(ef); }
        }
        if (!file) {
            fprintf(stderr, "Usage: wyn run <file.wyn>\n");
            return 1;
        }
        
        // Platform-specific link flags
        #ifdef _WIN32
        const char* platform_libs = "-lws2_32 -lpthread -lm";
        #else
        const char* platform_libs = "-lpthread -lm";
        #endif
        
        // Incremental: skip recompilation if binary is newer than source
        {
            char out_path[512];
            snprintf(out_path, sizeof(out_path), "%s.out", file);
            struct stat src_st, out_st;
            if (stat(file, &src_st) == 0 && stat(out_path, &out_st) == 0) {
                if (out_st.st_mtime >= src_st.st_mtime) {
                    char run_cmd[2048];
                    if (out_path[0] == '/') {
                        snprintf(run_cmd, sizeof(run_cmd), "%s", out_path);
                    } else {
#ifdef _WIN32
                        snprintf(run_cmd, sizeof(run_cmd), "%s", out_path);
#else
                        snprintf(run_cmd, sizeof(run_cmd), "./%s", out_path);
#endif
                    }
                    for (int i = 3; i < argc; i++) { strcat(run_cmd, " "); strcat(run_cmd, argv[i]); }
                    return system(run_cmd);
                }
            }
        }
        
        char* source = read_file(file);
        
        // Friendly message for empty files
        if (!source || strlen(source) == 0 || (strlen(source) == 1 && source[0] == '\n')) {
            printf("\033[33m○\033[0m Nothing to run. Try:\n\n  println(\"hello\")\n\n");
            return 0;
        }
        
        // Set source directory for module resolution
        { extern void set_source_directory(const char*); set_source_directory(file); }
        
        // Pre-load all imports before parsing
        extern void preload_imports(const char* source);
        extern bool has_circular_import(void);
        preload_imports(source);
        if (has_circular_import()) { fprintf(stderr, "Compilation failed due to circular imports\n"); free(source); return 1; }
        
        init_lexer(source);
        init_parser();
        set_parser_filename(file);  // Set filename for better error messages
        init_checker();
        check_all_modules();  // Type check loaded modules
        
        Program* prog = parse_program();
        if (!prog) {
            if (!parser_had_error()) { fprintf(stderr, "Error: Failed to parse program\n"); fprintf(stderr, "  Hint: Check for stray code outside functions or unmatched braces\n"); }
            free(source);
            return 1;
        }
        
        // Type check
        { extern void set_checker_source(const char*, const char*); set_checker_source(source, file); } check_program(prog);
        
        if (checker_had_error()) {
            fprintf(stderr, "Compilation failed due to errors\n"); wynter_encourage();
            free(source);
            return 1;
        }
        
        char out_path[256];
        snprintf(out_path, 256, "%s.c", file);
        FILE* out = fopen(out_path, "w");
        struct timespec _ts_start, _ts_end;
        clock_gettime(CLOCK_MONOTONIC, &_ts_start);
        init_codegen(out);
        { extern void codegen_set_slim_runtime(bool);
          extern void codegen_set_source_file(const char*);
          bool _slim = false;
          for (int _i = 2; _i < argc; _i++) if (strcmp(argv[_i], "--release") == 0) _slim = true;
          codegen_set_slim_runtime(_slim);
          codegen_set_source_file(file); }
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
        // SQLite: check for package in project, fall back to system library
        const char* sqlite_flags = "";
        if (strstr(source, "Db.")) {
            struct stat _ss;
            if (stat("./packages/sqlite/src/sqlite3.c", &_ss) == 0) {
                sqlite_flags = " -DWYN_USE_SQLITE -I ./packages/sqlite/src ./packages/sqlite/src/sqlite3.c";
            } else {
                sqlite_flags = " -DWYN_USE_SQLITE -lsqlite3";
            }
        }
        const char* gui_flags = strstr(source, "Gui.") ? " -DWYN_USE_GUI $(pkg-config --cflags --libs sdl2 2>/dev/null || echo '-lSDL2') " : "";
        const char* app_flags = "";
        if (strstr(source, "App.")) {
#ifdef __APPLE__
            static char _app_flags_buf[1024];
            snprintf(_app_flags_buf, sizeof(_app_flags_buf), " %s/src/wyn_webview.o -framework WebKit -framework Cocoa", wyn_root);
            app_flags = _app_flags_buf;
#elif defined(__linux__)
            app_flags = " $(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0 2>/dev/null)";
#elif defined(_WIN32)
            static char _app_flags_buf[1024];
            snprintf(_app_flags_buf, sizeof(_app_flags_buf), " %s/src/wyn_webview_win.c", wyn_root);
            app_flags = _app_flags_buf;
#endif
        }
        
        // Check for --fast flag (use -O0 for fastest compile)
        const char* opt_level = "-O1";
        int shared_mode = 0;  // 0=normal, 1=--shared, 2=--python, 4=--node
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--fast") == 0) { opt_level = "-O0"; }
            if (strcmp(argv[i], "--release") == 0) { opt_level = "-O3"; }
            if (strcmp(argv[i], "--shared") == 0) { shared_mode = 1; }
            if (strcmp(argv[i], "--python") == 0) { shared_mode = 2; }
            if (strcmp(argv[i], "--node") == 0) { shared_mode = 4; }
        }
        
        // WASM target — early check
        // Node.js — build shared lib + JS wrapper
        if (shared_mode == 4) { shared_mode = 1; }
        int generate_node = 0;
        int use_release = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--node") == 0) { generate_node = 1; }
            if (strcmp(argv[i], "--release") == 0) { use_release = 1; }
        }
        
        // Try TCC backend first (fast, no external deps) unless --release
        if (!use_release && !shared_mode && !generate_node && wyn_tcc_available()) {
            // Read the generated C source
            char* c_source = read_file(out_path);
            if (c_source) {
                char exe_path[512];
                snprintf(exe_path, sizeof(exe_path), "%s.out", file);
                int tcc_result = wyn_tcc_compile_to_exe(c_source, exe_path, wyn_root, NULL);
                free(c_source);
                if (tcc_result == 0) {
                    clock_gettime(CLOCK_MONOTONIC, &_ts_end);
                    double _ms = (_ts_end.tv_sec - _ts_start.tv_sec) * 1000.0 + (_ts_end.tv_nsec - _ts_start.tv_nsec) / 1e6;
                    fprintf(stderr, "\033[2mCompiled in %.0fms (tcc)\033[0m\n", _ms);
                    
                    // Run the compiled binary with user args
                    char run_cmd[4096];
                    int rc = 0;
                    if (file[0] == '/') rc = snprintf(run_cmd, sizeof(run_cmd), "%s.out", file);
                    else rc = snprintf(run_cmd, sizeof(run_cmd), "./%s.out", file);
                    if (user_args_start > 0) {
                        for (int i = user_args_start; i < argc && rc < (int)sizeof(run_cmd) - 2; i++) {
                            rc += snprintf(run_cmd + rc, sizeof(run_cmd) - rc, " %s", argv[i]);
                        }
                    }
                    int result = system(run_cmd);
                    free(source);
                    if (!keep_artifacts) {
                        char c_path[512]; snprintf(c_path, 512, "%s.c", file);
                        char out_path2[512]; snprintf(out_path2, 512, "%s.out", file);
                        unlink(c_path); unlink(out_path2);
                    }
                    unlink("wyn_cc_err.txt");
                    if (WIFEXITED(result)) return WEXITSTATUS(result);
                    return result;
                }
                // TCC failed — fall through to system cc
            }
        }
        
        char compile_cmd[8192];
        const char* cc = detect_cc();
        // Use precompiled runtime library for fast compilation
        // Falls back to source compilation if libwyn_rt.a doesn't exist
        char rt_lib[512];
        snprintf(rt_lib, sizeof(rt_lib), "%s/runtime/libwyn_rt.a", wyn_root);
        FILE* rt_check = fopen(rt_lib, "r");
        if (rt_check) {
            fclose(rt_check);
            snprintf(compile_cmd, sizeof(compile_cmd),
                     "%s -std=c11 %s -w -Wno-error -Wno-incompatible-pointer-types -Wno-int-conversion -I %s/src -o %s.out %s.c %s/runtime/libwyn_rt.a %s/runtime/parser_lib/libwyn_c_parser.a %s 2>wyn_cc_err.txt",
                     cc, opt_level, wyn_root, file, file, wyn_root, wyn_root, platform_libs);
        } else {
            // Fallback: compile from source using unified source list
            char src_list[4096];
            build_source_list(src_list, sizeof(src_list), wyn_root);
            snprintf(compile_cmd, sizeof(compile_cmd),
                     "%s -std=c11 %s -w -Wno-error -Wno-incompatible-pointer-types -Wno-int-conversion -I %s/src -I %s/vendor/minicoro -o %s.out %s.c %s %s 2>wyn_cc_err.txt",
                     cc, opt_level, wyn_root, wyn_root, file, file, src_list, platform_libs);
        }
        // Append optional flags before the redirect
        if (sqlite_flags[0] || gui_flags[0] || app_flags[0]) {
            char* redirect = strstr(compile_cmd, " 2>wyn_cc_err");
            if (redirect) {
                char tail[256];
                strncpy(tail, redirect, sizeof(tail)-1); tail[sizeof(tail)-1] = 0;
                snprintf(redirect, sizeof(compile_cmd) - (redirect - compile_cmd), "%s%s%s%s", sqlite_flags, gui_flags, app_flags, tail);
            }
        }
        
        // Shared library mode: compile as .so/.dylib instead of executable
        if (shared_mode > 0) {
            char lib_name[256] = {0};
            const char* base = strrchr(file, '/');
            base = base ? base + 1 : file;
            snprintf(lib_name, sizeof(lib_name), "%s", base);
            char* ldot = strrchr(lib_name, '.'); if (ldot) *ldot = 0;
#ifdef __APPLE__
            const char* lib_ext = "dylib"; const char* shared_flags = "-dynamiclib";
#elif _WIN32
            const char* lib_ext = "dll"; const char* shared_flags = "-shared";
#else
            const char* lib_ext = "so"; const char* shared_flags = "-shared";
#endif
            char lib_path[512];
            snprintf(lib_path, sizeof(lib_path), "lib%s.%s", lib_name, lib_ext);
            char shared_cmd[8192];
            if (rt_check) {
                snprintf(shared_cmd, sizeof(shared_cmd),
                         "gcc -std=c11 %s -w -fPIC %s -Wno-incompatible-pointer-types -Wno-int-conversion -I %s/src -o %s %s.c %s/runtime/libwyn_rt.a %s 2>wyn_cc_err.txt",
                         opt_level, shared_flags, wyn_root, lib_path, file, wyn_root, platform_libs);
            } else {
                char src_list[4096];
                build_source_list(src_list, sizeof(src_list), wyn_root);
                snprintf(shared_cmd, sizeof(shared_cmd),
                         "gcc -std=c11 %s -w -fPIC %s -Wno-incompatible-pointer-types -Wno-int-conversion -I %s/src -I %s/vendor/minicoro -o %s %s.c %s %s 2>wyn_cc_err.txt",
                         opt_level, shared_flags, wyn_root, wyn_root, lib_path, file, src_list, platform_libs);
            }
            int result = system(shared_cmd);
            if (result == 0) {
                clock_gettime(CLOCK_MONOTONIC, &_ts_end);
                double _ms = (_ts_end.tv_sec - _ts_start.tv_sec) * 1000.0 + (_ts_end.tv_nsec - _ts_start.tv_nsec) / 1e6;
                fprintf(stderr, "\033[32m✓\033[0m Built shared library: %s (%.0fms)\n", lib_path, _ms);
            }
            if (result == 0 && shared_mode == 2) {
                char py_path[512]; snprintf(py_path, sizeof(py_path), "%s.py", lib_name);
                FILE* py = fopen(py_path, "w");
                if (py) {
                    fprintf(py, "\"\"\"Auto-generated Python wrapper for %s.wyn — created by Wyn\"\"\"\n", lib_name);
                    fprintf(py, "import ctypes, os, sys\n\n");
                    fprintf(py, "_dir = os.path.dirname(os.path.abspath(__file__))\n");
                    fprintf(py, "if sys.platform == 'darwin':\n    _ext = 'dylib'\nelif sys.platform == 'win32':\n    _ext = 'dll'\nelse:\n    _ext = 'so'\n");
                    fprintf(py, "_lib = ctypes.CDLL(os.path.join(_dir, f'lib%s.{_ext}'))\n\n", lib_name);
                    for (int fi = 0; fi < prog->count; fi++) {
                        Stmt* s = prog->stmts[fi];
                        if (s->type != STMT_FN) continue;
                        if (s->fn.name.length == 4 && memcmp(s->fn.name.start, "main", 4) == 0) continue;
                        if (s->fn.receiver_type.length > 0) continue;
                        char fname[128]; snprintf(fname, sizeof(fname), "%.*s", s->fn.name.length, s->fn.name.start);
                        // C keyword prefix
                        const char* _ckw[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                        const char* cpfx = "";
                        for (int k = 0; _ckw[k]; k++) { if (strcmp(fname, _ckw[k]) == 0) { cpfx = "_"; break; } }
                        // Return type
                        const char* py_res = "ctypes.c_longlong";
                        int ret_is_str = 0;
                        if (s->fn.return_type && s->fn.return_type->type == EXPR_IDENT) {
                            Token rt = s->fn.return_type->token;
                            if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) { py_res = "ctypes.c_char_p"; ret_is_str = 1; }
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) py_res = "ctypes.c_double";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) py_res = "ctypes.c_bool";
                        } else if (!s->fn.return_type) { py_res = "None"; }
                        // Wyn signature as comment
                        fprintf(py, "# %s(", fname);
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (p > 0) fprintf(py, ", ");
                            fprintf(py, "%.*s: %.*s", s->fn.params[p].length, s->fn.params[p].start,
                                    s->fn.param_types[p] ? s->fn.param_types[p]->token.length : 3,
                                    s->fn.param_types[p] ? s->fn.param_types[p]->token.start : "int");
                        }
                        fprintf(py, ")");
                        if (s->fn.return_type && s->fn.return_type->type == EXPR_IDENT)
                            fprintf(py, " -> %.*s", s->fn.return_type->token.length, s->fn.return_type->token.start);
                        fprintf(py, "\n");
                        // argtypes
                        fprintf(py, "_lib.%s%s.argtypes = [", cpfx, fname);
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (p > 0) fprintf(py, ", ");
                            if (s->fn.param_types[p] && s->fn.param_types[p]->type == EXPR_IDENT) {
                                Token pt = s->fn.param_types[p]->token;
                                if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) fprintf(py, "ctypes.c_char_p");
                                else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) fprintf(py, "ctypes.c_double");
                                else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) fprintf(py, "ctypes.c_bool");
                                else fprintf(py, "ctypes.c_longlong");
                            } else fprintf(py, "ctypes.c_longlong");
                        }
                        fprintf(py, "]\n");
                        fprintf(py, "_lib.%s%s.restype = %s\n\n", cpfx, fname, py_res);
                        // Python wrapper function with type hints
                        fprintf(py, "def %s(", fname);
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (p > 0) fprintf(py, ", ");
                            fprintf(py, "%.*s", s->fn.params[p].length, s->fn.params[p].start);
                            // Add type hint
                            if (s->fn.param_types[p] && s->fn.param_types[p]->type == EXPR_IDENT) {
                                Token pt = s->fn.param_types[p]->token;
                                if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) fprintf(py, ": str");
                                else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) fprintf(py, ": float");
                                else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) fprintf(py, ": bool");
                                else if (pt.length == 3 && memcmp(pt.start, "int", 3) == 0) fprintf(py, ": int");
                            }
                        }
                        // Return type hint
                        if (ret_is_str) fprintf(py, ") -> str:\n");
                        else if (strcmp(py_res, "ctypes.c_double") == 0) fprintf(py, ") -> float:\n");
                        else if (strcmp(py_res, "ctypes.c_bool") == 0) fprintf(py, ") -> bool:\n");
                        else if (strcmp(py_res, "None") == 0) fprintf(py, ") -> None:\n");
                        else fprintf(py, ") -> int:\n");
                        // Encode string params
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (s->fn.param_types[p] && s->fn.param_types[p]->type == EXPR_IDENT) {
                                Token pt = s->fn.param_types[p]->token;
                                if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0)
                                    fprintf(py, "    %.*s = %.*s.encode() if isinstance(%.*s, str) else %.*s\n",
                                            s->fn.params[p].length, s->fn.params[p].start,
                                            s->fn.params[p].length, s->fn.params[p].start,
                                            s->fn.params[p].length, s->fn.params[p].start,
                                            s->fn.params[p].length, s->fn.params[p].start);
                            }
                        }
                        fprintf(py, "    _r = _lib.%s%s(", cpfx, fname);
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (p > 0) fprintf(py, ", ");
                            fprintf(py, "%.*s", s->fn.params[p].length, s->fn.params[p].start);
                        }
                        fprintf(py, ")\n");
                        if (ret_is_str) fprintf(py, "    return _r.decode() if _r else \"\"\n");
                        else if (strcmp(py_res, "None") != 0) fprintf(py, "    return _r\n");
                        fprintf(py, "\n");
                    }
                    fclose(py);
                    fprintf(stderr, "\033[32m✓\033[0m Generated Python wrapper: %s\n", py_path);
                }
            }
            // Generate Node.js wrapper if --node
            if (result == 0 && generate_node) {
                char js_path[512]; snprintf(js_path, sizeof(js_path), "%s.js", lib_name);
                FILE* js = fopen(js_path, "w");
                if (js) {
                    fprintf(js, "// Auto-generated Node.js wrapper for %s.wyn — created by Wyn\n", lib_name);
                    fprintf(js, "const ffi = require('ffi-napi');\n");
                    fprintf(js, "const path = require('path');\n\n");
                    fprintf(js, "const ext = process.platform === 'darwin' ? 'dylib' : process.platform === 'win32' ? 'dll' : 'so';\n");
                    fprintf(js, "const lib = ffi.Library(path.join(__dirname, `lib%s.${ext}`), {\n", lib_name);
                    
                    int first_fn = 1;
                    for (int fi = 0; fi < prog->count; fi++) {
                        Stmt* s = prog->stmts[fi];
                        if (s->type != STMT_FN) continue;
                        if (s->fn.name.length == 4 && memcmp(s->fn.name.start, "main", 4) == 0) continue;
                        if (s->fn.receiver_type.length > 0) continue;
                        
                        char fname[128]; snprintf(fname, sizeof(fname), "%.*s", s->fn.name.length, s->fn.name.start);
                        // C keyword prefix
                        const char* _ckw[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                        const char* cpfx = "";
                        for (int k = 0; _ckw[k]; k++) { if (strcmp(fname, _ckw[k]) == 0) { cpfx = "_"; break; } }
                        
                        // Return type
                        const char* js_ret = "'int64'";
                        if (s->fn.return_type && s->fn.return_type->type == EXPR_IDENT) {
                            Token rt = s->fn.return_type->token;
                            if (rt.length == 6 && memcmp(rt.start, "string", 6) == 0) js_ret = "'string'";
                            else if (rt.length == 5 && memcmp(rt.start, "float", 5) == 0) js_ret = "'double'";
                            else if (rt.length == 4 && memcmp(rt.start, "bool", 4) == 0) js_ret = "'bool'";
                        } else if (!s->fn.return_type) { js_ret = "'void'"; }
                        
                        if (!first_fn) fprintf(js, ",\n");
                        fprintf(js, "  '%s%s': [%s, [", cpfx, fname, js_ret);
                        for (int p = 0; p < s->fn.param_count; p++) {
                            if (p > 0) fprintf(js, ", ");
                            if (s->fn.param_types[p] && s->fn.param_types[p]->type == EXPR_IDENT) {
                                Token pt = s->fn.param_types[p]->token;
                                if (pt.length == 6 && memcmp(pt.start, "string", 6) == 0) fprintf(js, "'string'");
                                else if (pt.length == 5 && memcmp(pt.start, "float", 5) == 0) fprintf(js, "'double'");
                                else if (pt.length == 4 && memcmp(pt.start, "bool", 4) == 0) fprintf(js, "'bool'");
                                else fprintf(js, "'int64'");
                            } else fprintf(js, "'int64'");
                        }
                        fprintf(js, "]]");
                        first_fn = 0;
                    }
                    fprintf(js, "\n});\n\n");
                    
                    // Export wrapper functions
                    for (int fi = 0; fi < prog->count; fi++) {
                        Stmt* s = prog->stmts[fi];
                        if (s->type != STMT_FN) continue;
                        if (s->fn.name.length == 4 && memcmp(s->fn.name.start, "main", 4) == 0) continue;
                        if (s->fn.receiver_type.length > 0) continue;
                        char fname[128]; snprintf(fname, sizeof(fname), "%.*s", s->fn.name.length, s->fn.name.start);
                        const char* cpfx = "";
                        const char* _ckw2[] = {"double","float","int","char","void","return","if","else","while","for","switch","case","break","continue","struct","union","enum","typedef","static","extern","register","volatile","const","signed","unsigned","short","long","auto","default","do","goto","sizeof",NULL};
                        for (int k = 0; _ckw2[k]; k++) { if (strcmp(fname, _ckw2[k]) == 0) { cpfx = "_"; break; } }
                        fprintf(js, "exports.%s = lib.%s%s;\n", fname, cpfx, fname);
                    }
                    fclose(js);
                    fprintf(stderr, "\033[32m✓\033[0m Generated Node.js wrapper: %s\n", js_path);
                    fprintf(stderr, "  Install ffi-napi: npm install ffi-napi\n");
                }
            }
            if (!keep_artifacts) { char cp[512]; snprintf(cp, sizeof(cp), "%s.c", file); unlink(cp); }
            unlink("wyn_cc_err.txt");
            return result == 0 ? 0 : 1;
        }
        
        int result = system(compile_cmd);
        if (result == 0) {
            clock_gettime(CLOCK_MONOTONIC, &_ts_end);
            double _ms = (_ts_end.tv_sec - _ts_start.tv_sec) * 1000.0 + (_ts_end.tv_nsec - _ts_start.tv_nsec) / 1e6;
            fprintf(stderr, "\033[2mCompiled in %.0fms\033[0m\n", _ms);
        }
        if (result != 0) {
            fprintf(stderr, "Error: compilation failed (internal codegen error)\n");
            fprintf(stderr, "Run with WYN_DEBUG=1 for details\n");
            wynter_encourage();
            if (getenv("WYN_DEBUG")) {
                FILE* err_file = fopen("wyn_cc_err.txt", "r");
                if (err_file) {
                    char line[1024];
                    while (fgets(line, sizeof(line), err_file)) {
                        fprintf(stderr, "  %s", line);
                    }
                    fclose(err_file);
                } else {
                    fprintf(stderr, "  (no error file found - recompiling with visible errors)\n");
                    // Re-run without redirect so errors go to stderr
                    char debug_cmd[8192];
                    snprintf(debug_cmd, sizeof(debug_cmd),
                        "gcc -std=c11 -w -I %s/src -o /dev/null %s.c -lm 2>&1 | head -20",
                        wyn_root, file);
                    system(debug_cmd);
                }
            }
            free(source);
            return 1;
        }
        
        char run_cmd[4096];
        int _rc = 0;
        if (mem_stats) {
            char bin[512];
            if (file[0] == '/') snprintf(bin, 512, "%s.out", file);
            else snprintf(bin, 512, "./%s.out", file);
            _rc = snprintf(run_cmd, sizeof(run_cmd),
                "%s; /usr/bin/time -l %s < /dev/null 2>/tmp/__wyn_ms > /dev/null;"
                " echo '\\033[2m=== Memory Stats ===\\033[0m';"
                " grep -i 'maximum resident' /tmp/__wyn_ms | head -1;"
                " rm -f /tmp/__wyn_ms", bin, bin);
        } else {
            if (file[0] == '/')
                _rc = snprintf(run_cmd, sizeof(run_cmd), "%s.out", file);
            else
                _rc = snprintf(run_cmd, sizeof(run_cmd), "./%s.out", file);
            if (user_args_start > 0) {
                for (int i = user_args_start; i < argc && _rc < (int)sizeof(run_cmd) - 2; i++) {
                    _rc += snprintf(run_cmd + _rc, sizeof(run_cmd) - _rc, " %s", argv[i]);
                }
            }
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
    
    // Parse optimization flags
    OptLevel optimization = OPT_NONE;
    int file_arg_index = -1;
    
    // Check for flags (scan all args)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O1") == 0) {
            optimization = OPT_O1;
        } else if (strcmp(argv[i], "-O2") == 0) {
            optimization = OPT_O2;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            i++; // Skip -o value
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
    extern bool has_circular_import(void);
    preload_imports(source);
    if (has_circular_import()) {
        fprintf(stderr, "Compilation failed due to circular imports\n");
        free(source);
        return 1;
    }
    
    init_lexer(source);
    init_parser();
    set_parser_filename(argv[file_arg_index]);  // Set filename for better error messages
    init_checker();
    check_all_modules();  // Type check loaded modules
    
    Program* prog = parse_program();
    if (!prog) {
        if (!parser_had_error()) { fprintf(stderr, "Error: Failed to parse program\n"); fprintf(stderr, "  Hint: Check for stray code outside functions or unmatched braces\n"); }
        free(source);
        return 1;
    }
    
    // Type check
    { extern void set_checker_source(const char*, const char*); set_checker_source(source, argv[3]); } check_program(prog);
    
    if (checker_had_error()) {
        fprintf(stderr, "Compilation failed due to errors\n"); wynter_encourage();
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
    
}

int create_new_project(const char* project_name) {
    char path[512];
    
    // Check if wyn.toml already exists
    snprintf(path, sizeof(path), "%s/wyn.toml", project_name);
    if (access(path, F_OK) == 0) {
        fprintf(stderr, "Error: %s/wyn.toml already exists. Use a new directory.\n", project_name);
        return 1;
    }
    
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
    fprintf(main_file, "fn main() -> int {\n    println(\"Hello from %s! 🐉\")\n    return 0\n}\n", project_name);
    fclose(main_file);
    
    // Create test file
    snprintf(path, sizeof(path), "%s/tests/test_main.wyn", project_name);
    FILE* test_file = fopen(path, "w");
    if (!test_file) {
        fprintf(stderr, "Error: Failed to create test file\n");
        return 1;
    }
    fprintf(test_file, "// Tests for %s\n\ntest \"basic\" {\n    assert(1 + 1 == 2)\n    assert_eq(2 * 3, 6)\n}\n\ntest \"strings\" {\n    var name = \"Wyn\"\n    assert(name.len() == 3)\n    assert_eq(name.upper(), \"WYN\")\n}\n", project_name);
    fclose(test_file);
    
    // Create README.md
    snprintf(path, sizeof(path), "%s/README.md", project_name);
    FILE* readme_file = fopen(path, "w");
    if (readme_file) {
        fprintf(readme_file, "# %s\n\nA Wyn project.\n\n## Run\n\n```bash\nwyn run\n```\n\n## Test\n\n```bash\nwyn test\n```\n\n## Build\n\n```bash\nwyn build\n```\n", project_name);
        fclose(readme_file);
    }
    
    printf("Created new Wyn project: %s\n", project_name);
    printf("  %s/wyn.toml\n", project_name);
    printf("  %s/src/main.wyn\n", project_name);
    printf("  %s/tests/test_main.wyn\n", project_name);
    printf("  %s/README.md\n", project_name);
    printf("\nTo build and run:\n  cd %s\n  wyn run\n", project_name);
    printf("\nTo run tests:\n  wyn run tests/test_main.wyn\n");
    
    return 0;
}

int create_new_project_with_template(const char* name, const char* template, const char* lib_target) {
    if (strcmp(template, "default") == 0) return create_new_project(name);
    
    // Check if wyn.toml already exists
    char check[512];
    snprintf(check, sizeof(check), "%s/wyn.toml", name);
    if (access(check, F_OK) == 0) {
        fprintf(stderr, "Error: %s/wyn.toml already exists. Use a new directory.\n", name);
        return 1;
    }
    
    char cmd[512]; FILE* f;
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/src %s/tests", name, name);
    system(cmd);
    
    // wyn.toml
    snprintf(cmd, sizeof(cmd), "%s/wyn.toml", name);
    f = fopen(cmd, "w");
    fprintf(f, "[project]\nname = \"%s\"\nversion = \"0.1.0\"\nentry = \"src/main.wyn\"\n", name);
    if (strcmp(template, "lib") == 0 && lib_target && strcmp(lib_target, "wyn") == 0) {
        fprintf(f, "description = \"A Wyn package\"\n");
    }
    if (strcmp(template, "web") == 0 || strcmp(template, "api") == 0) {
        fprintf(f, "\n[packages]\nsqlite = { git = \"https://github.com/wynlang/sqlite\" }\n");
        fprintf(f, "\n[deploy.dev]\nhost = \"localhost\"\nuser = \"deploy\"\nkey = \"~/.ssh/id_ed25519\"\npath = \"/opt/%s\"\nos = \"linux\"\npre = \"systemctl stop %s\"\npost = \"systemctl start %s\"\n", name, name, name);
    }
    fclose(f);
    
    // .gitignore
    snprintf(cmd, sizeof(cmd), "%s/.gitignore", name);
    f = fopen(cmd, "w");
    fprintf(f, "*.wyn.c\n*.wyn.out\n*.out\nwyn_cc_err.txt\n*.db\npackages/\n");
    fclose(f);
    
    // .github/workflows/test.yml
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/.github/workflows", name);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%s/.github/workflows/test.yml", name);
    f = fopen(cmd, "w");
    fprintf(f,
        "name: Test\n"
        "on: [push, pull_request]\n"
        "jobs:\n"
        "  test:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        "      - uses: actions/checkout@v4\n"
        "      - name: Install Wyn\n"
        "        run: curl -fsSL https://wynlang.com/install.sh | sh\n"
        "      - name: Test\n"
        "        run: wyn test\n"
        "      - name: Build\n"
        "        run: wyn build .\n");
    fclose(f);
    
    // README
    snprintf(cmd, sizeof(cmd), "%s/README.md", name);
    f = fopen(cmd, "w");
    fprintf(f, "# %s\n\n", name);
    if (strcmp(template, "web") == 0) fprintf(f,
        "A REST API built with [Wyn](https://wynlang.com).\n\n"
        "## Quick Start\n\n"
        "```bash\nwyn run\n```\n\n"
        "Open http://localhost:8080\n\n"
        "## API\n\n"
        "| Method | Path | Description |\n"
        "|--------|------|-------------|\n"
        "| GET | `/` | HTML home page |\n"
        "| GET | `/api/items` | List items (JSON) |\n"
        "| POST | `/api/items` | Create item |\n"
        "| GET | `/api/health` | Health check |\n\n"
        "## Test\n\n```bash\nwyn run tests/test_main.wyn\n```\n\n"
        "## Deploy\n\n```bash\nwyn deploy dev\n```\n");
    else if (strcmp(template, "api") == 0) fprintf(f,
        "A REST API built with [Wyn](https://wynlang.com).\n\n"
        "## Quick Start\n\n"
        "```bash\nwyn run\n```\n\n"
        "## Endpoints\n\n"
        "| Method | Path | Description |\n"
        "|--------|------|-------------|\n"
        "| GET | `/health` | Health check |\n"
        "| GET | `/ready` | Readiness check |\n"
        "| GET | `/api/items` | List all items |\n"
        "| POST | `/api/items` | Create item (body = name) |\n"
        "| GET | `/api/items/:id` | Get item by ID |\n"
        "| DELETE | `/api/items/:id` | Delete item |\n\n"
        "## Examples\n\n"
        "```bash\n"
        "curl http://localhost:8080/health\n"
        "curl -X POST -d 'My Item' http://localhost:8080/api/items\n"
        "curl http://localhost:8080/api/items\n"
        "curl http://localhost:8080/api/items/1\n"
        "curl -X DELETE http://localhost:8080/api/items/1\n"
        "```\n\n"
        "## Test\n\n```bash\nwyn run tests/test_main.wyn\n```\n\n"
        "## Deploy\n\n```bash\nwyn deploy dev\n```\n");
    else if (strcmp(template, "cli") == 0) fprintf(f,
        "A CLI tool built with [Wyn](https://wynlang.com).\n\n"
        "## Usage\n\n"
        "```bash\nwyn run help\nwyn run info\nwyn run run <file>\nwyn run list\n```\n\n"
        "## Build\n\n```bash\nwyn build .\n./%s --help\n```\n\n"
        "## Test\n\n```bash\nwyn run tests/test_main.wyn\n```\n", name);
    else if (strcmp(template, "lib") == 0) {
        if (lib_target && strcmp(lib_target, "wyn") == 0)
            fprintf(f, "A Wyn package.\n\n## Install\n\n```bash\nwyn pkg install github.com/yourname/%s\n```\n\n## Usage\n\n```wyn\nimport %s\nprintln(%s.greet())\n```\n", name, name, name);
        else if (lib_target && strcmp(lib_target, "python") == 0)
            fprintf(f, "A Python extension built with Wyn.\n\n## Build\n\n```bash\nwyn build --python\n```\n\n## Usage\n\n```python\nfrom %s import add, greet\nprint(add(2, 3))    # 5\nprint(greet())       # Hello from %s, world!\n```\n", name, name);
        else if (lib_target && strcmp(lib_target, "node") == 0)
            fprintf(f, "A Node.js native addon built with Wyn.\n\n## Build\n\n```bash\nwyn build --node\n```\n\n## Usage\n\n```js\nconst { add, greet } = require('./%s');\nconsole.log(add(2, 3));  // 5\nconsole.log(greet());     // Hello from %s, world!\n```\n", name, name);
        else if (lib_target && strcmp(lib_target, "c") == 0)
            fprintf(f, "A C shared library built with Wyn.\n\n## Build\n\n```bash\nwyn build --shared\n```\n\n## Usage\n\n```c\n#include \"%s.h\"\nprintf(\"%%d\\n\", add(2, 3));\n```\n", name);
    }
    fclose(f);
    
    // main.wyn
    snprintf(cmd, sizeof(cmd), "%s/src/main.wyn", name);
    f = fopen(cmd, "w");
    if (strcmp(template, "web") == 0) {
        fprintf(f,
            "// %s — Web app with JSON API, SQLite, and HTML\n\n"
            "fn init_db() -> int {\n"
            "    var db = Db.open(\"%s.db\")\n"
            "    Db.exec(db, \"CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)\")\n"
            "    return db\n"
            "}\n\n"
            "fn home_page() -> string {\n"
            "    return \"<html><body><h1>%s</h1><p>Built with <a href=\\\"https://wynlang.com\\\">Wyn</a></p>\"\n"
            "        + \"<h2>API</h2><pre>GET  /api/items\\nPOST /api/items\\nGET  /api/health</pre></body></html>\"\n"
            "}\n\n"
            "fn handle(raw: string) {\n"
            "    var parts = raw.split(\"|\")\n"
            "    if parts.len() < 4 { return }\n"
            "    var method = parts[0]\n"
            "    var path = parts[1]\n"
            "    var body = parts[2]\n"
            "    var fd = parts[3].to_int()\n\n"
            "    if method == \"GET\" && path == \"/\" {\n"
            "        Http.respond_html(fd, 200, home_page())\n"
            "        return\n"
            "    }\n\n"
            "    if method == \"GET\" && path == \"/api/health\" {\n"
            "        Http.respond_json(fd, 200, \"{\\\"status\\\": \\\"ok\\\"}\")\n"
            "        return\n"
            "    }\n\n"
            "    if method == \"GET\" && path == \"/api/items\" {\n"
            "        var db = init_db()\n"
            "        var rows = Db.query(db, \"SELECT name FROM items ORDER BY id DESC LIMIT 50\")\n"
            "        Db.close(db)\n"
            "        Http.respond_json(fd, 200, \"{\\\"items\\\": [\" + rows + \"]}\")\n"
            "        return\n"
            "    }\n\n"
            "    if method == \"POST\" && path == \"/api/items\" {\n"
            "        if body.len() == 0 { Http.respond_json(fd, 400, \"{\\\"error\\\": \\\"body required\\\"}\"); return }\n"
            "        var db = init_db()\n"
            "        Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [body])\n"
            "        Db.close(db)\n"
            "        Http.respond_json(fd, 201, \"{\\\"status\\\": \\\"created\\\"}\")\n"
            "        return\n"
            "    }\n\n"
            "    Http.respond_json(fd, 404, \"{\\\"error\\\": \\\"not found\\\"}\")\n"
            "}\n\n"
            "fn main() {\n"
            "    var port = 8080\n"
            "    println(\"%s running on http://localhost:${port}\")\n"
            "    println(\"\")\n"
            "    println(\"  GET  /              — home page\")\n"
            "    println(\"  GET  /api/items     — list items (JSON)\")\n"
            "    println(\"  POST /api/items     — create item\")\n"
            "    println(\"  GET  /api/health    — health check\")\n"
            "    println(\"\")\n"
            "    init_db()\n"
            "    var server = Http.serve(port)\n"
            "    while true {\n"
            "        var raw = Http.accept(server)\n"
            "        if raw.len() > 0 {\n"
            "            spawn handle(raw)\n"
            "        }\n"
            "    }\n"
            "}\n", name, name, name, name);
        
        fclose(f);
        f = NULL;
    } else if (strcmp(template, "api") == 0) {
        fprintf(f,
            "// %s — REST API with JSON, parameterized routes, SQLite\n\n"
            "fn init_db() -> int {\n"
            "    var db = Db.open(\"%s.db\")\n"
            "    Db.exec(db, \"CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)\")\n"
            "    return db\n"
            "}\n\n"
            "fn handle(raw: string) {\n"
            "    var parts = raw.split(\"|\")\n"
            "    if parts.len() < 4 { return }\n"
            "    var method = parts[0]\n"
            "    var path = parts[1]\n"
            "    var body = parts[2]\n"
            "    var fd = parts[3].to_int()\n\n"
            "    if path == \"/health\" { Http.respond_json(fd, 200, \"{\\\"status\\\": \\\"ok\\\"}\"); return }\n"
            "    if path == \"/ready\"  { Http.respond_json(fd, 200, \"{\\\"status\\\": \\\"ready\\\"}\"); return }\n\n"
            "    if method == \"GET\" && path == \"/api/items\" {\n"
            "        var db = init_db()\n"
            "        var rows = Db.query(db, \"SELECT name FROM items ORDER BY id DESC LIMIT 100\")\n"
            "        Db.close(db)\n"
            "        Http.respond_json(fd, 200, \"{\\\"items\\\": [\" + rows + \"]}\")\n"
            "        return\n"
            "    }\n\n"
            "    if method == \"POST\" && path == \"/api/items\" {\n"
            "        if body.len() == 0 { Http.respond_json(fd, 400, \"{\\\"error\\\": \\\"body required\\\"}\"); return }\n"
            "        var db = init_db()\n"
            "        Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [body])\n"
            "        Db.close(db)\n"
            "        Http.respond_json(fd, 201, \"{\\\"status\\\": \\\"created\\\"}\")\n"
            "        return\n"
            "    }\n\n"
            "    Http.respond_json(fd, 404, \"{\\\"error\\\": \\\"not found\\\"}\")\n"
            "}\n\n"
            "fn main() {\n"
            "    var port = 8080\n"
            "    println(\"%s running on http://localhost:${port}\")\n"
            "    println(\"\")\n"
            "    println(\"  GET    /health          — health check\")\n"
            "    println(\"  GET    /ready           — readiness check\")\n"
            "    println(\"  GET    /api/items       — list items\")\n"
            "    println(\"  POST   /api/items       — create item\")\n"
            "    println(\"  GET    /api/items/:id   — get item\")\n"
            "    println(\"  DELETE /api/items/:id   — delete item\")\n"
            "    println(\"\")\n"
            "    init_db()\n"
            "    var server = Http.serve(port)\n"
            "    while true {\n"
            "        var raw = Http.accept(server)\n"
            "        if raw.len() > 0 {\n"
            "            spawn handle(raw)\n"
            "        }\n"
            "    }\n"
            "}\n", name, name, name);
        fclose(f);
        f = NULL;
    } else if (strcmp(template, "cli") == 0) {
        fprintf(f,
            "// %s — CLI tool with arg parsing and colored output\n\n"
            "fn print_help() {\n"
            "    println(\"%s v0.1.0\")\n"
            "    println(\"\")\n"
            "    println(\"Usage: %s <command> [options]\")\n"
            "    println(\"\")\n"
            "    println(\"Commands:\")\n"
            "    println(\"  run <file>     Process a file\")\n"
            "    println(\"  list           List items\")\n"
            "    println(\"  info           Show system info\")\n"
            "    println(\"  help           Show this help\")\n"
            "    println(\"  version        Show version\")\n"
            "}\n\n"
            "fn cmd_info() {\n"
            "    println(\"%s v0.1.0\")\n"
            "    println(\"  OS:   \" + System.exec(\"uname -s\").trim())\n"
            "    println(\"  Arch: \" + System.exec(\"uname -m\").trim())\n"
            "    println(\"  Dir:  \" + System.exec(\"pwd\").trim())\n"
            "}\n\n"
            "fn cmd_run(file: string) {\n"
            "    var content = File.read(file)\n"
            "    if content.len() == 0 {\n"
            "        println(\"Error: could not read: \" + file)\n"
            "        return\n"
            "    }\n"
            "    var lines = content.split(\"\\n\")\n"
            "    println(\"Processed ${lines.len()} lines from ${file}\")\n"
            "}\n\n"
            "fn cmd_list() {\n"
            "    var items = [\"alpha\", \"beta\", \"gamma\"]\n"
            "    for item in items {\n"
            "        println(\"  - \" + item)\n"
            "    }\n"
            "    println(\"\")\n"
            "    println(\"${items.len()} items\")\n"
            "}\n\n"
            "fn main() {\n"
            "    var args = System.args()\n"
            "    if args.len() < 2 {\n"
            "        print_help()\n"
            "        return 0\n"
            "    }\n"
            "    var cmd = args[1]\n"
            "    if cmd == \"help\" || cmd == \"--help\" || cmd == \"-h\" {\n"
            "        print_help()\n"
            "    } else if cmd == \"version\" || cmd == \"--version\" || cmd == \"-v\" {\n"
            "        println(\"%s v0.1.0\")\n"
            "    } else if cmd == \"info\" {\n"
            "        cmd_info()\n"
            "    } else if cmd == \"list\" {\n"
            "        cmd_list()\n"
            "    } else if cmd == \"run\" {\n"
            "        if args.len() < 3 {\n"
            "            println(\"Usage: %s run <file>\")\n"
            "            return 1\n"
            "        }\n"
            "        cmd_run(args[2])\n"
            "    } else {\n"
            "        println(\"Unknown command: \" + cmd)\n"
            "        println(\"Run '%s --help' for usage\")\n"
            "        return 1\n"
            "    }\n"
            "}\n", name, name, name, name, name, name, name);
    } else if (strcmp(template, "lib") == 0) {
        if (lib_target && strcmp(lib_target, "wyn") == 0) {
            // Wyn package — installable via wyn pkg install
            fprintf(f,
                "// %s — a Wyn package\n"
                "// Install: wyn pkg install github.com/yourname/%s\n\n"
                "pub fn add(a: int, b: int) -> int {\n"
                "    return a + b\n"
                "}\n\n"
                "pub fn greet(name: string = \"world\") -> string {\n"
                "    return \"Hello from %s, \" + name + \"!\"\n"
                "}\n", name, name, name);
        } else if (lib_target && strcmp(lib_target, "python") == 0) {
            fprintf(f,
                "// %s — Python extension module\n"
                "// Build: wyn build --python\n"
                "// Usage: python3 -c \"from %s import add; print(add(2, 3))\"\n\n"
                "pub fn add(a: int, b: int) -> int {\n"
                "    return a + b\n"
                "}\n\n"
                "pub fn greet(name: string = \"world\") -> string {\n"
                "    return \"Hello from %s, \" + name + \"!\"\n"
                "}\n\n"
                "fn main() {}\n", name, name, name);
        } else if (lib_target && strcmp(lib_target, "node") == 0) {
            fprintf(f,
                "// %s — Node.js native addon (N-API)\n"
                "// Build: wyn build --node\n"
                "// Usage: const lib = require('./%s'); console.log(lib.add(2, 3));\n\n"
                "pub fn add(a: int, b: int) -> int {\n"
                "    return a + b\n"
                "}\n\n"
                "pub fn greet(name: string = \"world\") -> string {\n"
                "    return \"Hello from %s, \" + name + \"!\"\n"
                "}\n\n"
                "fn main() {}\n", name, name, name);
        } else if (lib_target && strcmp(lib_target, "c") == 0) {
            fprintf(f,
                "// %s — C shared library\n"
                "// Build: wyn build --shared\n"
                "// Produces: lib%s.so (Linux) / lib%s.dylib (macOS)\n\n"
                "pub fn add(a: int, b: int) -> int {\n"
                "    return a + b\n"
                "}\n\n"
                "pub fn greet(name: string = \"world\") -> string {\n"
                "    return \"Hello from %s, \" + name + \"!\"\n"
                "}\n\n"
                "fn main() {}\n", name, name, name, name);
        }
    }
    if (f) fclose(f);
    
    // test
    snprintf(cmd, sizeof(cmd), "%s/tests/test_main.wyn", name);
    f = fopen(cmd, "w");
    if (strcmp(template, "web") == 0) {
        fprintf(f,
            "fn main() {\n"
            "    Test.init(\"%s\")\n\n"
            "    // Database\n"
            "    var db = Db.open(\"/tmp/%s_test.db\")\n"
            "    Db.exec(db, \"CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)\")\n"
            "    Db.exec(db, \"DELETE FROM items\")\n"
            "    Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [\"test\"])\n"
            "    var count = Db.query(db, \"SELECT COUNT(*) FROM items\")\n"
            "    Test.assert_eq_str(count, \"1\", \"db insert\")\n"
            "    Db.close(db)\n\n"
            "    // SQL injection prevention\n"
            "    var db2 = Db.open(\"/tmp/%s_test.db\")\n"
            "    Db.exec_p(db2, \"INSERT INTO items (name) VALUES (?)\", [\"'; DROP TABLE items; --\"])\n"
            "    var count2 = Db.query(db2, \"SELECT COUNT(*) FROM items\")\n"
            "    Test.assert_eq_str(count2, \"2\", \"sql injection safe\")\n"
            "    Db.close(db2)\n\n"
            "    Test.summary()\n"
            "}\n", name, name, name);
    } else if (strcmp(template, "api") == 0) {
        fprintf(f,
            "fn main() {\n"
            "    Test.init(\"%s\")\n\n"
            "    // Database CRUD\n"
            "    var db = Db.open(\"/tmp/%s_test.db\")\n"
            "    Db.exec(db, \"CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)\")\n"
            "    Db.exec(db, \"DELETE FROM items\")\n\n"
            "    // Create\n"
            "    Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [\"alpha\"])\n"
            "    Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [\"beta\"])\n"
            "    var count = Db.query(db, \"SELECT COUNT(*) FROM items\")\n"
            "    Test.assert_eq_str(count, \"2\", \"create items\")\n\n"
            "    // Read\n"
            "    var name = Db.query(db, \"SELECT name FROM items WHERE id = 1\")\n"
            "    Test.assert_eq_str(name, \"alpha\", \"read item\")\n\n"
            "    // Delete\n"
            "    Db.exec(db, \"DELETE FROM items WHERE id = 1\")\n"
            "    var count2 = Db.query(db, \"SELECT COUNT(*) FROM items\")\n"
            "    Test.assert_eq_str(count2, \"1\", \"delete item\")\n\n"
            "    // SQL injection\n"
            "    Db.exec_p(db, \"INSERT INTO items (name) VALUES (?)\", [\"'; DROP TABLE items; --\"])\n"
            "    var count3 = Db.query(db, \"SELECT COUNT(*) FROM items\")\n"
            "    Test.assert_eq_str(count3, \"2\", \"injection safe\")\n\n"
            "    Db.close(db)\n"
            "    Test.summary()\n"
            "}\n", name, name);
    } else if (strcmp(template, "cli") == 0) {
        fprintf(f,
            "test \"starts_with\" {\n"
            "    assert(\"hello\".starts_with(\"he\"))\n"
            "}\n\n"
            "test \"upper\" {\n"
            "    assert_eq(\"hello\".upper(), \"HELLO\")\n"
            "}\n\n"
            "test \"split\" {\n"
            "    assert_eq(\"a,b,c\".split(\",\").len(), 3)\n"
            "}\n\n"
            "test \"file roundtrip\" {\n"
            "    File.write(\"/tmp/%s_test.txt\", \"hello\")\n"
            "    var content = File.read(\"/tmp/%s_test.txt\")\n"
            "    assert_eq(content, \"hello\")\n"
            "}\n", name, name);
    } else if (strcmp(template, "lib") == 0) {
        fprintf(f,
            "fn add(a: int, b: int) -> int { return a + b }\n"
            "fn greet(name: string = \"world\") -> string { return \"Hello from %s, \" + name + \"!\" }\n\n"
            "test \"add\" {\n"
            "    assert_eq(add(2, 3), 5)\n"
            "    assert_eq(add(0, 0), 0)\n"
            "    assert_eq(add(-1, 1), 0)\n"
            "}\n\n"
            "test \"greet\" {\n"
            "    assert_eq(greet(), \"Hello from %s, world!\")\n"
            "    assert_eq(greet(\"wyn\"), \"Hello from %s, wyn!\")\n"
            "}\n", name, name, name);
    } else {
        fprintf(f, "test \"basic\" {\n    assert(1 + 1 == 2)\n}\n");
    }
    fclose(f);
    
    printf("\033[32m✓\033[0m Created %s project: %s/\n\n", template, name);
    printf("  %s/wyn.toml\n  %s/src/main.wyn\n  %s/tests/test_main.wyn\n  %s/README.md\n  %s/.gitignore\n\n", name, name, name, name, name);
    
    // Auto-install packages for templates that need them
    if (strcmp(template, "web") == 0 || strcmp(template, "api") == 0) {
        printf("Installing packages...\n");
        char install_cmd[1024];
        // Copy sqlite from official-packages if available locally, otherwise note for user
        char local_sqlite[512];
        snprintf(local_sqlite, sizeof(local_sqlite), "%s/../official-packages/sqlite", name);
        struct stat _ls;
        snprintf(install_cmd, sizeof(install_cmd), "mkdir -p %s/packages/sqlite/src && cp %s/src/sqlite3.c %s/src/sqlite3.h %s/src/sqlite.wyn %s/wyn.toml %s/packages/sqlite/ 2>/dev/null && cp %s/src/*.c %s/src/*.h %s/packages/sqlite/src/ 2>/dev/null",
            name, local_sqlite, local_sqlite, local_sqlite, local_sqlite, name, local_sqlite, local_sqlite, name);
        if (stat(local_sqlite, &_ls) == 0) {
            system(install_cmd);
            printf("  \033[32m✓\033[0m sqlite package installed\n\n");
        } else {
            printf("  \033[33m⚠\033[0m Run: cd %s && wyn pkg install github.com/wynlang/sqlite\n\n", name);
        }
    }
    
    if (strcmp(template, "web") == 0) printf("Run:\n  cd %s && wyn run\n  # Open http://localhost:8080\n", name);
    else if (strcmp(template, "api") == 0) printf("Run:\n  cd %s && wyn run\n\nTest:\n  curl http://localhost:8080/health\n  curl -X POST -d 'My Item' http://localhost:8080/api/items\n  curl http://localhost:8080/api/items\n", name);
    else if (strcmp(template, "cli") == 0) printf("Run:\n  cd %s && wyn run\n\nWith args (build first):\n  wyn build . && ./%s list\n", name, name);
    else if (strcmp(template, "lib") == 0) {
        if (lib_target && strcmp(lib_target, "wyn") == 0)
            printf("Run:\n  cd %s && wyn test\n\nPublish:\n  git push → wyn pkg install github.com/yourname/%s\n", name, name);
        else if (lib_target && strcmp(lib_target, "python") == 0)
            printf("Build:\n  cd %s && wyn build --python\n\nTest:\n  python3 -c \"from %s import add; print(add(2, 3))\"\n", name, name);
        else if (lib_target && strcmp(lib_target, "node") == 0)
            printf("Build:\n  cd %s && wyn build --node\n\nTest:\n  node -e \"const lib = require('./%s'); console.log(lib.add(2, 3))\"\n", name, name);
        else if (lib_target && strcmp(lib_target, "c") == 0)
            printf("Build:\n  cd %s && wyn build --shared\n\nProduces: lib%s.so / lib%s.dylib\n", name, name, name);
    }
    return 0;
}

void print_flight_rules() {
    const char* lines[] = {
        "",
        "  \033[36m✦ Flight Rules ✦\033[0m",
        "",
        "  One language is better than five.",
        "  Readable code is fast code — you'll optimize it.",
        "  Ship the binary, not the runtime.",
        "  If it compiles, it should work.",
        "  The best abstraction is the one you don't notice.",
        "  Concurrency should be a verb, not a thesis.",
        "  Every program starts as a script.",
        "  Errors are values. Handle them or don't — but know which.",
        "  A tool you don't use is a tool you don't need.",
        "  Simple today, powerful tomorrow.",
        "  The compiler is your first user.",
        "  Deploy on Friday. Your binary doesn't have dependencies.",
        "",
        "  \033[2m— Wynter\033[0m",
        "",
        NULL
    };
    for (int i = 0; lines[i]; i++) printf("%s\n", lines[i]);
}
