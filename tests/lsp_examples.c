#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/lsp.h"

void example_basic_lsp_server() {
    printf("=== Basic LSP Server Example ===\n");
    
    // Create and initialize server
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///workspace/my-project");
    
    printf("LSP server initialized for workspace: %s\n", server->root_uri);
    printf("Configuration:\n");
    printf("  - Hover enabled: %s\n", server->config.hover_enabled ? "yes" : "no");
    printf("  - Completion enabled: %s\n", server->config.completion_enabled ? "yes" : "no");
    printf("  - Diagnostics enabled: %s\n", server->config.diagnostics_enabled ? "yes" : "no");
    printf("  - Max completion items: %d\n", server->config.max_completion_items);
    
    wyn_lsp_server_free(server);
    printf("\n");
}

void example_document_lifecycle() {
    printf("=== Document Lifecycle Example ===\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///workspace");
    
    const char* uri = "file:///workspace/main.wyn";
    const char* initial_text = "fn main() {\n    // TODO: implement\n}";
    
    // Open document
    printf("Opening document: %s\n", uri);
    WynLspDocument* doc = wyn_lsp_document_open(server, uri, "wyn", 1, initial_text);
    printf("Document opened with version %d\n", doc->version);
    
    // Show diagnostics
    size_t diag_count;
    WynLspDiagnostic** diagnostics = wyn_lsp_get_diagnostics(server, uri, &diag_count);
    printf("Found %zu diagnostics:\n", diag_count);
    for (size_t i = 0; i < diag_count; i++) {
        printf("  - Line %d: %s\n", diagnostics[i]->range.start.line + 1, diagnostics[i]->message);
    }
    free(diagnostics);
    
    // Update document
    const char* updated_text = "fn main() {\n    println(\"Hello, Wyn!\");\n}";
    printf("\nUpdating document content...\n");
    wyn_lsp_document_change(server, uri, 2, updated_text);
    printf("Document updated to version %d\n", doc->version);
    
    // Check diagnostics again
    diagnostics = wyn_lsp_get_diagnostics(server, uri, &diag_count);
    printf("Found %zu diagnostics after update\n", diag_count);
    free(diagnostics);
    
    // Close document
    printf("Closing document...\n");
    wyn_lsp_document_close(server, uri);
    
    wyn_lsp_server_free(server);
    printf("\n");
}

void example_code_completion() {
    printf("=== Code Completion Example ===\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///workspace");
    
    const char* uri = "file:///workspace/example.wyn";
    const char* text = "f"; // User typed 'f'
    
    wyn_lsp_document_open(server, uri, "wyn", 1, text);
    
    // Get completions at position after 'f'
    WynLspPosition pos = {0, 1};
    WynLspCompletion* completions = wyn_lsp_get_completions(server, uri, pos);
    
    printf("Code completions for 'f':\n");
    WynLspCompletion* current = completions;
    int count = 0;
    while (current && count < 5) { // Show first 5
        printf("  - %s (%s)\n", current->label, 
               current->kind == WYN_LSP_COMPLETION_KEYWORD ? "keyword" : "other");
        current = current->next;
        count++;
    }
    
    // Free completions
    current = completions;
    while (current) {
        WynLspCompletion* next = current->next;
        wyn_lsp_free_completion(current);
        current = next;
    }
    
    wyn_lsp_server_free(server);
    printf("\n");
}

void example_hover_information() {
    printf("=== Hover Information Example ===\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///workspace");
    
    const char* uri = "file:///workspace/hover.wyn";
    const char* text = "fn calculate(x: i32, y: i32) -> i32 {\n    x + y\n}";
    
    wyn_lsp_document_open(server, uri, "wyn", 1, text);
    
    // Get hover info for function name
    WynLspPosition pos = {0, 5}; // Position on "calculate"
    char* hover_info = wyn_lsp_get_hover_info(server, uri, pos);
    
    printf("Hover information at position (line 1, char 6):\n");
    printf("  %s\n", hover_info);
    
    free(hover_info);
    wyn_lsp_server_free(server);
    printf("\n");
}

void example_position_utilities() {
    printf("=== Position Utilities Example ===\n");
    
    const char* sample_text = "fn main() {\n    let x = 42;\n    println(x);\n}";
    
    printf("Sample text:\n%s\n\n", sample_text);
    
    // Convert offset to position
    size_t offset = 15; // Position of 'let'
    WynLspPosition pos = wyn_lsp_offset_to_position(sample_text, offset);
    printf("Offset %zu -> Position (line %d, char %d)\n", offset, pos.line + 1, pos.character + 1);
    
    // Convert position back to offset
    size_t converted_offset = wyn_lsp_position_to_offset(sample_text, pos);
    printf("Position (line %d, char %d) -> Offset %zu\n", pos.line + 1, pos.character + 1, converted_offset);
    
    // Get word range
    WynLspPosition word_pos = {1, 6}; // Inside "let"
    WynLspRange word_range = wyn_lsp_word_range_at_position(sample_text, word_pos);
    printf("Word at position (line %d, char %d): range (line %d, char %d) to (line %d, char %d)\n",
           word_pos.line + 1, word_pos.character + 1,
           word_range.start.line + 1, word_range.start.character + 1,
           word_range.end.line + 1, word_range.end.character + 1);
    
    // Extract word text
    size_t start_offset = wyn_lsp_position_to_offset(sample_text, word_range.start);
    size_t end_offset = wyn_lsp_position_to_offset(sample_text, word_range.end);
    size_t word_length = end_offset - start_offset;
    char* word = malloc(word_length + 1);
    strncpy(word, sample_text + start_offset, word_length);
    word[word_length] = '\0';
    printf("Extracted word: '%s'\n", word);
    free(word);
    
    printf("\n");
}

void example_configuration_management() {
    printf("=== Configuration Management Example ===\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    
    printf("Default configuration:\n");
    printf("  - Hover: %s\n", server->config.hover_enabled ? "enabled" : "disabled");
    printf("  - Completion: %s\n", server->config.completion_enabled ? "enabled" : "disabled");
    printf("  - Diagnostics: %s\n", server->config.diagnostics_enabled ? "enabled" : "disabled");
    printf("  - Max completions: %d\n", server->config.max_completion_items);
    printf("  - Diagnostic delay: %d ms\n", server->config.diagnostic_delay_ms);
    
    // Customize configuration
    printf("\nCustomizing configuration...\n");
    server->config.max_completion_items = 25;
    server->config.diagnostic_delay_ms = 200;
    server->config.hover_enabled = false;
    
    printf("Updated configuration:\n");
    printf("  - Hover: %s\n", server->config.hover_enabled ? "enabled" : "disabled");
    printf("  - Max completions: %d\n", server->config.max_completion_items);
    printf("  - Diagnostic delay: %d ms\n", server->config.diagnostic_delay_ms);
    
    wyn_lsp_server_free(server);
    printf("\n");
}

int main() {
    printf("Wyn Language Server Protocol Examples\n");
    printf("=====================================\n\n");
    
    example_basic_lsp_server();
    example_document_lifecycle();
    example_code_completion();
    example_hover_information();
    example_position_utilities();
    example_configuration_management();
    
    printf("LSP Foundation Features:\n");
    printf("  ✓ Server lifecycle management\n");
    printf("  ✓ Document synchronization\n");
    printf("  ✓ Code completion\n");
    printf("  ✓ Hover information\n");
    printf("  ✓ Diagnostics\n");
    printf("  ✓ Position utilities\n");
    printf("  ✓ Configurable behavior\n");
    printf("\nReady for integration with editors and IDEs!\n");
    
    return 0;
}
