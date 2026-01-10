#include "../src/unicode.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_string_creation() {
    printf("Testing string creation...\n");
    
    // Test empty string
    WynString* empty = wyn_string_new();
    assert(empty != NULL);
    assert(wyn_string_is_empty(empty));
    assert(wyn_string_byte_len(empty) == 0);
    assert(wyn_string_char_count(empty) == 0);
    wyn_string_free(empty);
    
    // Test from C string
    WynString* hello = wyn_string_from_cstr("Hello");
    assert(hello != NULL);
    assert(!wyn_string_is_empty(hello));
    assert(wyn_string_byte_len(hello) == 5);
    assert(wyn_string_char_count(hello) == 5);
    assert(strcmp(wyn_string_as_cstr(hello), "Hello") == 0);
    wyn_string_free(hello);
    
    // Test UTF-8 string
    const char* utf8_text = "Hello, 世界! 🌍";
    WynUtf8Error error;
    WynString* utf8_str = wyn_string_from_utf8((const uint8_t*)utf8_text, strlen(utf8_text), &error);
    assert(utf8_str != NULL);
    assert(error == WYN_UTF8_VALID);
    assert(wyn_string_byte_len(utf8_str) == strlen(utf8_text));
    assert(wyn_string_char_count(utf8_str) == 12);  // H-e-l-l-o-,-space-世-界-!-space-🌍
    wyn_string_free(utf8_str);
    
    printf("✓ String creation tests passed\n");
}

void test_utf8_validation() {
    printf("Testing UTF-8 validation...\n");
    
    // Valid UTF-8 sequences
    const uint8_t valid1[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    assert(wyn_utf8_validate(valid1, 5) == WYN_UTF8_VALID);
    
    const uint8_t valid2[] = {0xE4, 0xB8, 0x96, 0xE7, 0x95, 0x8C};  // "世界"
    assert(wyn_utf8_validate(valid2, 6) == WYN_UTF8_VALID);
    
    const uint8_t valid3[] = {0xF0, 0x9F, 0x8C, 0x8D};  // "🌍"
    assert(wyn_utf8_validate(valid3, 4) == WYN_UTF8_VALID);
    
    // Invalid UTF-8 sequences
    const uint8_t invalid1[] = {0xFF, 0xFE};  // Invalid start bytes
    assert(wyn_utf8_validate(invalid1, 2) == WYN_UTF8_INVALID_SEQUENCE);
    
    const uint8_t invalid2[] = {0xC0, 0x80};  // Overlong encoding
    assert(wyn_utf8_validate(invalid2, 2) == WYN_UTF8_OVERLONG_ENCODING);
    
    const uint8_t invalid3[] = {0xE4, 0xB8};  // Incomplete sequence
    assert(wyn_utf8_validate(invalid3, 2) == WYN_UTF8_INVALID_SEQUENCE);
    
    printf("✓ UTF-8 validation tests passed\n");
}

void test_utf8_encoding_decoding() {
    printf("Testing UTF-8 encoding/decoding...\n");
    
    // Test ASCII character
    uint8_t buffer[4];
    size_t len = wyn_utf8_encode_char('A', buffer);
    assert(len == 1);
    assert(buffer[0] == 'A');
    
    uint32_t decoded;
    size_t decoded_len = wyn_utf8_decode_char(buffer, len, &decoded);
    assert(decoded_len == 1);
    assert(decoded == 'A');
    
    // Test 2-byte character (ñ)
    len = wyn_utf8_encode_char(0xF1, buffer);
    assert(len == 2);
    assert(buffer[0] == 0xC3);
    assert(buffer[1] == 0xB1);
    
    decoded_len = wyn_utf8_decode_char(buffer, len, &decoded);
    assert(decoded_len == 2);
    assert(decoded == 0xF1);
    
    // Test 3-byte character (世)
    len = wyn_utf8_encode_char(0x4E16, buffer);
    assert(len == 3);
    
    decoded_len = wyn_utf8_decode_char(buffer, len, &decoded);
    assert(decoded_len == 3);
    assert(decoded == 0x4E16);
    
    // Test 4-byte character (🌍)
    len = wyn_utf8_encode_char(0x1F30D, buffer);
    assert(len == 4);
    
    decoded_len = wyn_utf8_decode_char(buffer, len, &decoded);
    assert(decoded_len == 4);
    assert(decoded == 0x1F30D);
    
    printf("✓ UTF-8 encoding/decoding tests passed\n");
}

void test_string_modification() {
    printf("Testing string modification...\n");
    
    WynString* str = wyn_string_new();
    
    // Test push_char
    assert(wyn_string_push_char(str, 'H') == WYN_UTF8_VALID);
    assert(wyn_string_push_char(str, 'i') == WYN_UTF8_VALID);
    assert(wyn_string_char_count(str) == 2);
    assert(strcmp(wyn_string_as_cstr(str), "Hi") == 0);
    
    // Test push_str
    assert(wyn_string_push_str(str, ", World") == WYN_UTF8_VALID);
    assert(wyn_string_char_count(str) == 9);
    assert(strcmp(wyn_string_as_cstr(str), "Hi, World") == 0);
    
    // Test push Unicode characters
    assert(wyn_string_push_char(str, 0x4E16) == WYN_UTF8_VALID);  // 世
    assert(wyn_string_push_char(str, 0x754C) == WYN_UTF8_VALID);  // 界
    assert(wyn_string_char_count(str) == 11);
    
    // Test pop_char
    uint32_t popped;
    assert(wyn_string_pop_char(str, &popped));
    assert(popped == 0x754C);  // 界
    assert(wyn_string_char_count(str) == 10);
    
    // Test clear
    wyn_string_clear(str);
    assert(wyn_string_is_empty(str));
    assert(wyn_string_char_count(str) == 0);
    
    wyn_string_free(str);
    
    printf("✓ String modification tests passed\n");
}

void test_string_operations() {
    printf("Testing string operations...\n");
    
    WynString* str1 = wyn_string_from_cstr("Hello, World!");
    WynString* str2 = wyn_string_from_cstr("Hello, World!");
    WynString* str3 = wyn_string_from_cstr("Goodbye");
    
    // Test equality
    assert(wyn_string_equals(str1, str2));
    assert(!wyn_string_equals(str1, str3));
    
    // Test comparison
    assert(wyn_string_compare(str1, str2) == 0);
    assert(wyn_string_compare(str3, str1) < 0);  // "Goodbye" < "Hello, World!"
    
    // Test starts_with and ends_with
    assert(wyn_string_starts_with(str1, "Hello"));
    assert(!wyn_string_starts_with(str1, "Hi"));
    assert(wyn_string_ends_with(str1, "World!"));
    assert(!wyn_string_ends_with(str1, "Universe!"));
    
    // Test contains and find
    assert(wyn_string_contains(str1, "World"));
    assert(!wyn_string_contains(str1, "Universe"));
    assert(wyn_string_find(str1, "World") == 7);
    assert(wyn_string_find(str1, "Universe") == -1);
    
    // Test substring
    WynString* sub = wyn_string_substring(str1, 7, 12);  // "World"
    assert(sub != NULL);
    assert(strcmp(wyn_string_as_cstr(sub), "World") == 0);
    wyn_string_free(sub);
    
    // Test clone
    WynString* clone = wyn_string_clone(str1);
    assert(wyn_string_equals(str1, clone));
    wyn_string_free(clone);
    
    wyn_string_free(str1);
    wyn_string_free(str2);
    wyn_string_free(str3);
    
    printf("✓ String operations tests passed\n");
}

void test_string_transformation() {
    printf("Testing string transformation...\n");
    
    WynString* str = wyn_string_from_cstr("Hello, World!");
    
    // Test uppercase
    WynString* upper = wyn_string_to_uppercase(str);
    assert(strcmp(wyn_string_as_cstr(upper), "HELLO, WORLD!") == 0);
    wyn_string_free(upper);
    
    // Test lowercase
    WynString* lower = wyn_string_to_lowercase(str);
    assert(strcmp(wyn_string_as_cstr(lower), "hello, world!") == 0);
    wyn_string_free(lower);
    
    // Test trim
    WynString* padded = wyn_string_from_cstr("  Hello, World!  ");
    WynString* trimmed = wyn_string_trim(padded);
    assert(strcmp(wyn_string_as_cstr(trimmed), "Hello, World!") == 0);
    wyn_string_free(padded);
    wyn_string_free(trimmed);
    
    wyn_string_free(str);
    
    printf("✓ String transformation tests passed\n");
}

void test_string_iteration() {
    printf("Testing string iteration...\n");
    
    const char* utf8_text = "Hi世界🌍";
    WynString* str = wyn_string_from_cstr(utf8_text);
    
    WynStringIterator* iter = wyn_string_iter(str);
    assert(iter != NULL);
    
    uint32_t expected[] = {'H', 'i', 0x4E16, 0x754C, 0x1F30D};
    size_t expected_count = 5;
    
    uint32_t codepoint;
    size_t count = 0;
    while (wyn_string_iter_next(iter, &codepoint)) {
        assert(count < expected_count);
        assert(codepoint == expected[count]);
        count++;
    }
    
    assert(count == expected_count);
    
    wyn_string_iter_free(iter);
    wyn_string_free(str);
    
    printf("✓ String iteration tests passed\n");
}

void test_unicode_classification() {
    printf("Testing Unicode character classification...\n");
    
    // Test alphabetic
    assert(wyn_unicode_is_alphabetic('A'));
    assert(wyn_unicode_is_alphabetic('z'));
    assert(!wyn_unicode_is_alphabetic('1'));
    assert(!wyn_unicode_is_alphabetic(' '));
    
    // Test numeric
    assert(wyn_unicode_is_numeric('0'));
    assert(wyn_unicode_is_numeric('9'));
    assert(!wyn_unicode_is_numeric('A'));
    
    // Test alphanumeric
    assert(wyn_unicode_is_alphanumeric('A'));
    assert(wyn_unicode_is_alphanumeric('5'));
    assert(!wyn_unicode_is_alphanumeric(' '));
    
    // Test whitespace
    assert(wyn_unicode_is_whitespace(' '));
    assert(wyn_unicode_is_whitespace('\t'));
    assert(wyn_unicode_is_whitespace('\n'));
    assert(!wyn_unicode_is_whitespace('A'));
    
    // Test case
    assert(wyn_unicode_is_uppercase('A'));
    assert(!wyn_unicode_is_uppercase('a'));
    assert(wyn_unicode_is_lowercase('a'));
    assert(!wyn_unicode_is_lowercase('A'));
    
    // Test case conversion
    assert(wyn_unicode_to_uppercase('a') == 'A');
    assert(wyn_unicode_to_lowercase('A') == 'a');
    assert(wyn_unicode_to_uppercase('A') == 'A');
    assert(wyn_unicode_to_lowercase('a') == 'a');
    
    printf("✓ Unicode character classification tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    wyn_string_free(NULL);  // Should not crash
    assert(wyn_string_byte_len(NULL) == 0);
    assert(wyn_string_char_count(NULL) == 0);
    assert(wyn_string_is_empty(NULL) == true);
    
    // Test empty string operations
    WynString* empty = wyn_string_new();
    assert(wyn_string_substring(empty, 0, 0) != NULL);
    assert(wyn_string_is_empty(wyn_string_substring(empty, 0, 0)));
    
    uint32_t codepoint;
    assert(!wyn_string_pop_char(empty, &codepoint));
    
    wyn_string_free(empty);
    
    // Test invalid UTF-8 handling
    const uint8_t invalid[] = {0xFF, 0xFE};
    WynUtf8Error error;
    WynString* invalid_str = wyn_string_from_utf8(invalid, 2, &error);
    assert(invalid_str == NULL);
    assert(error != WYN_UTF8_VALID);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Unicode String System Tests\n");
    printf("===================================\n\n");
    
    test_string_creation();
    test_utf8_validation();
    test_utf8_encoding_decoding();
    test_string_modification();
    test_string_operations();
    test_string_transformation();
    test_string_iteration();
    test_unicode_classification();
    test_edge_cases();
    
    printf("\n✅ All Unicode string system tests passed!\n");
    return 0;
}
