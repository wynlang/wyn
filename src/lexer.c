#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "error.h"
#include "growable.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

static Lexer lexer;
static const char* lexer_source_begin = NULL;

// Lexer-level error reporting (unterminated strings). The lexer can't print
// diagnostics itself (no filename), so it returns TOKEN_ERROR and stashes the
// message + position of the OPENING quote here for the parser to report.
static const char* lexer_err_msg = NULL;
const char* lexer_error_msg(void) { return lexer_err_msg; }
int lexer_error_col = 1;
static Lexer* lexer_stack = NULL;
static int lexer_stack_depth = 0;
static int lexer_stack_cap = 0;

void save_lexer_state() {
    WYN_ENSURE_CAP(lexer_stack, lexer_stack_depth, lexer_stack_cap);
    lexer_stack[lexer_stack_depth++] = lexer;
}
void restore_lexer_state() {
    if (lexer_stack_depth > 0) lexer = lexer_stack[--lexer_stack_depth];
}
// Discard the most recent saved snapshot without rewinding.
void discard_lexer_state() {
    if (lexer_stack_depth > 0) lexer_stack_depth--;
}

void init_lexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
    lexer_source_begin = source;
    lexer_err_msg = NULL;
    // Skip a UTF-8 BOM (EF BB BF) - files saved by Windows editors carry one.
    // Left unhandled it silently derailed the parse: the whole program body
    // was dropped and the "compiled" binary did nothing.
    if ((unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF) {
        lexer.current += 3;
        lexer.start = lexer.current;
    }
    // Skip shebang line: #!/usr/bin/env wyn run
    if (lexer.current[0] == '#' && lexer.current[1] == '!') {
        while (*lexer.current && *lexer.current != '\n') lexer.current++;
        if (*lexer.current == '\n') { lexer.current++; lexer.line++; }
        lexer.start = lexer.current;
    }
}

static bool is_at_end() {
    return *lexer.current == '\0';
}

static char advance() {
    return *lexer.current++;
}

static char peek() {
    return *lexer.current;
}

static char peek_next() {
    if (is_at_end()) return '\0';
    return lexer.current[1];
}

static bool match(char expected) {
    if (is_at_end() || *lexer.current != expected) return false;
    lexer.current++;
    return true;
}

static void skip_whitespace() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance();
        } else if (c == '\n') {
            lexer.line++;
            advance();
        } else if (c == '/' && peek_next() == '/') {
            while (peek() != '\n' && !is_at_end()) advance();
        } else if (c == '/' && peek_next() == '*') {
            advance(); advance(); // skip /*
            while (!is_at_end()) {
                if (peek() == '\n') lexer.line++;
                if (peek() == '*' && peek_next() == '/') { advance(); advance(); break; }
                advance();
            }
        } else if (c == '#') {
            while (peek() != '\n' && !is_at_end()) advance();
        } else {
            break;
        }
    }
}

static Token make_token(WynTokenType type) {
    Token token;
    token.type = type;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token number() {
    // Hex literals: 0xFF
    if (*(lexer.current - 1) == '0' && (peek() == 'x' || peek() == 'X')) {
        advance();
        while (isxdigit(peek())) advance();
        return make_token(TOKEN_INT);
    }
    
    // Binary literals: 0b1010
    if (*(lexer.current - 1) == '0' && (peek() == 'b' || peek() == 'B')) {
        advance();
        while (peek() == '0' || peek() == '1') advance();
        return make_token(TOKEN_INT);
    }
    
    // Regular numbers with optional underscores
    while (isdigit(peek()) || peek() == '_') advance();
    bool is_float = false;
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek()) || peek() == '_') advance();
        is_float = true;
    }
    // Scientific notation: 1e10, 1.5e-6, 2E+8. Only consume the 'e' when a
    // valid exponent follows, so `5.elements` style member access still lexes.
    if (peek() == 'e' || peek() == 'E') {
        const char* save = lexer.current;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (isdigit(peek())) {
            while (isdigit(peek())) advance();
            is_float = true;
        } else {
            lexer.current = save;
        }
    }
    return make_token(is_float ? TOKEN_FLOAT : TOKEN_INT);
}

static WynTokenType keyword_type(const char* start, int length) {
    switch (start[0]) {
        case 'a': 
            if (length == 3 && memcmp(start, "and", 3) == 0) return TOKEN_AND;
            if (length == 2 && memcmp(start, "as", 2) == 0) return TOKEN_AS;
            // 'assert' is a regular function, not a keyword
            // if (length == 6 && memcmp(start, "assert", 6) == 0) return TOKEN_ASSERT;
            // 'async' removed - spawn/await work on plain fns (async was a deprecated no-op)
            if (length == 5 && memcmp(start, "await", 5) == 0) return TOKEN_AWAIT;
            break;
        case 'b': if (length == 5 && memcmp(start, "break", 5) == 0) return TOKEN_BREAK; break;
        case 'd': if (length == 5 && memcmp(start, "defer", 5) == 0) return TOKEN_DEFER; break;
        case 'c': 
            if (length == 5 && memcmp(start, "const", 5) == 0) return TOKEN_CONST;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TOKEN_CONTINUE;
            if (length == 7 && memcmp(start, "channel", 7) == 0) return TOKEN_CHANNEL;
            // 'catch' removed - use Result/Ok/Err + match/? for error handling
            break;
        case 'E':
            // Err is a regular identifier (can be enum variant)
            break;
        case 'e':
            if (length == 4 && memcmp(start, "else", 4) == 0) return TOKEN_ELSE;
            // 'elseif' removed - use 'else if' (works via else → statement → if)
            if (length == 4 && memcmp(start, "enum", 4) == 0) return TOKEN_ENUM;
            if (length == 6 && memcmp(start, "export", 6) == 0) return TOKEN_EXPORT;
            // Removed TOKEN_ERR - let err be a regular identifier
            if (length == 6 && memcmp(start, "extern", 6) == 0) return TOKEN_EXTERN;
            break;
        case 'f':
            if (length == 5 && memcmp(start, "false", 5) == 0) return TOKEN_FALSE;
            if (length == 2 && memcmp(start, "fn", 2) == 0) return TOKEN_FN;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TOKEN_FOR;
            if (length == 4 && memcmp(start, "from", 4) == 0) return TOKEN_FROM;
            // 'finally' removed - unused (try/catch is not fully implemented)
            break;
        case 'i':
            if (length == 2 && memcmp(start, "if", 2) == 0) return TOKEN_IF;
            if (length == 4 && memcmp(start, "impl", 4) == 0) return TOKEN_IMPL;
            if (length == 2 && memcmp(start, "in", 2) == 0) return TOKEN_IN;
            if (length == 6 && memcmp(start, "import", 6) == 0) return TOKEN_IMPORT;
            break;
        case 'l':
            // 'let' keyword removed in v1.2.3 - use 'var' or 'const' instead
            break;
        case 'm':
            if (length == 5 && memcmp(start, "match", 5) == 0) return TOKEN_MATCH;
            if (length == 3 && memcmp(start, "mut", 3) == 0) return TOKEN_MUT;
            // "map" is not a keyword - use {} for hashmaps
            // if (length == 3 && memcmp(start, "map", 3) == 0) return TOKEN_MAP;
            // 'module' removed - was never consumed by the parser (use import/from)
            // 'macro' removed - unimplemented, unused
            break;
        case 'n': 
            if (length == 3 && memcmp(start, "not", 3) == 0) return TOKEN_NOT;
            // 'null' removed - use 'None' for optionals (null never worked as a value)
            break;
        case 'N':
            if (length == 4 && memcmp(start, "None", 4) == 0) return TOKEN_NONE;
            break;
        case 'O':
            // Ok is a regular identifier (can be enum variant)
            break;
        case 'o': 
            if (length == 2 && memcmp(start, "or", 2) == 0) return TOKEN_OR;
            // 'object' removed - identical to 'struct' (use struct)
            // Removed TOKEN_OK - let ok be a regular identifier
            break;
        case 'p':
            if (length == 3 && memcmp(start, "pub", 3) == 0) return TOKEN_PUB;
            if (length == 8 && memcmp(start, "parallel", 8) == 0) return TOKEN_PARALLEL;
            break;
        case 'r': 
            if (length == 6 && memcmp(start, "return", 6) == 0) return TOKEN_RETURN;
            if (length == 4 && memcmp(start, "root", 4) == 0) return TOKEN_ROOT;
            break;
        case 's':
            if (length == 6 && memcmp(start, "struct", 6) == 0) return TOKEN_STRUCT;
            if (length == 5 && memcmp(start, "spawn", 5) == 0) return TOKEN_SPAWN;
            if (length == 4 && memcmp(start, "self", 4) == 0) return TOKEN_SELF;
            if (length == 6 && memcmp(start, "select", 6) == 0) return TOKEN_SELECT;
            break;
        case 'S':
            if (length == 4 && memcmp(start, "Some", 4) == 0) return TOKEN_SOME;
            break;
        case 't':
            if (length == 4 && memcmp(start, "true", 4) == 0) return TOKEN_TRUE;
            if (length == 4 && memcmp(start, "type", 4) == 0) return TOKEN_TYPEDEF;
            // 'test' is context-sensitive - only a keyword at top level
            // Allow it as identifier for function names, variables, etc.
            if (length == 4 && memcmp(start, "test", 4) == 0) return TOKEN_IDENT;
            // 'try'/'throw' removed - use Result/Ok/Err + match/? for error handling

            if (length == 5 && memcmp(start, "trait", 5) == 0) return TOKEN_TRAIT;
            break;
        case 'u':
            // 'unsafe' removed - unimplemented, unused
            break;
        case 'v': if (length == 3 && memcmp(start, "var", 3) == 0) return TOKEN_VAR; break;
        case 'w': if (length == 5 && memcmp(start, "while", 5) == 0) return TOKEN_WHILE; break;
        case 'y': if (length == 5 && memcmp(start, "yield", 5) == 0) return TOKEN_YIELD; break;
    }
    return TOKEN_IDENT;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    int length = (int)(lexer.current - lexer.start);
    WynTokenType type = keyword_type(lexer.start, length);
    return make_token(type);
}

// Unterminated string: return TOKEN_ERROR pointing at the OPENING quote
// (open_line + column computed from the source), with the message stashed
// for the parser. Before this, the lexer silently returned a fake TOKEN_EOF,
// so users saw "Expected '}' after function body" pointing at the file end.
static Token error_token(const char* msg, const char* open_quote, int open_line) {
    lexer_err_msg = msg;
    lexer_error_col = 1;
    for (const char* p = open_quote; p > lexer_source_begin && p[-1] != '\n'; p--)
        lexer_error_col++;
    Token token;
    token.type = TOKEN_ERROR;
    token.start = open_quote;
    token.length = 1;
    token.line = open_line;
    return token;
}

static Token string() {
    const char* open_quote = lexer.start;  // the opening " (start of this token)
    int open_line = lexer.line;
    // Check for multi-line string """
    if (peek() == '"' && peek_next() == '"') {
        advance(); // Skip second "
        advance(); // Skip third "

        while (true) {
            if (is_at_end()) return error_token("Unterminated multi-line string literal (opened here)", open_quote, open_line);
            if (peek() == '"' && peek_next() == '"' && *(lexer.current + 2) == '"') {
                advance(); // Skip first "
                advance(); // Skip second "
                advance(); // Skip third "
                break;
            }
            if (peek() == '\n') lexer.line++;
            advance();
        }
        return make_token(TOKEN_STRING);
    }
    
    // Regular string - handle ${} interpolation with nested quotes.
    // A newline ends the scan: regular strings are single-line (""" is the
    // multi-line form), so an unterminated string errors at ITS line instead
    // of swallowing the rest of the file and erroring at EOF.
    while (peek() != '"' && peek() != '\n' && !is_at_end()) {
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) advance();
        } else if (peek() == '$' && peek_next() == '{') {
            // Skip over ${...} including nested strings and braces
            advance(); advance(); // skip ${
            int depth = 1;
            while (depth > 0 && !is_at_end()) {
                if (peek() == '{') { depth++; advance(); }
                else if (peek() == '}') { depth--; if (depth > 0) advance(); else advance(); }
                else if (peek() == '"') {
                    advance(); // skip opening quote
                    // Handle nested string with its own ${} interpolation
                    while (peek() != '"' && !is_at_end()) {
                        if (peek() == '\\') { advance(); if (!is_at_end()) advance(); }
                        else if (peek() == '$' && peek_next() == '{') {
                            advance(); advance(); // skip ${
                            int inner = 1;
                            while (inner > 0 && !is_at_end()) {
                                if (peek() == '{') { inner++; advance(); }
                                else if (peek() == '}') { inner--; if (inner > 0) advance(); else advance(); }
                                else if (peek() == '"') {
                                    advance();
                                    while (peek() != '"' && !is_at_end()) {
                                        if (peek() == '\\') { advance(); if (!is_at_end()) advance(); }
                                        else advance();
                                    }
                                    if (!is_at_end()) advance();
                                }
                                else advance();
                            }
                        }
                        else advance();
                    }
                    if (!is_at_end()) advance(); // skip closing quote
                }
                else if (peek() == '\'') {
                    advance();
                    while (peek() != '\'' && !is_at_end()) advance();
                    if (!is_at_end()) advance();
                }
                else { if (peek() == '\n') lexer.line++; advance(); }
            }
            if (depth > 0 && is_at_end())
                return error_token("Unterminated string interpolation (string opened here)", open_quote, open_line);
        } else {
            advance();
        }
    }
    if (is_at_end() || peek() == '\n')
        return error_token("Unterminated string literal (opened here)", open_quote, open_line);
    advance();
    return make_token(TOKEN_STRING);
}

Token next_token() {
    skip_whitespace();
    lexer.start = lexer.current;
    
    if (is_at_end()) return make_token(TOKEN_EOF);
    
    char c = advance();
    
    if (isdigit(c)) return number();
    if (isalpha(c)) return identifier();
    if (c == '_') {
        // Check if it's a standalone underscore or part of an identifier
        if (isalnum(peek())) {
            return identifier(); // Part of identifier like _var or var_
        } else {
            return make_token(TOKEN_UNDERSCORE); // Standalone underscore for pattern matching
        }
    }
    if (c == '"') return string();
    if (c == '\'') {
        // Single-quote string (no interpolation, single-line like "")
        const char* open_quote = lexer.start;
        int open_line = lexer.line;
        while (peek() != '\'' && peek() != '\n' && !is_at_end()) {
            if (peek() == '\\') { advance(); if (!is_at_end()) advance(); }
            else advance();
        }
        if (is_at_end() || peek() == '\n')
            return error_token("Unterminated string literal (opened here)", open_quote, open_line);
        advance();
        return make_token(TOKEN_STRING);
    }
    
    switch (c) {
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case ',': return make_token(TOKEN_COMMA);
        case '.': 
            if (match('.')) {
                if (match('.')) return make_token(TOKEN_DOTDOTDOT);
                if (match('=')) return make_token(TOKEN_DOTDOTEQ);
                return make_token(TOKEN_DOTDOT);
            }
            return make_token(TOKEN_DOT);
        case ';': return make_token(TOKEN_SEMI);
        case ':':
            if (match(':')) return make_token(TOKEN_COLONCOLON);
            return make_token(TOKEN_COLON);
        case '+': 
            if (match('+')) return make_token(TOKEN_PLUSPLUS);
            if (match('=')) return make_token(TOKEN_PLUSEQ);
            return make_token(TOKEN_PLUS);
        case '-': 
            if (match('-')) return make_token(TOKEN_MINUSMINUS);
            if (match('=')) return make_token(TOKEN_MINUSEQ);
            if (match('>')) return make_token(TOKEN_ARROW);
            return make_token(TOKEN_MINUS);
        case '*': return match('=') ? make_token(TOKEN_STAREQ) : make_token(TOKEN_STAR);
        case '/': return match('=') ? make_token(TOKEN_SLASHEQ) : make_token(TOKEN_SLASH);
        case '%': if (match('=')) return make_token(TOKEN_PERCENTEQ); return make_token(TOKEN_PERCENT);
        case '&': return match('&') ? make_token(TOKEN_AMPAMP) : make_token(TOKEN_AMP);
        case '|': 
            if (match('|')) return make_token(TOKEN_PIPEPIPE);
            if (match('>')) return make_token(TOKEN_PIPEGT);
            return make_token(TOKEN_PIPE);
        case '^': return make_token(TOKEN_CARET);
        case '~': return make_token(TOKEN_TILDE);
        case '!': return match('=') ? make_token(TOKEN_BANGEQ) : make_token(TOKEN_BANG);
        case '=':
            if (match('=')) return make_token(TOKEN_EQEQ);
            if (match('>')) return make_token(TOKEN_FATARROW);
            return make_token(TOKEN_EQ);
        case '<':
            if (match('=')) return make_token(TOKEN_LTEQ);
            // `<<` never appears in type syntax (generic opens are `<Ident`),
            // so lexing it as one token is safe. `>>` is NOT lexed as one
            // token: nested generic closers like `HashMap<string, int>>` need
            // two separate GT tokens; the parser joins adjacent GTs into a
            // right-shift in expression position instead.
            if (match('<')) return make_token(TOKEN_LSHIFT);
            return make_token(TOKEN_LT);
        case '>': return match('=') ? make_token(TOKEN_GTEQ) : make_token(TOKEN_GT);
        case '?': 
            if (match('.')) return make_token(TOKEN_QUESTION_DOT);
            if (match('?')) return make_token(TOKEN_QUESTION_QUESTION);
            return make_token(TOKEN_QUESTION);
    }
    
    return make_token(TOKEN_EOF);
}
