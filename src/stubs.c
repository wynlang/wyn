#include <stdlib.h>
#include <stdio.h>

// Stub for resolve_module
void* resolve_module(const char* name) {
    return NULL;
}

// Stubs for safe memory functions
void* safe_malloc_checked(size_t size, const char* file, int line) {
    return malloc(size);
}

void* safe_calloc_checked(size_t count, size_t size, const char* file, int line) {
    return calloc(count, size);
}

void* safe_realloc_checked(void* ptr, size_t size, const char* file, int line) {
    return realloc(ptr, size);
}

void safe_free_checked(void* ptr, const char* file, int line) {
    free(ptr);
}
