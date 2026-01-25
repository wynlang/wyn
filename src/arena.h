#ifndef WYN_ARENA_H
#define WYN_ARENA_H

#include <stdlib.h>
#include <string.h>

#define ARENA_BLOCK_SIZE 4096

typedef struct ArenaBlock {
    char* data;
    size_t used;
    size_t capacity;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct {
    ArenaBlock* current;
    ArenaBlock* first;
} WynArena;

// Create new arena
WynArena* wyn_arena_new() {
    WynArena* arena = malloc(sizeof(WynArena));
    arena->first = malloc(sizeof(ArenaBlock));
    arena->first->data = malloc(ARENA_BLOCK_SIZE);
    arena->first->used = 0;
    arena->first->capacity = ARENA_BLOCK_SIZE;
    arena->first->next = NULL;
    arena->current = arena->first;
    return arena;
}

// Allocate memory from arena
void* wyn_arena_alloc(WynArena* arena, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    // Check if current block has space
    if (arena->current->used + size > arena->current->capacity) {
        // Need new block
        size_t block_size = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
        ArenaBlock* new_block = malloc(sizeof(ArenaBlock));
        new_block->data = malloc(block_size);
        new_block->used = 0;
        new_block->capacity = block_size;
        new_block->next = NULL;
        arena->current->next = new_block;
        arena->current = new_block;
    }
    
    void* ptr = arena->current->data + arena->current->used;
    arena->current->used += size;
    return ptr;
}

// Clear arena (reset to beginning, don't free memory)
void wyn_arena_clear(WynArena* arena) {
    ArenaBlock* block = arena->first;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    arena->current = arena->first;
}

// Free arena and all memory
void wyn_arena_free(WynArena* arena) {
    ArenaBlock* block = arena->first;
    while (block) {
        ArenaBlock* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    free(arena);
}

// Allocate int in arena
int* wyn_arena_alloc_int(WynArena* arena, int value) {
    int* ptr = (int*)wyn_arena_alloc(arena, sizeof(int));
    *ptr = value;
    return ptr;
}

// Allocate string in arena
char* wyn_arena_alloc_string(WynArena* arena, const char* str) {
    size_t len = strlen(str) + 1;
    char* ptr = (char*)wyn_arena_alloc(arena, len);
    memcpy(ptr, str, len);
    return ptr;
}

#endif // WYN_ARENA_H
