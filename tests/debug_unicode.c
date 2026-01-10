#include "../src/unicode.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char* utf8_text = "Hello, 世界! 🌍";
    printf("Text: %s\n", utf8_text);
    printf("Byte length: %zu\n", strlen(utf8_text));
    
    WynUtf8Error error;
    WynString* utf8_str = wyn_string_from_utf8((const uint8_t*)utf8_text, strlen(utf8_text), &error);
    
    if (utf8_str) {
        printf("String created successfully\n");
        printf("Byte length: %zu\n", wyn_string_byte_len(utf8_str));
        printf("Character count: %zu\n", wyn_string_char_count(utf8_str));
        
        // Manual count
        size_t manual_count = wyn_utf8_char_count((const uint8_t*)utf8_text, strlen(utf8_text));
        printf("Manual char count: %zu\n", manual_count);
        
        // Iterate through characters
        WynStringIterator* iter = wyn_string_iter(utf8_str);
        uint32_t codepoint;
        size_t iter_count = 0;
        printf("Characters: ");
        while (wyn_string_iter_next(iter, &codepoint)) {
            printf("U+%04X ", codepoint);
            iter_count++;
        }
        printf("\nIterator count: %zu\n", iter_count);
        
        wyn_string_iter_free(iter);
        wyn_string_free(utf8_str);
    } else {
        printf("Failed to create string: %s\n", wyn_utf8_error_string(error));
    }
    
    return 0;
}
