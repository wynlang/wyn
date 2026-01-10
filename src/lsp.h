#ifndef WYN_LSP_H
#define WYN_LSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynLspServer WynLspServer;
typedef struct WynLspDocument WynLspDocument;
typedef struct WynLspSymbol WynLspSymbol;
typedef struct WynLspDiagnostic WynLspDiagnostic;
typedef struct WynLspCompletion WynLspCompletion;

// LSP message types
typedef enum {
    WYN_LSP_REQUEST,
    WYN_LSP_RESPONSE,
    WYN_LSP_NOTIFICATION
} WynLspMessageType;

// LSP methods
typedef enum {
    WYN_LSP_INITIALIZE,
    WYN_LSP_INITIALIZED,
    WYN_LSP_SHUTDOWN,
    WYN_LSP_EXIT,
    WYN_LSP_TEXT_DOCUMENT_DID_OPEN,
    WYN_LSP_TEXT_DOCUMENT_DID_CHANGE,
    WYN_LSP_TEXT_DOCUMENT_DID_CLOSE,
    WYN_LSP_TEXT_DOCUMENT_HOVER,
    WYN_LSP_TEXT_DOCUMENT_COMPLETION,
    WYN_LSP_TEXT_DOCUMENT_DEFINITION,
    WYN_LSP_TEXT_DOCUMENT_REFERENCES,
    WYN_LSP_TEXT_DOCUMENT_RENAME,
    WYN_LSP_WORKSPACE_SYMBOL
} WynLspMethod;

// Position in document
typedef struct {
    uint32_t line;
    uint32_t character;
} WynLspPosition;

// Range in document
typedef struct {
    WynLspPosition start;
    WynLspPosition end;
} WynLspRange;

// Location (file + range)
typedef struct {
    char* uri;
    WynLspRange range;
} WynLspLocation;

// Symbol kinds
typedef enum {
    WYN_LSP_SYMBOL_FILE = 1,
    WYN_LSP_SYMBOL_MODULE = 2,
    WYN_LSP_SYMBOL_NAMESPACE = 3,
    WYN_LSP_SYMBOL_PACKAGE = 4,
    WYN_LSP_SYMBOL_CLASS = 5,
    WYN_LSP_SYMBOL_METHOD = 6,
    WYN_LSP_SYMBOL_PROPERTY = 7,
    WYN_LSP_SYMBOL_FIELD = 8,
    WYN_LSP_SYMBOL_CONSTRUCTOR = 9,
    WYN_LSP_SYMBOL_ENUM = 10,
    WYN_LSP_SYMBOL_INTERFACE = 11,
    WYN_LSP_SYMBOL_FUNCTION = 12,
    WYN_LSP_SYMBOL_VARIABLE = 13,
    WYN_LSP_SYMBOL_CONSTANT = 14,
    WYN_LSP_SYMBOL_STRING = 15,
    WYN_LSP_SYMBOL_NUMBER = 16,
    WYN_LSP_SYMBOL_BOOLEAN = 17,
    WYN_LSP_SYMBOL_ARRAY = 18,
    WYN_LSP_SYMBOL_OBJECT = 19,
    WYN_LSP_SYMBOL_KEY = 20,
    WYN_LSP_SYMBOL_NULL = 21,
    WYN_LSP_SYMBOL_ENUM_MEMBER = 22,
    WYN_LSP_SYMBOL_STRUCT = 23,
    WYN_LSP_SYMBOL_EVENT = 24,
    WYN_LSP_SYMBOL_OPERATOR = 25,
    WYN_LSP_SYMBOL_TYPE_PARAMETER = 26
} WynLspSymbolKind;

// Diagnostic severity
typedef enum {
    WYN_LSP_DIAGNOSTIC_ERROR = 1,
    WYN_LSP_DIAGNOSTIC_WARNING = 2,
    WYN_LSP_DIAGNOSTIC_INFORMATION = 3,
    WYN_LSP_DIAGNOSTIC_HINT = 4
} WynLspDiagnosticSeverity;

// Completion item kinds
typedef enum {
    WYN_LSP_COMPLETION_TEXT = 1,
    WYN_LSP_COMPLETION_METHOD = 2,
    WYN_LSP_COMPLETION_FUNCTION = 3,
    WYN_LSP_COMPLETION_CONSTRUCTOR = 4,
    WYN_LSP_COMPLETION_FIELD = 5,
    WYN_LSP_COMPLETION_VARIABLE = 6,
    WYN_LSP_COMPLETION_CLASS = 7,
    WYN_LSP_COMPLETION_INTERFACE = 8,
    WYN_LSP_COMPLETION_MODULE = 9,
    WYN_LSP_COMPLETION_PROPERTY = 10,
    WYN_LSP_COMPLETION_UNIT = 11,
    WYN_LSP_COMPLETION_VALUE = 12,
    WYN_LSP_COMPLETION_ENUM = 13,
    WYN_LSP_COMPLETION_KEYWORD = 14,
    WYN_LSP_COMPLETION_SNIPPET = 15,
    WYN_LSP_COMPLETION_COLOR = 16,
    WYN_LSP_COMPLETION_FILE = 17,
    WYN_LSP_COMPLETION_REFERENCE = 18,
    WYN_LSP_COMPLETION_FOLDER = 19,
    WYN_LSP_COMPLETION_ENUM_MEMBER = 20,
    WYN_LSP_COMPLETION_CONSTANT = 21,
    WYN_LSP_COMPLETION_STRUCT = 22,
    WYN_LSP_COMPLETION_EVENT = 23,
    WYN_LSP_COMPLETION_OPERATOR = 24,
    WYN_LSP_COMPLETION_TYPE_PARAMETER = 25
} WynLspCompletionItemKind;

// Symbol information
typedef struct WynLspSymbol {
    char* name;
    WynLspSymbolKind kind;
    WynLspLocation location;
    char* container_name;
    char* detail;
    char* documentation;
    struct WynLspSymbol* next;
} WynLspSymbol;

// Diagnostic information
typedef struct WynLspDiagnostic {
    WynLspRange range;
    WynLspDiagnosticSeverity severity;
    char* code;
    char* source;
    char* message;
    struct WynLspDiagnostic* next;
} WynLspDiagnostic;

// Completion item
typedef struct WynLspCompletion {
    char* label;
    WynLspCompletionItemKind kind;
    char* detail;
    char* documentation;
    char* insert_text;
    char* filter_text;
    char* sort_text;
    struct WynLspCompletion* next;
} WynLspCompletion;

// Document representation
typedef struct WynLspDocument {
    char* uri;
    char* language_id;
    int32_t version;
    char* text;
    size_t text_length;
    WynLspDiagnostic* diagnostics;
    WynLspSymbol* symbols;
    struct WynLspDocument* next;
} WynLspDocument;

// LSP server configuration
typedef struct {
    bool hover_enabled;
    bool completion_enabled;
    bool diagnostics_enabled;
    bool references_enabled;
    bool rename_enabled;
    bool workspace_symbols_enabled;
    int max_completion_items;
    int diagnostic_delay_ms;
} WynLspConfig;

// LSP server state
typedef struct WynLspServer {
    WynLspConfig config;
    WynLspDocument* documents;
    WynLspSymbol* workspace_symbols;
    bool initialized;
    bool shutdown_requested;
    char* root_uri;
    char* workspace_name;
} WynLspServer;

// Server lifecycle
WynLspServer* wyn_lsp_server_new(void);
void wyn_lsp_server_free(WynLspServer* server);
bool wyn_lsp_server_initialize(WynLspServer* server, const char* root_uri);
void wyn_lsp_server_shutdown(WynLspServer* server);

// Document management
WynLspDocument* wyn_lsp_document_open(WynLspServer* server, const char* uri, 
                                     const char* language_id, int32_t version, const char* text);
bool wyn_lsp_document_change(WynLspServer* server, const char* uri, 
                            int32_t version, const char* text);
bool wyn_lsp_document_close(WynLspServer* server, const char* uri);
WynLspDocument* wyn_lsp_find_document(WynLspServer* server, const char* uri);

// Language features
WynLspCompletion* wyn_lsp_get_completions(WynLspServer* server, const char* uri, 
                                         WynLspPosition position);
WynLspLocation* wyn_lsp_goto_definition(WynLspServer* server, const char* uri, 
                                       WynLspPosition position);
WynLspLocation** wyn_lsp_find_references(WynLspServer* server, const char* uri, 
                                        WynLspPosition position, size_t* count);
char* wyn_lsp_get_hover_info(WynLspServer* server, const char* uri, 
                             WynLspPosition position);
WynLspSymbol** wyn_lsp_get_document_symbols(WynLspServer* server, const char* uri, 
                                           size_t* count);
WynLspSymbol** wyn_lsp_get_workspace_symbols(WynLspServer* server, const char* query, 
                                            size_t* count);

// Diagnostics
void wyn_lsp_update_diagnostics(WynLspServer* server, const char* uri);
WynLspDiagnostic** wyn_lsp_get_diagnostics(WynLspServer* server, const char* uri, 
                                          size_t* count);

// Refactoring
bool wyn_lsp_rename_symbol(WynLspServer* server, const char* uri, 
                          WynLspPosition position, const char* new_name);
WynLspLocation** wyn_lsp_extract_function(WynLspServer* server, const char* uri, 
                                         WynLspRange range, const char* function_name, 
                                         size_t* count);

// Memory management
void wyn_lsp_free_symbol(WynLspSymbol* symbol);
void wyn_lsp_free_diagnostic(WynLspDiagnostic* diagnostic);
void wyn_lsp_free_completion(WynLspCompletion* completion);
void wyn_lsp_free_location(WynLspLocation* location);

// Utility functions
WynLspPosition wyn_lsp_offset_to_position(const char* text, size_t offset);
size_t wyn_lsp_position_to_offset(const char* text, WynLspPosition position);
bool wyn_lsp_position_in_range(WynLspPosition position, WynLspRange range);
WynLspRange wyn_lsp_word_range_at_position(const char* text, WynLspPosition position);

// JSON-RPC message handling (simplified)
typedef struct {
    WynLspMessageType type;
    int32_t id;
    WynLspMethod method;
    char* params;
    char* result;
    char* error;
} WynLspMessage;

WynLspMessage* wyn_lsp_parse_message(const char* json);
char* wyn_lsp_serialize_message(const WynLspMessage* message);
void wyn_lsp_free_message(WynLspMessage* message);

// Server main loop
int wyn_lsp_server_run(WynLspServer* server, int input_fd, int output_fd);
bool wyn_lsp_handle_message(WynLspServer* server, const WynLspMessage* message, 
                           WynLspMessage** response);

#endif // WYN_LSP_H
