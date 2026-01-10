#include "../src/regex.h"
#include <stdio.h>
#include <string.h>

int main() {
    WynRegexError error = {0};
    WynRegex* regex = wyn_regex_new("世", &error);
    
    if (!regex) {
        printf("Failed to create regex: %s\n", wyn_regex_error_string(error.code));
        return 1;
    }
    
    printf("Unicode regex created successfully\n");
    
    WynString* text = wyn_string_from_cstr("Hello 世界");
    printf("Text: %s\n", wyn_string_as_cstr(text));
    printf("Text length: %zu characters\n", wyn_string_char_count(text));
    
    // Check what the regex pattern looks like
    printf("Pattern codepoint: U+%04X\n", regex->root->codepoint);
    
    bool matches = wyn_regex_is_match(regex, text);
    printf("Matches: %s\n", matches ? "true" : "false");
    
    wyn_string_free(text);
    wyn_regex_free(regex);
    
    return 0;
}
