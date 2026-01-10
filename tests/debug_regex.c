#include "../src/regex.h"
#include <stdio.h>
#include <string.h>

int main() {
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("hello", &error);
    
    if (!regex) {
        printf("Failed to create regex: %s\n", wyn_regex_error_string(error.code));
        return 1;
    }
    
    printf("Regex created successfully\n");
    
    WynString* text = wyn_string_from_cstr("hello world");
    printf("Text: %s\n", wyn_string_as_cstr(text));
    printf("Text length: %zu characters\n", wyn_string_char_count(text));
    
    bool matches = wyn_regex_is_match(regex, text);
    printf("Matches: %s\n", matches ? "true" : "false");
    
    // Try to find the match
    WynMatch* match = wyn_regex_find(regex, text);
    if (match) {
        printf("Found match at %zu-%zu: %s\n", match->start, match->end, 
               wyn_string_as_cstr(match->text));
        wyn_match_free(match);
    } else {
        printf("No match found\n");
    }
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    return 0;
}
