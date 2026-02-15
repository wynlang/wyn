#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Regex compilation and destruction
WynRegex* wyn_regex_new(const char* pattern, WynRegexError* error) {
    return wyn_regex_new_with_flags(pattern, WYN_REGEX_UNICODE, error);
}

WynRegex* wyn_regex_new_with_flags(const char* pattern, size_t flags, WynRegexError* error) {
    if (!pattern) {
        if (error) {
            *error = *wyn_regex_error_new(WYN_REGEX_INVALID_PATTERN, "Pattern cannot be NULL", 0);
        }
        return NULL;
    }
    
    WynRegex* regex = malloc(sizeof(WynRegex));
    if (!regex) {
        if (error) {
            *error = *wyn_regex_error_new(WYN_REGEX_MEMORY_ERROR, "Failed to allocate regex", 0);
        }
        return NULL;
    }
    
    regex->group_count = 0;
    regex->group_names = NULL;
    regex->flags = flags;
    regex->last_error = NULL;
    
    // Parse the pattern
    size_t pos = 0;
    WynRegexError parse_error = {0};
    regex->root = wyn_regex_parse_pattern(pattern, &pos, &parse_error);
    
    if (!regex->root || parse_error.code != WYN_REGEX_OK) {
        if (error) *error = parse_error;
        wyn_regex_free(regex);
        return NULL;
    }
    
    // Optimize the pattern
    regex->root = wyn_regex_optimize_pattern(regex->root);
    
    if (error) {
        error->code = WYN_REGEX_OK;
        error->message = NULL;
        error->position = 0;
    }
    
    return regex;
}

// Helper function to free regex tree recursively
static void wyn_regex_free_node(WynRegexNode* node) {
    if (!node) return;
    
    wyn_regex_free_node(node->left);
    wyn_regex_free_node(node->right);
    wyn_regex_free_node(node->next);
    
    free(node->char_class);
    free(node->group_name);
    free(node);
}

void wyn_regex_free(WynRegex* regex) {
    if (!regex) return;
    
    // Free regex tree
    wyn_regex_free_node(regex->root);
    
    // Free group names
    if (regex->group_names) {
        for (int i = 0; i < regex->group_count; i++) {
            free(regex->group_names[i]);
        }
        free(regex->group_names);
    }
    
    if (regex->last_error) {
        wyn_regex_error_free(regex->last_error);
    }
    
    free(regex);
}

// Pattern matching
bool wyn_regex_is_match(const WynRegex* regex, const WynString* text) {
    if (!regex || !text) return false;
    
    WynMatch* match = wyn_regex_find(regex, text);
    bool result = (match != NULL);
    wyn_match_free(match);
    return result;
}

WynMatch* wyn_regex_find(const WynRegex* regex, const WynString* text) {
    if (!regex || !text) return NULL;
    
    // Simple implementation - try matching at each position
    size_t text_len = wyn_string_char_count((WynString*)text);
    
    for (size_t i = 0; i < text_len; i++) {
        size_t end_pos;
        if (wyn_regex_match_node(regex->root, text, i, &end_pos, NULL)) {
            // Extract matched substring
            WynString* match_text = wyn_string_substring(text, i, end_pos);
            WynMatch* match = wyn_match_new(i, end_pos, match_text);
            wyn_string_free(match_text);
            return match;
        }
    }
    
    return NULL;
}

WynMatch** wyn_regex_find_all(const WynRegex* regex, const WynString* text, size_t* count) {
    if (!regex || !text || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    // Simple implementation - collect all matches
    WynMatch** matches = NULL;
    size_t capacity = 0;
    *count = 0;
    
    size_t text_len = wyn_string_char_count((WynString*)text);
    size_t pos = 0;
    
    while (pos < text_len) {
        size_t end_pos;
        if (wyn_regex_match_node(regex->root, text, pos, &end_pos, NULL)) {
            // Grow matches array if needed
            if (*count >= capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                matches = realloc(matches, capacity * sizeof(WynMatch*));
                if (!matches) {
                    *count = 0;
                    return NULL;
                }
            }
            
            // Create match
            WynString* match_text = wyn_string_substring(text, pos, end_pos);
            matches[*count] = wyn_match_new(pos, end_pos, match_text);
            wyn_string_free(match_text);
            (*count)++;
            
            pos = end_pos > pos ? end_pos : pos + 1;  // Avoid infinite loop
        } else {
            pos++;
        }
    }
    
    return matches;
}

WynCaptures* wyn_regex_captures(const WynRegex* regex, const WynString* text) {
    if (!regex || !text) return NULL;
    
    WynCaptures* captures = wyn_captures_new(regex->group_count);
    if (!captures) return NULL;
    
    size_t end_pos;
    if (wyn_regex_match_node(regex->root, text, 0, &end_pos, captures)) {
        return captures;
    }
    
    wyn_captures_free(captures);
    return NULL;
}

// String replacement
WynString* wyn_regex_replace(const WynRegex* regex, const WynString* text, const char* replacement) {
    if (!regex || !text || !replacement) return NULL;
    
    WynMatch* match = wyn_regex_find(regex, text);
    if (!match) {
        return wyn_string_clone(text);
    }
    
    // Build replacement string
    WynString* result = wyn_string_new();
    
    // Add text before match
    WynString* before = wyn_string_substring(text, 0, match->start);
    wyn_string_push_string(result, before);
    wyn_string_free(before);
    
    // Add replacement
    wyn_string_push_str(result, replacement);
    
    // Add text after match
    size_t text_len = wyn_string_char_count((WynString*)text);
    WynString* after = wyn_string_substring(text, match->end, text_len);
    wyn_string_push_string(result, after);
    wyn_string_free(after);
    
    wyn_match_free(match);
    return result;
}

WynString* wyn_regex_replace_all(const WynRegex* regex, const WynString* text, const char* replacement) {
    if (!regex || !text || !replacement) return NULL;
    
    size_t match_count;
    WynMatch** matches = wyn_regex_find_all(regex, text, &match_count);
    
    if (match_count == 0) {
        return wyn_string_clone(text);
    }
    
    WynString* result = wyn_string_new();
    size_t last_end = 0;
    
    for (size_t i = 0; i < match_count; i++) {
        // Add text between matches
        WynString* between = wyn_string_substring(text, last_end, matches[i]->start);
        wyn_string_push_string(result, between);
        wyn_string_free(between);
        
        // Add replacement
        wyn_string_push_str(result, replacement);
        
        last_end = matches[i]->end;
        wyn_match_free(matches[i]);
    }
    
    // Add remaining text
    size_t text_len = wyn_string_char_count((WynString*)text);
    WynString* remaining = wyn_string_substring(text, last_end, text_len);
    wyn_string_push_string(result, remaining);
    wyn_string_free(remaining);
    
    free(matches);
    return result;
}

// Match and capture management
WynMatch* wyn_match_new(size_t start, size_t end, const WynString* text) {
    WynMatch* match = malloc(sizeof(WynMatch));
    if (!match) return NULL;
    
    match->start = start;
    match->end = end;
    match->text = text ? wyn_string_clone(text) : NULL;
    
    return match;
}

void wyn_match_free(WynMatch* match) {
    if (!match) return;
    wyn_string_free(match->text);
    free(match);
}

WynCaptures* wyn_captures_new(size_t group_count) {
    WynCaptures* captures = malloc(sizeof(WynCaptures));
    if (!captures) return NULL;
    
    captures->count = group_count;
    captures->matches = calloc(group_count, sizeof(WynMatch*));
    captures->named_groups = NULL;
    captures->named_matches = NULL;
    captures->named_count = 0;
    
    if (group_count > 0 && !captures->matches) {
        free(captures);
        return NULL;
    }
    
    return captures;
}

void wyn_captures_free(WynCaptures* captures) {
    if (!captures) return;
    
    for (size_t i = 0; i < captures->count; i++) {
        wyn_match_free(captures->matches[i]);
    }
    free(captures->matches);
    
    for (size_t i = 0; i < captures->named_count; i++) {
        free(captures->named_groups[i]);
        wyn_match_free(captures->named_matches[i]);
    }
    free(captures->named_groups);
    free(captures->named_matches);
    
    free(captures);
}

WynMatch* wyn_captures_get(const WynCaptures* captures, int group_id) {
    if (!captures || group_id < 0 || (size_t)group_id >= captures->count) {
        return NULL;
    }
    return captures->matches[group_id];
}

WynMatch* wyn_captures_get_named(const WynCaptures* captures, const char* name) {
    if (!captures || !name) return NULL;
    
    for (size_t i = 0; i < captures->named_count; i++) {
        if (strcmp(captures->named_groups[i], name) == 0) {
            return captures->named_matches[i];
        }
    }
    
    return NULL;
}

// Error handling
WynRegexError* wyn_regex_error_new(WynRegexErrorCode code, const char* message, size_t position) {
    WynRegexError* error = malloc(sizeof(WynRegexError));
    if (!error) return NULL;
    
    error->code = code;
    error->message = message ? strdup(message) : NULL;
    error->position = position;
    
    return error;
}

void wyn_regex_error_free(WynRegexError* error) {
    if (!error) return;
    free(error->message);
    free(error);
}

const char* wyn_regex_error_string(WynRegexErrorCode code) {
    switch (code) {
        case WYN_REGEX_OK: return "No error";
        case WYN_REGEX_INVALID_PATTERN: return "Invalid regex pattern";
        case WYN_REGEX_COMPILATION_ERROR: return "Regex compilation error";
        case WYN_REGEX_MEMORY_ERROR: return "Memory allocation error";
        case WYN_REGEX_INVALID_GROUP: return "Invalid capture group";
        case WYN_REGEX_UNICODE_ERROR: return "Unicode processing error";
        default: return "Unknown regex error";
    }
}

// Unicode character classes
bool wyn_regex_is_word_char(uint32_t codepoint) {
    return wyn_unicode_is_alphanumeric(codepoint) || codepoint == '_';
}

bool wyn_regex_is_digit_char(uint32_t codepoint) {
    return wyn_unicode_is_numeric(codepoint);
}

bool wyn_regex_is_space_char(uint32_t codepoint) {
    return wyn_unicode_is_whitespace(codepoint);
}

// Simplified pattern parsing (basic implementation)
WynRegexNode* wyn_regex_parse_pattern(const char* pattern, size_t* pos, WynRegexError* error) {
    if (!pattern || !pos) {
        if (error) {
            *error = *wyn_regex_error_new(WYN_REGEX_INVALID_PATTERN, "Invalid parameters", 0);
        }
        return NULL;
    }
    
    // Convert pattern to WynString for proper Unicode handling
    WynString* pattern_str = wyn_string_from_cstr(pattern);
    if (!pattern_str) {
        if (error) {
            *error = *wyn_regex_error_new(WYN_REGEX_UNICODE_ERROR, "Failed to parse pattern as UTF-8", 0);
        }
        return NULL;
    }
    
    size_t pattern_len = wyn_string_char_count(pattern_str);
    if (pattern_len == 0) {
        wyn_string_free(pattern_str);
        if (error) {
            error->code = WYN_REGEX_OK;
            error->message = NULL;
            error->position = 0;
        }
        return NULL;
    }
    
    // Create a chain of literal nodes from Unicode characters
    WynRegexNode* root = NULL;
    WynRegexNode* current = NULL;
    
    WynStringIterator* iter = wyn_string_iter(pattern_str);
    uint32_t codepoint;
    size_t char_pos = 0;
    
    while (wyn_string_iter_next(iter, &codepoint)) {
        WynRegexNode* node = malloc(sizeof(WynRegexNode));
        if (!node) {
            wyn_string_iter_free(iter);
            wyn_string_free(pattern_str);
            if (error) {
                *error = *wyn_regex_error_new(WYN_REGEX_MEMORY_ERROR, "Memory allocation failed", char_pos);
            }
            return NULL;
        }
        
        memset(node, 0, sizeof(WynRegexNode));
        
        // Handle special characters
        switch (codepoint) {
            case '.':
                node->type = REGEX_DOT;
                break;
            case '^':
                node->type = REGEX_CARET;
                break;
            case '$':
                node->type = REGEX_DOLLAR;
                break;
            default:
                // Regular character
                node->type = REGEX_LITERAL;
                node->codepoint = codepoint;
                break;
        }
        
        // Link nodes together
        if (!root) {
            root = node;
            current = node;
        } else {
            current->next = node;
            current = node;
        }
        
        char_pos++;
    }
    
    wyn_string_iter_free(iter);
    wyn_string_free(pattern_str);
    *pos = strlen(pattern);  // Set to end of byte string
    
    if (error) {
        error->code = WYN_REGEX_OK;
        error->message = NULL;
        error->position = 0;
    }
    
    return root;
}

// Simplified matching engine
bool wyn_regex_match_node(const WynRegexNode* node, const WynString* text, size_t pos, 
                         size_t* end_pos, WynCaptures* captures) {
    (void)captures; // Suppress unused parameter warning
    
    if (!node || !text) return false;
    
    size_t text_len = wyn_string_char_count((WynString*)text);
    size_t current_pos = pos;
    const WynRegexNode* current_node = node;
    
    // Match sequence of nodes
    while (current_node) {
        switch (current_node->type) {
            case REGEX_LITERAL: {
                if (current_pos >= text_len) return false;
                
                // Get character at position using substring approach
                WynString* char_str = wyn_string_substring(text, current_pos, current_pos + 1);
                if (!char_str || wyn_string_is_empty(char_str)) {
                    wyn_string_free(char_str);
                    return false;
                }
                
                // Get the first (and only) character
                WynStringIterator* iter = wyn_string_iter(char_str);
                uint32_t codepoint = 0;
                bool has_char = wyn_string_iter_next(iter, &codepoint);
                wyn_string_iter_free(iter);
                wyn_string_free(char_str);
                
                if (!has_char) return false;
                
                bool match = wyn_regex_match_literal(current_node->codepoint, codepoint, false);
                if (!match) return false;
                
                current_pos++;
                break;
            }
            
            case REGEX_DOT: {
                if (current_pos >= text_len) return false;
                current_pos++;
                break;
            }
            
            case REGEX_CARET: {
                if (current_pos != 0) return false;
                // Don't advance position for anchors
                break;
            }
            
            case REGEX_DOLLAR: {
                if (current_pos != text_len) return false;
                // Don't advance position for anchors
                break;
            }
            
            default:
                return false;
        }
        
        current_node = current_node->next;
    }
    
    if (end_pos) *end_pos = current_pos;
    return true;
}

bool wyn_regex_match_literal(uint32_t pattern_char, uint32_t text_char, bool case_insensitive) {
    if (case_insensitive) {
        return wyn_unicode_to_lowercase(pattern_char) == wyn_unicode_to_lowercase(text_char);
    }
    return pattern_char == text_char;
}

bool wyn_regex_match_char_class(const WynRegexNode* node, uint32_t codepoint) {
    if (!node || node->type != REGEX_BRACKET) return false;
    
    // Simple implementation - check if codepoint is in character class
    for (size_t i = 0; i < node->char_class_size; i++) {
        if (node->char_class[i] == codepoint) {
            return !node->negated;
        }
    }
    
    return node->negated;
}

// Optimization utilities (simplified)
WynRegexNode* wyn_regex_optimize_pattern(WynRegexNode* root) {
    // Basic optimization - just return as-is for now
    return root;
}

bool wyn_regex_is_literal_string(const WynRegexNode* node) {
    return node && node->type == REGEX_LITERAL;
}

WynString* wyn_regex_extract_literal_prefix(const WynRegexNode* node) {
    if (!wyn_regex_is_literal_string(node)) return NULL;
    
    WynString* result = wyn_string_new();
    wyn_string_push_char(result, node->codepoint);
    return result;
}

// Regex feature implementations (not yet implemented)
WynRegexNode* wyn_regex_parse_group(const char* pattern, size_t* pos, int* group_id, WynRegexError* error) {
    // Groups not implemented
    if (error) {
        *error = *wyn_regex_error_new(WYN_REGEX_COMPILATION_ERROR, "Groups not implemented", *pos);
    }
    return NULL;
}

WynRegexNode* wyn_regex_parse_bracket(const char* pattern, size_t* pos, WynRegexError* error) {
    // Bracket expressions not implemented
    if (error) {
        *error = *wyn_regex_error_new(WYN_REGEX_COMPILATION_ERROR, "Bracket expressions not implemented", *pos);
    }
    return NULL;
}

WynRegexNode* wyn_regex_parse_quantifier(const char* pattern, size_t* pos, WynRegexNode* node, WynRegexError* error) {
    // Quantifiers not implemented
    if (error) {
        *error = *wyn_regex_error_new(WYN_REGEX_COMPILATION_ERROR, "Quantifiers not implemented", *pos);
    }
    return node;
}
