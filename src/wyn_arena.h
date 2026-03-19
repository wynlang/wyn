#ifndef WYN_ARENA_H
#define WYN_ARENA_H

#include <stdlib.h>
#include <string.h>

typedef struct WynArenaBlock {
    char* data;
    size_t used;
    size_t capacity;
    struct WynArenaBlock* next;
} WynArenaBlock;

void* wyn_arena_alloc(size_t size);
void wyn_arena_reset(void);
char* wyn_str_alloc(size_t len);
char* wyn_strdup(const char* s);

#endif
