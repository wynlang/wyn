// String concatenation runtime support for Wyn
// This file provides the missing wyn_string_concat_safe function

#include <stdlib.h>
#include <string.h>

// Safe string concatenation that allocates new memory
const char* wyn_string_concat_safe(const char* left, const char* right) {
    if (!left) left = "";
    if (!right) right = "";
    
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    size_t total_len = left_len + right_len;
    
    char* result = (char*)malloc(total_len + 1);
    if (!result) {
        // Out of memory - return empty string rather than crash
        return "";
    }
    
    memcpy(result, left, left_len);
    memcpy(result + left_len, right, right_len);
    result[total_len] = '\0';
    
    return result;
}
