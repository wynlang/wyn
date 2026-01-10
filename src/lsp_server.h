#ifndef WYN_LSP_SERVER_H
#define WYN_LSP_SERVER_H

#include <stddef.h>
#include <stdbool.h>

// LSP message types
typedef enum {
    LSP_INITIALIZE,
    LSP_INITIALIZED,
    LSP_SHUTDOWN,
    LSP_EXIT,
    LSP_TEXT_DOCUMENT_DID_OPEN,
    LSP_TEXT_DOCUMENT_DID_CHANGE,
    LSP_TEXT_DOCUMENT_DID_SAVE,
    LSP_TEXT_DOCUMENT_DID_CLOSE,
    LSP_TEXT_DOCUMENT_COMPLETION,
    LSP_TEXT_DOCUMENT_HOVER,
    LSP_TEXT_DOCUMENT_DEFINITION,
    LSP_TEXT_DOCUMENT_REFERENCES,
    LSP_TEXT_DOCUMENT_DIAGNOSTICS
} LSPMessageType;

// LSP position
typedef struct {
    int line;
    int character;
} LSPPosition;

// LSP range
typedef struct {
    LSPPosition start;
    LSPPosition end;
} LSPRange;

// LSP diagnostic
typedef struct {
    LSPRange range;
    char* message;
    int severity; // 1=Error, 2=Warning, 3=Info, 4=Hint
    char* source;
} LSPDiagnostic;

// LSP completion item
typedef struct {
    char* label;
    char* detail;
    char* documentation;
    int kind; // 1=Text, 2=Method, 3=Function, etc.
} LSPCompletionItem;

// LSP server state
typedef struct {
    bool initialized;
    char* root_uri;
    char* workspace_folder;
    bool shutdown_requested;
} LSPServer;

// Initialize LSP server
int wyn_lsp_init(void);

// Process LSP message
int wyn_lsp_process_message(const char* message);

// Handle specific LSP requests
int wyn_lsp_handle_initialize(const char* params);
int wyn_lsp_handle_text_document_open(const char* params);
int wyn_lsp_handle_text_document_change(const char* params);
int wyn_lsp_handle_completion(const char* params);
int wyn_lsp_handle_hover(const char* params);
int wyn_lsp_handle_definition(const char* params);
int wyn_lsp_handle_references(const char* params);

// Language analysis functions
LSPDiagnostic* wyn_lsp_analyze_document(const char* uri, const char* content, size_t* diagnostic_count);
LSPCompletionItem* wyn_lsp_get_completions(const char* uri, LSPPosition position, size_t* completion_count);
LSPPosition* wyn_lsp_find_definition(const char* uri, LSPPosition position);
LSPPosition* wyn_lsp_find_references(const char* uri, LSPPosition position, size_t* reference_count);

// Utility functions
void wyn_lsp_send_response(int id, const char* result);
void wyn_lsp_send_notification(const char* method, const char* params);
void wyn_lsp_send_diagnostics(const char* uri, LSPDiagnostic* diagnostics, size_t count);

// Cleanup
void wyn_lsp_cleanup(void);

#endif // WYN_LSP_SERVER_H
