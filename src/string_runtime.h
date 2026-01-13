#ifndef STRING_RUNTIME_H
#define STRING_RUNTIME_H

#include <stddef.h>
#include <stdbool.h>

// Runtime support functions for safe string operations in generated code

// Safe string operations for codegen
const char* wyn_string_concat_safe(const char* left, const char* right);
const char* wyn_string_get_cstr(void* str);
void wyn_assign_string_var(char** dest, const char* src);

// String comparison and utility functions
int wyn_string_compare_safe(const char* a, const char* b);
size_t wyn_string_length_safe(const char* str);
bool wyn_string_contains_safe(const char* haystack, const char* needle);
bool wyn_string_starts_with_safe(const char* str, const char* prefix);
bool wyn_string_ends_with_safe(const char* str, const char* suffix);

// String manipulation functions
const char* wyn_string_substring_safe(const char* str, size_t start, size_t end);
const char* wyn_string_replace_safe(const char* str, const char* old_str, const char* new_str);
const char* wyn_string_to_upper_safe(const char* str);
const char* wyn_string_to_lower_safe(const char* str);
const char* wyn_string_trim_safe(const char* str);

#endif // STRING_RUNTIME_H