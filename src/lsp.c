// Wyn LSP Server — real compiler integration
// Provides diagnostics, hover, go-to-definition, completions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>

// ── JSON helpers ──────────────────────────────────────────────

static void lsp_send(const char* json) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(json), json);
    fflush(stdout);
}

static void lsp_respond(const char* id, const char* result) {
    char buf[65536];
    snprintf(buf, sizeof(buf), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id, result);
    lsp_send(buf);
}

static void lsp_notify(const char* method, const char* params) {
    char buf[65536];
    snprintf(buf, sizeof(buf), "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, params);
    lsp_send(buf);
}

static char* lsp_read_message(void) {
    char header[256];
    int content_length = 0;
    while (fgets(header, sizeof(header), stdin)) {
        if (strcmp(header, "\r\n") == 0) break;
        if (strncmp(header, "Content-Length:", 15) == 0)
            content_length = atoi(header + 15);
    }
    if (content_length == 0) return NULL;
    char* content = malloc(content_length + 1);
    if ((int)fread(content, 1, content_length, stdin) != content_length) { free(content); return NULL; }
    content[content_length] = '\0';
    return content;
}

// Simple JSON field extraction (no full parser needed)
static int json_get_int(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ') p++;
    return atoi(p);
}

static int json_get_string(const char* json, const char* key, char* out, int max) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    char* p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    int i = 0;
    while (*p && *p != '"' && i < max - 1) out[i++] = *p++;
    out[i] = '\0';
    return i;
}

// ── Document store ───────────────────────────────────────────

typedef struct { char uri[512]; char path[512]; char* content; } LspDoc;
static LspDoc lsp_docs[256];
static int lsp_doc_count = 0;

static void uri_to_path(const char* uri, char* path, int max) {
    if (strncmp(uri, "file://", 7) == 0) {
        // URL decode %XX sequences
        const char* src = uri + 7;
        int i = 0;
        while (*src && i < max - 1) {
            if (*src == '%' && src[1] && src[2]) {
                char hex[3] = { src[1], src[2], 0 };
                path[i++] = (char)strtol(hex, NULL, 16);
                src += 3;
            } else {
                path[i++] = *src++;
            }
        }
        path[i] = '\0';
    } else {
        strncpy(path, uri, max - 1);
    }
}

static LspDoc* doc_find(const char* uri) {
    for (int i = 0; i < lsp_doc_count; i++)
        if (strcmp(lsp_docs[i].uri, uri) == 0) return &lsp_docs[i];
    return NULL;
}

static LspDoc* doc_open(const char* uri, const char* content) {
    LspDoc* d = doc_find(uri);
    if (!d) { if (lsp_doc_count >= 256) return NULL; d = &lsp_docs[lsp_doc_count++]; }
    else { free(d->content); }
    strncpy(d->uri, uri, sizeof(d->uri) - 1);
    uri_to_path(uri, d->path, sizeof(d->path));
    d->content = strdup(content);
    return d;
}

static void doc_update(const char* uri, const char* content) {
    LspDoc* d = doc_find(uri);
    if (d) { free(d->content); d->content = strdup(content); }
}

// ── Diagnostics via compiler ─────────────────────────────────

static char wyn_binary[1024] = "";

static void publish_diagnostics(const char* uri, const char* path) {
    // Write content to temp file and run compiler check
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "/tmp/wyn_lsp_%d.wyn", getpid());
    
    LspDoc* d = doc_find(uri);
    if (d && d->content) {
        FILE* f = fopen(tmp, "w");
        if (f) { fputs(d->content, f); fclose(f); }
    } else {
        // Use the actual file
        snprintf(tmp, sizeof(tmp), "%s", path);
    }
    
    // Run compiler in check mode (parse + typecheck, capture errors)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s run %s 2>&1; true", wyn_binary, tmp);
    
    FILE* fp = popen(cmd, "r");
    char output[8192] = "";
    if (fp) { fread(output, 1, sizeof(output) - 1, fp); pclose(fp); }
    
    // Parse errors from output: "Error at line N: message" or "--> file:N:C"
    char diags[32768];
    int pos = 0;
    pos += snprintf(diags + pos, sizeof(diags) - pos,
        "{\"uri\":\"%s\",\"diagnostics\":[", uri);
    
    int diag_count = 0;
    char* line = output;
    while (line && *line) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        
        int err_line = -1;
        int err_col = 0;
        char* msg_start = NULL;
        int severity = 1; // 1=error, 2=warning
        
        // Pattern: "Error at line N: msg"
        char* at_line = strstr(line, "Error at line ");
        if (at_line) {
            err_line = atoi(at_line + 14) - 1; // LSP is 0-indexed
            msg_start = strchr(at_line + 14, ':');
            if (msg_start) msg_start += 2;
            else msg_start = at_line;
        }
        // Pattern: "--> file:LINE:COL"
        if (!at_line && strstr(line, "-->")) {
            char* colon1 = strrchr(line, ':');
            if (colon1) {
                err_col = atoi(colon1 + 1);
                *colon1 = '\0';
                char* colon2 = strrchr(line, ':');
                if (colon2) err_line = atoi(colon2 + 1) - 1;
            }
        }
        // Pattern: "Warning: msg (line N)"
        if (strncmp(line, "Warning:", 8) == 0) {
            severity = 2;
            msg_start = line + 9;
            char* paren = strstr(line, "(line ");
            if (paren) err_line = atoi(paren + 6) - 1;
        }
        
        if (err_line >= 0) {
            if (err_col < 0) err_col = 0;
            if (!msg_start) msg_start = line;
            // Escape quotes in message
            char escaped[512];
            int ei = 0;
            for (char* c = msg_start; *c && ei < 500; c++) {
                if (*c == '"' || *c == '\\') escaped[ei++] = '\\';
                if (*c != '\n' && *c != '\r') escaped[ei++] = *c;
            }
            escaped[ei] = '\0';
            
            if (diag_count > 0) pos += snprintf(diags + pos, sizeof(diags) - pos, ",");
            pos += snprintf(diags + pos, sizeof(diags) - pos,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"severity\":%d,\"source\":\"wyn\","
                "\"message\":\"%s\"}",
                err_line, err_col, err_line, err_col + 1, severity, escaped);
            diag_count++;
        }
        
        line = nl ? nl + 1 : NULL;
    }
    
    pos += snprintf(diags + pos, sizeof(diags) - pos, "]}");
    lsp_notify("textDocument/publishDiagnostics", diags);
    
    // Clean up temp file
    if (strncmp(tmp, "/tmp/", 5) == 0) unlink(tmp);
}

// ── Hover: find word at position and provide type info ───────

static void get_word_at(const char* content, int line, int col, char* word, int max) {
    word[0] = '\0';
    const char* p = content;
    int cur_line = 0;
    while (p && *p && cur_line < line) { if (*p == '\n') cur_line++; p++; }
    if (cur_line != line) return;
    // Move to column
    for (int i = 0; i < col && *p && *p != '\n'; i++) p++;
    // Find word boundaries
    const char* start = p;
    while (start > content && (isalnum(*(start-1)) || *(start-1) == '_')) start--;
    const char* end = p;
    while (*end && (isalnum(*end) || *end == '_')) end++;
    int len = end - start;
    if (len > 0 && len < max) { memcpy(word, start, len); word[len] = '\0'; }
}

// ── Completions ──────────────────────────────────────────────

static const char* KEYWORD_COMPLETIONS =
    "[{\"label\":\"fn\",\"kind\":14},{\"label\":\"var\",\"kind\":14},"
    "{\"label\":\"const\",\"kind\":14},{\"label\":\"struct\",\"kind\":14},"
    "{\"label\":\"enum\",\"kind\":14},{\"label\":\"impl\",\"kind\":14},"
    "{\"label\":\"import\",\"kind\":14},{\"label\":\"export\",\"kind\":14},"
    "{\"label\":\"if\",\"kind\":14},{\"label\":\"else\",\"kind\":14},"
    "{\"label\":\"while\",\"kind\":14},{\"label\":\"for\",\"kind\":14},"
    "{\"label\":\"match\",\"kind\":14},{\"label\":\"return\",\"kind\":14},"
    "{\"label\":\"spawn\",\"kind\":14},{\"label\":\"await\",\"kind\":14},"
    "{\"label\":\"println\",\"kind\":3,\"detail\":\"fn(string)\"},"
    "{\"label\":\"print\",\"kind\":3,\"detail\":\"fn(string)\"},"
    "{\"label\":\"File\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"System\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"HashMap\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Math\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Http\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Json\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"DateTime\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Terminal\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Random\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Crypto\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Path\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Regex\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Color\",\"kind\":9,\"detail\":\"module\"},"
    "{\"label\":\"Ok\",\"kind\":12,\"detail\":\"Result constructor\"},"
    "{\"label\":\"Err\",\"kind\":12,\"detail\":\"Result constructor\"},"
    "{\"label\":\"Some\",\"kind\":12,\"detail\":\"Option constructor\"},"
    "{\"label\":\"None\",\"kind\":12,\"detail\":\"Option constructor\"}]";

static const char* METHOD_COMPLETIONS =
    "[{\"label\":\"len\",\"kind\":2,\"detail\":\"() -> int\"},"
    "{\"label\":\"contains\",\"kind\":2,\"detail\":\"(val) -> bool\"},"
    "{\"label\":\"push\",\"kind\":2,\"detail\":\"(val)\"},"
    "{\"label\":\"pop\",\"kind\":2,\"detail\":\"() -> int\"},"
    "{\"label\":\"map\",\"kind\":2,\"detail\":\"(fn) -> array\"},"
    "{\"label\":\"filter\",\"kind\":2,\"detail\":\"(fn) -> array\"},"
    "{\"label\":\"join\",\"kind\":2,\"detail\":\"(sep) -> string\"},"
    "{\"label\":\"split\",\"kind\":2,\"detail\":\"(delim) -> array\"},"
    "{\"label\":\"trim\",\"kind\":2,\"detail\":\"() -> string\"},"
    "{\"label\":\"upper\",\"kind\":2,\"detail\":\"() -> string\"},"
    "{\"label\":\"lower\",\"kind\":2,\"detail\":\"() -> string\"},"
    "{\"label\":\"replace\",\"kind\":2,\"detail\":\"(old, new) -> string\"},"
    "{\"label\":\"to_int\",\"kind\":2,\"detail\":\"() -> int\"},"
    "{\"label\":\"to_string\",\"kind\":2,\"detail\":\"() -> string\"},"
    "{\"label\":\"is_ok\",\"kind\":2,\"detail\":\"() -> bool\"},"
    "{\"label\":\"is_err\",\"kind\":2,\"detail\":\"() -> bool\"},"
    "{\"label\":\"unwrap\",\"kind\":2,\"detail\":\"() -> T\"},"
    "{\"label\":\"unwrap_or\",\"kind\":2,\"detail\":\"(default) -> T\"},"
    "{\"label\":\"get\",\"kind\":2,\"detail\":\"(key) -> string\"},"
    "{\"label\":\"set\",\"kind\":2,\"detail\":\"(key, val)\"},"
    "{\"label\":\"keys\",\"kind\":2,\"detail\":\"() -> array\"},"
    "{\"label\":\"index_of\",\"kind\":2,\"detail\":\"(val) -> int\"},"
    "{\"label\":\"substring\",\"kind\":2,\"detail\":\"(start, end) -> string\"},"
    "{\"label\":\"starts_with\",\"kind\":2,\"detail\":\"(prefix) -> bool\"},"
    "{\"label\":\"ends_with\",\"kind\":2,\"detail\":\"(suffix) -> bool\"}]";

// ── Find references helpers ──────────────────────────────────

static int is_word_char_lsp(char c) { return isalnum(c) || c == '_'; }
typedef struct { int line; int col; int len; } WordMatch;

static int find_all_refs(const char* content, const char* word, WordMatch* m, int max) {
    int count = 0, wlen = strlen(word); if (!wlen) return 0;
    int line = 0, col = 0; bool in_str = false, in_interp = false, in_cmt = false; int idepth = 0;
    for (const char* p = content; *p; p++) {
        if (!in_str && !in_cmt && *p == '/' && *(p+1) == '/') in_cmt = true;
        if (in_cmt && *p == '\n') in_cmt = false;
        if (!in_cmt) {
            if (in_str && !in_interp && *p == '$' && *(p+1) == '{') { in_interp = true; idepth = 1; p++; col++; if (*p=='\n'){line++;col=0;}else col++; continue; }
            if (in_interp) { if (*p=='{') idepth++; else if (*p=='}'){idepth--;if(!idepth){in_interp=false;if(*p=='\n'){line++;col=0;}else col++;continue;}} }
            else if (*p == '"' && (p == content || *(p-1) != '\\')) in_str = !in_str;
        }
        if ((!in_str || in_interp) && !in_cmt && strncmp(p, word, wlen) == 0 &&
            (p == content || !is_word_char_lsp(*(p-1))) && !is_word_char_lsp(*(p+wlen)) && count < max) {
            m[count].line = line; m[count].col = col; m[count].len = wlen; count++;
        }
        if (*p == '\n') { line++; col = 0; } else col++;
    }
    return count;
}

static void find_scope(const char* c, int tgt, int* s, int* e) {
    *s = -1; *e = -1; int line = 0, depth = 0, fss[64], fds[64], fsp = 0; bool ins = false;
    for (const char* p = c; *p; p++) {
        if (*p == '"' && (p == c || *(p-1) != '\\')) ins = !ins;
        if (ins) { if (*p == '\n') line++; continue; }
        if (*p == '/' && *(p+1) == '/') { while (*p && *p != '\n') p++; if (*p) line++; continue; }
        if (*p == '{') { const char* ls = p; while (ls > c && *(ls-1) != '\n') ls--;
            if (strstr(ls, "fn ") && strstr(ls, "fn ") < p && fsp < 64) { fss[fsp] = line; fds[fsp] = depth; fsp++; }
            depth++; }
        if (*p == '}') { depth--; if (fsp > 0 && fds[fsp-1] == depth) {
            int fs = fss[fsp-1], fe = line; fsp--; if (tgt >= fs && tgt <= fe) { *s = fs; *e = fe; } } }
        if (*p == '\n') line++;
    }
}

static bool is_local_sym(const char* c, const char* w, int ss, int se) {
    int line = 0; char vp[140]; snprintf(vp, sizeof(vp), "var %s", w); int vl = strlen(vp);
    for (const char* p = c; *p; p++) {
        if (line >= ss && line <= se) {
            if (strncmp(p, vp, vl) == 0 && !is_word_char_lsp(*(p+vl))) return true;
            char pp[140]; snprintf(pp, sizeof(pp), "%s:", w); int pl = strlen(pp);
            if (strncmp(p, pp, pl) == 0) { const char* b = p-1; while (b > c && *b == ' ') b--; if (*b == '(' || *b == ',') return true; }
        }
        if (*p == '\n') line++;
    }
    return false;
}

// ── Main LSP loop ────────────────────────────────────────────

int lsp_server_start(void) {
    // Find our own binary path for running compiler checks
    char* path = getenv("_");
    if (path && strstr(path, "wyn")) {
        strncpy(wyn_binary, path, sizeof(wyn_binary) - 1);
    } else {
        strcpy(wyn_binary, "./wyn");
    }
    
    fprintf(stderr, "Wyn LSP Server v1.9.0\n");
    fprintf(stderr, "Binary: %s\n", wyn_binary);
    
    while (1) {
        char* msg = lsp_read_message();
        if (!msg) break;
        
        // Extract method and id
        char method[128] = "";
        char* m = strstr(msg, "\"method\":\"");
        if (m) { m += 10; int i = 0; while (*m && *m != '"' && i < 127) method[i++] = *m++; method[i] = '\0'; }
        
        char id[64] = "null";
        char* id_p = strstr(msg, "\"id\":");
        if (id_p) {
            id_p += 5; while (*id_p == ' ') id_p++;
            int i = 0;
            while (*id_p && *id_p != ',' && *id_p != '}' && i < 63) id[i++] = *id_p++;
            id[i] = '\0';
        }
        
        fprintf(stderr, "LSP: %s\n", method);
        
        if (strcmp(method, "initialize") == 0) {
            lsp_respond(id,
                "{\"capabilities\":{"
                "\"textDocumentSync\":{\"openClose\":true,\"change\":1},"
                "\"hoverProvider\":true,"
                "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
                "\"definitionProvider\":true,"
                "\"referencesProvider\":true,"
                "\"renameProvider\":true"
                "},\"serverInfo\":{\"name\":\"wyn-lsp\",\"version\":\"1.9.0\"}}");
        }
        else if (strcmp(method, "initialized") == 0) { /* noop */ }
        else if (strcmp(method, "shutdown") == 0) {
            lsp_respond(id, "null");
            free(msg); break;
        }
        else if (strcmp(method, "textDocument/didOpen") == 0) {
            char uri[512] = "";
            json_get_string(msg, "uri", uri, sizeof(uri));
            // Extract text from params.textDocument.text
            char* text_start = strstr(msg, "\"text\":\"");
            if (text_start && uri[0]) {
                text_start += 8;
                // Unescape JSON string (basic: \n, \t, \\, \")
                int len = strlen(text_start);
                char* text = malloc(len + 1);
                int ti = 0;
                for (int i = 0; i < len; i++) {
                    if (text_start[i] == '"' && (i == 0 || text_start[i-1] != '\\')) break;
                    if (text_start[i] == '\\' && i + 1 < len) {
                        i++;
                        if (text_start[i] == 'n') text[ti++] = '\n';
                        else if (text_start[i] == 't') text[ti++] = '\t';
                        else if (text_start[i] == '\\') text[ti++] = '\\';
                        else if (text_start[i] == '"') text[ti++] = '"';
                        else { text[ti++] = '\\'; text[ti++] = text_start[i]; }
                    } else {
                        text[ti++] = text_start[i];
                    }
                }
                text[ti] = '\0';
                LspDoc* d = doc_open(uri, text);
                free(text);
                if (d) publish_diagnostics(uri, d->path);
            }
        }
        else if (strcmp(method, "textDocument/didChange") == 0) {
            char uri[512] = "";
            json_get_string(msg, "uri", uri, sizeof(uri));
            // Full sync: extract new text
            char* text_start = strstr(msg, "\"text\":\"");
            if (text_start && uri[0]) {
                text_start += 8;
                int len = strlen(text_start);
                char* text = malloc(len + 1);
                int ti = 0;
                for (int i = 0; i < len; i++) {
                    if (text_start[i] == '"' && (i == 0 || text_start[i-1] != '\\')) break;
                    if (text_start[i] == '\\' && i + 1 < len) {
                        i++;
                        if (text_start[i] == 'n') text[ti++] = '\n';
                        else if (text_start[i] == 't') text[ti++] = '\t';
                        else if (text_start[i] == '\\') text[ti++] = '\\';
                        else if (text_start[i] == '"') text[ti++] = '"';
                        else { text[ti++] = '\\'; text[ti++] = text_start[i]; }
                    } else {
                        text[ti++] = text_start[i];
                    }
                }
                text[ti] = '\0';
                doc_update(uri, text);
                free(text);
                LspDoc* d = doc_find(uri);
                if (d) publish_diagnostics(uri, d->path);
            }
        }
        else if (strcmp(method, "textDocument/didClose") == 0) {
            char uri[512] = "";
            json_get_string(msg, "uri", uri, sizeof(uri));
            // Clear diagnostics
            char clear[1024];
            snprintf(clear, sizeof(clear), "{\"uri\":\"%s\",\"diagnostics\":[]}", uri);
            lsp_notify("textDocument/publishDiagnostics", clear);
        }
        else if (strcmp(method, "textDocument/hover") == 0) {
            char uri[512] = "";
            json_get_string(msg, "uri", uri, sizeof(uri));
            int line = json_get_int(msg, "line");
            int col = json_get_int(msg, "character");
            
            LspDoc* d = doc_find(uri);
            if (d && d->content) {
                char word[128];
                get_word_at(d->content, line, col, word, sizeof(word));
                if (word[0]) {
                    char hover[1024];
                    snprintf(hover, sizeof(hover),
                        "{\"contents\":{\"kind\":\"markdown\",\"value\":\"`%s`\"}}", word);
                    lsp_respond(id, hover);
                } else {
                    lsp_respond(id, "null");
                }
            } else {
                lsp_respond(id, "null");
            }
        }
        else if (strcmp(method, "textDocument/completion") == 0) {
            char* trigger = strstr(msg, "\"triggerCharacter\":\"");
            if (trigger && trigger[19] == '.') {
                lsp_respond(id, METHOD_COMPLETIONS);
            } else {
                lsp_respond(id, KEYWORD_COMPLETIONS);
            }
        }
        else if (strcmp(method, "textDocument/definition") == 0) {
            // Search document for function/struct definition
            char uri[512] = "";
            json_get_string(msg, "uri", uri, sizeof(uri));
            int line = json_get_int(msg, "line");
            int col = json_get_int(msg, "character");
            
            LspDoc* d = doc_find(uri);
            if (d && d->content) {
                char word[128];
                get_word_at(d->content, line, col, word, sizeof(word));
                // Search for "fn word(" or "struct word" or "enum word"
                char patterns[3][256];
                snprintf(patterns[0], 256, "fn %s(", word);
                snprintf(patterns[1], 256, "struct %s ", word);
                snprintf(patterns[2], 256, "enum %s ", word);
                
                const char* p = d->content;
                int cur_line = 0;
                bool found = false;
                while (p && *p) {
                    for (int pi = 0; pi < 3; pi++) {
                        if (strncmp(p, patterns[pi], strlen(patterns[pi])) == 0) {
                            char def[512];
                            snprintf(def, sizeof(def),
                                "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                                "\"end\":{\"line\":%d,\"character\":0}}}", uri, cur_line, cur_line);
                            lsp_respond(id, def);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                    if (*p == '\n') cur_line++;
                    p++;
                }
                if (!found) lsp_respond(id, "null");
            } else {
                lsp_respond(id, "null");
            }
        }
        else if (strcmp(method, "textDocument/references") == 0) {
            char uri[512] = ""; json_get_string(msg, "uri", uri, sizeof(uri));
            int line = json_get_int(msg, "line"), col = json_get_int(msg, "character");
            LspDoc* d = doc_find(uri);
            if (d && d->content) {
                char word[128]; get_word_at(d->content, line, col, word, sizeof(word));
                if (word[0]) {
                    int ss = -1, se = -1; find_scope(d->content, line, &ss, &se);
                    bool local = ss >= 0 && is_local_sym(d->content, word, ss, se);
                    char result[65536]; int pos = 0, total = 0;
                    pos += snprintf(result+pos, sizeof(result)-pos, "[");
                    for (int di = 0; di < lsp_doc_count; di++) {
                        if (!lsp_docs[di].content) continue;
                        bool same = strcmp(lsp_docs[di].uri, uri) == 0;
                        if (local && !same) continue;
                        WordMatch wm[512]; int cnt = find_all_refs(lsp_docs[di].content, word, wm, 512);
                        for (int i = 0; i < cnt; i++) {
                            if (local && same && (wm[i].line < ss || wm[i].line > se)) continue;
                            if (total) pos += snprintf(result+pos, sizeof(result)-pos, ",");
                            pos += snprintf(result+pos, sizeof(result)-pos,
                                "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}",
                                lsp_docs[di].uri, wm[i].line, wm[i].col, wm[i].line, wm[i].col+wm[i].len); total++;
                        }
                    }
                    pos += snprintf(result+pos, sizeof(result)-pos, "]");
                    lsp_respond(id, result);
                } else lsp_respond(id, "[]");
            } else lsp_respond(id, "[]");
        }
        else if (strcmp(method, "textDocument/rename") == 0) {
            char uri[512] = ""; json_get_string(msg, "uri", uri, sizeof(uri));
            int line = json_get_int(msg, "line"), col = json_get_int(msg, "character");
            char nn[256] = ""; json_get_string(msg, "newName", nn, sizeof(nn));
            LspDoc* d = doc_find(uri);
            if (d && d->content && nn[0]) {
                char word[128]; get_word_at(d->content, line, col, word, sizeof(word));
                if (word[0]) {
                    int ss = -1, se = -1; find_scope(d->content, line, &ss, &se);
                    bool local = ss >= 0 && is_local_sym(d->content, word, ss, se);
                    char result[65536]; int pos = 0, dc = 0;
                    pos += snprintf(result+pos, sizeof(result)-pos, "{\"changes\":{");
                    for (int di = 0; di < lsp_doc_count; di++) {
                        if (!lsp_docs[di].content) continue;
                        bool same = strcmp(lsp_docs[di].uri, uri) == 0;
                        if (local && !same) continue;
                        WordMatch wm[512]; int cnt = find_all_refs(lsp_docs[di].content, word, wm, 512);
                        int fc = 0;
                        for (int i = 0; i < cnt; i++) { if (local && same && (wm[i].line < ss || wm[i].line > se)) continue; fc++; }
                        if (!fc) continue;
                        if (dc) pos += snprintf(result+pos, sizeof(result)-pos, ",");
                        pos += snprintf(result+pos, sizeof(result)-pos, "\"%s\":[", lsp_docs[di].uri);
                        int ec = 0;
                        for (int i = 0; i < cnt; i++) {
                            if (local && same && (wm[i].line < ss || wm[i].line > se)) continue;
                            if (ec) pos += snprintf(result+pos, sizeof(result)-pos, ",");
                            pos += snprintf(result+pos, sizeof(result)-pos,
                                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},\"newText\":\"%s\"}",
                                wm[i].line, wm[i].col, wm[i].line, wm[i].col+wm[i].len, nn); ec++;
                        }
                        pos += snprintf(result+pos, sizeof(result)-pos, "]"); dc++;
                    }
                    pos += snprintf(result+pos, sizeof(result)-pos, "}}");
                    lsp_respond(id, result);
                } else lsp_respond(id, "null");
            } else lsp_respond(id, "null");
        }
        else if (strcmp(method, "textDocument/prepareRename") == 0) {
            char uri[512] = ""; json_get_string(msg, "uri", uri, sizeof(uri));
            int line = json_get_int(msg, "line"), col = json_get_int(msg, "character");
            LspDoc* d = doc_find(uri);
            if (d && d->content) {
                char word[128]; get_word_at(d->content, line, col, word, sizeof(word));
                static const char* kw[] = {"fn","var","const","struct","enum","impl","import","export","if","else","while","for","match","return","spawn","await","yield","true","false","pub","trait","type",NULL};
                bool isk = false; for (int i = 0; kw[i]; i++) if (strcmp(word, kw[i]) == 0) { isk = true; break; }
                if (word[0] && !isk) {
                    const char* p = d->content; int cl = 0;
                    while (p && *p && cl < line) { if (*p == '\n') cl++; p++; }
                    for (int i = 0; i < col && *p && *p != '\n'; i++) p++;
                    const char* s = p; while (s > d->content && is_word_char_lsp(*(s-1))) s--;
                    const char* e = p; while (*e && is_word_char_lsp(*e)) e++;
                    int sc = col - (int)(p - s), ec = sc + (int)(e - s);
                    char r[256]; snprintf(r, sizeof(r), "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}", line, sc, line, ec);
                    lsp_respond(id, r);
                } else lsp_respond(id, "null");
            } else lsp_respond(id, "null");
        }
        else {
            if (strcmp(id, "null") != 0) lsp_respond(id, "null");
        }
        
        free(msg);
    }
    
    fprintf(stderr, "Wyn LSP Server stopped\n");
    return 0;
}
