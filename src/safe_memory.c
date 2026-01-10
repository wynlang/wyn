#include "safe_memory.h"
#include <stdio.h>

void* safe_malloc(size_t size) {
    if (size == 0) return NULL;
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed for size %zu\n", size);
        exit(1);
    }
    return ptr;
}

void* safe_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    void* ptr = calloc(nmemb, size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed for %zu * %zu\n", nmemb, size);
        exit(1);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t size) {
    if (size == 0) {
        safe_free(ptr);
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Memory reallocation failed for size %zu\n", size);
        exit(1);
    }
    return new_ptr;
}

char* safe_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = safe_malloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// T1.1.4: Safe string concatenation with bounds checking
char* safe_strncat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return NULL;
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        return NULL; // Would overflow
    }
    
    strncat(dest, src, dest_size - dest_len - 1);
    return dest;
}

// T1.1.4: Safe realloc with old size tracking
void* safe_realloc_with_old_size(void* ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        safe_free(ptr);
        return NULL;
    }
    
    void* new_ptr = safe_malloc(new_size);
    if (!new_ptr) return NULL;
    
    if (ptr && old_size > 0) {
        size_t copy_size = (old_size < new_size) ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        safe_free(ptr);
    }
    
    return new_ptr;
}

// T1.1.4: Safe array access validation
bool safe_array_access(void* array, size_t index, size_t length, size_t element_size) {
    if (!array || element_size == 0) return false;
    return index < length;
}

// T1.1.4: Safe buffer copy with bounds checking
bool safe_buffer_copy(void* dest, size_t dest_size, const void* src, size_t src_size) {
    if (!dest || !src || dest_size == 0 || src_size == 0) return false;
    if (src_size > dest_size) return false;
    
    memcpy(dest, src, src_size);
    return true;
}

// T1.1.4: Safe string concatenation
char* safe_string_concat(const char* str1, const char* str2) {
    if (!str1 || !str2) return NULL;
    
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    
    char* result = safe_malloc(len1 + len2 + 1);
    if (!result) return NULL;
    
    memcpy(result, str1, len1);
    memcpy(result + len1, str2, len2 + 1);
    
    return result;
}

// Memory validation functions
bool validate_memory_range(void* ptr, size_t size) {
    return ptr != NULL && size > 0;
}

bool validate_string_bounds(const char* str, size_t max_len) {
    if (!str) return false;
    return strnlen(str, max_len + 1) <= max_len;
}
