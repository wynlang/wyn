#ifndef WYN_UNICODE_H
#define WYN_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynString WynString;
typedef struct WynStringIterator WynStringIterator;

// UTF-8 validation result
typedef enum {
    WYN_UTF8_VALID,
    WYN_UTF8_INVALID_SEQUENCE,
    WYN_UTF8_INCOMPLETE_SEQUENCE,
    WYN_UTF8_OVERLONG_ENCODING,
    WYN_UTF8_INVALID_CODEPOINT
} WynUtf8Error;

// Unicode string structure
typedef struct WynString {
    uint8_t* data;
    size_t byte_len;
    size_t byte_cap;
    size_t char_count;  // Cached character count
    bool char_count_valid;
} WynString;

// String iterator for Unicode characters
typedef struct WynStringIterator {
    const WynString* string;
    size_t byte_pos;
    size_t char_pos;
} WynStringIterator;

// String creation and destruction
WynString* wyn_string_new(void);
WynString* wyn_string_from_cstr(const char* cstr);
WynString* wyn_string_from_utf8(const uint8_t* bytes, size_t len, WynUtf8Error* error);
WynString* wyn_string_with_capacity(size_t capacity);
void wyn_string_free(WynString* str);

// String properties
size_t wyn_string_byte_len(const WynString* str);
size_t wyn_string_char_count(WynString* str);
size_t wyn_string_capacity(const WynString* str);
bool wyn_string_is_empty(const WynString* str);
const char* wyn_string_as_cstr(const WynString* str);
const uint8_t* wyn_string_as_bytes(const WynString* str);

// String modification
WynUtf8Error wyn_string_push_char(WynString* str, uint32_t codepoint);
WynUtf8Error wyn_string_push_str(WynString* str, const char* cstr);
WynUtf8Error wyn_string_push_string(WynString* str, const WynString* other);
bool wyn_string_pop_char(WynString* str, uint32_t* codepoint);
void wyn_string_clear(WynString* str);
WynUtf8Error wyn_string_insert_char(WynString* str, size_t char_index, uint32_t codepoint);
WynUtf8Error wyn_string_insert_str(WynString* str, size_t char_index, const char* cstr);
bool wyn_string_remove_char(WynString* str, size_t char_index, uint32_t* removed);

// String operations
WynString* wyn_string_substring(const WynString* str, size_t start, size_t end);
WynString* wyn_string_clone(const WynString* str);
int wyn_string_compare(const WynString* a, const WynString* b);
bool wyn_string_equals(const WynString* a, const WynString* b);
bool wyn_string_starts_with(const WynString* str, const char* prefix);
bool wyn_string_ends_with(const WynString* str, const char* suffix);
bool wyn_string_contains(const WynString* str, const char* needle);
int wyn_string_find(const WynString* str, const char* needle);

// String transformation
WynString* wyn_string_to_uppercase(const WynString* str);
WynString* wyn_string_to_lowercase(const WynString* str);
WynString* wyn_string_trim(const WynString* str);
WynString* wyn_string_trim_start(const WynString* str);
WynString* wyn_string_trim_end(const WynString* str);

// String iteration
WynStringIterator* wyn_string_iter(const WynString* str);
bool wyn_string_iter_next(WynStringIterator* iter, uint32_t* codepoint);
void wyn_string_iter_free(WynStringIterator* iter);

// UTF-8 utilities
WynUtf8Error wyn_utf8_validate(const uint8_t* bytes, size_t len);
size_t wyn_utf8_char_count(const uint8_t* bytes, size_t len);
size_t wyn_utf8_encode_char(uint32_t codepoint, uint8_t* buffer);
size_t wyn_utf8_decode_char(const uint8_t* bytes, size_t len, uint32_t* codepoint);
bool wyn_utf8_is_continuation_byte(uint8_t byte);
size_t wyn_utf8_char_byte_len(uint8_t first_byte);

// Unicode character classification
bool wyn_unicode_is_alphabetic(uint32_t codepoint);
bool wyn_unicode_is_numeric(uint32_t codepoint);
bool wyn_unicode_is_alphanumeric(uint32_t codepoint);
bool wyn_unicode_is_whitespace(uint32_t codepoint);
bool wyn_unicode_is_uppercase(uint32_t codepoint);
bool wyn_unicode_is_lowercase(uint32_t codepoint);
uint32_t wyn_unicode_to_uppercase(uint32_t codepoint);
uint32_t wyn_unicode_to_lowercase(uint32_t codepoint);

// Error handling
const char* wyn_utf8_error_string(WynUtf8Error error);

#endif // WYN_UNICODE_H
