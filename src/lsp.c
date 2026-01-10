#include "lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Server lifecycle
WynLspServer* wyn_lsp_server_new(void) {
    WynLspServer* server = malloc(sizeof(WynLspServer));
    if (!server) return NULL;
    
    memset(server, 0, sizeof(WynLspServer));
    
    // Set default configuration
    server->config.hover_enabled = true;
    server->config.completion_enabled = true;
    server->config.diagnostics_enabled = true;
    server->config.references_enabled = true;
    server->config.rename_enabled = true;
    server->config.workspace_symbols_enabled = true;
    server->config.max_completion_items = 100;
    server->config.diagnostic_delay_ms = 500;
    
    server->initialized = false;
    server->shutdown_requested = false;
    
    return server;
}

void wyn_lsp_server_free(WynLspServer* server) {
    if (!server) return;
    
    // Free documents
    WynLspDocument* doc = server->documents;
    while (doc) {
        WynLspDocument* next = doc->next;
        free(doc->uri);
        free(doc->language_id);
        free(doc->text);
        
        // Free diagnostics
        WynLspDiagnostic* diag = doc->diagnostics;
        while (diag) {
            WynLspDiagnostic* next_diag = diag->next;
            wyn_lsp_free_diagnostic(diag);
            diag = next_diag;
        }
        
        // Free symbols
        WynLspSymbol* sym = doc->symbols;
        while (sym) {
            WynLspSymbol* next_sym = sym->next;
            wyn_lsp_free_symbol(sym);
            sym = next_sym;
        }
        
        free(doc);
        doc = next;
    }
    
    // Free workspace symbols
    WynLspSymbol* sym = server->workspace_symbols;
    while (sym) {
        WynLspSymbol* next = sym->next;
        wyn_lsp_free_symbol(sym);
        sym = next;
    }
    
    free(server->root_uri);
    free(server->workspace_name);
    free(server);
}

bool wyn_lsp_server_initialize(WynLspServer* server, const char* root_uri) {
    if (!server) return false;
    
    server->root_uri = root_uri ? strdup(root_uri) : NULL;
    server->workspace_name = strdup("Wyn Workspace");
    server->initialized = true;
    
    return true;
}

void wyn_lsp_server_shutdown(WynLspServer* server) {
    if (!server) return;
    server->shutdown_requested = true;
}

// Document management
WynLspDocument* wyn_lsp_document_open(WynLspServer* server, const char* uri, 
                                     const char* language_id, int32_t version, const char* text) {
    if (!server || !uri || !text) return NULL;
    
    // Check if document already exists
    WynLspDocument* existing = wyn_lsp_find_document(server, uri);
    if (existing) {
        // Update existing document
        free(existing->text);
        existing->text = strdup(text);
        existing->text_length = strlen(text);
        existing->version = version;
        return existing;
    }
    
    // Create new document
    WynLspDocument* doc = malloc(sizeof(WynLspDocument));
    if (!doc) return NULL;
    
    doc->uri = strdup(uri);
    doc->language_id = language_id ? strdup(language_id) : strdup("wyn");
    doc->version = version;
    doc->text = strdup(text);
    doc->text_length = strlen(text);
    doc->diagnostics = NULL;
    doc->symbols = NULL;
    doc->next = server->documents;
    
    server->documents = doc;
    
    // Update diagnostics
    wyn_lsp_update_diagnostics(server, uri);
    
    return doc;
}

bool wyn_lsp_document_change(WynLspServer* server, const char* uri, 
                            int32_t version, const char* text) {
    if (!server || !uri || !text) return false;
    
    WynLspDocument* doc = wyn_lsp_find_document(server, uri);
    if (!doc) return false;
    
    // Update document content
    free(doc->text);
    doc->text = strdup(text);
    doc->text_length = strlen(text);
    doc->version = version;
    
    // Update diagnostics
    wyn_lsp_update_diagnostics(server, uri);
    
    return true;
}

bool wyn_lsp_document_close(WynLspServer* server, const char* uri) {
    if (!server || !uri) return false;
    
    WynLspDocument** current = &server->documents;
    while (*current) {
        if (strcmp((*current)->uri, uri) == 0) {
            WynLspDocument* to_remove = *current;
            *current = (*current)->next;
            
            free(to_remove->uri);
            free(to_remove->language_id);
            free(to_remove->text);
            free(to_remove);
            
            return true;
        }
        current = &(*current)->next;
    }
    
    return false;
}

WynLspDocument* wyn_lsp_find_document(WynLspServer* server, const char* uri) {
    if (!server || !uri) return NULL;
    
    WynLspDocument* doc = server->documents;
    while (doc) {
        if (strcmp(doc->uri, uri) == 0) {
            return doc;
        }
        doc = doc->next;
    }
    
    return NULL;
}

// Language features (simplified implementations)
WynLspCompletion* wyn_lsp_get_completions(WynLspServer* server, const char* uri, 
                                         WynLspPosition position) {
    (void)server; (void)uri; (void)position;
    
    // Simplified completion - return basic keywords
    WynLspCompletion* completions = NULL;
    WynLspCompletion* current = NULL;
    
    const char* keywords[] = {"fn", "struct", "impl", "let", "var", "if", "else", "while", "for", "match"};
    size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);
    
    for (size_t i = 0; i < keyword_count; i++) {
        WynLspCompletion* completion = malloc(sizeof(WynLspCompletion));
        if (!completion) continue;
        
        memset(completion, 0, sizeof(WynLspCompletion));
        completion->label = strdup(keywords[i]);
        completion->kind = WYN_LSP_COMPLETION_KEYWORD;
        completion->detail = strdup("Wyn keyword");
        completion->insert_text = strdup(keywords[i]);
        completion->next = NULL;
        
        if (!completions) {
            completions = completion;
            current = completion;
        } else {
            current->next = completion;
            current = completion;
        }
    }
    
    return completions;
}

WynLspLocation* wyn_lsp_goto_definition(WynLspServer* server, const char* uri, 
                                       WynLspPosition position) {
    (void)server; (void)uri; (void)position;
    
    // Stub implementation
    return NULL;
}

WynLspLocation** wyn_lsp_find_references(WynLspServer* server, const char* uri, 
                                        WynLspPosition position, size_t* count) {
    (void)server; (void)uri; (void)position;
    
    if (count) *count = 0;
    return NULL;
}

char* wyn_lsp_get_hover_info(WynLspServer* server, const char* uri, 
                             WynLspPosition position) {
    (void)server; (void)uri; (void)position;
    
    // Simplified hover - return basic info
    return strdup("Wyn language element");
}

// Diagnostics
void wyn_lsp_update_diagnostics(WynLspServer* server, const char* uri) {
    if (!server || !uri) return;
    
    WynLspDocument* doc = wyn_lsp_find_document(server, uri);
    if (!doc) return;
    
    // Clear existing diagnostics
    WynLspDiagnostic* diag = doc->diagnostics;
    while (diag) {
        WynLspDiagnostic* next = diag->next;
        wyn_lsp_free_diagnostic(diag);
        diag = next;
    }
    doc->diagnostics = NULL;
    
    // Simple diagnostic - check for basic syntax issues
    if (strstr(doc->text, "TODO")) {
        WynLspDiagnostic* todo_diag = malloc(sizeof(WynLspDiagnostic));
        if (todo_diag) {
            memset(todo_diag, 0, sizeof(WynLspDiagnostic));
            todo_diag->range.start.line = 0;
            todo_diag->range.start.character = 0;
            todo_diag->range.end.line = 0;
            todo_diag->range.end.character = 4;
            todo_diag->severity = WYN_LSP_DIAGNOSTIC_INFORMATION;
            todo_diag->message = strdup("TODO comment found");
            todo_diag->source = strdup("wyn-lsp");
            todo_diag->next = NULL;
            
            doc->diagnostics = todo_diag;
        }
    }
}

WynLspDiagnostic** wyn_lsp_get_diagnostics(WynLspServer* server, const char* uri, 
                                          size_t* count) {
    if (!server || !uri || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    WynLspDocument* doc = wyn_lsp_find_document(server, uri);
    if (!doc) {
        *count = 0;
        return NULL;
    }
    
    // Count diagnostics
    size_t diag_count = 0;
    WynLspDiagnostic* diag = doc->diagnostics;
    while (diag) {
        diag_count++;
        diag = diag->next;
    }
    
    if (diag_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Create array
    WynLspDiagnostic** diagnostics = malloc(diag_count * sizeof(WynLspDiagnostic*));
    if (!diagnostics) {
        *count = 0;
        return NULL;
    }
    
    diag = doc->diagnostics;
    for (size_t i = 0; i < diag_count; i++) {
        diagnostics[i] = diag;
        diag = diag->next;
    }
    
    *count = diag_count;
    return diagnostics;
}

// Memory management
void wyn_lsp_free_symbol(WynLspSymbol* symbol) {
    if (!symbol) return;
    free(symbol->name);
    free(symbol->location.uri);
    free(symbol->container_name);
    free(symbol->detail);
    free(symbol->documentation);
    free(symbol);
}

void wyn_lsp_free_diagnostic(WynLspDiagnostic* diagnostic) {
    if (!diagnostic) return;
    free(diagnostic->code);
    free(diagnostic->source);
    free(diagnostic->message);
    free(diagnostic);
}

void wyn_lsp_free_completion(WynLspCompletion* completion) {
    if (!completion) return;
    free(completion->label);
    free(completion->detail);
    free(completion->documentation);
    free(completion->insert_text);
    free(completion->filter_text);
    free(completion->sort_text);
    free(completion);
}

void wyn_lsp_free_location(WynLspLocation* location) {
    if (!location) return;
    free(location->uri);
    free(location);
}

// Utility functions
WynLspPosition wyn_lsp_offset_to_position(const char* text, size_t offset) {
    WynLspPosition pos = {0, 0};
    if (!text) return pos;
    
    for (size_t i = 0; i < offset && text[i]; i++) {
        if (text[i] == '\n') {
            pos.line++;
            pos.character = 0;
        } else {
            pos.character++;
        }
    }
    
    return pos;
}

size_t wyn_lsp_position_to_offset(const char* text, WynLspPosition position) {
    if (!text) return 0;
    
    size_t offset = 0;
    uint32_t current_line = 0;
    uint32_t current_char = 0;
    
    while (text[offset] && (current_line < position.line || 
                           (current_line == position.line && current_char < position.character))) {
        if (text[offset] == '\n') {
            current_line++;
            current_char = 0;
        } else {
            current_char++;
        }
        offset++;
    }
    
    return offset;
}

bool wyn_lsp_position_in_range(WynLspPosition position, WynLspRange range) {
    if (position.line < range.start.line || position.line > range.end.line) {
        return false;
    }
    
    if (position.line == range.start.line && position.character < range.start.character) {
        return false;
    }
    
    if (position.line == range.end.line && position.character > range.end.character) {
        return false;
    }
    
    return true;
}

WynLspRange wyn_lsp_word_range_at_position(const char* text, WynLspPosition position) {
    WynLspRange range = {{0, 0}, {0, 0}};
    if (!text) return range;
    
    size_t offset = wyn_lsp_position_to_offset(text, position);
    size_t text_len = strlen(text);
    
    // Find word boundaries
    size_t start = offset;
    while (start > 0 && (isalnum(text[start - 1]) || text[start - 1] == '_')) {
        start--;
    }
    
    size_t end = offset;
    while (end < text_len && (isalnum(text[end]) || text[end] == '_')) {
        end++;
    }
    
    range.start = wyn_lsp_offset_to_position(text, start);
    range.end = wyn_lsp_offset_to_position(text, end);
    
    return range;
}

// Simplified message handling (stubs)
WynLspMessage* wyn_lsp_parse_message(const char* json) {
    (void)json;
    // Stub - would implement JSON-RPC parsing
    return NULL;
}

char* wyn_lsp_serialize_message(const WynLspMessage* message) {
    (void)message;
    // Stub - would implement JSON-RPC serialization
    return NULL;
}

void wyn_lsp_free_message(WynLspMessage* message) {
    if (!message) return;
    free(message->params);
    free(message->result);
    free(message->error);
    free(message);
}

// Server main loop (simplified)
int wyn_lsp_server_run(WynLspServer* server, int input_fd, int output_fd) {
    (void)server; (void)input_fd; (void)output_fd;
    
    // Stub - would implement main LSP message loop
    printf("LSP server would run here with input_fd=%d, output_fd=%d\n", input_fd, output_fd);
    return 0;
}

bool wyn_lsp_handle_message(WynLspServer* server, const WynLspMessage* message, 
                           WynLspMessage** response) {
    (void)server; (void)message; (void)response;
    
    // Stub - would implement message handling
    return false;
}

// Stub implementations for unimplemented features
WynLspSymbol** wyn_lsp_get_document_symbols(WynLspServer* server, const char* uri, 
                                           size_t* count) {
    (void)server; (void)uri;
    if (count) *count = 0;
    return NULL;
}

WynLspSymbol** wyn_lsp_get_workspace_symbols(WynLspServer* server, const char* query, 
                                            size_t* count) {
    (void)server; (void)query;
    if (count) *count = 0;
    return NULL;
}

bool wyn_lsp_rename_symbol(WynLspServer* server, const char* uri, 
                          WynLspPosition position, const char* new_name) {
    (void)server; (void)uri; (void)position; (void)new_name;
    return false;
}

WynLspLocation** wyn_lsp_extract_function(WynLspServer* server, const char* uri, 
                                         WynLspRange range, const char* function_name, 
                                         size_t* count) {
    (void)server; (void)uri; (void)range; (void)function_name;
    if (count) *count = 0;
    return NULL;
}
