#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// T1.3.1: String Data Structure Design
// Memory-efficient string representation with small string optimization
#define SMALL_STRING_SIZE 23

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
    bool is_heap_allocated;
    uint32_t hash_cache;  // Cached hash value
    bool hash_valid;
} WynString;

// Small string optimization structure
typedef struct {
    union {
        struct {
            char* data;
            size_t length;
            size_t capacity;
        } heap;
        struct {
            char data[SMALL_STRING_SIZE];
            uint8_t length;  // MSB indicates heap/stack
        } stack;
    };
    uint32_t hash_cache;
    bool hash_valid;
} OptimizedWynString;

// Core string functions
WynString* wyn_string_new(const char* cstr);
WynString* wyn_string_with_capacity(size_t capacity);
void wyn_string_free(WynString* str);

// UTF-8 support functions
bool wyn_string_is_valid_utf8(const char* str, size_t len);
size_t wyn_string_utf8_length(const WynString* str);
bool wyn_string_utf8_validate(WynString* str);

// Hash functions
uint32_t wyn_string_hash(WynString* str);
void wyn_string_invalidate_hash(WynString* str);

// T1.3.2: Basic String Operations
WynString* wyn_string_concat(WynString* a, WynString* b);
int wyn_string_compare(WynString* a, WynString* b);
WynString* wyn_string_copy(WynString* src);
WynString* wyn_string_substring(WynString* str, size_t start, size_t end);
size_t wyn_string_find(WynString* haystack, WynString* needle);
WynString* wyn_string_replace(WynString* str, WynString* old, WynString* new_str);

// T1.3.3: String Interpolation Parser
typedef struct StringInterpPart {
    enum {
        INTERP_TEXT,
        INTERP_EXPR
    } type;
    union {
        WynString* text;
        struct Expr* expr;  // Forward declaration
    };
    struct StringInterpPart* next;
} StringInterpPart;

typedef struct {
    StringInterpPart* parts;
    size_t part_count;
} StringInterpolation;

// String interpolation functions
StringInterpolation* parse_string_interpolation(const char* str, size_t len);
void free_string_interpolation(StringInterpolation* interp);
WynString* evaluate_string_interpolation(StringInterpolation* interp);

// T1.3.4: String Method Integration
typedef enum {
    STRING_METHOD_LENGTH,
    STRING_METHOD_UPPER,
    STRING_METHOD_LOWER,
    STRING_METHOD_TRIM,
    STRING_METHOD_CONTAINS
} StringMethodType;

// String method functions
size_t wyn_string_method_length(WynString* str);
WynString* wyn_string_method_upper(WynString* str);
WynString* wyn_string_method_lower(WynString* str);
WynString* wyn_string_method_trim(WynString* str);
bool wyn_string_method_contains(WynString* str, WynString* substr);

#endif
