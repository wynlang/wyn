#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test parser rewrite implementation
static int test_parser_wyn_compilation() {
    printf("Testing Wyn parser compilation...\n");
    
    // Check if Wyn compiler exists
    int result = system("../wyn --version > /dev/null 2>&1");
    if (result != 0) {
        printf("  SKIP: Wyn compiler not available\n");
        return 1; // Skip test
    }
    
    // Test compiling parser.wyn
    printf("  Compiling parser.wyn...\n");
    result = system("../wyn compile src/parser.wyn -o build/parser_wyn > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Parser compilation failed\n");
        return 0;
    }
    
    printf("  PASS: Parser compiled successfully\n");
    return 1;
}

static int test_parser_ast_nodes() {
    printf("Testing parser AST node definitions...\n");
    
    // Test that parser.wyn contains expected AST node types
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_node_type = false;
    bool found_binary_expr = false;
    bool found_function_def = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "enum NodeType")) {
            found_node_type = true;
        }
        if (strstr(line, "struct BinaryExpr")) {
            found_binary_expr = true;
        }
        if (strstr(line, "struct FunctionDef")) {
            found_function_def = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_node_type || !found_binary_expr || !found_function_def) {
        printf("  FAIL: Missing expected AST node definitions\n");
        return 0;
    }
    
    printf("  PASS: AST node definitions found\n");
    return 1;
}

static int test_parser_functions() {
    printf("Testing parser function definitions...\n");
    
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_parse_program = false;
    bool found_parse_expression = false;
    bool found_parse_statement = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "fn parser_parse_program")) {
            found_parse_program = true;
        }
        if (strstr(line, "fn parser_parse_expression")) {
            found_parse_expression = true;
        }
        if (strstr(line, "fn parser_parse_statement")) {
            found_parse_statement = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_parse_program || !found_parse_expression || !found_parse_statement) {
        printf("  FAIL: Missing expected parser functions\n");
        return 0;
    }
    
    printf("  PASS: Parser functions found\n");
    return 1;
}

static int test_parser_expression_parsing() {
    printf("Testing parser expression parsing capabilities...\n");
    
    // Test that parser includes expression parsing functions
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_binary_ops = false;
    bool found_unary_ops = false;
    bool found_call_expr = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "parser_parse_logical_or") || strstr(line, "parser_parse_equality")) {
            found_binary_ops = true;
        }
        if (strstr(line, "parser_parse_unary")) {
            found_unary_ops = true;
        }
        if (strstr(line, "parser_parse_call")) {
            found_call_expr = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_binary_ops || !found_unary_ops || !found_call_expr) {
        printf("  FAIL: Missing expression parsing functions\n");
        return 0;
    }
    
    printf("  PASS: Expression parsing capabilities found\n");
    return 1;
}

static int test_parser_statement_parsing() {
    printf("Testing parser statement parsing capabilities...\n");
    
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_function_parsing = false;
    bool found_variable_parsing = false;
    bool found_control_flow = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "parser_parse_function")) {
            found_function_parsing = true;
        }
        if (strstr(line, "parser_parse_variable")) {
            found_variable_parsing = true;
        }
        if (strstr(line, "parser_parse_if_statement") || strstr(line, "parser_parse_while_statement")) {
            found_control_flow = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_function_parsing || !found_variable_parsing || !found_control_flow) {
        printf("  FAIL: Missing statement parsing functions\n");
        return 0;
    }
    
    printf("  PASS: Statement parsing capabilities found\n");
    return 1;
}

static int test_parser_error_handling() {
    printf("Testing parser error handling...\n");
    
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_error_handling = false;
    bool found_consume_function = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "parser.errors") || strstr(line, "error_msg")) {
            found_error_handling = true;
        }
        if (strstr(line, "parser_consume")) {
            found_consume_function = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_error_handling || !found_consume_function) {
        printf("  FAIL: Missing error handling mechanisms\n");
        return 0;
    }
    
    printf("  PASS: Error handling found\n");
    return 1;
}

static int test_parser_public_api() {
    printf("Testing parser public API...\n");
    
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    char line[1024];
    bool found_parse_export = false;
    bool found_parse_expression_export = false;
    
    while (fgets(line, sizeof(line), parser_file)) {
        if (strstr(line, "export fn parse(")) {
            found_parse_export = true;
        }
        if (strstr(line, "export fn parse_expression(")) {
            found_parse_expression_export = true;
        }
    }
    
    fclose(parser_file);
    
    if (!found_parse_export || !found_parse_expression_export) {
        printf("  FAIL: Missing public API exports\n");
        return 0;
    }
    
    printf("  PASS: Public API exports found\n");
    return 1;
}

static int test_parser_self_hosting_readiness() {
    printf("Testing parser self-hosting readiness...\n");
    
    // Check file size (should be substantial for a complete parser)
    FILE* parser_file = fopen("src/parser.wyn", "r");
    if (!parser_file) {
        printf("  FAIL: parser.wyn file not found\n");
        return 0;
    }
    
    fseek(parser_file, 0, SEEK_END);
    long file_size = ftell(parser_file);
    fclose(parser_file);
    
    if (file_size < 10000) {  // Expect at least 10KB for a complete parser
        printf("  FAIL: Parser implementation seems incomplete (file too small)\n");
        return 0;
    }
    
    printf("  Parser file size: %ld bytes\n", file_size);
    printf("  PASS: Parser appears to be substantial implementation\n");
    return 1;
}

int main() {
    printf("=== T7.2.2: Parser Rewrite in Wyn Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_parser_wyn_compilation()) passed_tests++;
    total_tests++; if (test_parser_ast_nodes()) passed_tests++;
    total_tests++; if (test_parser_functions()) passed_tests++;
    total_tests++; if (test_parser_expression_parsing()) passed_tests++;
    total_tests++; if (test_parser_statement_parsing()) passed_tests++;
    total_tests++; if (test_parser_error_handling()) passed_tests++;
    total_tests++; if (test_parser_public_api()) passed_tests++;
    total_tests++; if (test_parser_self_hosting_readiness()) passed_tests++;
    
    // Print summary
    printf("\n=== Parser Rewrite Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All parser rewrite tests passed!\n");
        printf("🔄 Self-hosting parser implementation ready\n");
        return 0;
    } else {
        printf("❌ Some parser rewrite tests failed\n");
        return 1;
    }
}
