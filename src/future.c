// Lightweight Future — 16 bytes, recyclable slab, zero malloc
#include <stdlib.h>
#include <stdatomic.h>
#include <sched.h>

typedef enum { FUTURE_FREE = -1, FUTURE_PENDING = 0, FUTURE_READY = 1 } FutureState;

typedef struct Future {
    _Atomic int state;
    void* result;
} Future;

// === Recyclable slab allocator ===
// Futures are recycled after await — memory stays constant regardless of spawn count
#define FUTURE_SLAB_SIZE (64 * 1024)  // 64K futures = 1MB slab
static Future future_slab[FUTURE_SLAB_SIZE];
static _Atomic int future_slab_idx = 0;

// Lock-free free list using indices
#define FREE_STACK_SIZE FUTURE_SLAB_SIZE
static _Atomic int free_stack[FREE_STACK_SIZE];
static _Atomic int free_top = 0;  // number of items in free stack

static inline void future_recycle(Future* f) {
    int idx = (int)(f - future_slab);
    if (idx < 0 || idx >= FUTURE_SLAB_SIZE) { free(f); return; }
    int top = atomic_fetch_add_explicit(&free_top, 1, memory_order_relaxed);
    if (top < FREE_STACK_SIZE) {
        atomic_store_explicit(&free_stack[top], idx, memory_order_release);
    }
}

Future* future_new(void) {
    Future* f;
    // Try free list first (recycled futures)
    int top = atomic_load_explicit(&free_top, memory_order_acquire);
    while (top > 0) {
        if (atomic_compare_exchange_weak_explicit(&free_top, &top, top - 1,
                memory_order_acq_rel, memory_order_acquire)) {
            int idx = atomic_load_explicit(&free_stack[top - 1], memory_order_acquire);
            if (idx >= 0 && idx < FUTURE_SLAB_SIZE) {
                f = &future_slab[idx];
                atomic_store_explicit(&f->state, FUTURE_PENDING, memory_order_relaxed);
                f->result = NULL;
                return f;
            }
        }
        top = atomic_load_explicit(&free_top, memory_order_acquire);
    }
    // Slab allocation
    int idx = atomic_fetch_add_explicit(&future_slab_idx, 1, memory_order_relaxed);
    if (idx < FUTURE_SLAB_SIZE) {
        f = &future_slab[idx];
    } else {
        f = malloc(sizeof(Future));
    }
    atomic_store_explicit(&f->state, FUTURE_PENDING, memory_order_relaxed);
    f->result = NULL;
    return f;
}

void future_set(Future* f, void* result) {
    f->result = result;
    atomic_store_explicit(&f->state, FUTURE_READY, memory_order_release);
}

void* future_get(Future* f) {
    if (!f) return NULL;
    // Fast path
    if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
        void* r = f->result;
        future_recycle(f);
        return r;
    }
    // Spin with CPU hints
    for (int i = 0; i < 256; i++) {
        if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
            void* r = f->result;
            future_recycle(f);
            return r;
        }
        #ifdef __x86_64__
        __asm__ volatile("pause");
        #elif defined(__aarch64__) && !defined(__TINYC__)
        __asm__ volatile("isb");
        #endif
    }
    // Yield loop
    while (atomic_load_explicit(&f->state, memory_order_acquire) != FUTURE_READY)
        sched_yield();
    void* r = f->result;
    future_recycle(f);
    return r;
}

void* future_get_timeout(Future* f, int timeout_ms) {
    if (!f) return NULL;
    if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
        void* r = f->result;
        future_recycle(f);
        return r;
    }
    for (int i = 0; i < timeout_ms * 100; i++) {
        if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
            void* r = f->result;
            future_recycle(f);
            return r;
        }
        if (i < 256) {
            #ifdef __x86_64__
            __asm__ volatile("pause");
            #elif defined(__aarch64__) && !defined(__TINYC__)
            __asm__ volatile("isb");
            #endif
        } else {
            sched_yield();
        }
    }
    return NULL;
}

int future_is_ready(Future* f) {
    return atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY;
}

void future_free(Future* f) { if (f) future_recycle(f); }

// === Combinators ===
typedef void* (*MapFunc)(void*);

Future* future_map(Future* f, MapFunc map_fn) {
    void* input = future_get(f);
    Future* result = future_new();
    future_set(result, map_fn(input));
    return result;
}

Future* future_all(Future** futures, int count) {
    void** results = malloc(sizeof(void*) * count);
    for (int i = 0; i < count; i++) results[i] = future_get(futures[i]);
    Future* all = future_new();
    future_set(all, results);
    return all;
}

Future* future_race(Future** futures, int count) {
    while (1) {
        for (int i = 0; i < count; i++) {
            if (future_is_ready(futures[i])) {
                Future* winner = future_new();
                future_set(winner, future_get(futures[i]));
                return winner;
            }
        }
        sched_yield();
    }
}

typedef struct { void* result; int index; } SelectResult;

Future* future_select(Future** futures, int count) {
    while (1) {
        for (int i = 0; i < count; i++) {
            if (future_is_ready(futures[i])) {
                SelectResult* sr = malloc(sizeof(SelectResult));
                sr->result = future_get(futures[i]);
                sr->index = i;
                Future* selected = future_new();
                future_set(selected, sr);
                return selected;
            }
        }
        sched_yield();
    }
}
