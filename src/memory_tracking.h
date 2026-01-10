#ifndef MEMORY_TRACKING_H
#define MEMORY_TRACKING_H

#include <stdlib.h>

// Memory tracking for debugging
#ifdef DEBUG_MEMORY
#define TRACKED_MALLOC(size) tracked_malloc(size, __FILE__, __LINE__)
#define TRACKED_FREE(ptr) tracked_free(ptr, __FILE__, __LINE__)

void* tracked_malloc(size_t size, const char* file, int line);
void tracked_free(void* ptr, const char* file, int line);
void print_memory_leaks(void);
#else
#define TRACKED_MALLOC(size) malloc(size)
#define TRACKED_FREE(ptr) free(ptr)
#endif

#endif
