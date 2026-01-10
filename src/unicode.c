#include "unicode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// UTF-8 byte patterns
#define UTF8_1_BYTE_MASK 0x80
#define UTF8_1_BYTE_PATTERN 0x00
#define UTF8_2_BYTE_MASK 0xE0
#define UTF8_2_BYTE_PATTERN 0xC0
#define UTF8_3_BYTE_MASK 0xF0
#define UTF8_3_BYTE_PATTERN 0xE0
#define UTF8_4_BYTE_MASK 0xF8
#define UTF8_4_BYTE_PATTERN 0xF0
#define UTF8_CONTINUATION_MASK 0xC0
#define UTF8_CONTINUATION_PATTERN 0x80

// Default string capacity
#define WYN_STRING_DEFAULT_CAPACITY 16

// String creation and destruction
WynString* wyn_string_new(void) {
    return wyn_string_with_capacity(WYN_STRING_DEFAULT_CAPACITY);
}

WynString* wyn_string_from_cstr(const char* cstr) {
    if (!cstr) return NULL;
    
    size_t len = strlen(cstr);
    WynUtf8Error error;
    return wyn_string_from_utf8((const uint8_t*)cstr, len, &error);
}

WynString* wyn_string_from_utf8(const uint8_t* bytes, size_t len, WynUtf8Error* error) {
    if (!bytes) {
        if (error) *error = WYN_UTF8_INVALID_SEQUENCE;
        return NULL;
    }
    
    // Validate UTF-8
    WynUtf8Error validation = wyn_utf8_validate(bytes, len);
    if (validation != WYN_UTF8_VALID) {
        if (error) *error = validation;
        return NULL;
    }
    
    WynString* str = malloc(sizeof(WynString));
    if (!str) {
        if (error) *error = WYN_UTF8_INVALID_SEQUENCE;
        return NULL;
    }
    
    str->data = malloc(len + 1);
    if (!str->data) {
        free(str);
        if (error) *error = WYN_UTF8_INVALID_SEQUENCE;
        return NULL;
    }
    
    memcpy(str->data, bytes, len);
    str->data[len] = '\0';
    str->byte_len = len;
    str->byte_cap = len + 1;
    str->char_count = 0;
    str->char_count_valid = false;
    
    if (error) *error = WYN_UTF8_VALID;
    return str;
}

WynString* wyn_string_with_capacity(size_t capacity) {
    WynString* str = malloc(sizeof(WynString));
    if (!str) return NULL;
    
    str->data = malloc(capacity);
    if (!str->data) {
        free(str);
        return NULL;
    }
    
    str->data[0] = '\0';
    str->byte_len = 0;
    str->byte_cap = capacity;
    str->char_count = 0;
    str->char_count_valid = true;
    
    return str;
}

void wyn_string_free(WynString* str) {
    if (!str) return;
    free(str->data);
    free(str);
}

// Ensure string has enough capacity
static bool wyn_string_ensure_capacity(WynString* str, size_t needed) {
    if (str->byte_cap >= needed) return true;
    
    size_t new_cap = str->byte_cap;
    while (new_cap < needed) {
        new_cap = new_cap == 0 ? WYN_STRING_DEFAULT_CAPACITY : new_cap * 2;
    }
    
    uint8_t* new_data = realloc(str->data, new_cap);
    if (!new_data) return false;
    
    str->data = new_data;
    str->byte_cap = new_cap;
    return true;
}

// String properties
size_t wyn_string_byte_len(const WynString* str) {
    return str ? str->byte_len : 0;
}

size_t wyn_string_char_count(WynString* str) {
    if (!str) return 0;
    
    if (!str->char_count_valid) {
        str->char_count = wyn_utf8_char_count(str->data, str->byte_len);
        str->char_count_valid = true;
    }
    
    return str->char_count;
}

size_t wyn_string_capacity(const WynString* str) {
    return str ? str->byte_cap : 0;
}

bool wyn_string_is_empty(const WynString* str) {
    return str ? (str->byte_len == 0) : true;
}

const char* wyn_string_as_cstr(const WynString* str) {
    return str ? (const char*)str->data : NULL;
}

const uint8_t* wyn_string_as_bytes(const WynString* str) {
    return str ? str->data : NULL;
}

// String modification
WynUtf8Error wyn_string_push_char(WynString* str, uint32_t codepoint) {
    if (!str) return WYN_UTF8_INVALID_SEQUENCE;
    
    uint8_t buffer[4];
    size_t char_len = wyn_utf8_encode_char(codepoint, buffer);
    if (char_len == 0) return WYN_UTF8_INVALID_CODEPOINT;
    
    if (!wyn_string_ensure_capacity(str, str->byte_len + char_len + 1)) {
        return WYN_UTF8_INVALID_SEQUENCE;
    }
    
    memcpy(str->data + str->byte_len, buffer, char_len);
    str->byte_len += char_len;
    str->data[str->byte_len] = '\0';
    
    if (str->char_count_valid) {
        str->char_count++;
    }
    
    return WYN_UTF8_VALID;
}

WynUtf8Error wyn_string_push_str(WynString* str, const char* cstr) {
    if (!str || !cstr) return WYN_UTF8_INVALID_SEQUENCE;
    
    size_t cstr_len = strlen(cstr);
    WynUtf8Error validation = wyn_utf8_validate((const uint8_t*)cstr, cstr_len);
    if (validation != WYN_UTF8_VALID) return validation;
    
    if (!wyn_string_ensure_capacity(str, str->byte_len + cstr_len + 1)) {
        return WYN_UTF8_INVALID_SEQUENCE;
    }
    
    memcpy(str->data + str->byte_len, cstr, cstr_len);
    str->byte_len += cstr_len;
    str->data[str->byte_len] = '\0';
    
    str->char_count_valid = false;  // Invalidate cached count
    
    return WYN_UTF8_VALID;
}

WynUtf8Error wyn_string_push_string(WynString* str, const WynString* other) {
    if (!str || !other) return WYN_UTF8_INVALID_SEQUENCE;
    
    if (!wyn_string_ensure_capacity(str, str->byte_len + other->byte_len + 1)) {
        return WYN_UTF8_INVALID_SEQUENCE;
    }
    
    memcpy(str->data + str->byte_len, other->data, other->byte_len);
    str->byte_len += other->byte_len;
    str->data[str->byte_len] = '\0';
    
    str->char_count_valid = false;  // Invalidate cached count
    
    return WYN_UTF8_VALID;
}

bool wyn_string_pop_char(WynString* str, uint32_t* codepoint) {
    if (!str || str->byte_len == 0) return false;
    
    // Find the start of the last character
    size_t pos = str->byte_len;
    while (pos > 0 && wyn_utf8_is_continuation_byte(str->data[pos - 1])) {
        pos--;
    }
    if (pos > 0) pos--;
    
    // Decode the character
    if (codepoint) {
        wyn_utf8_decode_char(str->data + pos, str->byte_len - pos, codepoint);
    }
    
    // Remove the character
    str->byte_len = pos;
    str->data[str->byte_len] = '\0';
    
    if (str->char_count_valid && str->char_count > 0) {
        str->char_count--;
    }
    
    return true;
}

void wyn_string_clear(WynString* str) {
    if (!str) return;
    
    str->byte_len = 0;
    str->data[0] = '\0';
    str->char_count = 0;
    str->char_count_valid = true;
}

// String operations
WynString* wyn_string_substring(const WynString* str, size_t start, size_t end) {
    if (!str || start > end) return NULL;
    
    size_t char_pos = 0;
    size_t byte_start = 0;
    size_t byte_end = str->byte_len;
    bool found_start = false;
    
    for (size_t i = 0; i < str->byte_len; i++) {
        if (!wyn_utf8_is_continuation_byte(str->data[i])) {
            if (char_pos == start) {
                byte_start = i;
                found_start = true;
            }
            if (char_pos == end) {
                byte_end = i;
                break;
            }
            char_pos++;
        }
    }
    
    if (!found_start) return wyn_string_new();
    
    size_t sub_len = byte_end - byte_start;
    WynUtf8Error error;
    return wyn_string_from_utf8(str->data + byte_start, sub_len, &error);
}

WynString* wyn_string_clone(const WynString* str) {
    if (!str) return NULL;
    
    WynUtf8Error error;
    return wyn_string_from_utf8(str->data, str->byte_len, &error);
}

int wyn_string_compare(const WynString* a, const WynString* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    size_t min_len = a->byte_len < b->byte_len ? a->byte_len : b->byte_len;
    int result = memcmp(a->data, b->data, min_len);
    
    if (result == 0) {
        if (a->byte_len < b->byte_len) return -1;
        if (a->byte_len > b->byte_len) return 1;
    }
    
    return result;
}

bool wyn_string_equals(const WynString* a, const WynString* b) {
    return wyn_string_compare(a, b) == 0;
}

bool wyn_string_starts_with(const WynString* str, const char* prefix) {
    if (!str || !prefix) return false;
    
    size_t prefix_len = strlen(prefix);
    if (prefix_len > str->byte_len) return false;
    
    return memcmp(str->data, prefix, prefix_len) == 0;
}

bool wyn_string_ends_with(const WynString* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str->byte_len) return false;
    
    return memcmp(str->data + str->byte_len - suffix_len, suffix, suffix_len) == 0;
}

bool wyn_string_contains(const WynString* str, const char* needle) {
    return wyn_string_find(str, needle) >= 0;
}

int wyn_string_find(const WynString* str, const char* needle) {
    if (!str || !needle) return -1;
    
    const char* found = strstr((const char*)str->data, needle);
    if (!found) return -1;
    
    // Convert byte position to character position
    size_t byte_pos = found - (const char*)str->data;
    size_t char_pos = 0;
    
    for (size_t i = 0; i < byte_pos; i++) {
        if (!wyn_utf8_is_continuation_byte(str->data[i])) {
            char_pos++;
        }
    }
    
    return (int)char_pos;
}

// String transformation (simplified - basic ASCII for now)
WynString* wyn_string_to_uppercase(const WynString* str) {
    if (!str) return NULL;
    
    WynString* result = wyn_string_clone(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->byte_len; i++) {
        if (result->data[i] < 128) {  // ASCII only for now
            result->data[i] = toupper(result->data[i]);
        }
    }
    
    return result;
}

WynString* wyn_string_to_lowercase(const WynString* str) {
    if (!str) return NULL;
    
    WynString* result = wyn_string_clone(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->byte_len; i++) {
        if (result->data[i] < 128) {  // ASCII only for now
            result->data[i] = tolower(result->data[i]);
        }
    }
    
    return result;
}

WynString* wyn_string_trim(const WynString* str) {
    if (!str) return NULL;
    
    // Find start of non-whitespace
    size_t start = 0;
    while (start < str->byte_len && isspace(str->data[start])) {
        start++;
    }
    
    // Find end of non-whitespace
    size_t end = str->byte_len;
    while (end > start && isspace(str->data[end - 1])) {
        end--;
    }
    
    WynUtf8Error error;
    return wyn_string_from_utf8(str->data + start, end - start, &error);
}

// String iteration
WynStringIterator* wyn_string_iter(const WynString* str) {
    if (!str) return NULL;
    
    WynStringIterator* iter = malloc(sizeof(WynStringIterator));
    if (!iter) return NULL;
    
    iter->string = str;
    iter->byte_pos = 0;
    iter->char_pos = 0;
    
    return iter;
}

bool wyn_string_iter_next(WynStringIterator* iter, uint32_t* codepoint) {
    if (!iter || iter->byte_pos >= iter->string->byte_len) return false;
    
    size_t char_len = wyn_utf8_decode_char(
        iter->string->data + iter->byte_pos,
        iter->string->byte_len - iter->byte_pos,
        codepoint
    );
    
    if (char_len == 0) return false;
    
    iter->byte_pos += char_len;
    iter->char_pos++;
    
    return true;
}

void wyn_string_iter_free(WynStringIterator* iter) {
    free(iter);
}

// UTF-8 utilities
WynUtf8Error wyn_utf8_validate(const uint8_t* bytes, size_t len) {
    if (!bytes) return WYN_UTF8_INVALID_SEQUENCE;
    
    for (size_t i = 0; i < len; ) {
        uint8_t byte = bytes[i];
        size_t char_len = wyn_utf8_char_byte_len(byte);
        
        if (char_len == 0 || i + char_len > len) {
            return WYN_UTF8_INVALID_SEQUENCE;
        }
        
        // Validate continuation bytes
        for (size_t j = 1; j < char_len; j++) {
            if (!wyn_utf8_is_continuation_byte(bytes[i + j])) {
                return WYN_UTF8_INVALID_SEQUENCE;
            }
        }
        
        // Check for overlong encoding
        uint32_t codepoint;
        wyn_utf8_decode_char(bytes + i, char_len, &codepoint);
        
        if ((char_len == 2 && codepoint < 0x80) ||
            (char_len == 3 && codepoint < 0x800) ||
            (char_len == 4 && codepoint < 0x10000)) {
            return WYN_UTF8_OVERLONG_ENCODING;
        }
        
        // Check for invalid codepoints
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return WYN_UTF8_INVALID_CODEPOINT;
        }
        
        i += char_len;
    }
    
    return WYN_UTF8_VALID;
}

size_t wyn_utf8_char_count(const uint8_t* bytes, size_t len) {
    if (!bytes) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (!wyn_utf8_is_continuation_byte(bytes[i])) {
            count++;
        }
    }
    
    return count;
}

size_t wyn_utf8_encode_char(uint32_t codepoint, uint8_t* buffer) {
    if (!buffer) return 0;
    
    if (codepoint <= 0x7F) {
        // 1-byte sequence
        buffer[0] = (uint8_t)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        // 2-byte sequence
        buffer[0] = 0xC0 | (codepoint >> 6);
        buffer[1] = 0x80 | (codepoint & 0x3F);
        return 2;
    } else if (codepoint <= 0xFFFF) {
        // 3-byte sequence
        buffer[0] = 0xE0 | (codepoint >> 12);
        buffer[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[2] = 0x80 | (codepoint & 0x3F);
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        // 4-byte sequence
        buffer[0] = 0xF0 | (codepoint >> 18);
        buffer[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        buffer[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[3] = 0x80 | (codepoint & 0x3F);
        return 4;
    }
    
    return 0;  // Invalid codepoint
}

size_t wyn_utf8_decode_char(const uint8_t* bytes, size_t len, uint32_t* codepoint) {
    if (!bytes || len == 0) return 0;
    
    uint8_t first = bytes[0];
    
    if ((first & UTF8_1_BYTE_MASK) == UTF8_1_BYTE_PATTERN) {
        // 1-byte character
        if (codepoint) *codepoint = first;
        return 1;
    } else if ((first & UTF8_2_BYTE_MASK) == UTF8_2_BYTE_PATTERN) {
        // 2-byte character
        if (len < 2) return 0;
        if (codepoint) {
            *codepoint = ((first & 0x1F) << 6) | (bytes[1] & 0x3F);
        }
        return 2;
    } else if ((first & UTF8_3_BYTE_MASK) == UTF8_3_BYTE_PATTERN) {
        // 3-byte character
        if (len < 3) return 0;
        if (codepoint) {
            *codepoint = ((first & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
        }
        return 3;
    } else if ((first & UTF8_4_BYTE_MASK) == UTF8_4_BYTE_PATTERN) {
        // 4-byte character
        if (len < 4) return 0;
        if (codepoint) {
            *codepoint = ((first & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) | 
                        ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
        }
        return 4;
    }
    
    return 0;  // Invalid UTF-8
}

bool wyn_utf8_is_continuation_byte(uint8_t byte) {
    return (byte & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_PATTERN;
}

size_t wyn_utf8_char_byte_len(uint8_t first_byte) {
    if ((first_byte & UTF8_1_BYTE_MASK) == UTF8_1_BYTE_PATTERN) return 1;
    if ((first_byte & UTF8_2_BYTE_MASK) == UTF8_2_BYTE_PATTERN) return 2;
    if ((first_byte & UTF8_3_BYTE_MASK) == UTF8_3_BYTE_PATTERN) return 3;
    if ((first_byte & UTF8_4_BYTE_MASK) == UTF8_4_BYTE_PATTERN) return 4;
    return 0;  // Invalid
}

// Unicode character classification (basic ASCII for now)
bool wyn_unicode_is_alphabetic(uint32_t codepoint) {
    return (codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z');
}

bool wyn_unicode_is_numeric(uint32_t codepoint) {
    return codepoint >= '0' && codepoint <= '9';
}

bool wyn_unicode_is_alphanumeric(uint32_t codepoint) {
    return wyn_unicode_is_alphabetic(codepoint) || wyn_unicode_is_numeric(codepoint);
}

bool wyn_unicode_is_whitespace(uint32_t codepoint) {
    return codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || codepoint == '\r';
}

bool wyn_unicode_is_uppercase(uint32_t codepoint) {
    return codepoint >= 'A' && codepoint <= 'Z';
}

bool wyn_unicode_is_lowercase(uint32_t codepoint) {
    return codepoint >= 'a' && codepoint <= 'z';
}

uint32_t wyn_unicode_to_uppercase(uint32_t codepoint) {
    if (wyn_unicode_is_lowercase(codepoint)) {
        return codepoint - 'a' + 'A';
    }
    return codepoint;
}

uint32_t wyn_unicode_to_lowercase(uint32_t codepoint) {
    if (wyn_unicode_is_uppercase(codepoint)) {
        return codepoint - 'A' + 'a';
    }
    return codepoint;
}

// Error handling
const char* wyn_utf8_error_string(WynUtf8Error error) {
    switch (error) {
        case WYN_UTF8_VALID: return "Valid UTF-8";
        case WYN_UTF8_INVALID_SEQUENCE: return "Invalid UTF-8 sequence";
        case WYN_UTF8_INCOMPLETE_SEQUENCE: return "Incomplete UTF-8 sequence";
        case WYN_UTF8_OVERLONG_ENCODING: return "Overlong UTF-8 encoding";
        case WYN_UTF8_INVALID_CODEPOINT: return "Invalid Unicode codepoint";
        default: return "Unknown UTF-8 error";
    }
}
