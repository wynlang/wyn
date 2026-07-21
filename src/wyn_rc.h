#ifndef WYN_RC_H
#define WYN_RC_H

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

// Reference counting header - placed before heap-allocated objects.
// Layout: [magic(4)][refcount(4)][capacity(4)][length(4)][magic2(4)][user data...]
// The pointer returned to user code points to user data, not the header.
// This MUST stay byte-identical to WynRcHeaderFull (wyn_rc.c) and the RcHdr
// mirror in wyn_runtime.h. magic2 = ~magic is the complement sentinel.

// Sentinel refcount for immortal objects (string literals, stack values)
#define WYN_RC_IMMORTAL INT32_MAX

typedef struct {
    uint32_t magic;
    _Atomic int32_t refcount;
    uint32_t capacity;
    uint32_t length;
    uint32_t magic2;
} WynRcHeader;

// Get header from user pointer
static inline WynRcHeader* wyn_rc_header(const void* ptr) {
    return (WynRcHeader*)((char*)ptr - sizeof(WynRcHeader));
}

// Allocate a refcounted block. Returns pointer to user data (after header).
void* wyn_rc_alloc(size_t size);

// Increment refcount. No-op for NULL or immortal objects.
void wyn_rc_retain(const void* ptr);

// Decrement refcount. Frees when it reaches 0. No-op for NULL or immortal.
void wyn_rc_release(const void* ptr);

// Get current refcount (for debugging/testing). Returns -1 for immortal.
int wyn_rc_count(const void* ptr);

// Check if a pointer is refcounted (heap-allocated via wyn_rc_alloc)
int wyn_rc_is_heap(const void* ptr);
void wyn_rc_set_length(const void* ptr, uint32_t len);
uint32_t wyn_rc_get_length(const void* ptr);

#endif
