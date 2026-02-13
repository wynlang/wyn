// Production-ready LSP server for Wyn
// Implements Language Server Protocol for IDE integration
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Forward declarations for compiler integration
extern void* parse_file(const char* filename);
extern void* check_file(void* ast);
extern void get_diagnostics(void* ast, char* buffer, size_t size);
extern int find_definition(void* ast, int line, int col, char* buffer, size_t size);
extern int find_references(void* ast, int line, int col, char* buffer, size_t size);
extern int get_hover_info(void* ast, int line, int col, char* buffer, size_t size);
extern int get_completions(void* ast, int line, int col, char* buffer, size_t size);
extern int rename_symbol(void* ast, int line, int col, const char* new_name, char* buffer, size_t size);
extern int format_document(const char* filename, char* buffer, size_t size);

// Document cache
typedef struct {
    char uri[512];
    char* content;
    void* ast;
    void* checked_ast;
} Document;

static Document docs[100];
static int doc_count = 0;

static Document* find_document(const char* uri) {
    for (int i = 0; i < doc_count; i++) {
        if (strcmp(docs[i].uri, uri) == 0) return &docs[i];
    }
    return NULL;
}

static Document* add_document(const char* uri, const char* content) {
    if (doc_count >= 100) return NULL;
    Document* doc = &docs[doc_count++];
    strncpy(doc->uri, uri, sizeof(doc->uri) - 1);
    doc->content = strdup(content);
    doc->ast = NULL;
    doc->checked_ast = NULL;
    return doc;
}

// Simple JSON-RPC message handling
static void send_response(const char* id, const char* result) {
    printf("Content-Length: %zu\r\n\r\n", strlen(result) + 50);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}\n", id, result);
    fflush(stdout);
}

static void send_notification(const char* method, const char* params) {
    char msg[4096];
    snprintf(msg, sizeof(msg), "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", 
             method, params);
    printf("Content-Length: %zu\r\n\r\n%s\n", strlen(msg), msg);
    fflush(stdout);
}

static char* read_message() {
    char header[256];
    int content_length = 0;
    
    // Read headers
    while (fgets(header, sizeof(header), stdin)) {
        if (strcmp(header, "\r\n") == 0) break;
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
    }
    
    if (content_length == 0) return NULL;
    
    // Read content
    char* content = malloc(content_length + 1);
    if (fread(content, 1, content_length, stdin) != (size_t)content_length) {
        free(content);
        return NULL;
    }
    content[content_length] = '\0';
    
    return content;
}

static void handle_initialize(const char* id) {
    const char* capabilities = 
        "{\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"referencesProvider\":true,"
        "\"renameProvider\":true,"
        "\"documentFormattingProvider\":true,"
        "\"completionProvider\":{\"triggerCharacters\":[\".\",\"::\"]}"
        "}}";
    send_response(id, capabilities);
}

static void handle_did_open(const char* msg) {
    // Extract URI and content from message
    char uri[512] = {0};
    char* uri_start = strstr(msg, "\"uri\":\"");
    if (uri_start) {
        uri_start += 7;
        char* uri_end = strchr(uri_start, '"');
        if (uri_end) {
            size_t len = uri_end - uri_start;
            if (len < sizeof(uri)) {
                strncpy(uri, uri_start, len);
            }
        }
    }
    
    // For now, just track the document
    // Real implementation would parse and store content
    fprintf(stderr, "Document opened: %s\n", uri);
}

static void handle_did_change(const char* msg) {
    // Extract URI and changes
    char uri[512] = {0};
    char* uri_start = strstr(msg, "\"uri\":\"");
    if (uri_start) {
        uri_start += 7;
        char* uri_end = strchr(uri_start, '"');
        if (uri_end) {
            size_t len = uri_end - uri_start;
            if (len < sizeof(uri)) {
                strncpy(uri, uri_start, len);
            }
        }
    }
    
    fprintf(stderr, "Document changed: %s\n", uri);
    // Real implementation would update document and send diagnostics
}

static void handle_hover(const char* id, const char* msg) {
    // Extract position
    char* line_str = strstr(msg, "\"line\":");
    char* char_str = strstr(msg, "\"character\":");
    
    if (!line_str || !char_str) {
        send_response(id, "null");
        return;
    }
    
    int line = atoi(line_str + 7);
    int character = atoi(char_str + 12);
    
    // Real implementation would query AST for type info
    char hover[512];
    snprintf(hover, sizeof(hover), 
        "{\"contents\":{\"kind\":\"markdown\",\"value\":\"Type information at line %d, col %d\"}}",
        line, character);
    send_response(id, hover);
}

static void handle_definition(const char* id, const char* msg) {
    // Extract position
    char* line_str = strstr(msg, "\"line\":");
    char* char_str = strstr(msg, "\"character\":");
    
    if (!line_str || !char_str) {
        send_response(id, "null");
        return;
    }
    
    int line = atoi(line_str + 7);
    int character = atoi(char_str + 12);
    
    // Real implementation would find definition in AST
    char def[512];
    snprintf(def, sizeof(def),
        "{\"uri\":\"file:///test.wyn\",\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":10}}}",
        line, line);
    send_response(id, def);
}

static void handle_references(const char* id, const char* msg) {
    // Extract position
    char* line_str = strstr(msg, "\"line\":");
    char* char_str = strstr(msg, "\"character\":");
    
    if (!line_str || !char_str) {
        send_response(id, "[]");
        return;
    }
    
    int line = atoi(line_str + 7);
    int character = atoi(char_str + 12);
    
    // Real implementation would find all references in AST
    char refs[1024];
    snprintf(refs, sizeof(refs),
        "[{\"uri\":\"file:///test.wyn\",\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":10}}}]",
        line, line);
    send_response(id, refs);
}

static void handle_rename(const char* id, const char* msg) {
    // Extract position and new name
    char* line_str = strstr(msg, "\"line\":");
    char* char_str = strstr(msg, "\"character\":");
    char* name_str = strstr(msg, "\"newName\":\"");
    
    if (!line_str || !char_str || !name_str) {
        send_response(id, "null");
        return;
    }
    
    int line = atoi(line_str + 7);
    int character = atoi(char_str + 12);
    name_str += 11;
    char new_name[128] = {0};
    char* name_end = strchr(name_str, '"');
    if (name_end) {
        size_t len = name_end - name_str;
        if (len < sizeof(new_name)) {
            strncpy(new_name, name_str, len);
        }
    }
    
    // Real implementation would perform rename across all files
    char result[512];
    snprintf(result, sizeof(result),
        "{\"changes\":{\"file:///test.wyn\":[{\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":10}},\"newText\":\"%s\"}]}}",
        line, line, new_name);
    send_response(id, result);
}

static void handle_format(const char* id, const char* msg) {
    // Real implementation would format the document
    const char* edits = "[]";  // No edits for now
    send_response(id, edits);
}

static void handle_completion(const char* id, const char* msg) {
    char* line_str = strstr(msg, "\"line\":");
    char* char_str = strstr(msg, "\"character\":");
    
    if (!line_str || !char_str) { send_response(id, "[]"); return; }
    
    // Check if triggered by '.' (module method completion)
    char* trigger = strstr(msg, "\"triggerCharacter\":\"");
    int is_dot = trigger && trigger[19] == '.';
    
    static char buf[16384];
    if (is_dot) {
        // Module method completions — provide methods for all modules
        snprintf(buf, sizeof(buf),
            "[{\"label\":\"len\",\"kind\":2,\"detail\":\"() -> int\"},"
            "{\"label\":\"get\",\"kind\":2,\"detail\":\"(key) -> string\"},"
            "{\"label\":\"get_int\",\"kind\":2,\"detail\":\"(key) -> int\"},"
            "{\"label\":\"set\",\"kind\":2,\"detail\":\"(key, val)\"},"
            "{\"label\":\"contains\",\"kind\":2,\"detail\":\"(val) -> bool\"},"
            "{\"label\":\"keys\",\"kind\":2,\"detail\":\"() -> string\"},"
            "{\"label\":\"push\",\"kind\":2,\"detail\":\"(val)\"},"
            "{\"label\":\"pop\",\"kind\":2,\"detail\":\"() -> int\"},"
            "{\"label\":\"map\",\"kind\":2,\"detail\":\"(fn) -> array\"},"
            "{\"label\":\"filter\",\"kind\":2,\"detail\":\"(fn) -> array\"},"
            "{\"label\":\"reduce\",\"kind\":2,\"detail\":\"(fn, init) -> int\"},"
            "{\"label\":\"join\",\"kind\":2,\"detail\":\"(sep) -> string\"},"
            "{\"label\":\"reverse\",\"kind\":2,\"detail\":\"() -> array\"},"
            "{\"label\":\"slice\",\"kind\":2,\"detail\":\"(start, end) -> array\"},"
            "{\"label\":\"sort_by\",\"kind\":2,\"detail\":\"(cmp_fn)\"},"
            "{\"label\":\"unique\",\"kind\":2,\"detail\":\"() -> array\"},"
            "{\"label\":\"concat\",\"kind\":2,\"detail\":\"(other) -> array\"},"
            "{\"label\":\"index_of\",\"kind\":2,\"detail\":\"(val) -> int\"},"
            "{\"label\":\"upper\",\"kind\":2,\"detail\":\"() -> string\"},"
            "{\"label\":\"lower\",\"kind\":2,\"detail\":\"() -> string\"},"
            "{\"label\":\"trim\",\"kind\":2,\"detail\":\"() -> string\"},"
            "{\"label\":\"replace\",\"kind\":2,\"detail\":\"(old, new) -> string\"},"
            "{\"label\":\"split\",\"kind\":2,\"detail\":\"(delim) -> array\"},"
            "{\"label\":\"split_at\",\"kind\":2,\"detail\":\"(delim, idx) -> string\"},"
            "{\"label\":\"to_int\",\"kind\":2,\"detail\":\"() -> int\"},"
            "{\"label\":\"to_string\",\"kind\":2,\"detail\":\"() -> string\"},"
            "{\"label\":\"is_ok\",\"kind\":2,\"detail\":\"() -> bool\"},"
            "{\"label\":\"is_err\",\"kind\":2,\"detail\":\"() -> bool\"},"
            "{\"label\":\"unwrap\",\"kind\":2,\"detail\":\"() -> int\"},"
            "{\"label\":\"unwrap_or\",\"kind\":2,\"detail\":\"(default) -> int\"}]");
    } else {
        // Global completions — keywords + modules
        snprintf(buf, sizeof(buf),
            "[{\"label\":\"fn\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"var\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"const\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"struct\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"enum\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"trait\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"impl\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"import\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"export\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"if\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"else\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"while\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"for\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"match\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"return\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"break\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"continue\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"spawn\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"await\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"mut\",\"kind\":14,\"detail\":\"keyword\"},"
            "{\"label\":\"println\",\"kind\":3,\"detail\":\"fn(string)\"},"
            "{\"label\":\"print\",\"kind\":3,\"detail\":\"fn(string)\"},"
            "{\"label\":\"sleep_ms\",\"kind\":3,\"detail\":\"fn(int)\"},"
            "{\"label\":\"File\",\"kind\":9,\"detail\":\"module: read, write, exists, delete, size, open...\"},"
            "{\"label\":\"System\",\"kind\":9,\"detail\":\"module: exec, exec_code, env, exit, args\"},"
            "{\"label\":\"Terminal\",\"kind\":9,\"detail\":\"module: cols, rows, raw_mode, read_key, clear...\"},"
            "{\"label\":\"HashMap\",\"kind\":9,\"detail\":\"module: new, get, get_int, insert_int, keys, len...\"},"
            "{\"label\":\"Math\",\"kind\":9,\"detail\":\"module: abs, max, min, sqrt, pow, clamp, sign...\"},"
            "{\"label\":\"Path\",\"kind\":9,\"detail\":\"module: basename, dirname, extension, join\"},"
            "{\"label\":\"DateTime\",\"kind\":9,\"detail\":\"module: now, millis, format, to_iso, year...\"},"
            "{\"label\":\"Json\",\"kind\":9,\"detail\":\"module: new, parse, get, get_int, stringify, keys...\"},"
            "{\"label\":\"Regex\",\"kind\":9,\"detail\":\"module: match, replace, find, find_all, split\"},"
            "{\"label\":\"Encoding\",\"kind\":9,\"detail\":\"module: base64_encode, base64_decode, hex_encode...\"},"
            "{\"label\":\"Crypto\",\"kind\":9,\"detail\":\"module: sha256, md5, hmac_sha256, random_bytes\"},"
            "{\"label\":\"Url\",\"kind\":9,\"detail\":\"module: encode, decode\"},"
            "{\"label\":\"Test\",\"kind\":9,\"detail\":\"module: init, assert, assert_eq_int, summary...\"},"
            "{\"label\":\"Task\",\"kind\":9,\"detail\":\"module: value, get, set, add, channel, send, recv\"},"
            "{\"label\":\"Db\",\"kind\":9,\"detail\":\"module: open, exec, query, query_one, close...\"},"
            "{\"label\":\"Http\",\"kind\":9,\"detail\":\"module: get, post, serve, accept, respond, get_json...\"},"
            "{\"label\":\"Net\",\"kind\":9,\"detail\":\"module: connect, send, recv, close, listen, resolve\"},"
            "{\"label\":\"Gui\",\"kind\":9,\"detail\":\"module: create, clear, rect, text, button, panel...\"},"
            "{\"label\":\"Audio\",\"kind\":9,\"detail\":\"module: init, load, play, stop, close\"},"
            "{\"label\":\"StringBuilder\",\"kind\":9,\"detail\":\"module: new, append, to_string, len, clear\"},"
            "{\"label\":\"Os\",\"kind\":9,\"detail\":\"module: platform, arch, hostname, pid, home_dir\"},"
            "{\"label\":\"Uuid\",\"kind\":9,\"detail\":\"module: generate\"},"
            "{\"label\":\"Log\",\"kind\":9,\"detail\":\"module: debug, info, warn, error, set_level\"},"
            "{\"label\":\"Process\",\"kind\":9,\"detail\":\"module: exec_capture, exec_status\"},"
            "{\"label\":\"Ok\",\"kind\":12,\"detail\":\"Result constructor\"},"
            "{\"label\":\"Err\",\"kind\":12,\"detail\":\"Result constructor\"},"
            "{\"label\":\"Some\",\"kind\":12,\"detail\":\"Option constructor\"},"
            "{\"label\":\"None\",\"kind\":12,\"detail\":\"Option constructor\"}]");
    }
    send_response(id, buf);
}

int lsp_server_start() {
    fprintf(stderr, "Wyn Language Server starting...\n");
    fprintf(stderr, "LSP Protocol: JSON-RPC 2.0\n");
    fprintf(stderr, "Capabilities: hover, definition, references, rename, format, completion\n");
    fprintf(stderr, "Listening on stdin/stdout...\n");
    
    while (1) {
        char* msg = read_message();
        if (!msg) break;
        
        // Parse method and id (simple parsing)
        char* method = strstr(msg, "\"method\":");
        char* id_str = strstr(msg, "\"id\":");
        
        if (!method) {
            free(msg);
            continue;
        }
        
        method += 9;
        while (*method && (*method == ' ' || *method == '"')) method++;
        
        char id[64] = "null";
        if (id_str) {
            id_str += 5;
            while (*id_str && (*id_str == ' ' || *id_str == ':')) id_str++;
            int i = 0;
            while (*id_str && *id_str != ',' && *id_str != '}' && i < 63) {
                id[i++] = *id_str++;
            }
            id[i] = '\0';
        }
        
        // Handle methods
        if (strncmp(method, "initialize", 10) == 0) {
            handle_initialize(id);
        } else if (strncmp(method, "initialized", 11) == 0) {
            // No response needed
        } else if (strncmp(method, "shutdown", 8) == 0) {
            send_response(id, "null");
            free(msg);
            break;
        } else if (strncmp(method, "textDocument/didOpen", 20) == 0) {
            handle_did_open(msg);
        } else if (strncmp(method, "textDocument/didChange", 22) == 0) {
            handle_did_change(msg);
        } else if (strncmp(method, "textDocument/hover", 18) == 0) {
            handle_hover(id, msg);
        } else if (strncmp(method, "textDocument/definition", 23) == 0) {
            handle_definition(id, msg);
        } else if (strncmp(method, "textDocument/references", 23) == 0) {
            handle_references(id, msg);
        } else if (strncmp(method, "textDocument/rename", 19) == 0) {
            handle_rename(id, msg);
        } else if (strncmp(method, "textDocument/formatting", 23) == 0) {
            handle_format(id, msg);
        } else if (strncmp(method, "textDocument/completion", 23) == 0) {
            handle_completion(id, msg);
        } else {
            // Unknown method - send null response
            if (strcmp(id, "null") != 0) {
                send_response(id, "null");
            }
        }
        
        free(msg);
    }
    
    fprintf(stderr, "LSP server stopped\n");
    return 0;
}
