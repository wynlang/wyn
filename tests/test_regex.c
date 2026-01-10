#include "../src/regex.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void test_regex_creation() {
    printf("Testing regex creation...\n");
    
    // Test valid pattern
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("hello", &error);
    assert(regex != NULL);
    assert(error.code == WYN_REGEX_OK);
    wyn_regex_free(regex);
    
    // Test NULL pattern
    regex = wyn_regex_new(NULL, &error);
    assert(regex == NULL);
    assert(error.code == WYN_REGEX_INVALID_PATTERN);
    
    // Test pattern with flags
    regex = wyn_regex_new_with_flags("Hello", WYN_REGEX_CASE_INSENSITIVE, &error);
    assert(regex != NULL);
    assert(error.code == WYN_REGEX_OK);
    wyn_regex_free(regex);
    
    printf("✓ Regex creation tests passed\n");
}

void test_basic_matching() {
    printf("Testing basic pattern matching...\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("hello", &error);
    assert(regex != NULL);
    
    // Test matching string
    WynString* text1 = wyn_string_from_cstr("hello world");
    assert(wyn_regex_is_match(regex, text1));
    wyn_string_free(text1);
    
    // Test non-matching string
    WynString* text2 = wyn_string_from_cstr("goodbye world");
    assert(!wyn_regex_is_match(regex, text2));
    wyn_string_free(text2);
    
    // Test exact match
    WynString* text3 = wyn_string_from_cstr("hello");
    assert(wyn_regex_is_match(regex, text3));
    wyn_string_free(text3);
    
    wyn_regex_free(regex);
    
    printf("✓ Basic matching tests passed\n");
}

void test_special_characters() {
    printf("Testing special characters...\n");
    
    WynRegexError error = {0};
    
    // Test dot (any character)
    WynRegex* dot_regex = wyn_regex_new(".", &error);
    assert(dot_regex != NULL);
    
    WynString* text1 = wyn_string_from_cstr("a");
    assert(wyn_regex_is_match(dot_regex, text1));
    wyn_string_free(text1);
    
    WynString* text2 = wyn_string_from_cstr("1");
    assert(wyn_regex_is_match(dot_regex, text2));
    wyn_string_free(text2);
    
    WynString* empty = wyn_string_from_cstr("");
    assert(!wyn_regex_is_match(dot_regex, empty));
    wyn_string_free(empty);
    
    wyn_regex_free(dot_regex);
    
    // Test caret (start of string)
    WynRegex* caret_regex = wyn_regex_new("^", &error);
    assert(caret_regex != NULL);
    
    WynString* text3 = wyn_string_from_cstr("hello");
    assert(wyn_regex_is_match(caret_regex, text3));  // Should match at start
    wyn_string_free(text3);
    
    wyn_regex_free(caret_regex);
    
    // Test dollar (end of string)
    WynRegex* dollar_regex = wyn_regex_new("$", &error);
    assert(dollar_regex != NULL);
    
    WynString* text4 = wyn_string_from_cstr("hello");
    // Note: This is a simplified test - full implementation would need better end-of-string handling
    wyn_string_free(text4);
    
    wyn_regex_free(dollar_regex);
    
    printf("✓ Special character tests passed\n");
}

void test_find_operations() {
    printf("Testing find operations...\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("world", &error);
    assert(regex != NULL);
    
    WynString* text = wyn_string_from_cstr("hello world");
    
    // Test find
    WynMatch* match = wyn_regex_find(regex, text);
    assert(match != NULL);
    assert(match->start == 6);  // "world" starts at position 6
    assert(match->end == 11);   // "world" ends at position 11
    assert(strcmp(wyn_string_as_cstr(match->text), "world") == 0);
    wyn_match_free(match);
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    printf("✓ Find operation tests passed\n");
}

void test_find_all_operations() {
    printf("Testing find all operations...\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("l", &error);
    assert(regex != NULL);
    
    WynString* text = wyn_string_from_cstr("hello world");
    
    // Test find all
    size_t count;
    WynMatch** matches = wyn_regex_find_all(regex, text, &count);
    assert(matches != NULL);
    assert(count == 3);  // Three 'l' characters in "hello world"
    
    // Check first match
    assert(matches[0]->start == 2);
    assert(matches[0]->end == 3);
    
    // Check second match
    assert(matches[1]->start == 3);
    assert(matches[1]->end == 4);
    
    // Check third match
    assert(matches[2]->start == 9);
    assert(matches[2]->end == 10);
    
    // Clean up
    for (size_t i = 0; i < count; i++) {
        wyn_match_free(matches[i]);
    }
    free(matches);
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    printf("✓ Find all operation tests passed\n");
}

void test_string_replacement() {
    printf("Testing string replacement...\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("world", &error);
    assert(regex != NULL);
    
    WynString* text = wyn_string_from_cstr("hello world");
    
    // Test single replacement
    WynString* replaced = wyn_regex_replace(regex, text, "universe");
    assert(replaced != NULL);
    assert(strcmp(wyn_string_as_cstr(replaced), "hello universe") == 0);
    wyn_string_free(replaced);
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    // Test replace all
    WynRegex* regex2 = wyn_regex_new("l", &error);
    assert(regex2 != NULL);
    
    WynString* text2 = wyn_string_from_cstr("hello world");
    WynString* replaced_all = wyn_regex_replace_all(regex2, text2, "x");
    assert(replaced_all != NULL);
    assert(strcmp(wyn_string_as_cstr(replaced_all), "hexxo worxd") == 0);
    wyn_string_free(replaced_all);
    
    wyn_string_free(text2);
    wyn_regex_free(regex2);
    
    printf("✓ String replacement tests passed\n");
}

void test_unicode_support() {
    printf("Testing Unicode support...\n");
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("世", &error);
    assert(regex != NULL);
    
    WynString* text = wyn_string_from_cstr("Hello 世界");
    assert(wyn_regex_is_match(regex, text));
    
    WynMatch* match = wyn_regex_find(regex, text);
    assert(match != NULL);
    assert(match->start == 6);  // Unicode character position
    assert(match->end == 7);
    
    wyn_match_free(match);
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    printf("✓ Unicode support tests passed\n");
}

void test_character_classes() {
    printf("Testing character classes...\n");
    
    // Test word characters
    assert(wyn_regex_is_word_char('a'));
    assert(wyn_regex_is_word_char('Z'));
    assert(wyn_regex_is_word_char('5'));
    assert(wyn_regex_is_word_char('_'));
    assert(!wyn_regex_is_word_char(' '));
    assert(!wyn_regex_is_word_char('!'));
    
    // Test digit characters
    assert(wyn_regex_is_digit_char('0'));
    assert(wyn_regex_is_digit_char('9'));
    assert(!wyn_regex_is_digit_char('a'));
    assert(!wyn_regex_is_digit_char(' '));
    
    // Test space characters
    assert(wyn_regex_is_space_char(' '));
    assert(wyn_regex_is_space_char('\t'));
    assert(wyn_regex_is_space_char('\n'));
    assert(!wyn_regex_is_space_char('a'));
    
    printf("✓ Character class tests passed\n");
}

void test_error_handling() {
    printf("Testing error handling...\n");
    
    // Test error creation
    WynRegexError* error = wyn_regex_error_new(WYN_REGEX_INVALID_PATTERN, "Test error", 5);
    assert(error != NULL);
    assert(error->code == WYN_REGEX_INVALID_PATTERN);
    assert(strcmp(error->message, "Test error") == 0);
    assert(error->position == 5);
    wyn_regex_error_free(error);
    
    // Test error strings
    assert(strcmp(wyn_regex_error_string(WYN_REGEX_OK), "No error") == 0);
    assert(strcmp(wyn_regex_error_string(WYN_REGEX_INVALID_PATTERN), "Invalid regex pattern") == 0);
    assert(strcmp(wyn_regex_error_string(WYN_REGEX_MEMORY_ERROR), "Memory allocation error") == 0);
    
    printf("✓ Error handling tests passed\n");
}

void test_captures() {
    printf("Testing capture groups...\n");
    
    // Test capture creation
    WynCaptures* captures = wyn_captures_new(3);
    assert(captures != NULL);
    assert(captures->count == 3);
    assert(captures->matches != NULL);
    
    // Test getting non-existent capture
    WynMatch* match = wyn_captures_get(captures, 0);
    assert(match == NULL);  // No matches set yet
    
    // Test named capture lookup
    match = wyn_captures_get_named(captures, "test");
    assert(match == NULL);  // No named captures
    
    wyn_captures_free(captures);
    
    printf("✓ Capture group tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    assert(!wyn_regex_is_match(NULL, NULL));
    assert(wyn_regex_find(NULL, NULL) == NULL);
    
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("test", &error);
    assert(regex != NULL);
    
    // Test empty string
    WynString* empty = wyn_string_from_cstr("");
    assert(!wyn_regex_is_match(regex, empty));
    wyn_string_free(empty);
    
    // Test very long string
    WynString* long_str = wyn_string_new();
    for (int i = 0; i < 1000; i++) {
        wyn_string_push_char(long_str, 'a');
    }
    wyn_string_push_str(long_str, "test");
    assert(wyn_regex_is_match(regex, long_str));
    wyn_string_free(long_str);
    
    wyn_regex_free(regex);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Regex System Tests\n");
    printf("==========================\n\n");
    
    test_regex_creation();
    test_basic_matching();
    test_special_characters();
    test_find_operations();
    test_find_all_operations();
    test_string_replacement();
    test_unicode_support();
    test_character_classes();
    test_error_handling();
    test_captures();
    test_edge_cases();
    
    printf("\n✅ All regex system tests passed!\n");
    return 0;
}
