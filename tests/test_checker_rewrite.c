#include "test.h"
#include <stdio.h>
#include <string.h>

// Test the Wyn type checker rewrite
static int test_checker_rewrite_exists() {
    FILE* file = fopen("src/checker.wyn", "r");
    if (!file) {
        printf("✗ checker.wyn file not found\n");
        return 0;
    }
    
    char buffer[1000];
    size_t content_size = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        content_size += strlen(buffer);
    }
    fclose(file);
    
    if (content_size < 1000) {
        printf("✗ checker.wyn file too small\n");
        return 0;
    }
    
    return 1;
}

static int test_checker_has_core_functions() {
    FILE* file = fopen("src/checker.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_check_expression = false;
    bool has_check_statement = false;
    bool has_type_checking = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "check_expression")) has_check_expression = true;
        if (strstr(line, "check_statement")) has_check_statement = true;
        if (strstr(line, "type_check_program")) has_type_checking = true;
    }
    fclose(file);
    
    return has_check_expression && has_check_statement && has_type_checking;
}

static int test_checker_has_symbol_table() {
    FILE* file = fopen("src/checker.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_symbol_table = false;
    bool has_add_symbol = false;
    bool has_lookup_symbol = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "SymbolTable")) has_symbol_table = true;
        if (strstr(line, "add_symbol")) has_add_symbol = true;
        if (strstr(line, "lookup_symbol")) has_lookup_symbol = true;
    }
    fclose(file);
    
    return has_symbol_table && has_add_symbol && has_lookup_symbol;
}

static int test_checker_has_type_system() {
    FILE* file = fopen("src/checker.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_type_kind = false;
    bool has_type_struct = false;
    bool has_type_compatibility = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "TypeKind")) has_type_kind = true;
        if (strstr(line, "struct Type")) has_type_struct = true;
        if (strstr(line, "types_compatible")) has_type_compatibility = true;
    }
    fclose(file);
    
    return has_type_kind && has_type_struct && has_type_compatibility;
}

static int test_checker_self_hosting() {
    // Verify this is written in Wyn syntax, not C
    FILE* file = fopen("src/checker.wyn", "r");
    if (!file) return 0;
    
    char line[500];
    bool has_wyn_syntax = false;
    bool has_match_expr = false;
    bool has_fn_keyword = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "fn ") && strstr(line, "->")) has_fn_keyword = true;
        if (strstr(line, "match ")) has_match_expr = true;
        if (strstr(line, "import std.")) has_wyn_syntax = true;
    }
    fclose(file);
    
    return has_wyn_syntax && has_match_expr && has_fn_keyword;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Type Checker Rewrite Tests ===\n");
    
    total++; if (test_checker_rewrite_exists()) { printf("✓ Checker rewrite file exists\n"); passed++; } else printf("✗ Checker rewrite file missing\n");
    total++; if (test_checker_has_core_functions()) { printf("✓ Core type checking functions\n"); passed++; } else printf("✗ Missing core functions\n");
    total++; if (test_checker_has_symbol_table()) { printf("✓ Symbol table implementation\n"); passed++; } else printf("✗ Missing symbol table\n");
    total++; if (test_checker_has_type_system()) { printf("✓ Type system definitions\n"); passed++; } else printf("✗ Missing type system\n");
    total++; if (test_checker_self_hosting()) { printf("✓ Self-hosting Wyn syntax\n"); passed++; } else printf("✗ Not using Wyn syntax\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All type checker rewrite tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
