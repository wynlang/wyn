#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "error.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

static Lexer lexer;

void init_lexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
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
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek()) || peek() == '_') advance();
        return make_token(TOKEN_FLOAT);
    }
    return make_token(TOKEN_INT);
}

static WynTokenType keyword_type(const char* start, int length) {
    switch (start[0]) {
        case 'a': 
            if (length == 3 && memcmp(start, "and", 3) == 0) return TOKEN_AND;
            if (length == 2 && memcmp(start, "as", 2) == 0) return TOKEN_AS;
            if (length == 6 && memcmp(start, "assert", 6) == 0) return TOKEN_ASSERT;
            if (length == 5 && memcmp(start, "async", 5) == 0) return TOKEN_ASYNC;
            if (length == 5 && memcmp(start, "await", 5) == 0) return TOKEN_AWAIT;
            break;
        case 'b': if (length == 5 && memcmp(start, "break", 5) == 0) return TOKEN_BREAK; break;
        case 'c': 
            if (length == 5 && memcmp(start, "const", 5) == 0) return TOKEN_CONST;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TOKEN_CONTINUE;
            if (length == 7 && memcmp(start, "channel", 7) == 0) return TOKEN_CHANNEL;
            if (length == 5 && memcmp(start, "catch", 5) == 0) return TOKEN_CATCH;
            break;
        case 'E':
            if (length == 3 && memcmp(start, "Err", 3) == 0) return TOKEN_ERR;
            break;
        case 'e':
            if (length == 4 && memcmp(start, "else", 4) == 0) return TOKEN_ELSE;
            if (length == 6 && memcmp(start, "elseif", 6) == 0) return TOKEN_ELSEIF;
            if (length == 4 && memcmp(start, "enum", 4) == 0) return TOKEN_ENUM;
            if (length == 6 && memcmp(start, "export", 6) == 0) return TOKEN_EXPORT;
            if (length == 3 && memcmp(start, "err", 3) == 0) return TOKEN_ERR;
            if (length == 6 && memcmp(start, "extern", 6) == 0) return TOKEN_EXTERN;
            break;
        case 'f':
            if (length == 5 && memcmp(start, "false", 5) == 0) return TOKEN_FALSE;
            if (length == 2 && memcmp(start, "fn", 2) == 0) return TOKEN_FN;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TOKEN_FOR;
            if (length == 4 && memcmp(start, "from", 4) == 0) return TOKEN_FROM;
            if (length == 7 && memcmp(start, "finally", 7) == 0) return TOKEN_FINALLY;
            break;
        case 'i':
            if (length == 2 && memcmp(start, "if", 2) == 0) return TOKEN_IF;
            if (length == 4 && memcmp(start, "impl", 4) == 0) return TOKEN_IMPL;
            if (length == 2 && memcmp(start, "in", 2) == 0) return TOKEN_IN;
            if (length == 6 && memcmp(start, "import", 6) == 0) return TOKEN_IMPORT;
            break;
        case 'l':
            if (length == 3 && memcmp(start, "let", 3) == 0) return TOKEN_LET;
            break;
        case 'm':
            if (length == 5 && memcmp(start, "match", 5) == 0) return TOKEN_MATCH;
            if (length == 3 && memcmp(start, "mut", 3) == 0) return TOKEN_MUT;
            if (length == 3 && memcmp(start, "map", 3) == 0) return TOKEN_MAP;
            if (length == 6 && memcmp(start, "module", 6) == 0) return TOKEN_MODULE;
            if (length == 5 && memcmp(start, "macro", 5) == 0) return TOKEN_MACRO;
            break;
        case 'n': 
            if (length == 3 && memcmp(start, "not", 3) == 0) return TOKEN_NOT;
            if (length == 4 && memcmp(start, "none", 4) == 0) return TOKEN_NONE;
            if (length == 4 && memcmp(start, "null", 4) == 0) return TOKEN_NULL;
            break;
        case 'N':
            if (length == 4 && memcmp(start, "None", 4) == 0) return TOKEN_NONE;
            break;
        case 'O':
            if (length == 2 && memcmp(start, "Ok", 2) == 0) return TOKEN_OK;
            break;
        case 'o': 
            if (length == 2 && memcmp(start, "or", 2) == 0) return TOKEN_OR;
            if (length == 6 && memcmp(start, "object", 6) == 0) return TOKEN_OBJECT;
            if (length == 2 && memcmp(start, "ok", 2) == 0) return TOKEN_OK;
            break;
        case 'p': if (length == 3 && memcmp(start, "pub", 3) == 0) return TOKEN_PUB; break;
        case 'r': if (length == 6 && memcmp(start, "return", 6) == 0) return TOKEN_RETURN; break;
        case 's': 
            if (length == 6 && memcmp(start, "struct", 6) == 0) return TOKEN_STRUCT;
            if (length == 4 && memcmp(start, "some", 4) == 0) return TOKEN_SOME;
            if (length == 5 && memcmp(start, "spawn", 5) == 0) return TOKEN_SPAWN;
            break;
        case 'S':
            if (length == 4 && memcmp(start, "Some", 4) == 0) return TOKEN_SOME;
            break;
        case 't':
            if (length == 4 && memcmp(start, "true", 4) == 0) return TOKEN_TRUE;
            if (length == 4 && memcmp(start, "type", 4) == 0) return TOKEN_TYPEDEF;
            if (length == 4 && memcmp(start, "test", 4) == 0) return TOKEN_TEST;
            if (length == 3 && memcmp(start, "try", 3) == 0) return TOKEN_TRY;
            if (length == 5 && memcmp(start, "throw", 5) == 0) return TOKEN_THROW;
            if (length == 5 && memcmp(start, "trait", 5) == 0) return TOKEN_TRAIT;
            break;
        case 'u':
            if (length == 6 && memcmp(start, "unsafe", 6) == 0) return TOKEN_UNSAFE;
            break;
        case 'v': if (length == 3 && memcmp(start, "var", 3) == 0) return TOKEN_VAR; break;
        case 'w': if (length == 5 && memcmp(start, "while", 5) == 0) return TOKEN_WHILE; break;
    }
    return TOKEN_IDENT;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    int length = (int)(lexer.current - lexer.start);
    WynTokenType type = keyword_type(lexer.start, length);
    return make_token(type);
}

static Token string() {
    // Check for multi-line string """
    if (peek() == '"' && peek_next() == '"') {
        advance(); // Skip second "
        advance(); // Skip third "
        
        while (true) {
            if (is_at_end()) return make_token(TOKEN_EOF);
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
    
    // Regular string
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) advance();
        } else {
            if (peek() == '\n') lexer.line++;
            advance();
        }
    }
    if (is_at_end()) return make_token(TOKEN_EOF);
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
        // Character literal
        advance(); // Skip opening '
        if (peek() == '\\') advance(); // Handle escape
        advance(); // The character
        if (peek() == '\'') advance(); // Skip closing '
        return make_token(TOKEN_CHAR);
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
        case '%': return make_token(TOKEN_PERCENT);
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
        case '<': return match('=') ? make_token(TOKEN_LTEQ) : make_token(TOKEN_LT);
        case '>': return match('=') ? make_token(TOKEN_GTEQ) : make_token(TOKEN_GT);
        case '?': 
            if (match('.')) return make_token(TOKEN_QUESTION_DOT);
            if (match('?')) return make_token(TOKEN_QUESTION_QUESTION);
            return make_token(TOKEN_QUESTION);
    }
    
    return make_token(TOKEN_EOF);
}

/*
// Error recovery functions - TODO: Implement properly in T1.2.x
static void skip_invalid_character() {
//     // report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//     //            "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     // report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//     //            "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         // report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//         //            "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     // report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//     //            "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
// 
// // Error recovery functions
// static void skip_invalid_character() {
//     report_error(ERR_INVALID_CHARACTER, current_file, current_line, current_column, 
//                 "Invalid character encountered");
//     advance(); // Skip the invalid character
// }
// 
// static Token handle_unterminated_string() {
//     report_error(ERR_UNTERMINATED_STRING, current_file, current_line, current_column,
//                 "Unterminated string literal");
//     // Return error token but continue parsing
//     return make_token(TOKEN_ERROR);
// }
// 
// // Enhanced number scanning with error recovery
// static Token scan_number_safe() {
//     while (isdigit(peek())) advance();
//     
//     // Look for decimal point
//     if (peek() == '.' && isdigit(peek_next())) {
//         advance(); // Consume '.'
//         while (isdigit(peek())) advance();
//     }
//     
//     // Validate number format
//     if (isalpha(peek())) {
//         report_error(ERR_INVALID_NUMBER, current_file, current_line, current_column,
//                     "Invalid number format");
//         // Skip invalid characters
//         while (isalnum(peek())) advance();
//         return make_token(TOKEN_ERROR);
//     }
//     
//     return make_token(TOKEN_NUMBER);
// }
*/
