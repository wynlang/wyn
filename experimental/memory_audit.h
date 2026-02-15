#ifndef MEMORY_AUDIT_H
#define MEMORY_AUDIT_H

#include <stddef.h>

typedef enum {
    OWNER_CALLER,    // Caller must free
    OWNER_CALLEE,    // Function frees internally
    OWNER_SHARED     // Reference counted
} MemoryOwnership;

typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    MemoryOwnership ownership;
} AllocationRecord;

void audit_malloc_free_pairs(void);
void document_allocation_ownership(void* ptr, MemoryOwnership ownership);
void report_memory_leaks(void);

#endif // MEMORY_AUDIT_H
