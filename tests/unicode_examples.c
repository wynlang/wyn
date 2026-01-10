#include "../src/unicode.h"
#include <stdio.h>
#include <string.h>

void example_basic_string_operations() {
    printf("=== Basic String Operations ===\n");
    
    // Create strings
    WynString* greeting = wyn_string_from_cstr("Hello");
    WynString* target = wyn_string_from_cstr("World");
    WynString* message = wyn_string_new();
    
    // Build a message
    wyn_string_push_string(message, greeting);
    wyn_string_push_str(message, ", ");
    wyn_string_push_string(message, target);
    wyn_string_push_char(message, '!');
    
    printf("Message: %s\n", wyn_string_as_cstr(message));
    printf("Length: %zu bytes, %zu characters\n", 
           wyn_string_byte_len(message), wyn_string_char_count(message));
    
    // String comparison
    WynString* copy = wyn_string_clone(message);
    printf("Original equals copy: %s\n", wyn_string_equals(message, copy) ? "true" : "false");
    
    // Substring
    WynString* sub = wyn_string_substring(message, 0, 5);  // "Hello"
    printf("Substring (0-5): %s\n", wyn_string_as_cstr(sub));
    
    wyn_string_free(greeting);
    wyn_string_free(target);
    wyn_string_free(message);
    wyn_string_free(copy);
    wyn_string_free(sub);
    
    printf("\n");
}

void example_unicode_support() {
    printf("=== Unicode Support ===\n");
    
    // Create Unicode string
    const char* multilingual = "Hello, 世界! Здравствуй мир! 🌍🚀";
    WynString* unicode_str = wyn_string_from_cstr(multilingual);
    
    printf("Unicode text: %s\n", wyn_string_as_cstr(unicode_str));
    printf("Byte length: %zu\n", wyn_string_byte_len(unicode_str));
    printf("Character count: %zu\n", wyn_string_char_count(unicode_str));
    
    // Iterate through Unicode characters
    printf("Characters: ");
    WynStringIterator* iter = wyn_string_iter(unicode_str);
    uint32_t codepoint;
    int count = 0;
    while (wyn_string_iter_next(iter, &codepoint) && count < 10) {
        if (codepoint < 128) {
            printf("'%c' ", (char)codepoint);
        } else {
            printf("U+%04X ", codepoint);
        }
        count++;
    }
    printf("...\n");
    
    wyn_string_iter_free(iter);
    wyn_string_free(unicode_str);
    
    printf("\n");
}

void example_string_building() {
    printf("=== String Building ===\n");
    
    WynString* builder = wyn_string_with_capacity(100);
    
    // Build a formatted string
    wyn_string_push_str(builder, "Programming languages: ");
    
    const char* languages[] = {"C", "Rust", "Python", "JavaScript"};
    size_t lang_count = sizeof(languages) / sizeof(languages[0]);
    
    for (size_t i = 0; i < lang_count; i++) {
        wyn_string_push_str(builder, languages[i]);
        if (i < lang_count - 1) {
            wyn_string_push_str(builder, ", ");
        }
    }
    
    // Add Unicode bullet points
    wyn_string_push_str(builder, " • ");
    wyn_string_push_char(builder, 0x2713);  // ✓ checkmark
    wyn_string_push_str(builder, " Complete");
    
    printf("Built string: %s\n", wyn_string_as_cstr(builder));
    printf("Capacity: %zu, Used: %zu\n", 
           wyn_string_capacity(builder), wyn_string_byte_len(builder));
    
    wyn_string_free(builder);
    
    printf("\n");
}

void example_string_manipulation() {
    printf("=== String Manipulation ===\n");
    
    WynString* text = wyn_string_from_cstr("  Hello, Beautiful World!  ");
    
    printf("Original: '%s'\n", wyn_string_as_cstr(text));
    
    // Trim whitespace
    WynString* trimmed = wyn_string_trim(text);
    printf("Trimmed: '%s'\n", wyn_string_as_cstr(trimmed));
    
    // Case conversion
    WynString* upper = wyn_string_to_uppercase(trimmed);
    WynString* lower = wyn_string_to_lowercase(trimmed);
    
    printf("Uppercase: %s\n", wyn_string_as_cstr(upper));
    printf("Lowercase: %s\n", wyn_string_as_cstr(lower));
    
    // Search operations
    printf("Contains 'World': %s\n", 
           wyn_string_contains(trimmed, "World") ? "true" : "false");
    printf("Starts with 'Hello': %s\n", 
           wyn_string_starts_with(trimmed, "Hello") ? "true" : "false");
    printf("Position of 'Beautiful': %d\n", wyn_string_find(trimmed, "Beautiful"));
    
    wyn_string_free(text);
    wyn_string_free(trimmed);
    wyn_string_free(upper);
    wyn_string_free(lower);
    
    printf("\n");
}

void example_utf8_validation() {
    printf("=== UTF-8 Validation ===\n");
    
    // Valid UTF-8 sequences
    const uint8_t valid_utf8[] = {
        0x48, 0x65, 0x6C, 0x6C, 0x6F,  // "Hello"
        0x2C, 0x20,                      // ", "
        0xE4, 0xB8, 0x96,               // "世"
        0xE7, 0x95, 0x8C,               // "界"
        0x21                             // "!"
    };
    
    WynUtf8Error error = wyn_utf8_validate(valid_utf8, sizeof(valid_utf8));
    printf("Valid UTF-8 sequence: %s\n", wyn_utf8_error_string(error));
    
    if (error == WYN_UTF8_VALID) {
        WynString* str = wyn_string_from_utf8(valid_utf8, sizeof(valid_utf8), &error);
        printf("Created string: %s\n", wyn_string_as_cstr(str));
        wyn_string_free(str);
    }
    
    // Invalid UTF-8 sequence
    const uint8_t invalid_utf8[] = {0xFF, 0xFE, 0x00};
    error = wyn_utf8_validate(invalid_utf8, sizeof(invalid_utf8));
    printf("Invalid UTF-8 sequence: %s\n", wyn_utf8_error_string(error));
    
    printf("\n");
}

void example_character_classification() {
    printf("=== Character Classification ===\n");
    
    const char* test_chars = "A1 α";
    WynString* str = wyn_string_from_cstr(test_chars);
    
    WynStringIterator* iter = wyn_string_iter(str);
    uint32_t codepoint;
    
    printf("Analyzing characters in '%s':\n", test_chars);
    while (wyn_string_iter_next(iter, &codepoint)) {
        if (codepoint < 128) {
            printf("'%c' (U+%04X): ", (char)codepoint, codepoint);
        } else {
            printf("U+%04X: ", codepoint);
        }
        
        if (wyn_unicode_is_alphabetic(codepoint)) printf("alphabetic ");
        if (wyn_unicode_is_numeric(codepoint)) printf("numeric ");
        if (wyn_unicode_is_whitespace(codepoint)) printf("whitespace ");
        if (wyn_unicode_is_uppercase(codepoint)) printf("uppercase ");
        if (wyn_unicode_is_lowercase(codepoint)) printf("lowercase ");
        
        printf("\n");
    }
    
    wyn_string_iter_free(iter);
    wyn_string_free(str);
    
    printf("\n");
}

void example_performance_demo() {
    printf("=== Performance Demo ===\n");
    
    WynString* large_str = wyn_string_with_capacity(1000);
    
    // Build a large string efficiently
    for (int i = 0; i < 100; i++) {
        wyn_string_push_str(large_str, "Hello ");
        wyn_string_push_char(large_str, 0x4E16);  // 世
        wyn_string_push_char(large_str, 0x754C);  // 界
        wyn_string_push_str(large_str, "! ");
    }
    
    printf("Built large string with %zu characters (%zu bytes)\n",
           wyn_string_char_count(large_str), wyn_string_byte_len(large_str));
    
    // Show first few characters
    WynString* preview = wyn_string_substring(large_str, 0, 20);
    printf("Preview: %s...\n", wyn_string_as_cstr(preview));
    
    wyn_string_free(preview);
    wyn_string_free(large_str);
    
    printf("\n");
}

int main() {
    printf("Wyn Unicode String System Examples\n");
    printf("==================================\n\n");
    
    example_basic_string_operations();
    example_unicode_support();
    example_string_building();
    example_string_manipulation();
    example_utf8_validation();
    example_character_classification();
    example_performance_demo();
    
    printf("All examples completed successfully!\n");
    return 0;
}
