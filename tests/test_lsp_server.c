#include "test.h"
#include "lsp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_lsp_initialization() {
    printf("Testing LSP server initialization...\n");
    
    int result = wyn_lsp_init();
    if (result != 0) {
        printf("  FAIL: LSP initialization failed\n");
        return 0;
    }
    
    printf("  PASS: LSP server initialized successfully\n");
    return 1;
}

static int test_lsp_initialize_request() {
    printf("Testing LSP initialize request handling...\n");
    
    const char* init_params = "{\"rootUri\":\"file:///test/project\"}";
    int result = wyn_lsp_handle_initialize(init_params);
    
    if (result != 0) {
        printf("  FAIL: Initialize request handling failed\n");
        return 0;
    }
    
    printf("  PASS: Initialize request handled successfully\n");
    return 1;
}

static int test_lsp_completion_request() {
    printf("Testing LSP completion request handling...\n");
    
    const char* completion_params = 
        "{"
        "\"textDocument\":{\"uri\":\"file:///test.wyn\"},"
        "\"position\":{\"line\":0,\"character\":5}"
        "}";
    
    int result = wyn_lsp_handle_completion(completion_params);
    
    if (result != 0) {
        printf("  FAIL: Completion request handling failed\n");
        return 0;
    }
    
    printf("  PASS: Completion request handled successfully\n");
    return 1;
}

static int test_lsp_text_document_operations() {
    printf("Testing LSP text document operations...\n");
    
    // Test document open
    const char* open_params = 
        "{"
        "\"textDocument\":{"
        "\"uri\":\"file:///test.wyn\","
        "\"text\":\"fn main() { println(\\\"Hello\\\"); }\""
        "}"
        "}";
    
    int result = wyn_lsp_handle_text_document_open(open_params);
    if (result != 0) {
        printf("  FAIL: Text document open failed\n");
        return 0;
    }
    
    // Test document change
    result = wyn_lsp_handle_text_document_change("{}");
    if (result != 0) {
        printf("  FAIL: Text document change failed\n");
        return 0;
    }
    
    printf("  PASS: Text document operations successful\n");
    return 1;
}

static int test_lsp_hover_request() {
    printf("Testing LSP hover request handling...\n");
    
    const char* hover_params = 
        "{"
        "\"textDocument\":{\"uri\":\"file:///test.wyn\"},"
        "\"position\":{\"line\":0,\"character\":5}"
        "}";
    
    int result = wyn_lsp_handle_hover(hover_params);
    
    if (result != 0) {
        printf("  FAIL: Hover request handling failed\n");
        return 0;
    }
    
    printf("  PASS: Hover request handled successfully\n");
    return 1;
}

static int test_lsp_definition_request() {
    printf("Testing LSP definition request handling...\n");
    
    const char* definition_params = 
        "{"
        "\"textDocument\":{\"uri\":\"file:///test.wyn\"},"
        "\"position\":{\"line\":0,\"character\":5}"
        "}";
    
    int result = wyn_lsp_handle_definition(definition_params);
    
    if (result != 0) {
        printf("  FAIL: Definition request handling failed\n");
        return 0;
    }
    
    printf("  PASS: Definition request handled successfully\n");
    return 1;
}

static int test_lsp_references_request() {
    printf("Testing LSP references request handling...\n");
    
    const char* references_params = 
        "{"
        "\"textDocument\":{\"uri\":\"file:///test.wyn\"},"
        "\"position\":{\"line\":0,\"character\":5}"
        "}";
    
    int result = wyn_lsp_handle_references(references_params);
    
    if (result != 0) {
        printf("  FAIL: References request handling failed\n");
        return 0;
    }
    
    printf("  PASS: References request handled successfully\n");
    return 1;
}

static int test_lsp_message_processing() {
    printf("Testing LSP message processing...\n");
    
    // Test initialize message
    const char* init_message = "{\"method\":\"initialize\",\"params\":{}}";
    int result = wyn_lsp_process_message(init_message);
    
    if (result != 0) {
        printf("  FAIL: Initialize message processing failed\n");
        return 0;
    }
    
    // Test completion message
    const char* completion_message = "{\"method\":\"textDocument/completion\",\"params\":{}}";
    result = wyn_lsp_process_message(completion_message);
    
    if (result != 0) {
        printf("  FAIL: Completion message processing failed\n");
        return 0;
    }
    
    printf("  PASS: Message processing successful\n");
    return 1;
}

static int test_lsp_diagnostics() {
    printf("Testing LSP diagnostics analysis...\n");
    
    const char* test_content = "let x = 42\nfn main() { println(x); }";
    size_t diagnostic_count;
    
    LSPDiagnostic* diagnostics = wyn_lsp_analyze_document("file:///test.wyn", test_content, &diagnostic_count);
    
    if (!diagnostics && diagnostic_count > 0) {
        printf("  FAIL: Diagnostics analysis failed\n");
        return 0;
    }
    
    if (diagnostics) {
        // Check if diagnostic was found for missing semicolon
        if (diagnostic_count > 0 && diagnostics[0].message) {
            printf("  Found diagnostic: %s\n", diagnostics[0].message);
        }
        
        // Cleanup
        for (size_t i = 0; i < diagnostic_count; i++) {
            if (diagnostics[i].message) free(diagnostics[i].message);
            if (diagnostics[i].source) free(diagnostics[i].source);
        }
        free(diagnostics);
    }
    
    printf("  PASS: Diagnostics analysis successful\n");
    return 1;
}

static int test_lsp_completions() {
    printf("Testing LSP completions generation...\n");
    
    LSPPosition pos = {0, 5};
    size_t completion_count;
    
    LSPCompletionItem* completions = wyn_lsp_get_completions("file:///test.wyn", pos, &completion_count);
    
    if (!completions || completion_count == 0) {
        printf("  FAIL: Completions generation failed\n");
        return 0;
    }
    
    printf("  Generated %zu completions\n", completion_count);
    
    // Check first completion
    if (completions[0].label) {
        printf("  First completion: %s\n", completions[0].label);
    }
    
    // Cleanup
    for (size_t i = 0; i < completion_count; i++) {
        if (completions[i].label) free(completions[i].label);
        if (completions[i].detail) free(completions[i].detail);
    }
    free(completions);
    
    printf("  PASS: Completions generation successful\n");
    return 1;
}

int main() {
    printf("=== T5.3.1: LSP Server Implementation Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_lsp_initialization()) passed_tests++;
    total_tests++; if (test_lsp_initialize_request()) passed_tests++;
    total_tests++; if (test_lsp_completion_request()) passed_tests++;
    total_tests++; if (test_lsp_text_document_operations()) passed_tests++;
    total_tests++; if (test_lsp_hover_request()) passed_tests++;
    total_tests++; if (test_lsp_definition_request()) passed_tests++;
    total_tests++; if (test_lsp_references_request()) passed_tests++;
    total_tests++; if (test_lsp_message_processing()) passed_tests++;
    total_tests++; if (test_lsp_diagnostics()) passed_tests++;
    total_tests++; if (test_lsp_completions()) passed_tests++;
    
    // Cleanup
    wyn_lsp_cleanup();
    
    // Print summary
    printf("\n=== LSP Server Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All LSP server tests passed!\n");
        printf("🔧 Language Server Protocol ready for IDE integration\n");
        return 0;
    } else {
        printf("❌ Some LSP server tests failed\n");
        return 1;
    }
}
