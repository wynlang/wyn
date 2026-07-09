#include "wyn_rc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// RC header with dual magic sentinel for heap detection.
// Layout: [magic(4)][refcount(4)][capacity(4)][length(4)][magic2(4)][user data...]
// Two independent sentinels (magic and its complement) bracket the numeric
// fields so a stray in-range pointer must match BOTH to be treated as heap RC,
// making false positives astronomically unlikely. magic2 sits last so magic,
// refcount, capacity, and length keep byte-identical offsets across the three
// hand-mirrored layouts (here, RcHdr in wyn_runtime.h, WynRcHeader in wyn_rc.h).
#define WYN_RC_MAGIC 0x57594E52  // "WYNR"
#define WYN_RC_MAGIC2 (~WYN_RC_MAGIC & 0xFFFFFFFFu)  // 0xA8A6B1AD

typedef struct {
    uint32_t magic;
    _Atomic int32_t refcount;
    uint32_t capacity;  // Allocated bytes (for realloc-in-place)
    uint32_t length;    // Cached string length (avoids O(n) strlen)
    uint32_t magic2;    // Complement sentinel; must equal ~magic
} WynRcHeaderFull;

// Track the heap range to avoid reading before string literals
static _Atomic(void*) heap_low = NULL;
static _Atomic(void*) heap_high = NULL;

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
    hdr->magic2 = WYN_RC_MAGIC2;
    void* ptr = (char*)hdr + sizeof(WynRcHeaderFull);
    // Track heap range
    // Track heap range (atomic CAS for thread safety)
    void* lo = atomic_load_explicit(&heap_low, memory_order_relaxed);
    while (!lo || (void*)hdr < lo) {
        if (atomic_compare_exchange_weak_explicit(&heap_low, &lo, (void*)hdr,
                memory_order_relaxed, memory_order_relaxed)) break;
    }
    void* hi = atomic_load_explicit(&heap_high, memory_order_relaxed);
    void* new_hi = (void*)((char*)ptr + size);
    while (!hi || new_hi > hi) {
        if (atomic_compare_exchange_weak_explicit(&heap_high, &hi, new_hi,
                memory_order_relaxed, memory_order_relaxed)) break;
    }
    return ptr;
}

int wyn_rc_is_heap(const void* ptr) {
    if (!ptr) return 0;
    void* lo = atomic_load_explicit(&heap_low, memory_order_relaxed);
    if (!lo) return 0;
    void* hi = atomic_load_explicit(&heap_high, memory_order_relaxed);
    if (ptr < lo || ptr > hi) return 0;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    return hdr->magic == WYN_RC_MAGIC && hdr->magic2 == WYN_RC_MAGIC2;
}

void wyn_rc_set_length(const void* ptr, uint32_t len) {
    if (ptr && wyn_rc_is_heap(ptr)) {
        rc_full_header(ptr)->length = len;
    }
}

uint32_t wyn_rc_get_length(const void* ptr) {
    if (ptr && wyn_rc_is_heap(ptr)) {
        return rc_full_header(ptr)->length;
    }
    return 0;
}

void wyn_rc_retain(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load_explicit(&hdr->refcount, memory_order_relaxed);
    if (rc == WYN_RC_IMMORTAL) return;
    atomic_fetch_add_explicit(&hdr->refcount, 1, memory_order_relaxed);
}

void wyn_rc_release(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    WynRcHeaderFull* hdr = rc_full_header(ptr);
    int32_t rc = atomic_load_explicit(&hdr->refcount, memory_order_relaxed);
    if (rc == WYN_RC_IMMORTAL) return;
    if (atomic_fetch_sub_explicit(&hdr->refcount, 1, memory_order_acq_rel) == 1) {
        atomic_thread_fence(memory_order_acquire); // Ensure all reads complete before free
        hdr->magic = 0;
        hdr->magic2 = 0;
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
