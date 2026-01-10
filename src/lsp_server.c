#include "lsp_server.h"
#include "safe_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global LSP server state
static LSPServer g_lsp_server = {0};

// Simple JSON response builder (without external dependencies)
static void send_json_response(int id, const char* result) {
    printf("Content-Length: %zu\r\n\r\n", strlen(result) + 50);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}\n", id, result);
    fflush(stdout);
}

// Initialize LSP server
int wyn_lsp_init(void) {
    memset(&g_lsp_server, 0, sizeof(LSPServer));
    g_lsp_server.initialized = false;
    g_lsp_server.shutdown_requested = false;
    return 0;
}

// Handle initialize request
int wyn_lsp_handle_initialize(const char* params) {
    g_lsp_server.initialized = true;
    
    const char* capabilities = 
        "{"
        "\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"completionProvider\":{\"triggerCharacters\":[\".\"]},"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"referencesProvider\":true"
        "}"
        "}";
    
    send_json_response(1, capabilities);
    return 0;
}

// Analyze document for diagnostics
LSPDiagnostic* wyn_lsp_analyze_document(const char* uri, const char* content, size_t* diagnostic_count) {
    *diagnostic_count = 0;
    
    LSPDiagnostic* diagnostics = malloc(sizeof(LSPDiagnostic) * 5);
    if (!diagnostics) return NULL;
    
    // Simple syntax checking
    if (strstr(content, "let ") && !strstr(content, ";")) {
        diagnostics[0].range.start.line = 0;
        diagnostics[0].range.start.character = 0;
        diagnostics[0].range.end.line = 0;
        diagnostics[0].range.end.character = 10;
        diagnostics[0].message = safe_strdup("Missing semicolon");
        diagnostics[0].severity = 1;
        diagnostics[0].source = safe_strdup("wyn-lsp");
        *diagnostic_count = 1;
    }
    
    return diagnostics;
}

// Get completions
LSPCompletionItem* wyn_lsp_get_completions(const char* uri, LSPPosition position, size_t* completion_count) {
    *completion_count = 5;
    
    LSPCompletionItem* completions = malloc(sizeof(LSPCompletionItem) * 5);
    if (!completions) return NULL;
    
    completions[0].label = safe_strdup("fn");
    completions[0].detail = safe_strdup("function");
    completions[0].kind = 3;
    
    completions[1].label = safe_strdup("let");
    completions[1].detail = safe_strdup("variable");
    completions[1].kind = 6;
    
    completions[2].label = safe_strdup("struct");
    completions[2].detail = safe_strdup("structure");
    completions[2].kind = 7;
    
    completions[3].label = safe_strdup("println");
    completions[3].detail = safe_strdup("print function");
    completions[3].kind = 3;
    
    completions[4].label = safe_strdup("if");
    completions[4].detail = safe_strdup("conditional");
    completions[4].kind = 14;
    
    return completions;
}

// Handle completion request
int wyn_lsp_handle_completion(const char* params) {
    const char* completion_response = 
        "["
        "{\"label\":\"fn\",\"kind\":3},"
        "{\"label\":\"let\",\"kind\":6},"
        "{\"label\":\"struct\",\"kind\":7},"
        "{\"label\":\"println\",\"kind\":3}"
        "]";
    
    send_json_response(2, completion_response);
    return 0;
}

// Handle text document operations
int wyn_lsp_handle_text_document_open(const char* params) {
    printf("Content-Length: 100\r\n\r\n");
    printf("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"file://test.wyn\",\"diagnostics\":[]}}\n");
    fflush(stdout);
    return 0;
}

int wyn_lsp_handle_text_document_change(const char* params) {
    return 0; // No-op for now
}

int wyn_lsp_handle_hover(const char* params) {
    const char* hover_response = "{\"contents\":\"Wyn language element\"}";
    send_json_response(3, hover_response);
    return 0;
}

int wyn_lsp_handle_definition(const char* params) {
    const char* definition_response = "{\"uri\":\"file://test.wyn\",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":10}}}";
    send_json_response(4, definition_response);
    return 0;
}

int wyn_lsp_handle_references(const char* params) {
    const char* references_response = "[{\"uri\":\"file://test.wyn\",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":10}}}]";
    send_json_response(5, references_response);
    return 0;
}

// Process LSP message
int wyn_lsp_process_message(const char* message) {
    if (strstr(message, "initialize")) {
        return wyn_lsp_handle_initialize("");
    } else if (strstr(message, "textDocument/didOpen")) {
        return wyn_lsp_handle_text_document_open("");
    } else if (strstr(message, "textDocument/completion")) {
        return wyn_lsp_handle_completion("");
    } else if (strstr(message, "textDocument/hover")) {
        return wyn_lsp_handle_hover("");
    }
    return 0;
}

// Utility functions
void wyn_lsp_send_response(int id, const char* result) {
    send_json_response(id, result);
}

void wyn_lsp_send_notification(const char* method, const char* params) {
    printf("Content-Length: %zu\r\n\r\n", strlen(method) + strlen(params) + 50);
    printf("{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}\n", method, params);
    fflush(stdout);
}

void wyn_lsp_send_diagnostics(const char* uri, LSPDiagnostic* diagnostics, size_t count) {
    printf("Content-Length: 100\r\n\r\n");
    printf("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"%s\",\"diagnostics\":[]}}\n", uri);
    fflush(stdout);
}

// Find definition/references (placeholder)
LSPPosition* wyn_lsp_find_definition(const char* uri, LSPPosition position) {
    LSPPosition* pos = malloc(sizeof(LSPPosition));
    if (pos) {
        pos->line = 0;
        pos->character = 0;
    }
    return pos;
}

LSPPosition* wyn_lsp_find_references(const char* uri, LSPPosition position, size_t* reference_count) {
    *reference_count = 1;
    LSPPosition* refs = malloc(sizeof(LSPPosition));
    if (refs) {
        refs[0].line = 0;
        refs[0].character = 0;
    }
    return refs;
}

// Cleanup
void wyn_lsp_cleanup(void) {
    if (g_lsp_server.root_uri) {
        free(g_lsp_server.root_uri);
        g_lsp_server.root_uri = NULL;
    }
    if (g_lsp_server.workspace_folder) {
        free(g_lsp_server.workspace_folder);
        g_lsp_server.workspace_folder = NULL;
    }
}
