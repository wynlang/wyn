#include "wyn_rc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// RC header with magic number for heap detection.
// Layout: [magic(4)][refcount(4)][user data...]
#define WYN_RC_MAGIC 0x57594E52  // "WYNR"

typedef struct {
    uint32_t magic;
    _Atomic int32_t refcount;
    uint32_t capacity;  // Allocated bytes (for realloc-in-place)
    uint32_t length;    // Cached string length (avoids O(n) strlen)
} WynRcHeaderFull;

// Track the heap range to avoid reading before string literals
static void* heap_low = NULL;
static void* heap_high = NULL;

static inline WynRcHeaderFull* rc_full_header(const void* ptr) {
    return (WynRcHeaderFull*)((char*)ptr - sizeof(WynRcHeaderFull));
}

void* wyn_rc_alloc(size_t size) {
    WynRcHeaderFull* hdr = malloc(sizeof(WynRcHeaderFull) + size);
    if (!hdr) return NULL;
    hdr->magic = WYN_RC_MAGIC;
    atomic_store(&hdr->refcount, 1);
    hdr->capacity = (uint32_t)size;
    hdr->length = 0;  // Set by concat when known
    void* ptr = (char*)hdr + sizeof(WynRcHeaderFull);
    // Track heap range
    if (!heap_low || ptr < heap_low) heap_low = hdr;
    if (!heap_high || (char*)ptr + size > (char*)heap_high) heap_high = (char*)ptr + size;
    return ptr;
}

int wyn_rc_is_heap(const void* ptr) {
    if (!ptr || !heap_low) return 0;
    // Quick range check: only read header if pointer is in heap range
    if (ptr < heap_low || ptr > heap_high) return 0;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return hdr->magic == WYN_RC_MAGIC;
}

void wyn_rc_retain(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load(&hdr->refcount);
    if (rc == WYN_RC_IMMORTAL) return;
    atomic_fetch_add(&hdr->refcount, 1);
}

void wyn_rc_release(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load(&hdr->refcount);
    if (rc == WYN_RC_IMMORTAL) return;
    if (atomic_fetch_sub(&hdr->refcount, 1) == 1) {
        hdr->magic = 0;
        free(hdr);
    }
}

int wyn_rc_count(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return -1;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return atomic_load(&hdr->refcount);
}

uint32_t wyn_rc_capacity(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return 0;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return hdr->capacity;
}

void wyn_rc_set_capacity(const void* ptr, uint32_t cap) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    hdr->capacity = cap;
}
