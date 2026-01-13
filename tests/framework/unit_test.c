#include "unit_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MemoryBlock* memory_blocks = NULL;

void* test_malloc(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    
    MemoryBlock* block = malloc(sizeof(MemoryBlock));
    if (!block) {
        free(ptr);
        return NULL;
    }
    
    block->ptr = ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->next = memory_blocks;
    memory_blocks = block;
    
    return ptr;
}

void test_free(void* ptr, const char* file, int line) {
    if (!ptr) return;
    
    MemoryBlock** current = &memory_blocks;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemoryBlock* to_remove = *current;
            *current = (*current)->next;
            free(to_remove->ptr);
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
    
    fprintf(stderr, "Warning: Attempting to free untracked pointer at %s:%d\n", file, line);
    free(ptr);
}

void check_memory_leaks() {
    int leak_count = 0;
    MemoryBlock* current = memory_blocks;
    
    while (current) {
        if (leak_count == 0) {
            printf("\nMemory leaks detected:\n");
        }
        printf("  Leak: %zu bytes allocated at %s:%d\n", 
               current->size, current->file, current->line);
        leak_count++;
        current = current->next;
    }
    
    if (leak_count > 0) {
        printf("Total memory leaks: %d\n", leak_count);
        exit(1);
    } else {
        printf("No memory leaks detected.\n");
    }
}