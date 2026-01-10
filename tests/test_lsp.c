#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/lsp.h"

void test_server_lifecycle() {
    printf("Testing LSP server lifecycle...\n");
    
    // Create server
    WynLspServer* server = wyn_lsp_server_new();
    assert(server != NULL);
    assert(server->initialized == false);
    assert(server->shutdown_requested == false);
    
    // Initialize server
    bool init_result = wyn_lsp_server_initialize(server, "file:///test/workspace");
    assert(init_result == true);
    assert(server->initialized == true);
    assert(server->root_uri != NULL);
    assert(strcmp(server->root_uri, "file:///test/workspace") == 0);
    
    // Shutdown server
    wyn_lsp_server_shutdown(server);
    assert(server->shutdown_requested == true);
    
    // Free server
    wyn_lsp_server_free(server);
    
    printf("✓ Server lifecycle tests passed\n");
}

void test_document_management() {
    printf("Testing document management...\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///test");
    
    const char* uri = "file:///test/example.wyn";
    const char* text = "fn main() {\n    println(\"Hello, World!\");\n}";
    
    // Open document
    WynLspDocument* doc = wyn_lsp_document_open(server, uri, "wyn", 1, text);
    assert(doc != NULL);
    assert(strcmp(doc->uri, uri) == 0);
    assert(strcmp(doc->language_id, "wyn") == 0);
    assert(doc->version == 1);
    assert(strcmp(doc->text, text) == 0);
    
    // Find document
    WynLspDocument* found = wyn_lsp_find_document(server, uri);
    assert(found == doc);
    
    // Change document
    const char* new_text = "fn main() {\n    println(\"Hello, Wyn!\");\n}";
    bool change_result = wyn_lsp_document_change(server, uri, 2, new_text);
    assert(change_result == true);
    assert(doc->version == 2);
    assert(strcmp(doc->text, new_text) == 0);
    
    // Close document
    bool close_result = wyn_lsp_document_close(server, uri);
    assert(close_result == true);
    
    // Verify document is removed
    WynLspDocument* not_found = wyn_lsp_find_document(server, uri);
    assert(not_found == NULL);
    
    wyn_lsp_server_free(server);
    
    printf("✓ Document management tests passed\n");
}

void test_completions() {
    printf("Testing code completions...\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///test");
    
    const char* uri = "file:///test/example.wyn";
    const char* text = "f";
    
    wyn_lsp_document_open(server, uri, "wyn", 1, text);
    
    WynLspPosition pos = {0, 1};
    WynLspCompletion* completions = wyn_lsp_get_completions(server, uri, pos);
    
    assert(completions != NULL);
    
    // Check that we have some completions
    int count = 0;
    WynLspCompletion* current = completions;
    while (current) {
        count++;
        assert(current->label != NULL);
        assert(current->kind >= WYN_LSP_COMPLETION_TEXT);
        current = current->next;
    }
    
    assert(count > 0);
    
    // Free completions
    current = completions;
    while (current) {
        WynLspCompletion* next = current->next;
        wyn_lsp_free_completion(current);
        current = next;
    }
    
    wyn_lsp_server_free(server);
    
    printf("✓ Code completion tests passed\n");
}

void test_diagnostics() {
    printf("Testing diagnostics...\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///test");
    
    const char* uri = "file:///test/example.wyn";
    const char* text = "// TODO: implement this function\nfn main() {}";
    
    wyn_lsp_document_open(server, uri, "wyn", 1, text);
    
    size_t count;
    WynLspDiagnostic** diagnostics = wyn_lsp_get_diagnostics(server, uri, &count);
    
    assert(count > 0);
    assert(diagnostics != NULL);
    assert(diagnostics[0]->message != NULL);
    assert(diagnostics[0]->severity == WYN_LSP_DIAGNOSTIC_INFORMATION);
    
    free(diagnostics);
    wyn_lsp_server_free(server);
    
    printf("✓ Diagnostics tests passed\n");
}

void test_hover_info() {
    printf("Testing hover information...\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    wyn_lsp_server_initialize(server, "file:///test");
    
    const char* uri = "file:///test/example.wyn";
    const char* text = "fn main() {}";
    
    wyn_lsp_document_open(server, uri, "wyn", 1, text);
    
    WynLspPosition pos = {0, 3}; // Position on "main"
    char* hover_info = wyn_lsp_get_hover_info(server, uri, pos);
    
    assert(hover_info != NULL);
    assert(strlen(hover_info) > 0);
    
    free(hover_info);
    wyn_lsp_server_free(server);
    
    printf("✓ Hover information tests passed\n");
}

void test_position_utilities() {
    printf("Testing position utilities...\n");
    
    const char* text = "line 1\nline 2\nline 3";
    
    // Test offset to position
    WynLspPosition pos1 = wyn_lsp_offset_to_position(text, 0);
    assert(pos1.line == 0 && pos1.character == 0);
    
    WynLspPosition pos2 = wyn_lsp_offset_to_position(text, 7); // Start of "line 2"
    assert(pos2.line == 1 && pos2.character == 0);
    
    // Test position to offset
    WynLspPosition test_pos = {1, 0};
    size_t offset = wyn_lsp_position_to_offset(text, test_pos);
    assert(offset == 7);
    
    // Test word range
    WynLspPosition word_pos = {0, 2}; // Inside "line"
    WynLspRange word_range = wyn_lsp_word_range_at_position(text, word_pos);
    assert(word_range.start.line == 0 && word_range.start.character == 0);
    assert(word_range.end.line == 0 && word_range.end.character == 4);
    
    // Test position in range
    WynLspRange test_range = {{0, 0}, {0, 4}};
    WynLspPosition in_range = {0, 2};
    WynLspPosition out_range = {0, 5};
    
    assert(wyn_lsp_position_in_range(in_range, test_range) == true);
    assert(wyn_lsp_position_in_range(out_range, test_range) == false);
    
    printf("✓ Position utility tests passed\n");
}

void test_configuration() {
    printf("Testing LSP configuration...\n");
    
    WynLspServer* server = wyn_lsp_server_new();
    
    // Check default configuration
    assert(server->config.hover_enabled == true);
    assert(server->config.completion_enabled == true);
    assert(server->config.diagnostics_enabled == true);
    assert(server->config.references_enabled == true);
    assert(server->config.rename_enabled == true);
    assert(server->config.workspace_symbols_enabled == true);
    assert(server->config.max_completion_items == 100);
    assert(server->config.diagnostic_delay_ms == 500);
    
    // Modify configuration
    server->config.max_completion_items = 50;
    server->config.diagnostic_delay_ms = 1000;
    
    assert(server->config.max_completion_items == 50);
    assert(server->config.diagnostic_delay_ms == 1000);
    
    wyn_lsp_server_free(server);
    
    printf("✓ Configuration tests passed\n");
}

int main() {
    printf("Running LSP Foundation Tests\n");
    printf("============================\n\n");
    
    test_server_lifecycle();
    test_document_management();
    test_completions();
    test_diagnostics();
    test_hover_info();
    test_position_utilities();
    test_configuration();
    
    printf("\n✓ All LSP foundation tests passed!\n");
    printf("LSP server provides:\n");
    printf("  - Document lifecycle management\n");
    printf("  - Code completion with keywords\n");
    printf("  - Basic diagnostics (TODO detection)\n");
    printf("  - Hover information\n");
    printf("  - Position/range utilities\n");
    printf("  - Configurable features\n");
    
    return 0;
}
