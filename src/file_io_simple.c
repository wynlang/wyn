#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple file I/O wrappers for LLVM backend

int wyn_file_write_simple(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    
    return (written == len) ? 1 : 0;
}

int wyn_file_read_simple(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    fclose(f);
    return 1;  // Just return success if file can be opened
}

int wyn_file_append_simple(const char* path, const char* content) {
    FILE* f = fopen(path, "a");
    if (!f) return 0;
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    
    return (written == len) ? 1 : 0;
}

int wyn_file_exists_simple(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    fclose(f);
    return 1;
}
