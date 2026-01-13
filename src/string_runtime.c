#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_memory.h"
#include "string_runtime.h"
#include "arc_runtime.h"
#include "wyn_string.h"

// Simple string concatenation for codegen (returns malloc'd string)
const char* wyn_string_concat_safe(const char* left, const char* right) {
    if (!left || !right) return NULL;
    
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    size_t total_len = left_len + right_len;
    
    char* result = malloc(total_len + 1);
    if (!result) return NULL;
    
    memcpy(result, left, left_len);
    memcpy(result + left_len, right, right_len);
    result[total_len] = '\0';
    
    return result;
}

// Get C string (identity function for now)
const char* wyn_string_get_cstr(void* str) {
    return (const char*)str;
}

// Safe string assignment for variables
void wyn_assign_string_var(char** dest, const char* src) {
    if (!dest) return;
    
    // Free old string if exists
    if (*dest) {
        free(*dest);
        *dest = NULL;
    }
    
    // Assign new string
    if (src) {
        size_t len = strlen(src);
        *dest = malloc(len + 1);
        if (*dest) {
            memcpy(*dest, src, len + 1);
        }
    }
}

// String comparison for generated code
int wyn_string_compare_safe(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

// String length for generated code
size_t wyn_string_length_safe(const char* str) {
    return str ? strlen(str) : 0;
}

// String contains check for generated code
bool wyn_string_contains_safe(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

// String starts with check
bool wyn_string_starts_with_safe(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t prefix_len = strlen(prefix);
    size_t str_len = strlen(str);
    if (prefix_len > str_len) return false;
    return memcmp(str, prefix, prefix_len) == 0;
}

// String ends with check
bool wyn_string_ends_with_safe(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t suffix_len = strlen(suffix);
    size_t str_len = strlen(str);
    if (suffix_len > str_len) return false;
    return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// String substring extraction
const char* wyn_string_substring_safe(const char* str, size_t start, size_t end) {
    if (!str) return NULL;
    
    size_t str_len = strlen(str);
    if (start >= str_len || start >= end) return NULL;
    if (end > str_len) end = str_len;
    
    size_t sub_len = end - start;
    char* result = malloc(sub_len + 1);
    if (!result) return NULL;
    
    memcpy(result, str + start, sub_len);
    result[sub_len] = '\0';
    
    return result;
}

// String replace operation (simple version - replaces first occurrence)
const char* wyn_string_replace_safe(const char* str, const char* old_str, const char* new_str) {
    if (!str || !old_str || !new_str) return str;
    
    const char* pos = strstr(str, old_str);
    if (!pos) {
        // No replacement needed, return copy
        size_t len = strlen(str);
        char* result = malloc(len + 1);
        if (result) {
            memcpy(result, str, len + 1);
        }
        return result;
    }
    
    size_t str_len = strlen(str);
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    size_t result_len = str_len - old_len + new_len;
    
    char* result = malloc(result_len + 1);
    if (!result) return str;
    
    // Copy parts
    size_t prefix_len = pos - str;
    memcpy(result, str, prefix_len);
    memcpy(result + prefix_len, new_str, new_len);
    memcpy(result + prefix_len + new_len, pos + old_len, str_len - prefix_len - old_len);
    result[result_len] = '\0';
    
    return result;
}

// String to uppercase
const char* wyn_string_to_upper_safe(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return str;
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        result[i] = (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
    }
    result[len] = '\0';
    
    return result;
}

// String to lowercase
const char* wyn_string_to_lower_safe(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return str;
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        result[i] = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
    }
    result[len] = '\0';
    
    return result;
}

// String trim (remove whitespace)
const char* wyn_string_trim_safe(const char* str) {
    if (!str) return NULL;
    
    // Find start of non-whitespace
    const char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    // Find end of non-whitespace
    const char* end = str + strlen(str) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    size_t len = end - start + 1;
    if (len == 0) {
        // Return empty string
        char* result = malloc(1);
        if (result) result[0] = '\0';
        return result;
    }
    
    char* result = malloc(len + 1);
    if (!result) return str;
    
    memcpy(result, start, len);
    result[len] = '\0';
    
    return result;
}