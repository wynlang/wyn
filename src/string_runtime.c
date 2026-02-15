#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// String runtime functions for Wyn

char* wyn_substring(const char* str, int start, int end) {
    if (!str) return NULL;
    int len = strlen(str);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");
    
    int sub_len = end - start;
    char* result = malloc(sub_len + 1);
    if (!result) return NULL;
    
    memcpy(result, str + start, sub_len);
    result[sub_len] = '\0';
    return result;
}

char* wyn_trim(const char* str) {
    if (!str) return NULL;
    
    // Find first non-whitespace
    const char* start = str;
    while (*start && isspace(*start)) start++;
    
    // Find last non-whitespace
    const char* end = str + strlen(str) - 1;
    while (end > start && isspace(*end)) end--;
    
    // Allocate and copy
    int len = end - start + 1;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char* wyn_replace(const char* str, const char* old, const char* new) {
    if (!str || !old || !new) return NULL;
    
    // Find first occurrence
    const char* pos = strstr(str, old);
    if (!pos) return strdup(str);
    
    int old_len = strlen(old);
    int new_len = strlen(new);
    int prefix_len = pos - str;
    int suffix_len = strlen(pos + old_len);
    
    // Allocate result
    char* result = malloc(prefix_len + new_len + suffix_len + 1);
    if (!result) return NULL;
    
    // Copy parts
    memcpy(result, str, prefix_len);
    memcpy(result + prefix_len, new, new_len);
    memcpy(result + prefix_len + new_len, pos + old_len, suffix_len);
    result[prefix_len + new_len + suffix_len] = '\0';
    
    return result;
}
