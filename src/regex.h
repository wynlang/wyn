#ifndef WYN_REGEX_H
#define WYN_REGEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "unicode.h"

// Forward declarations
typedef struct WynRegex WynRegex;
typedef struct WynMatch WynMatch;
typedef struct WynCaptures WynCaptures;
typedef struct WynRegexError WynRegexError;

// Regex error types
typedef enum {
    WYN_REGEX_OK,
    WYN_REGEX_INVALID_PATTERN,
    WYN_REGEX_COMPILATION_ERROR,
    WYN_REGEX_MEMORY_ERROR,
    WYN_REGEX_INVALID_GROUP,
    WYN_REGEX_UNICODE_ERROR
} WynRegexErrorCode;

// Match result structure
typedef struct WynMatch {
    size_t start;
    size_t end;
    WynString* text;
} WynMatch;

// Capture groups structure
typedef struct WynCaptures {
    WynMatch** matches;     // Array of match pointers (NULL for no match)
    size_t count;           // Number of capture groups
    char** named_groups;    // Array of named group names
    WynMatch** named_matches; // Array of named group matches
    size_t named_count;     // Number of named groups
} WynCaptures;

// Regex error structure
typedef struct WynRegexError {
    WynRegexErrorCode code;
    char* message;
    size_t position;        // Position in pattern where error occurred
} WynRegexError;

// Internal regex pattern representation
typedef enum {
    REGEX_LITERAL,
    REGEX_DOT,              // .
    REGEX_STAR,             // *
    REGEX_PLUS,             // +
    REGEX_QUESTION,         // ?
    REGEX_CARET,            // ^
    REGEX_DOLLAR,           // $
    REGEX_BRACKET,          // [...]
    REGEX_GROUP,            // (...)
    REGEX_ALTERNATION,      // |
    REGEX_ESCAPE,           // backslash
    REGEX_QUANTIFIER        // {n,m}
} WynRegexNodeType;

typedef struct WynRegexNode {
    WynRegexNodeType type;
    uint32_t codepoint;     // For literals
    struct WynRegexNode* left;
    struct WynRegexNode* right;
    struct WynRegexNode* next;
    
    // Quantifier data
    int min_count;
    int max_count;
    
    // Character class data
    uint32_t* char_class;
    size_t char_class_size;
    bool negated;
    
    // Group data
    int group_id;
    char* group_name;
    
    // Flags
    bool greedy;
    bool case_insensitive;
} WynRegexNode;

// Compiled regex structure
typedef struct WynRegex {
    WynRegexNode* root;
    int group_count;
    char** group_names;
    size_t flags;
    WynRegexError* last_error;
} WynRegex;

// Regex compilation and destruction
WynRegex* wyn_regex_new(const char* pattern, WynRegexError* error);
WynRegex* wyn_regex_new_with_flags(const char* pattern, size_t flags, WynRegexError* error);
void wyn_regex_free(WynRegex* regex);

// Pattern matching
bool wyn_regex_is_match(const WynRegex* regex, const WynString* text);
WynMatch* wyn_regex_find(const WynRegex* regex, const WynString* text);
WynMatch** wyn_regex_find_all(const WynRegex* regex, const WynString* text, size_t* count);
WynCaptures* wyn_regex_captures(const WynRegex* regex, const WynString* text);

// String replacement
WynString* wyn_regex_replace(const WynRegex* regex, const WynString* text, const char* replacement);
WynString* wyn_regex_replace_all(const WynRegex* regex, const WynString* text, const char* replacement);

// Match and capture management
WynMatch* wyn_match_new(size_t start, size_t end, const WynString* text);
void wyn_match_free(WynMatch* match);
WynCaptures* wyn_captures_new(size_t group_count);
void wyn_captures_free(WynCaptures* captures);
WynMatch* wyn_captures_get(const WynCaptures* captures, int group_id);
WynMatch* wyn_captures_get_named(const WynCaptures* captures, const char* name);

// Error handling
WynRegexError* wyn_regex_error_new(WynRegexErrorCode code, const char* message, size_t position);
void wyn_regex_error_free(WynRegexError* error);
const char* wyn_regex_error_string(WynRegexErrorCode code);

// Regex flags
#define WYN_REGEX_CASE_INSENSITIVE  (1 << 0)
#define WYN_REGEX_MULTILINE         (1 << 1)
#define WYN_REGEX_DOTALL            (1 << 2)
#define WYN_REGEX_UNICODE           (1 << 3)
#define WYN_REGEX_EXTENDED          (1 << 4)

// Unicode character classes
bool wyn_regex_is_word_char(uint32_t codepoint);
bool wyn_regex_is_digit_char(uint32_t codepoint);
bool wyn_regex_is_space_char(uint32_t codepoint);

// Pattern compilation utilities
WynRegexNode* wyn_regex_parse_pattern(const char* pattern, size_t* pos, WynRegexError* error);
WynRegexNode* wyn_regex_parse_group(const char* pattern, size_t* pos, int* group_id, WynRegexError* error);
WynRegexNode* wyn_regex_parse_bracket(const char* pattern, size_t* pos, WynRegexError* error);
WynRegexNode* wyn_regex_parse_quantifier(const char* pattern, size_t* pos, WynRegexNode* node, WynRegexError* error);

// Pattern matching engine
bool wyn_regex_match_node(const WynRegexNode* node, const WynString* text, size_t pos, 
                         size_t* end_pos, WynCaptures* captures);
bool wyn_regex_match_literal(uint32_t pattern_char, uint32_t text_char, bool case_insensitive);
bool wyn_regex_match_char_class(const WynRegexNode* node, uint32_t codepoint);

// Optimization utilities
WynRegexNode* wyn_regex_optimize_pattern(WynRegexNode* root);
bool wyn_regex_is_literal_string(const WynRegexNode* node);
WynString* wyn_regex_extract_literal_prefix(const WynRegexNode* node);

#endif // WYN_REGEX_H
