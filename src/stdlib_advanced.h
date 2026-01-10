#ifndef WYN_STDLIB_ADVANCED_H
#define WYN_STDLIB_ADVANCED_H

#include <stddef.h>
#include <stdbool.h>

// T4.x.x: Advanced Standard Library - Collections and I/O

// Advanced Collections
typedef struct WynVector WynVector;

WynVector* wyn_vector_new(size_t element_size);
void wyn_vector_push(WynVector* vec, void* element);
void* wyn_vector_get(WynVector* vec, size_t index);
void wyn_vector_free(WynVector* vec);

// Advanced I/O
typedef struct WynAsyncIO WynAsyncIO;

WynAsyncIO* wyn_async_io_new(const char* filename, const char* mode);
size_t wyn_async_read(WynAsyncIO* io, void* buffer, size_t size);
size_t wyn_async_write(WynAsyncIO* io, const void* buffer, size_t size);
void wyn_async_io_free(WynAsyncIO* io);

// Statistics
void wyn_stdlib_print_stats(void);
void wyn_stdlib_reset_stats(void);

#endif
