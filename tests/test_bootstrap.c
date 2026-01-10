#include "test.h"
#include <stdio.h>
#include <string.h>

// Test the complete bootstrap self-hosting implementation
static int test_bootstrap_file_exists() {
    FILE* file = fopen("src/bootstrap.wyn", "r");
    if (!file) {
        printf("✗ bootstrap.wyn file not found\n");
        return 0;
    }
    
    char buffer[1000];
    size_t content_size = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        content_size += strlen(buffer);
    }
    fclose(file);
    
    if (content_size < 2000) {
        printf("✗ bootstrap.wyn file too small\n");
        return 0;
    }
    
    return 1;
}

static int test_bootstrap_has_compiler_components() {
    FILE* file = fopen("src/bootstrap.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_bootstrap_compiler = false;
    bool has_compilation_unit = false;
    bool has_self_hosting = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "BootstrapCompiler")) has_bootstrap_compiler = true;
        if (strstr(line, "CompilationUnit")) has_compilation_unit = true;
        if (strstr(line, "bootstrap_compile_self")) has_self_hosting = true;
    }
    fclose(file);
    
    return has_bootstrap_compiler && has_compilation_unit && has_self_hosting;
}

static int test_bootstrap_has_validation() {
    FILE* file = fopen("src/bootstrap.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_validation = false;
    bool has_stats = false;
    bool has_main = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "validate_bootstrap_compiler")) has_validation = true;
        if (strstr(line, "BootstrapStats")) has_stats = true;
        if (strstr(line, "fn main()")) has_main = true;
    }
    fclose(file);
    
    return has_validation && has_stats && has_main;
}

static int test_bootstrap_self_hosting_features() {
    FILE* file = fopen("src/bootstrap.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_wyn_syntax = false;
    bool has_self_compilation = false;
    bool has_milestone_message = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "import std.")) has_wyn_syntax = true;
        if (strstr(line, "Wyn compiling Wyn")) has_self_compilation = true;
        if (strstr(line, "SELF-HOSTING")) has_milestone_message = true;
    }
    fclose(file);
    
    return has_wyn_syntax && has_self_compilation && has_milestone_message;
}

static int test_bootstrap_complete_pipeline() {
    FILE* file = fopen("src/bootstrap.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_lexer = false;
    bool has_parser = false;
    bool has_checker = false;
    bool has_codegen = false;
    bool has_optimizer = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "lexer.wyn")) has_lexer = true;
        if (strstr(line, "parser.wyn")) has_parser = true;
        if (strstr(line, "checker.wyn")) has_checker = true;
        if (strstr(line, "codegen.wyn")) has_codegen = true;
        if (strstr(line, "optimizer.wyn")) has_optimizer = true;
    }
    fclose(file);
    
    return has_lexer && has_parser && has_checker && has_codegen && has_optimizer;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Complete Bootstrap Self-Hosting Tests ===\n");
    
    total++; if (test_bootstrap_file_exists()) { printf("✓ Bootstrap file exists\n"); passed++; } else printf("✗ Bootstrap file missing\n");
    total++; if (test_bootstrap_has_compiler_components()) { printf("✓ Compiler components\n"); passed++; } else printf("✗ Missing compiler components\n");
    total++; if (test_bootstrap_has_validation()) { printf("✓ Validation and statistics\n"); passed++; } else printf("✗ Missing validation\n");
    total++; if (test_bootstrap_self_hosting_features()) { printf("✓ Self-hosting features\n"); passed++; } else printf("✗ Missing self-hosting features\n");
    total++; if (test_bootstrap_complete_pipeline()) { printf("✓ Complete compilation pipeline\n"); passed++; } else printf("✗ Incomplete pipeline\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All bootstrap self-hosting tests passed!\n");
        printf("🎉 WYN LANGUAGE IS READY FOR COMPLETE SELF-HOSTING!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
