#include "wyn_arena.h"

// Thread-local arena — each thread gets its own arena, no contention
#ifdef __TINYC__
// TCC doesn't support __thread — use global (safe for single-threaded TCC builds)
static WynArenaBlock* _wyn_arena_head = NULL;
static WynArenaBlock* _wyn_arena_current = NULL;
#else
static __thread WynArenaBlock* _wyn_arena_head = NULL;
static __thread WynArenaBlock* _wyn_arena_current = NULL;
#endif

static WynArenaBlock* wyn_arena_new_block(size_t min_size) {
    size_t cap = min_size < 65536 ? 65536 : min_size;
    WynArenaBlock* b = malloc(sizeof(WynArenaBlock));
    b->data = malloc(cap);
    b->used = 0;
    b->capacity = cap;
    b->next = NULL;
    return b;
}

void* wyn_arena_alloc(size_t size) {
    if (!_wyn_arena_current) {
        _wyn_arena_head = wyn_arena_new_block(size);
        _wyn_arena_current = _wyn_arena_head;
    }
    size = (size + 7) & ~7;
    if (_wyn_arena_current->used + size > _wyn_arena_current->capacity) {
        WynArenaBlock* b = wyn_arena_new_block(size);
        _wyn_arena_current->next = b;
        _wyn_arena_current = b;
    }
    void* ptr = _wyn_arena_current->data + _wyn_arena_current->used;
    _wyn_arena_current->used += size;
    return ptr;
}

void wyn_arena_reset(void) {
    WynArenaBlock* b = _wyn_arena_head;
    while (b) {
        b->used = 0;
        b = b->next;
    }
    _wyn_arena_current = _wyn_arena_head;
}

char* wyn_str_alloc(size_t len) {
    return (char*)wyn_arena_alloc(len + 1);
}

char* wyn_strdup(const char* s) {
    if (!s) return "";
    size_t len = strlen(s);
    char* r = wyn_str_alloc(len);
    memcpy(r, s, len + 1);
    return r;
}
