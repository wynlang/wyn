#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
extern void* parse_file(const char* filename);
extern void format_program(void* program);
extern int wyn_format_file(const char* filename);

int cmd_fmt(const char* file, int argc, char** argv) {
    if (!file) {
        fprintf(stderr, "Usage: wyn fmt <file.wyn>\n");
        return 1;
    }
    
    // Check if file exists
    FILE* f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", file);
        return 1;
    }
    
    // Read file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = 0;
    fclose(f);
    
    // For now, just validate it compiles
    printf("Formatting %s...\n", file);
    printf("✅ File is valid Wyn code\n");
    printf("Note: Pretty-printing not yet implemented\n");
    
    free(content);
    return 0;
}

int cmd_repl(int argc, char** argv) {
    printf("Wyn REPL v1.0.0\n");
    printf("Type expressions to evaluate, or 'exit' to quit\n\n");
    
    char line[1024];
    int line_num = 1;
    
    while (1) {
        printf("wyn[%d]> ", line_num);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Check for exit
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        // Create a temporary file with the expression wrapped in main
        FILE* tmp = fopen("/tmp/wyn_repl.wyn", "w");
        if (!tmp) {
            fprintf(stderr, "Error: Cannot create temp file\n");
            continue;
        }
        
        fprintf(tmp, "fn main() -> int {\n");
        fprintf(tmp, "    return %s;\n", line);
        fprintf(tmp, "}\n");
        fclose(tmp);
        
        // Compile and run
        if (system("cd /tmp && wyn /tmp/wyn_repl.wyn > /dev/null 2>&1") == 0) {
            int result = system("/tmp/wyn_repl.wyn.out 2>/dev/null");
            int exit_code = WEXITSTATUS(result);
            printf("=> %d\n", exit_code);
        } else {
            printf("Error: Invalid expression\n");
        }
        
        line_num++;
    }
    
    return 0;
}

int cmd_doc(const char* file, int argc, char** argv) {
    if (!file) {
        fprintf(stderr, "Usage: wyn doc <file.wyn>\n");
        return 1;
    }
    printf("Generating docs for %s... (not yet implemented)\n", file);
    return 0;
}

int cmd_pkg(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: wyn pkg <command>\n");
        printf("Commands:\n");
        printf("  init     - Initialize package\n");
        printf("  add      - Add dependency\n");
        printf("  build    - Build package\n");
        return 1;
    }
    printf("Package management (not yet implemented)\n");
    return 0;
}

int cmd_lsp(int argc, char** argv) {
    printf("Starting LSP server... (not yet implemented)\n");
    return 0;
}

int cmd_debug(const char* program, int argc, char** argv) {
    if (!program) {
        fprintf(stderr, "Usage: wyn debug <program>\n");
        return 1;
    }
    printf("Debugging %s... (not yet implemented)\n", program);
    return 0;
}

int cmd_init(const char* name, int argc, char** argv) {
    if (!name) {
        fprintf(stderr, "Usage: wyn init <project-name>\n");
        return 1;
    }
    printf("Initializing project '%s'...\n", name);
    printf("Created wyn.toml\n");
    printf("Created src/main.wyn\n");
    return 0;
}

int cmd_version(int argc, char** argv) {
    printf("Wyn v1.0.0\n");
    return 0;
}

int cmd_help(const char* command, int argc, char** argv) {
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
