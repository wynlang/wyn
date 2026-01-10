#include "../src/regex.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void example_basic_matching() {
    printf("=== Basic Pattern Matching ===\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("hello", &error);
    
    if (!regex) {
        printf("Failed to create regex: %s\n", wyn_regex_error_string(error.code));
        return;
    }
    
    // Test various strings
    const char* test_strings[] = {
        "hello world",
        "say hello to everyone", 
        "Hello World",
        "goodbye world",
        "hello"
    };
    
    for (size_t i = 0; i < sizeof(test_strings) / sizeof(test_strings[0]); i++) {
        WynString* text = wyn_string_from_cstr(test_strings[i]);
        bool matches = wyn_regex_is_match(regex, text);
        
        printf("'%s' matches 'hello': %s\n", test_strings[i], matches ? "✓" : "✗");
        
        wyn_string_free(text);
    }
    
    wyn_regex_free(regex);
    printf("\n");
}

void example_special_characters() {
    printf("=== Special Characters ===\n");
    
    WynRegexError error = {0};
    
    // Dot matches any character
    WynRegex* dot_regex = wyn_regex_new("h.llo", &error);
    WynString* text1 = wyn_string_from_cstr("hello");
    WynString* text2 = wyn_string_from_cstr("hallo");
    WynString* text3 = wyn_string_from_cstr("h3llo");
    
    printf("'h.llo' pattern:\n");
    printf("  'hello': %s\n", wyn_regex_is_match(dot_regex, text1) ? "✓" : "✗");
    printf("  'hallo': %s\n", wyn_regex_is_match(dot_regex, text2) ? "✓" : "✗");
    printf("  'h3llo': %s\n", wyn_regex_is_match(dot_regex, text3) ? "✓" : "✗");
    
    wyn_string_free(text1);
    wyn_string_free(text2);
    wyn_string_free(text3);
    wyn_regex_free(dot_regex);
    
    // Start and end anchors
    WynRegex* start_regex = wyn_regex_new("^hello", &error);
    WynString* text4 = wyn_string_from_cstr("hello world");
    WynString* text5 = wyn_string_from_cstr("say hello");
    
    printf("'^hello' pattern (start anchor):\n");
    printf("  'hello world': %s\n", wyn_regex_is_match(start_regex, text4) ? "✓" : "✗");
    printf("  'say hello': %s\n", wyn_regex_is_match(start_regex, text5) ? "✓" : "✗");
    
    wyn_string_free(text4);
    wyn_string_free(text5);
    wyn_regex_free(start_regex);
    
    printf("\n");
}

void example_find_operations() {
    printf("=== Find Operations ===\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("world", &error);
    
    WynString* text = wyn_string_from_cstr("hello world, wonderful world!");
    
    // Find first match
    WynMatch* match = wyn_regex_find(regex, text);
    if (match) {
        printf("First match: '%s' at position %zu-%zu\n", 
               wyn_string_as_cstr(match->text), match->start, match->end);
        wyn_match_free(match);
    }
    
    // Find all matches
    size_t count;
    WynMatch** matches = wyn_regex_find_all(regex, text, &count);
    printf("Found %zu matches:\n", count);
    
    for (size_t i = 0; i < count; i++) {
        printf("  Match %zu: '%s' at %zu-%zu\n", i + 1,
               wyn_string_as_cstr(matches[i]->text), 
               matches[i]->start, matches[i]->end);
        wyn_match_free(matches[i]);
    }
    free(matches);
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    printf("\n");
}

void example_string_replacement() {
    printf("=== String Replacement ===\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("world", &error);
    
    WynString* text = wyn_string_from_cstr("hello world");
    
    // Replace first occurrence
    WynString* replaced = wyn_regex_replace(regex, text, "universe");
    printf("Original: %s\n", wyn_string_as_cstr(text));
    printf("Replaced: %s\n", wyn_string_as_cstr(replaced));
    wyn_string_free(replaced);
    
    // Replace all occurrences
    WynString* text2 = wyn_string_from_cstr("world world world");
    WynString* replaced_all = wyn_regex_replace_all(regex, text2, "universe");
    printf("Original: %s\n", wyn_string_as_cstr(text2));
    printf("Replace all: %s\n", wyn_string_as_cstr(replaced_all));
    
    wyn_string_free(text);
    wyn_string_free(text2);
    wyn_string_free(replaced_all);
    wyn_regex_free(regex);
    
    printf("\n");
}

void example_unicode_support() {
    printf("=== Unicode Support ===\n");
    
    WynRegexError error = {0};
    
    // Match Unicode characters
    WynRegex* unicode_regex = wyn_regex_new("世界", &error);
    WynString* text1 = wyn_string_from_cstr("Hello 世界!");
    WynString* text2 = wyn_string_from_cstr("世界 is world in Chinese");
    
    printf("Pattern '世界':\n");
    printf("  'Hello 世界!': %s\n", wyn_regex_is_match(unicode_regex, text1) ? "✓" : "✗");
    printf("  '世界 is world in Chinese': %s\n", wyn_regex_is_match(unicode_regex, text2) ? "✓" : "✗");
    
    // Find Unicode matches
    WynMatch* match = wyn_regex_find(unicode_regex, text1);
    if (match) {
        printf("Found Unicode match: '%s' at character position %zu-%zu\n",
               wyn_string_as_cstr(match->text), match->start, match->end);
        wyn_match_free(match);
    }
    
    wyn_string_free(text1);
    wyn_string_free(text2);
    wyn_regex_free(unicode_regex);
    
    // Emoji support
    WynRegex* emoji_regex = wyn_regex_new("🌍", &error);
    WynString* emoji_text = wyn_string_from_cstr("Hello 🌍 World!");
    
    printf("Emoji pattern '🌍':\n");
    printf("  'Hello 🌍 World!': %s\n", wyn_regex_is_match(emoji_regex, emoji_text) ? "✓" : "✗");
    
    wyn_string_free(emoji_text);
    wyn_regex_free(emoji_regex);
    
    printf("\n");
}

void example_character_classes() {
    printf("=== Character Classes ===\n");
    
    // Test built-in character class functions
    printf("Character classification:\n");
    
    uint32_t test_chars[] = {'a', 'Z', '5', '_', ' ', '!', 0x4E16}; // Including Unicode
    const char* char_names[] = {"'a'", "'Z'", "'5'", "'_'", "' '", "'!'", "'世'"};
    
    for (size_t i = 0; i < sizeof(test_chars) / sizeof(test_chars[0]); i++) {
        uint32_t ch = test_chars[i];
        printf("  %s: ", char_names[i]);
        
        if (wyn_regex_is_word_char(ch)) printf("word ");
        if (wyn_regex_is_digit_char(ch)) printf("digit ");
        if (wyn_regex_is_space_char(ch)) printf("space ");
        
        printf("\n");
    }
    
    printf("\n");
}

void example_error_handling() {
    printf("=== Error Handling ===\n");
    
    WynRegexError error = {0};
    
    // Test invalid pattern
    WynRegex* regex = wyn_regex_new(NULL, &error);
    if (!regex) {
        printf("Expected error for NULL pattern: %s\n", wyn_regex_error_string(error.code));
    }
    
    // Test valid pattern
    regex = wyn_regex_new("valid", &error);
    if (regex) {
        printf("Valid pattern created successfully: %s\n", wyn_regex_error_string(error.code));
        wyn_regex_free(regex);
    }
    
    // Test error messages
    printf("Error code meanings:\n");
    printf("  WYN_REGEX_OK: %s\n", wyn_regex_error_string(WYN_REGEX_OK));
    printf("  WYN_REGEX_INVALID_PATTERN: %s\n", wyn_regex_error_string(WYN_REGEX_INVALID_PATTERN));
    printf("  WYN_REGEX_MEMORY_ERROR: %s\n", wyn_regex_error_string(WYN_REGEX_MEMORY_ERROR));
    
    printf("\n");
}

void example_performance_demo() {
    printf("=== Performance Demo ===\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("test", &error);
    
    // Build a large text string
    WynString* large_text = wyn_string_new();
    for (int i = 0; i < 1000; i++) {
        wyn_string_push_str(large_text, "This is a test string. ");
    }
    
    printf("Testing pattern 'test' against large text (%zu characters)...\n",
           wyn_string_char_count(large_text));
    
    // Find all matches
    size_t count;
    WynMatch** matches = wyn_regex_find_all(regex, large_text, &count);
    
    printf("Found %zu matches in large text\n", count);
    
    // Show first few matches
    size_t show_count = count < 5 ? count : 5;
    for (size_t i = 0; i < show_count; i++) {
        printf("  Match %zu at position %zu\n", i + 1, matches[i]->start);
        wyn_match_free(matches[i]);
    }
    
    // Clean up remaining matches
    for (size_t i = show_count; i < count; i++) {
        wyn_match_free(matches[i]);
    }
    free(matches);
    
    wyn_string_free(large_text);
    wyn_regex_free(regex);
    
    printf("\n");
}

int main() {
    printf("Wyn Regular Expression System Examples\n");
    printf("=====================================\n\n");
    
    example_basic_matching();
    example_special_characters();
    example_find_operations();
    example_string_replacement();
    example_unicode_support();
    example_character_classes();
    example_error_handling();
    example_performance_demo();
    
    printf("All regex examples completed successfully!\n");
    return 0;
}
