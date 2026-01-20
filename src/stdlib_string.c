#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// String module - comprehensive string manipulation functions

// Length: get string length
int wyn_string_len(const char* str) {
    return strlen(str);
}

// Contains: check if string contains substring
int wyn_string_contains(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}

// Starts with: check if string starts with prefix
int wyn_string_starts_with(const char* str, const char* prefix) {
    size_t len = strlen(prefix);
    return strncmp(str, prefix, len) == 0;
}

// Ends with: check if string ends with suffix
int wyn_string_ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// To upper: convert string to uppercase
char* wyn_string_to_upper(const char* str) {
    char* result = malloc(strlen(str) + 1);
    for (int i = 0; str[i]; i++) {
        result[i] = toupper(str[i]);
    }
    result[strlen(str)] = '\0';
    return result;
}

// To lower: convert string to lowercase
char* wyn_string_to_lower(const char* str) {
    char* result = malloc(strlen(str) + 1);
    for (int i = 0; str[i]; i++) {
        result[i] = tolower(str[i]);
    }
    result[strlen(str)] = '\0';
    return result;
}

// Trim: remove leading/trailing whitespace
char* wyn_string_trim(const char* str) {
    while (isspace(*str)) str++;
    if (*str == 0) return strdup("");
    
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    
    size_t len = end - str + 1;
    char* result = malloc(len + 1);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

// Replace: replace all occurrences of old with new
char* wyn_str_replace(const char* str, const char* old, const char* new) {
    if (!str || !old || !new) return strdup(str ? str : "");
    
    size_t old_len = strlen(old);
    size_t new_len = strlen(new);
    if (old_len == 0) return strdup(str);
    
    // Count occurrences
    int count = 0;
    const char* p = str;
    while ((p = strstr(p, old)) != NULL) {
        count++;
        p += old_len;
    }
    
    if (count == 0) return strdup(str);
    
    // Allocate result
    size_t result_len = strlen(str) + count * (new_len - old_len);
    char* result = malloc(result_len + 1);
    char* dst = result;
    
    // Replace
    p = str;
    while (*p) {
        const char* match = strstr(p, old);
        if (match == p) {
            memcpy(dst, new, new_len);
            dst += new_len;
            p += old_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}

// Split: split string by delimiter
char** wyn_string_split(const char* str, const char* delim, int* count) {
    if (!str || !delim) {
        *count = 0;
        return NULL;
    }
    
    // Count parts
    int parts = 1;
    const char* p = str;
    while ((p = strstr(p, delim)) != NULL) {
        parts++;
        p += strlen(delim);
    }
    
    // Allocate array
    char** result = malloc(sizeof(char*) * parts);
    *count = parts;
    
    // Split
    char* str_copy = strdup(str);
    char* token = strtok(str_copy, delim);
    int i = 0;
    while (token && i < parts) {
        result[i++] = strdup(token);
        token = strtok(NULL, delim);
    }
    free(str_copy);
    
    return result;
}

// Join: join array of strings with delimiter
char* wyn_string_join(char** strings, int count, const char* delim) {
    if (!strings || count == 0) return strdup("");
    
    // Calculate total length
    size_t total_len = 0;
    size_t delim_len = strlen(delim);
    for (int i = 0; i < count; i++) {
        total_len += strlen(strings[i]);
        if (i < count - 1) total_len += delim_len;
    }
    
    // Build result
    char* result = malloc(total_len + 1);
    char* p = result;
    for (int i = 0; i < count; i++) {
        size_t len = strlen(strings[i]);
        memcpy(p, strings[i], len);
        p += len;
        if (i < count - 1) {
            memcpy(p, delim, delim_len);
            p += delim_len;
        }
    }
    *p = '\0';
    return result;
}

// Substring: extract substring
char* wyn_str_substring(const char* str, int start, int end) {
    int len = strlen(str);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");
    
    int sub_len = end - start;
    char* result = malloc(sub_len + 1);
    memcpy(result, str + start, sub_len);
    result[sub_len] = '\0';
    return result;
}

// Index of: find first occurrence of substring
int wyn_string_index_of(const char* str, const char* substr) {
    const char* p = strstr(str, substr);
    return p ? (int)(p - str) : -1;
}

// Last index of: find last occurrence of substring
int wyn_string_last_index_of(const char* str, const char* substr) {
    const char* last = NULL;
    const char* p = str;
    while ((p = strstr(p, substr)) != NULL) {
        last = p;
        p++;
    }
    return last ? (int)(last - str) : -1;
}

// Repeat: repeat string n times
char* wyn_string_repeat(const char* str, int n) {
    if (n <= 0) return strdup("");
    size_t len = strlen(str);
    char* result = malloc(len * n + 1);
    char* p = result;
    for (int i = 0; i < n; i++) {
        memcpy(p, str, len);
        p += len;
    }
    *p = '\0';
    return result;
}

// Reverse: reverse string
char* wyn_string_reverse(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        result[i] = str[len - 1 - i];
    }
    result[len] = '\0';
    return result;
}
