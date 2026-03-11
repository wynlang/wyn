#include "wyn_rc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simple heap tracking: we tag rc_alloc'd pointers by storing a magic
// value in the header. This lets us distinguish heap strings from
// string literals (which live in .rodata and must never be freed).
#define WYN_RC_MAGIC 0x57594E52  // "WYNR"

typedef struct {
    uint32_t magic;
    _Atomic int32_t refcount;
} WynRcHeaderFull;

static inline WynRcHeaderFull* rc_full_header(const void* ptr) {
    return (WynRcHeaderFull*)((char*)ptr - sizeof(WynRcHeaderFull));
}

void* wyn_rc_alloc(size_t size) {
    WynRcHeaderFull* hdr = malloc(sizeof(WynRcHeaderFull) + size);
    if (!hdr) return NULL;
    hdr->magic = WYN_RC_MAGIC;
    atomic_store(&hdr->refcount, 1);
    return (char*)hdr + sizeof(WynRcHeaderFull);
}

int wyn_rc_is_heap(const void* ptr) {
    if (!ptr) return 0;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return hdr->magic == WYN_RC_MAGIC;
}

void wyn_rc_retain(const void* ptr) {
    if (!ptr) return;
    if (!wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load(&hdr->refcount);
    if (rc == WYN_RC_IMMORTAL) return;
    atomic_fetch_add(&hdr->refcount, 1);
}

void wyn_rc_release(const void* ptr) {
    if (!ptr) return;
    if (!wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load(&hdr->refcount);
    if (rc == WYN_RC_IMMORTAL) return;
    if (atomic_fetch_sub(&hdr->refcount, 1) == 1) {
        hdr->magic = 0; // invalidate
        free(hdr);
    }
}

int wyn_rc_count(const void* ptr) {
    if (!ptr) return 0;
    if (!wyn_rc_is_heap(ptr)) return -1; // immortal/literal
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return atomic_load(&hdr->refcount);
}
