// Lightweight Future — recyclable slab, zero malloc
#include <stdlib.h>
#include <stdatomic.h>
#include <sched.h>
#include "coroutine.h"
#include "io_loop.h"

typedef enum { FUTURE_FREE = -1, FUTURE_PENDING = 0, FUTURE_READY = 1 } FutureState;

// Forward declaration
extern void wyn_sched_enqueue(void* task_ptr);

typedef struct Future {
    _Atomic int state;
    void* result;
    _Atomic(void*) waiter;  // Task* waiting on this future — atomic to prevent race
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
    // Atomically claim a slot — if full, just drop (slab still owns the memory)
    int top = atomic_load_explicit(&free_top, memory_order_relaxed);
    while (top < FREE_STACK_SIZE) {
        if (atomic_compare_exchange_weak_explicit(&free_top, &top, top + 1,
                memory_order_acq_rel, memory_order_relaxed)) {
            atomic_store_explicit(&free_stack[top], idx, memory_order_release);
            return;
        }
    }
    // Free stack full — future stays allocated in slab, will be reused when slab wraps
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
                f->result = NULL; atomic_store_explicit(&f->waiter, NULL, memory_order_relaxed);
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
    f->result = NULL; atomic_store_explicit(&f->waiter, NULL, memory_order_relaxed);
    return f;
}

void future_set(Future* f, void* result) {
    f->result = result;
    atomic_store_explicit(&f->state, FUTURE_READY, memory_order_release);
    // Wake the waiting coroutine if any
    void* waiter = atomic_exchange_explicit(&f->waiter, NULL, memory_order_acq_rel);
    if (waiter) {
        wyn_sched_enqueue(waiter);
    }
}

void* future_get(Future* f) {
    if (!f) return NULL;
    // Fast path
    if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
        void* r = f->result;
        future_recycle(f);
        return r;
    }
    // If inside a coroutine, park and wait for future_set to wake us
    if (wyn_coro_current()) {
        void* task = wyn_current_task();
        if (task) {
            // Try to register as waiter using CAS.
            void* expected = NULL;
            if (atomic_compare_exchange_strong_explicit(&f->waiter, &expected, task,
                    memory_order_acq_rel, memory_order_acquire)) {
                // We stored the waiter. Check if future became ready in the meantime.
                if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY) {
                    // Race: future_set ran concurrently. Try to reclaim our waiter.
                    void* reclaimed = atomic_exchange_explicit(&f->waiter, NULL, memory_order_acq_rel);
                    if (!reclaimed) {
                        // future_set already took it and enqueued us.
                        // Park so execute_task doesn't re-enqueue, then yield.
                        // The spurious enqueue will resume us properly.
                        wyn_io_park();
                        wyn_coro_yield();
                    }
                    // Result is ready — fall through
                } else {
                    // Future not ready — park and yield. future_set will wake us.
                    wyn_io_park();
                    wyn_coro_yield();
                }
            }
            // CAS failed means someone else set waiter (shouldn't happen in normal use).
            // Spin-yield until result is visible.
            while (atomic_load_explicit(&f->state, memory_order_acquire) != FUTURE_READY)
                wyn_coro_yield();
        } else {
            // No task context (shouldn't happen) — busy yield
            while (atomic_load_explicit(&f->state, memory_order_acquire) != FUTURE_READY)
                wyn_coro_yield();
        }
        void* r = f->result;
        future_recycle(f);
        return r;
    }
    // Main thread: spin with CPU hints then yield loop
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
        if (wyn_coro_current()) wyn_coro_yield();
        else sched_yield();
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
        if (wyn_coro_current()) wyn_coro_yield();
        else sched_yield();
    }
}
