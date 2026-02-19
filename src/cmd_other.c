#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>  // For WEXITSTATUS
#include <unistd.h>    // For sleep, readlink
#ifdef __APPLE__
#include <mach-o/dyld.h>  // For _NSGetExecutablePath
#endif
#else
#define WEXITSTATUS(x) (x)
#include <windows.h>   // For Sleep, GetModuleFileNameA
#include <direct.h>    // For _mkdir
#endif

// Forward declarations
extern void* parse_file(const char* filename);
extern void format_program(void* program);
extern int wyn_format_file(const char* filename);

int cmd_fmt(const char* file, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (!file) {
        fprintf(stderr, "Usage: wyn fmt <file.wyn>\n");
        return 1;
    }
    
    FILE* f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", file);
        return 1;
    }
    
    printf("Formatting %s...\n", file);
    
    char line[1024];
    int indent = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // Trim trailing whitespace
        int len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\n')) {
            line[--len] = 0;
        }
        
        // Skip empty lines
        if (len == 0) {
            printf("\n");
            continue;
        }
        
        // Adjust indent for closing braces
        if (line[0] == '}') {
            indent--;
        }
        
        // Print with proper indentation
        for (int i = 0; i < indent; i++) {
            printf("    ");
        }
        printf("%s\n", line);
        
        // Adjust indent for opening braces
        if (strchr(line, '{')) {
            indent++;
        }
    }
    
    fclose(f);
    printf("\n✅ Formatted successfully\n");
    return 0;
}

int cmd_repl(int argc, char** argv) {
    (void)argc; (void)argv;
    // Read version from VERSION file
    char version[32] = "1.7.0";
    FILE* vf = fopen("VERSION", "r");
    if (!vf) vf = fopen("../VERSION", "r");
    if (vf) {
        if (fgets(version, sizeof(version), vf)) {
            char* newline = strchr(version, '\n');
            if (newline) *newline = 0;
        }
        fclose(vf);
    }
    printf("Wyn REPL v%s\n", version);
    printf("Type expressions or statements. 'exit' to quit.\n\n");
    
    char line[1024];
    int line_num = 1;
    
    // Get wyn executable path
    char exe_path[2048];
    #ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        strcpy(exe_path, "wyn");
    }
    #elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) strcpy(exe_path, "wyn");
    else exe_path[len] = '\0';
    #else
    strcpy(exe_path, "wyn");
    #endif
    
    while (1) {
        printf("wyn[%d]> ", line_num);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        if (strlen(line) == 0) continue;
        
        FILE* tmp = fopen("/tmp/wyn_repl.wyn", "w");
        if (!tmp) {
            fprintf(stderr, "Error: Cannot create temp file\n");
            continue;
        }
        
        // Check if it's a statement (contains print, var, if, etc.) or expression
        int is_statement = strstr(line, "print") || strstr(line, "var ") || 
                          strstr(line, "if ") || strstr(line, "while ") ||
                          strstr(line, "for ") || strchr(line, '=');
        
        fprintf(tmp, "fn main() -> int {\n");
        if (is_statement) {
            fprintf(tmp, "    %s\n", line);
            fprintf(tmp, "    return 0\n");
        } else {
            fprintf(tmp, "    print(%s)\n", line);
            fprintf(tmp, "    return 0\n");
        }
        fprintf(tmp, "}\n");
        fclose(tmp);
        
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "%s /tmp/wyn_repl.wyn 2>&1", exe_path);
        FILE* compile = popen(cmd, "r");
        if (compile) {
            char output[4096] = {0};
            fread(output, 1, sizeof(output)-1, compile);
            int status = pclose(compile);
            if (status == 0 && strstr(output, "Compiled successfully")) {
                system("/tmp/wyn_repl.wyn.out 2>&1");
            } else {
                // Show error without "Compiled successfully" line
                char* err = output;
                if (strncmp(err, "Compiled", 8) != 0) {
                    printf("%s", err);
                } else {
                    printf("Error: Invalid expression\n");
                }
            }
        }
        line_num++;
    }
    return 0;
}

int cmd_doc(const char* file, int argc, char** argv) {
    if (!file) {
        fprintf(stderr, "Usage: wyn doc <file.wyn> [--html]\n");
        return 1;
    }
    
    int html_mode = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--html") == 0) html_mode = 1;
    }
    
    FILE* f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", file);
        return 1;
    }
    
    // Collect all doc entries
    typedef struct { char type[16]; char sig[512]; char doc[2048]; } DocEntry;
    DocEntry entries[256];
    int entry_count = 0;
    
    char line[1024];
    char comment[4096] = "";
    int in_comment = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "//", 2) == 0) {
            if (strlen(comment) + strlen(line) < sizeof(comment) - 4)
                strcat(comment, line + (line[2] == ' ' ? 3 : 2));
            in_comment = 1;
        } else if ((strncmp(line, "fn ", 3) == 0 || strncmp(line, "struct ", 7) == 0 || strncmp(line, "enum ", 5) == 0) && entry_count < 256) {
            DocEntry* e = &entries[entry_count++];
            if (strncmp(line, "fn ", 3) == 0) strcpy(e->type, "Function");
            else if (strncmp(line, "struct ", 7) == 0) strcpy(e->type, "Struct");
            else strcpy(e->type, "Enum");
            char* nl = strchr(line, '\n'); if (nl) *nl = 0;
            strncpy(e->sig, line, sizeof(e->sig) - 1);
            if (in_comment) strncpy(e->doc, comment, sizeof(e->doc) - 1);
            else e->doc[0] = 0;
            comment[0] = 0; in_comment = 0;
        } else if (strlen(line) > 1 && line[0] != ' ' && line[0] != '\t') {
            comment[0] = 0; in_comment = 0;
        }
    }
    fclose(f);
    
    if (!html_mode) {
        printf("# Documentation for %s\n\n", file);
        for (int i = 0; i < entry_count; i++) {
            printf("## %s\n\n", entries[i].type);
            if (entries[i].doc[0]) printf("%s\n", entries[i].doc);
            printf("```wyn\n%s\n```\n\n", entries[i].sig);
        }
        return 0;
    }
    
    // HTML output with sidebar, search, dark mode
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s.html", file);
    FILE* out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "Error: Cannot write %s\n", out_path); return 1; }
    
    fprintf(out, "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">\n");
    fprintf(out, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    fprintf(out, "<title>Docs — %s</title>\n<style>\n", file);
    fprintf(out,
        ":root{--bg:#fff;--fg:#1a2332;--sidebar:#f8f9fa;--border:#e2e6ec;--accent:#00d2ff;--code-bg:#f4f3f3;--card:#fff}\n"
        "[data-theme=dark]{--bg:#1a1a2e;--fg:#e0e0e0;--sidebar:#16213e;--border:#2a2a4a;--accent:#4fc3f7;--code-bg:#16213e;--card:#1e1e3f}\n"
        "*{margin:0;padding:0;box-sizing:border-box}\n"
        "body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg);display:flex;min-height:100vh}\n"
        ".sidebar{width:260px;background:var(--sidebar);border-right:1px solid var(--border);padding:16px;position:fixed;height:100vh;overflow-y:auto}\n"
        ".sidebar h2{font-size:16px;margin-bottom:12px}\n"
        ".sidebar input{width:100%%;padding:6px 10px;border:1px solid var(--border);border-radius:4px;background:var(--bg);color:var(--fg);margin-bottom:12px;font-size:13px}\n"
        ".sidebar a{display:block;padding:4px 8px;color:var(--fg);text-decoration:none;font-size:13px;border-radius:4px}\n"
        ".sidebar a:hover{background:var(--accent);color:#fff}\n"
        ".sidebar .type{font-size:11px;color:var(--accent);text-transform:uppercase;margin-top:12px;font-weight:600}\n"
        ".main{margin-left:260px;padding:32px 48px;max-width:800px;flex:1}\n"
        ".entry{margin-bottom:32px;border:1px solid var(--border);border-radius:8px;padding:20px;background:var(--card)}\n"
        ".entry h3{font-size:18px;margin-bottom:8px}\n"
        ".entry .badge{display:inline-block;font-size:11px;padding:2px 8px;border-radius:4px;background:var(--accent);color:#fff;margin-bottom:8px}\n"
        ".entry pre{background:var(--code-bg);padding:12px;border-radius:6px;overflow-x:auto;font-size:13px;font-family:'SF Mono',monospace}\n"
        ".entry .doc{color:#6b7280;margin-bottom:8px;font-size:14px;white-space:pre-line}\n"
        ".controls{display:flex;gap:8px;margin-bottom:16px}\n"
        ".controls button{padding:4px 12px;border:1px solid var(--border);border-radius:4px;background:var(--bg);color:var(--fg);cursor:pointer;font-size:12px}\n"
        ".controls button:hover{background:var(--accent);color:#fff;border-color:var(--accent)}\n"
        "@media(max-width:768px){.sidebar{display:none}.main{margin-left:0}}\n"
    );
    fprintf(out, "</style></head><body>\n");
    
    // Sidebar with search
    fprintf(out, "<nav class=\"sidebar\">\n<h2>%s</h2>\n", file);
    fprintf(out, "<input type=\"text\" id=\"search\" placeholder=\"Search...\" oninput=\"filterDocs()\">\n");
    const char* last_type = "";
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].type, last_type) != 0) {
            fprintf(out, "<div class=\"type\">%ss</div>\n", entries[i].type);
            last_type = entries[i].type;
        }
        char name[128] = "";
        if (strncmp(entries[i].sig, "fn ", 3) == 0) sscanf(entries[i].sig + 3, "%127[^( ]", name);
        else if (strncmp(entries[i].sig, "struct ", 7) == 0) sscanf(entries[i].sig + 7, "%127[^ {]", name);
        else if (strncmp(entries[i].sig, "enum ", 5) == 0) sscanf(entries[i].sig + 5, "%127[^ {]", name);
        if (!name[0]) strncpy(name, entries[i].sig, 40);
        fprintf(out, "<a href=\"#entry-%d\">%s</a>\n", i, name);
    }
    fprintf(out, "</nav>\n");
    
    // Main content
    fprintf(out, "<div class=\"main\">\n");
    fprintf(out, "<div class=\"controls\"><button onclick=\"toggleTheme()\">Dark Mode</button></div>\n");
    for (int i = 0; i < entry_count; i++) {
        fprintf(out, "<div class=\"entry\" id=\"entry-%d\" data-name=\"%s\">\n", i, entries[i].sig);
        fprintf(out, "<span class=\"badge\">%s</span>\n", entries[i].type);
        char name[128] = "";
        if (strncmp(entries[i].sig, "fn ", 3) == 0) sscanf(entries[i].sig + 3, "%127[^( ]", name);
        else if (strncmp(entries[i].sig, "struct ", 7) == 0) sscanf(entries[i].sig + 7, "%127[^ {]", name);
        else if (strncmp(entries[i].sig, "enum ", 5) == 0) sscanf(entries[i].sig + 5, "%127[^ {]", name);
        fprintf(out, "<h3>%s</h3>\n", name[0] ? name : entries[i].sig);
        if (entries[i].doc[0]) fprintf(out, "<div class=\"doc\">%s</div>\n", entries[i].doc);
        fprintf(out, "<pre>%s</pre>\n</div>\n", entries[i].sig);
    }
    fprintf(out, "</div>\n");
    
    fprintf(out, "<script>\n"
        "function toggleTheme(){var h=document.documentElement;h.dataset.theme=h.dataset.theme==='dark'?'':'dark'}\n"
        "function filterDocs(){var q=document.getElementById('search').value.toLowerCase();document.querySelectorAll('.entry').forEach(function(e){e.style.display=e.dataset.name.toLowerCase().includes(q)?'':'none'});document.querySelectorAll('.sidebar a').forEach(function(a){a.style.display=a.textContent.toLowerCase().includes(q)?'':'none'})}\n"
        "</script>\n</body></html>\n");
    
    fclose(out);
    printf("Generated %s (%d entries)\n", out_path, entry_count);
    return 0;
}

int cmd_pkg(int argc, char** argv) {
    if (argc < 3) {
        printf("Wyn Package Manager\n\n");
        printf("Usage: wyn pkg <command>\n\n");
        printf("Commands:\n");
        printf("  init     - Initialize new package\n");
        printf("  add      - Add dependency\n");
        printf("  remove   - Remove dependency\n");
        printf("  list     - List dependencies\n");
        printf("  build    - Build package\n");
        return 1;
    }
    
    char* cmd = argv[2];
    
    if (strcmp(cmd, "init") == 0) {
        printf("Initializing Wyn package...\n");
        
        // Create wyn.toml
        FILE* f = fopen("wyn.toml", "w");
        if (!f) {
            fprintf(stderr, "Error: Cannot create wyn.toml\n");
            return 1;
        }
        
        fprintf(f, "[package]\n");
        fprintf(f, "name = \"my-package\"\n");
        fprintf(f, "version = \"0.1.0\"\n");
        fprintf(f, "authors = []\n\n");
        fprintf(f, "[dependencies]\n");
        fclose(f);
        
        printf("✅ Created wyn.toml\n");
        
        // Create src directory
        system("mkdir -p src");
        
        // Create main.wyn
        FILE* main = fopen("src/main.wyn", "w");
        if (main) {
            fprintf(main, "fn main() -> int {\n");
            fprintf(main, "    return 0;\n");
            fprintf(main, "}\n");
            fclose(main);
            printf("✅ Created src/main.wyn\n");
        }
        
        printf("\n🎉 Package initialized!\n");
        return 0;
    }
    
    if (strcmp(cmd, "add") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: wyn pkg add <package-name>\n");
            return 1;
        }
        
        char* pkg_name = argv[3];
        printf("Adding dependency: %s\n", pkg_name);
        
        // Append to wyn.toml
        FILE* f = fopen("wyn.toml", "a");
        if (!f) {
            fprintf(stderr, "Error: wyn.toml not found. Run 'wyn pkg init' first.\n");
            return 1;
        }
        
        fprintf(f, "%s = \"*\"\n", pkg_name);
        fclose(f);
        
        printf("✅ Added %s to dependencies\n", pkg_name);
        return 0;
    }
    
    if (strcmp(cmd, "list") == 0) {
        FILE* f = fopen("wyn.toml", "r");
        if (!f) {
            fprintf(stderr, "Error: wyn.toml not found\n");
            return 1;
        }
        
        printf("Dependencies:\n");
        char line[256];
        int in_deps = 0;
        
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "[dependencies]")) {
                in_deps = 1;
                continue;
            }
            if (in_deps && line[0] != '[' && strlen(line) > 1) {
                printf("  - %s", line);
            }
            if (in_deps && line[0] == '[') {
                break;
            }
        }
        
        fclose(f);
        return 0;
    }
    
    if (strcmp(cmd, "build") == 0) {
        printf("Building package...\n");
        
        // Compile src/main.wyn
        int result = system("wyn src/main.wyn");
        if (result == 0) {
            printf("✅ Build successful\n");
        } else {
            printf("❌ Build failed\n");
        }
        return result;
    }
    
    fprintf(stderr, "Unknown command: %s\n", cmd);
    return 1;
}

int cmd_lsp(int argc, char** argv) {
    (void)argc;
    (void)argv;
    printf("Wyn LSP Server v1.0\n");
    printf("Listening on stdin/stdout...\n\n");
    
    // Simple LSP server that responds to basic requests
    char line[4096];
    
    while (fgets(line, sizeof(line), stdin)) {
        // Parse Content-Length header
        if (strncmp(line, "Content-Length:", 15) == 0) {
            int content_length = atoi(line + 16);
            
            // Read empty line
            fgets(line, sizeof(line), stdin);
            
            // Read JSON content
            char* content = malloc(content_length + 1);
            fread(content, 1, content_length, stdin);
            content[content_length] = 0;
            
            // Check for initialize request
            if (strstr(content, "\"method\":\"initialize\"")) {
                char* response = 
                    "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{"
                    "\"capabilities\":{"
                    "\"textDocumentSync\":1,"
                    "\"completionProvider\":{\"triggerCharacters\":[\".\"]}"
                    "},\"serverInfo\":{\"name\":\"wyn-lsp\",\"version\":\"1.0\"}}}";
                
                printf("Content-Length: %ld\r\n\r\n%s", strlen(response), response);
                fflush(stdout);
            }
            // Check for shutdown request
            else if (strstr(content, "\"method\":\"shutdown\"")) {
                char* response = "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":null}";
                printf("Content-Length: %ld\r\n\r\n%s", strlen(response), response);
                fflush(stdout);
            }
            // Check for exit notification
            else if (strstr(content, "\"method\":\"exit\"")) {
                free(content);
                break;
            }
            
            free(content);
        }
    }
    
    printf("\nLSP server stopped.\n");
    return 0;
}

int cmd_debug(const char* program, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (!program) {
        fprintf(stderr, "Usage: wyn debug <program>\n");
        return 1;
    }
    
    printf("Wyn Debugger v1.0\n");
    printf("Debugging: %s\n\n", program);
    
    // Check if program exists
    FILE* f = fopen(program, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", program);
        return 1;
    }
    fclose(f);
    
    // Compile the program first
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "./wyn %s > /dev/null 2>&1", program);
    if (system(compile_cmd) != 0) {
        fprintf(stderr, "Error: Failed to compile %s\n", program);
        return 1;
    }
    
    // Get the output binary name
    char binary[512];
    snprintf(binary, sizeof(binary), "%s.out", program);
    
    printf("Compiled successfully. Binary: %s\n", binary);
    printf("\nDebugger commands:\n");
    printf("  run    - Run the program\n");
    printf("  step   - Step through execution (simulated)\n");
    printf("  info   - Show program info\n");
    printf("  quit   - Exit debugger\n\n");
    
    char cmd[256];
    while (1) {
        printf("(wyn-db) ");
        fflush(stdout);
        
        if (!fgets(cmd, sizeof(cmd), stdin)) {
            break;
        }
        
        // Remove newline
        cmd[strcspn(cmd, "\n")] = 0;
        
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            printf("Exiting debugger.\n");
            break;
        }
        else if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
            printf("Running %s...\n", binary);
            int result = system(binary);
            int exit_code = WEXITSTATUS(result);
            printf("Program exited with code: %d\n", exit_code);
        }
        else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "s") == 0) {
            printf("Stepping through execution (simulated)...\n");
            printf("  -> Line 1: fn main() -> int {\n");
            printf("  -> Line 2:     return 0;\n");
            printf("  -> Line 3: }\n");
            printf("Program completed.\n");
        }
        else if (strcmp(cmd, "info") == 0 || strcmp(cmd, "i") == 0) {
            printf("Program: %s\n", program);
            printf("Binary: %s\n", binary);
            
            // Get file size
            struct stat st;
            if (stat(binary, &st) == 0) {
                printf("Binary size: %lld bytes\n", (long long)st.st_size);
            }
        }
        else if (strlen(cmd) > 0) {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'quit' to exit or 'run' to execute.\n");
        }
    }
    
    return 0;
}

int cmd_init(const char* name, int argc, char** argv) {
    (void)argc; (void)argv;  // Unused
    
    if (!name) {
        fprintf(stderr, "Usage: wyn init <project-name>\n");
        return 1;
    }
    
    // Create project directory
    #ifdef _WIN32
    if (_mkdir(name) != 0) {
    #else
    if (mkdir(name, 0755) != 0) {
    #endif
        fprintf(stderr, "Error: Could not create directory '%s'\n", name);
        return 1;
    }
    
    printf("Initializing project '%s'...\n", name);
    
    // Create wyn.toml
    char toml_path[512];
    snprintf(toml_path, sizeof(toml_path), "%s/wyn.toml", name);
    FILE* toml = fopen(toml_path, "w");
    if (!toml) {
        fprintf(stderr, "Error: Could not create wyn.toml\n");
        return 1;
    }
    fprintf(toml, "[project]\n");
    fprintf(toml, "name = \"%s\"\n", name);
    fprintf(toml, "version = \"0.1.0\"\n");
    fprintf(toml, "entry = \"main.wyn\"\n");
    fprintf(toml, "\n[dependencies]\n");
    fprintf(toml, "# Add dependencies here\n");
    fclose(toml);
    printf("  Created wyn.toml\n");
    
    // Create main.wyn
    char main_path[512];
    snprintf(main_path, sizeof(main_path), "%s/main.wyn", name);
    FILE* main_file = fopen(main_path, "w");
    if (!main_file) {
        fprintf(stderr, "Error: Could not create main.wyn\n");
        return 1;
    }
    fprintf(main_file, "// %s - A Wyn project\n\n", name);
    fprintf(main_file, "fn main() {\n");
    fprintf(main_file, "    print(\"Hello from %s!\");\n", name);
    fprintf(main_file, "}\n");
    fclose(main_file);
    printf("  Created main.wyn\n");
    
    // Create README.md
    char readme_path[512];
    snprintf(readme_path, sizeof(readme_path), "%s/README.md", name);
    FILE* readme = fopen(readme_path, "w");
    if (readme) {
        fprintf(readme, "# %s\n\n", name);
        fprintf(readme, "A Wyn project.\n\n");
        fprintf(readme, "## Build\n\n");
        fprintf(readme, "```bash\n");
        fprintf(readme, "wyn run main.wyn\n");
        fprintf(readme, "```\n");
        fclose(readme);
        printf("  Created README.md\n");
    }
    
    printf("\n✓ Project '%s' initialized successfully!\n", name);
    printf("\nNext steps:\n");
    printf("  cd %s\n", name);
    printf("  wyn run main.wyn\n");
    
    return 0;
}

int cmd_watch(const char* file, int argc, char** argv) {
    (void)argc; (void)argv;  // Unused
    
    if (!file) {
        fprintf(stderr, "Usage: wyn watch <file.wyn>\n");
        return 1;
    }
    
    // Get current executable path
    char exe_path[2048];
    #ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        fprintf(stderr, "Error: Could not determine executable path\n");
        return 1;
    }
    #elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        fprintf(stderr, "Error: Could not determine executable path\n");
        return 1;
    }
    exe_path[len] = '\0';
    #elif defined(_WIN32)
    if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path)) == 0) {
        fprintf(stderr, "Error: Could not determine executable path\n");
        return 1;
    }
    #else
    strcpy(exe_path, "wyn");  // Fallback
    #endif
    
    printf("Watching %s for changes (Ctrl+C to stop)...\n", file);
    fflush(stdout);
    
    // Initial build
    printf("\n[%s] Building...\n", file);
    fflush(stdout);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", exe_path, file);
    int result = system(cmd);
    if (result == 0) {
        printf("[%s] ✓ Build successful\n", file);
    } else {
        printf("[%s] ✗ Build failed\n", file);
    }
    fflush(stdout);
    
    // Watch for changes
    struct stat last_stat;
    if (stat(file, &last_stat) != 0) {
        fprintf(stderr, "Error: Could not stat file '%s'\n", file);
        return 1;
    }
    
    while (1) {
        // Sleep for 1 second
        #ifdef _WIN32
        Sleep(1000);
        #else
        sleep(1);
        #endif
        
        struct stat current_stat;
        if (stat(file, &current_stat) != 0) continue;
        
        if (current_stat.st_mtime > last_stat.st_mtime) {
            last_stat = current_stat;
            printf("\n[%s] File changed, rebuilding...\n", file);
            result = system(cmd);
            if (result == 0) {
                printf("[%s] ✓ Build successful\n", file);
            } else {
                printf("[%s] ✗ Build failed\n", file);
            }
        }
    }
    
    return 0;
}

int cmd_version(int argc, char** argv) {
    (void)argc; (void)argv;
    // Read version from VERSION file
    FILE* f = fopen("VERSION", "r");
    if (!f) f = fopen("../VERSION", "r");
    if (f) {
        char version[32];
        if (fgets(version, sizeof(version), f)) {
            char* newline = strchr(version, '\n');
            if (newline) *newline = 0;
            printf("Wyn v%s\n", version);
        } else {
            printf("Wyn v1.7.0\n");
        }
        fclose(f);
    } else {
    (void)argc;
    (void)argv;
        printf("Wyn v1.7.0\n");
    }
    return 0;
}

int cmd_help(const char* command, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (command) {
        printf("Help for '%s' (not yet implemented)\n", command);
        return 0;
    }
    
    printf("Wyn Compiler v1.0.0\n\n");
    printf("Usage: wyn <command> [options]\n\n");
    printf("Commands:\n");
    printf("  compile [file]   Compile Wyn source (default: current directory)\n");
    printf("  test [dir]       Run tests (default: tests/)\n");
    printf("  fmt <file>       Format source code\n");
    printf("  repl             Interactive shell\n");
    printf("  doc <file>       Generate documentation\n");
    printf("  pkg <cmd>        Package management\n");
    printf("  lsp              Start LSP server\n");
    printf("  debug <prog>     Debug program\n");
    printf("  init <name>      Initialize new project\n");
    printf("  version          Show version\n");
    printf("  help [cmd]       Show help\n");
    return 0;
}
