#include "wyn_string.h"
#include "safe_memory.h"
#include <string.h>
#include <stdlib.h>

// T1.3.1: String Data Structure Implementation

// Core string functions
WynString* wyn_string_new(const char* cstr) {
    WynString* str = safe_malloc(sizeof(WynString));
    if (!str) return NULL;
    
    if (cstr) {
        str->length = strlen(cstr);
        str->capacity = str->length + 1;
        str->data = safe_malloc(str->capacity);
        if (!str->data) {
            safe_free(str);
            return NULL;
        }
        memcpy(str->data, cstr, str->length + 1);
    } else {
        str->length = 0;
        str->capacity = 1;
        str->data = safe_malloc(1);
        if (!str->data) {
            safe_free(str);
            return NULL;
        }
        str->data[0] = '\0';
    }
    
    str->is_heap_allocated = true;
    str->hash_valid = false;
    str->hash_cache = 0;
    
    return str;
}

WynString* wyn_string_with_capacity(size_t capacity) {
    WynString* str = safe_malloc(sizeof(WynString));
    if (!str) return NULL;
    
    str->capacity = capacity > 0 ? capacity : 1;
    str->data = safe_malloc(str->capacity);
    if (!str->data) {
        safe_free(str);
        return NULL;
    }
    
    str->length = 0;
    str->data[0] = '\0';
    str->is_heap_allocated = true;
    str->hash_valid = false;
    str->hash_cache = 0;
    
    return str;
}

void wyn_string_free(WynString* str) {
    if (!str) return;
    
    if (str->is_heap_allocated && str->data) {
        safe_free(str->data);
    }
    safe_free(str);
}

// UTF-8 support functions
bool wyn_string_is_valid_utf8(const char* str, size_t len) {
    if (!str) return false;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        
        if (c < 0x80) {
            // ASCII character
            continue;
        } else if ((c >> 5) == 0x06) {
            // 110xxxxx - 2 byte sequence
            if (i + 1 >= len || (str[i + 1] & 0xC0) != 0x80) return false;
            i++;
        } else if ((c >> 4) == 0x0E) {
            // 1110xxxx - 3 byte sequence
            if (i + 2 >= len || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c >> 3) == 0x1E) {
            // 11110xxx - 4 byte sequence
            if (i + 3 >= len || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80) return false;
            i += 3;
        } else {
            return false;
        }
    }
    
    return true;
}

size_t wyn_string_utf8_length(const WynString* str) {
    if (!str || !str->data) return 0;
    
    size_t utf8_len = 0;
    for (size_t i = 0; i < str->length; i++) {
        unsigned char c = str->data[i];
        if ((c & 0xC0) != 0x80) {
            utf8_len++;
        }
    }
    
    return utf8_len;
}

bool wyn_string_utf8_validate(WynString* str) {
    if (!str || !str->data) return false;
    return wyn_string_is_valid_utf8(str->data, str->length);
}

// Hash functions (FNV-1a hash)
uint32_t wyn_string_hash(WynString* str) {
    if (!str || !str->data) return 0;
    
    if (str->hash_valid) {
        return str->hash_cache;
    }
    
    uint32_t hash = 2166136261u; // FNV offset basis
    for (size_t i = 0; i < str->length; i++) {
        hash ^= (unsigned char)str->data[i];
        hash *= 16777619u; // FNV prime
    }
    
    str->hash_cache = hash;
    str->hash_valid = true;
    
    return hash;
}

void wyn_string_invalidate_hash(WynString* str) {
    if (str) {
        str->hash_valid = false;
        str->hash_cache = 0;
    }
}

// T1.3.2: Basic String Operations Implementation

WynString* wyn_string_concat(WynString* a, WynString* b) {
    if (!a || !b) return NULL;
    
    size_t new_length = a->length + b->length;
    WynString* result = wyn_string_with_capacity(new_length + 1);
    if (!result) return NULL;
    
    memcpy(result->data, a->data, a->length);
    memcpy(result->data + a->length, b->data, b->length);
    result->data[new_length] = '\0';
    result->length = new_length;
    
    return result;
}

int wyn_string_compare(WynString* a, WynString* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    size_t min_len = a->length < b->length ? a->length : b->length;
    int cmp = memcmp(a->data, b->data, min_len);
    
    if (cmp == 0) {
        if (a->length < b->length) return -1;
        if (a->length > b->length) return 1;
        return 0;
    }
    
    return cmp;
}

WynString* wyn_string_copy(WynString* src) {
    if (!src) return NULL;
    
    return wyn_string_new(src->data);
}

WynString* wyn_string_substring(WynString* str, size_t start, size_t end) {
    if (!str || start >= str->length || start >= end) return NULL;
    
    if (end > str->length) end = str->length;
    
    size_t sub_length = end - start;
    WynString* result = wyn_string_with_capacity(sub_length + 1);
    if (!result) return NULL;
    
    memcpy(result->data, str->data + start, sub_length);
    result->data[sub_length] = '\0';
    result->length = sub_length;
    
    return result;
}

size_t wyn_string_find(WynString* haystack, WynString* needle) {
    if (!haystack || !needle || needle->length == 0) return SIZE_MAX;
    if (needle->length > haystack->length) return SIZE_MAX;
    
    for (size_t i = 0; i <= haystack->length - needle->length; i++) {
        if (memcmp(haystack->data + i, needle->data, needle->length) == 0) {
            return i;
        }
    }
    
    return SIZE_MAX;
}

WynString* wyn_string_replace(WynString* str, WynString* old, WynString* new_str) {
    if (!str || !old || !new_str || old->length == 0) return NULL;
    
    size_t pos = wyn_string_find(str, old);
    if (pos == SIZE_MAX) {
        return wyn_string_copy(str);
    }
    
    size_t new_length = str->length - old->length + new_str->length;
    WynString* result = wyn_string_with_capacity(new_length + 1);
    if (!result) return NULL;
    
    // Copy before the match
    memcpy(result->data, str->data, pos);
    
    // Copy the replacement
    memcpy(result->data + pos, new_str->data, new_str->length);
    
    // Copy after the match
    memcpy(result->data + pos + new_str->length, 
           str->data + pos + old->length, 
           str->length - pos - old->length);
    
    result->data[new_length] = '\0';
    result->length = new_length;
    
    return result;
}

// T1.3.3: String Interpolation Parser Implementation

StringInterpolation* parse_string_interpolation(const char* str, size_t len) {
    if (!str || len == 0) return NULL;
    
    StringInterpolation* interp = safe_malloc(sizeof(StringInterpolation));
    if (!interp) return NULL;
    
    interp->parts = NULL;
    interp->part_count = 0;
    
    StringInterpPart* last_part = NULL;
    
    size_t i = 0;
    size_t text_start = 0;
    
    while (i < len) {
        if (str[i] == '{' && i + 1 < len) {
            // Add text part before the expression (if any)
            if (i > text_start) {
                StringInterpPart* text_part = safe_malloc(sizeof(StringInterpPart));
                if (!text_part) {
                    free_string_interpolation(interp);
                    return NULL;
                }
                
                text_part->type = INTERP_TEXT;
                
                // Create text string
                size_t text_len = i - text_start;
                char* text_data = safe_malloc(text_len + 1);
                if (!text_data) {
                    safe_free(text_part);
                    free_string_interpolation(interp);
                    return NULL;
                }
                
                memcpy(text_data, str + text_start, text_len);
                text_data[text_len] = '\0';
                text_part->text = wyn_string_new(text_data);
                safe_free(text_data);
                
                text_part->next = NULL;
                
                if (!interp->parts) {
                    interp->parts = text_part;
                } else {
                    last_part->next = text_part;
                }
                last_part = text_part;
                interp->part_count++;
            }
            
            // Find the end of the expression
            size_t expr_start = i + 1;
            size_t expr_end = expr_start;
            int brace_count = 1;
            
            while (expr_end < len && brace_count > 0) {
                if (str[expr_end] == '{') {
                    brace_count++;
                } else if (str[expr_end] == '}') {
                    brace_count--;
                }
                expr_end++;
            }
            
            if (brace_count > 0) {
                // Malformed interpolation - missing closing brace
                free_string_interpolation(interp);
                return NULL;
            }
            
            // Create expression part (simplified - just store as text for now)
            StringInterpPart* expr_part = safe_malloc(sizeof(StringInterpPart));
            if (!expr_part) {
                free_string_interpolation(interp);
                return NULL;
            }
            
            expr_part->type = INTERP_EXPR;
            expr_part->expr = NULL; // Simplified for minimal implementation
            expr_part->next = NULL;
            
            if (!interp->parts) {
                interp->parts = expr_part;
            } else {
                last_part->next = expr_part;
            }
            last_part = expr_part;
            interp->part_count++;
            
            i = expr_end;
            text_start = i;
        } else {
            i++;
        }
    }
    
    // Add remaining text (if any)
    if (text_start < len) {
        StringInterpPart* text_part = safe_malloc(sizeof(StringInterpPart));
        if (!text_part) {
            free_string_interpolation(interp);
            return NULL;
        }
        
        text_part->type = INTERP_TEXT;
        
        size_t text_len = len - text_start;
        char* text_data = safe_malloc(text_len + 1);
        if (!text_data) {
            safe_free(text_part);
            free_string_interpolation(interp);
            return NULL;
        }
        
        memcpy(text_data, str + text_start, text_len);
        text_data[text_len] = '\0';
        text_part->text = wyn_string_new(text_data);
        safe_free(text_data);
        
        text_part->next = NULL;
        
        if (!interp->parts) {
            interp->parts = text_part;
        } else {
            last_part->next = text_part;
        }
        interp->part_count++;
    }
    
    return interp;
}

void free_string_interpolation(StringInterpolation* interp) {
    if (!interp) return;
    
    StringInterpPart* current = interp->parts;
    while (current) {
        StringInterpPart* next = current->next;
        
        if (current->type == INTERP_TEXT && current->text) {
            wyn_string_free(current->text);
        }
        // Note: expr cleanup would be handled by AST cleanup
        
        safe_free(current);
        current = next;
    }
    
    safe_free(interp);
}

WynString* evaluate_string_interpolation(StringInterpolation* interp) {
    if (!interp || !interp->parts) return wyn_string_new("");
    
    // For minimal implementation, just concatenate text parts
    WynString* result = wyn_string_new("");
    if (!result) return NULL;
    
    StringInterpPart* current = interp->parts;
    while (current) {
        if (current->type == INTERP_TEXT && current->text) {
            WynString* new_result = wyn_string_concat(result, current->text);
            wyn_string_free(result);
            result = new_result;
            if (!result) return NULL;
        } else if (current->type == INTERP_EXPR) {
            // Simplified: add placeholder for expressions
            WynString* placeholder = wyn_string_new("{expr}");
            if (placeholder) {
                WynString* new_result = wyn_string_concat(result, placeholder);
                wyn_string_free(result);
                wyn_string_free(placeholder);
                result = new_result;
                if (!result) return NULL;
            }
        }
        current = current->next;
    }
    
    return result;
}

// T1.3.4: String Method Integration Implementation

size_t wyn_string_method_length(WynString* str) {
    if (!str) return 0;
    return wyn_string_utf8_length(str);
}

WynString* wyn_string_method_upper(WynString* str) {
    if (!str) return NULL;
    
    WynString* result = wyn_string_copy(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->length; i++) {
        if (result->data[i] >= 'a' && result->data[i] <= 'z') {
            result->data[i] = result->data[i] - 'a' + 'A';
        }
    }
    
    wyn_string_invalidate_hash(result);
    return result;
}

WynString* wyn_string_method_lower(WynString* str) {
    if (!str) return NULL;
    
    WynString* result = wyn_string_copy(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->length; i++) {
        if (result->data[i] >= 'A' && result->data[i] <= 'Z') {
            result->data[i] = result->data[i] - 'A' + 'a';
        }
    }
    
    wyn_string_invalidate_hash(result);
    return result;
}

WynString* wyn_string_method_trim(WynString* str) {
    if (!str || str->length == 0) return wyn_string_new("");
    
    // Find start of non-whitespace
    size_t start = 0;
    while (start < str->length && 
           (str->data[start] == ' ' || str->data[start] == '\t' || 
            str->data[start] == '\n' || str->data[start] == '\r')) {
        start++;
    }
    
    // Find end of non-whitespace
    size_t end = str->length;
    while (end > start && 
           (str->data[end-1] == ' ' || str->data[end-1] == '\t' || 
            str->data[end-1] == '\n' || str->data[end-1] == '\r')) {
        end--;
    }
    
    if (start >= end) return wyn_string_new("");
    
    return wyn_string_substring(str, start, end);
}

bool wyn_string_method_contains(WynString* str, WynString* substr) {
    if (!str || !substr) return false;
    return wyn_string_find(str, substr) != SIZE_MAX;
}
