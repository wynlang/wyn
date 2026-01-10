#ifndef SAFE_MEMORY_H
#define SAFE_MEMORY_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Safe memory allocation functions
void* safe_malloc(size_t size);
void* safe_calloc(size_t nmemb, size_t size);
void* safe_realloc(void* ptr, size_t size);
char* safe_strdup(const char* str);
void safe_free(void* ptr);

// T1.1.4: Additional safe memory utilities
char* safe_strncat(char* dest, const char* src, size_t dest_size);
void* safe_realloc_with_old_size(void* ptr, size_t old_size, size_t new_size);
bool safe_array_access(void* array, size_t index, size_t length, size_t element_size);
bool safe_buffer_copy(void* dest, size_t dest_size, const void* src, size_t src_size);
char* safe_string_concat(const char* str1, const char* str2);

// Memory validation
int validate_pointer(void* ptr);
bool validate_memory_range(void* ptr, size_t size);
bool validate_string_bounds(const char* str, size_t max_len);

#endif
