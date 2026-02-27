// Wyn coroutine wrapper — thin layer over minicoro.h
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

// Override minicoro defaults before including
#define MCO_USE_VMEM_ALLOCATOR      // Use mmap — only commits physical pages as stack grows
#define MCO_MIN_STACK_SIZE 4096     // 4KB minimum
#define MCO_DEFAULT_STACK_SIZE 8388608 // 8MB virtual (matches Go's default goroutine max)
#define MINICORO_IMPL
#include "../vendor/minicoro/minicoro.h"
#include "coroutine.h"

#define WYN_CORO_STACK_SIZE 8388608 // 8MB virtual — OS only commits pages on use

static size_t wyn_coro_stack_size(void) {
    static size_t sz = 0;
    if (sz) return sz;
    const char* env = getenv("WYN_CORO_STACK");
    sz = (env && atoi(env) > 0) ? (size_t)atoi(env) : WYN_CORO_STACK_SIZE;
    return sz;
}

// Forward declaration
static void coro_trampoline(mco_coro* co);

// === Coroutine block pool ===
// With vmem allocator, we let minicoro handle allocation via mmap.
// The pool just recycles mco_coro structs (not stacks — those are mmap'd).

static _Atomic size_t coro_block_size = 0;

static size_t coro_pool_get_size(void) __attribute__((unused));
static size_t coro_pool_get_size(void) {
    size_t sz = atomic_load_explicit(&coro_block_size, memory_order_acquire);
    if (sz > 0) return sz;
    mco_desc desc = mco_desc_init(coro_trampoline, wyn_coro_stack_size());
    sz = desc.coro_size;
    atomic_store_explicit(&coro_block_size, sz, memory_order_release);
    return sz;
}

// With vmem, don't pool — let mmap/munmap handle it for proper page management
static void* coro_pool_acquire(void) __attribute__((unused));
static void* coro_pool_acquire(void) {
    return NULL; // Force minicoro to allocate via mmap
}

// Unused with vmem — kept for API compatibility
static void coro_pool_release(void* block) __attribute__((unused));
static void coro_pool_release(void* block) {
    (void)block;
}

static _Atomic long wyn_coro_live_count = 0;

// WynCoroutine wraps mco_coro + user function/arg
struct WynCoroutine {
    mco_coro* co;
    void (*fn)(void*);
    void* arg;
};

// === WynCoroutine struct pool (avoid malloc/free per spawn) ===
#define WC_POOL_SIZE 4096
static WynCoroutine wc_pool[WC_POOL_SIZE];
static _Atomic int wc_pool_head = 0;
static WynCoroutine* wc_free_stack[WC_POOL_SIZE];
static _Atomic int wc_free_top = 0;

static WynCoroutine* wc_alloc(void) {
    int top = atomic_load_explicit(&wc_free_top, memory_order_relaxed);
    while (top > 0) {
        if (atomic_compare_exchange_weak_explicit(&wc_free_top, &top, top - 1,
                memory_order_acq_rel, memory_order_relaxed))
            return wc_free_stack[top - 1];
    }
    int idx = atomic_fetch_add(&wc_pool_head, 1);
    if (idx < WC_POOL_SIZE) return &wc_pool[idx];
    return malloc(sizeof(WynCoroutine));
}

static void wc_dealloc(WynCoroutine* wc) {
    if (wc >= wc_pool && wc < wc_pool + WC_POOL_SIZE) {
        int top = atomic_load_explicit(&wc_free_top, memory_order_relaxed);
        while (top < WC_POOL_SIZE) {
            if (atomic_compare_exchange_weak_explicit(&wc_free_top, &top, top + 1,
                    memory_order_acq_rel, memory_order_relaxed)) {
                wc_free_stack[top] = wc;
                return;
            }
        }
    }
    free(wc);
}

// Trampoline: minicoro calls this, we call the user's fn(arg)
static void coro_trampoline(mco_coro* co) {
    WynCoroutine* wc = (WynCoroutine*)mco_get_user_data(co);
    wc->fn(wc->arg);
}

WynCoroutine* wyn_coro_create(void (*fn)(void*), void* arg) {
    WynCoroutine* wc = wc_alloc();
    if (!wc) return NULL;
    wc->fn = fn;
    wc->arg = arg;

    mco_desc desc = mco_desc_init(coro_trampoline, wyn_coro_stack_size());
    desc.user_data = wc;

    mco_coro* co = NULL;
    if (mco_create(&co, &desc) != MCO_SUCCESS) {
        wc_dealloc(wc);
        return NULL;
    }
    wc->co = co;
    atomic_fetch_add(&wyn_coro_live_count, 1);
    return wc;
}

bool wyn_coro_resume(WynCoroutine* wc) {
    if (!wc || !wc->co) return false;
    mco_result res = mco_resume(wc->co);
    if (res != MCO_SUCCESS) {
        if (res == MCO_STACK_OVERFLOW) {
            fprintf(stderr, "\033[31mError:\033[0m coroutine stack overflow (64KB limit)\n");
            fprintf(stderr, "  Set WYN_CORO_STACK=262144 for larger stacks.\n");
        }
        // Any error (overflow, not suspended, etc.) — treat as dead
        return false;
    }
    return mco_status(wc->co) != MCO_DEAD;
}

void wyn_coro_yield(void) {
    mco_coro* co = mco_running();
    if (co) mco_yield(co);
}

bool wyn_coro_done(WynCoroutine* wc) {
    if (!wc || !wc->co) return true;
    return mco_status(wc->co) == MCO_DEAD;
}

void wyn_coro_destroy(WynCoroutine* wc) {
    if (!wc) return;
    if (wc->co) {
        mco_destroy(wc->co);
    }
    atomic_fetch_sub(&wyn_coro_live_count, 1);
    wc_dealloc(wc);
}

long wyn_coro_get_live_count(void) {
    return atomic_load(&wyn_coro_live_count);
}

WynCoroutine* wyn_coro_current(void) {
    mco_coro* co = mco_running();
    if (!co) return NULL;
    return (WynCoroutine*)mco_get_user_data(co);
}
