#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "common.h"
#include "ast.h"
#include "wyn_interface.h"

// Forward declarations
extern void init_lexer(const char* source);
extern void init_parser();
extern void init_checker();
extern void init_codegen(FILE* output);
extern Program* parse_program();
extern void check_program(Program* prog);
extern bool checker_had_error();
extern void free_program(Program* prog);
extern void codegen_c_header();
extern void codegen_program(Program* prog);

// Compile a single file
static int compile_file_with_output(const char* filename, const char* output_name) {
    char* source = wyn_read_file(filename);
    
    // Generate output filename
    char output_c[512];
    snprintf(output_c, sizeof(output_c), "%s.c", filename);
    
    FILE* output = fopen(output_c, "w");
    if (!output) {
        fprintf(stderr, "Error: Could not create output file '%s'\n", output_c);
        free(source);
        return 1;
    }
    
    init_lexer(source);
    init_parser();
    init_checker();
    init_codegen(output);
    
    Program* prog = parse_program();
    if (!prog) {
        fprintf(stderr, "Error: Failed to parse program\n");
        fclose(output);
        free(source);
        return 1;
    }
    
    check_program(prog);
    if (checker_had_error()) {
        fclose(output);
        free_program(prog);
        free(source);
        return 1;
    }
    
    codegen_c_header();
    codegen_program(prog);
    
    fclose(output);
    free_program(prog);
    free(source);
    
    // Compile C to binary
    char output_bin[512];
    if (output_name) {
        snprintf(output_bin, sizeof(output_bin), "%s", output_name);
    } else {
        snprintf(output_bin, sizeof(output_bin), "%s.out", filename);
    }
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), 
             "gcc -O0 -I src -o %s %s src/wyn_wrapper.c src/wyn_interface.c "
             "src/io.c src/optional.c src/result.c src/arc_runtime.c "
             "src/safe_memory.c src/error.c src/string_runtime.c src/hashmap.c src/hashset.c src/json.c -lm 2>&1",
             output_bin, output_c);
    
    int result = system(cmd);
    if (result != 0) {
        return 1;
    }
    
    printf("Compiled successfully: %s\n", output_bin);
    return 0;
}

static int compile_file(const char* filename) {
    return compile_file_with_output(filename, NULL);
}

// Find main.wyn in directory
static char* find_main_file(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return NULL;
    
    struct dirent* entry;
    static char path[512];
    
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, "main.wyn") == 0) {
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
            closedir(d);
            return path;
        }
    }
    
    closedir(d);
    return NULL;
}

// Main compile command
int cmd_compile(const char* target, int argc, char** argv) {
    // Check for -o flag
    const char* output_name = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[i + 1];
            break;
        }
    }
    
    // Check if target is a file or directory
    FILE* f = fopen(target, "r");
    if (f) {
        // It's a file
        fclose(f);
        return compile_file_with_output(target, output_name);
    }
    
    // Try as directory
    char* main_file = find_main_file(target);
    if (main_file) {
        return compile_file_with_output(main_file, output_name);
    }
    
    fprintf(stderr, "Error: Could not find '%s' or main.wyn in directory\n", target);
    return 1;
}
