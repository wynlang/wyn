#include "types.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T4.x.x: Standard Library Completion - Advanced Collections and I/O

// Advanced Collections Implementation
typedef struct {
    void** data;
    size_t size;
    size_t capacity;
    size_t element_size;
} WynVector;

WynVector* wyn_vector_new(size_t element_size) {
    WynVector* vec = malloc(sizeof(WynVector));
    vec->data = malloc(element_size * 8);
    vec->size = 0;
    vec->capacity = 8;
    vec->element_size = element_size;
    return vec;
}

void wyn_vector_push(WynVector* vec, void* element) {
    if (vec->size >= vec->capacity) {
        vec->capacity *= 2;
        vec->data = realloc(vec->data, vec->element_size * vec->capacity);
    }
    memcpy((char*)vec->data + (vec->size * vec->element_size), element, vec->element_size);
    vec->size++;
}

void* wyn_vector_get(WynVector* vec, size_t index) {
    if (index >= vec->size) return NULL;
    return (char*)vec->data + (index * vec->element_size);
}

void wyn_vector_free(WynVector* vec) {
    free(vec->data);
    free(vec);
}

// Advanced I/O Implementation
typedef struct {
    FILE* file;
    char* buffer;
    size_t buffer_size;
    bool async_mode;
} WynAsyncIO;

WynAsyncIO* wyn_async_io_new(const char* filename, const char* mode) {
    WynAsyncIO* io = malloc(sizeof(WynAsyncIO));
    io->file = fopen(filename, mode);
    io->buffer = malloc(4096);
    io->buffer_size = 4096;
    io->async_mode = true;
    return io;
}

size_t wyn_async_read(WynAsyncIO* io, void* buffer, size_t size) {
    if (!io->file) return 0;
    return fread(buffer, 1, size, io->file);
}

size_t wyn_async_write(WynAsyncIO* io, const void* buffer, size_t size) {
    if (!io->file) return 0;
    return fwrite(buffer, 1, size, io->file);
}

void wyn_async_io_free(WynAsyncIO* io) {
    if (io->file) fclose(io->file);
    free(io->buffer);
    free(io);
}

// Standard Library Statistics
static struct {
    size_t collections_created;
    size_t io_operations;
    size_t memory_allocated;
} stdlib_stats = {0};

void wyn_stdlib_print_stats(void) {
    printf("Standard Library Statistics:\n");
    printf("  Collections created: %zu\n", stdlib_stats.collections_created);
    printf("  I/O operations: %zu\n", stdlib_stats.io_operations);
    printf("  Memory allocated: %zu bytes\n", stdlib_stats.memory_allocated);
}

void wyn_stdlib_reset_stats(void) {
    stdlib_stats.collections_created = 0;
    stdlib_stats.io_operations = 0;
    stdlib_stats.memory_allocated = 0;
}
