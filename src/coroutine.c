// Wyn coroutine wrapper — thin layer over minicoro.h
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#include <sys/mman.h>
#else
#include <windows.h>
// Minimal mmap shim for Windows
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED 0x10
#define MAP_FAILED ((void*)-1)
#define PROT_READ 0x1
#define PROT_WRITE 0x2
static inline void* mmap(void* addr, size_t len, int prot, int flags, int fd, long off) {
    (void)prot; (void)fd; (void)off;
    if (flags & MAP_FIXED) { VirtualFree(addr, 0, MEM_RELEASE); }
    return VirtualAlloc(addr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static inline int munmap(void* addr, size_t len) { (void)len; VirtualFree(addr, 0, MEM_RELEASE); return 0; }
#endif

#define MCO_USE_VMEM_ALLOCATOR
#define MCO_MIN_STACK_SIZE 4096
#define MCO_DEFAULT_STACK_SIZE 8388608
#define MINICORO_IMPL
#define MCO_LOG(s) ((void)0)
#include "../vendor/minicoro/minicoro.h"
#include "coroutine.h"

#define WYN_CORO_STACK_SIZE 65536

static size_t wyn_coro_stack_size(void) {
    static size_t sz = 0;
    if (sz) return sz;
    const char* env = getenv("WYN_CORO_STACK");
    sz = (env && atoi(env) > 0) ? (size_t)atoi(env) : WYN_CORO_STACK_SIZE;
    return sz;
}

static void coro_trampoline(mco_coro* co);
static _Atomic long wyn_coro_live_count = 0;

struct WynCoroutine {
    mco_coro* co;
    void (*fn)(void*);
    void* arg;
};

// === WynCoroutine struct pool ===
#define WC_POOL_SIZE (64 * 1024)
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

// === Coroutine memory pool ===
// Recycles mmap'd blocks via MAP_FIXED remap (fresh zeroed pages).
// Avoids munmap+mmap round-trip while keeping virtual address reserved.
#define CORO_POOL_MAX 4096
static void* coro_pool_slots[CORO_POOL_MAX];
static int coro_pool_count = 0;
static size_t coro_pool_block_size = 0;
#ifdef _WIN32
static CRITICAL_SECTION coro_pool_cs;
static int coro_pool_cs_init = 0;
#define coro_pool_lock() do { if (!coro_pool_cs_init) { InitializeCriticalSection(&coro_pool_cs); coro_pool_cs_init = 1; } EnterCriticalSection(&coro_pool_cs); } while(0)
#define coro_pool_unlock() LeaveCriticalSection(&coro_pool_cs)
#else
static pthread_mutex_t coro_pool_mtx = PTHREAD_MUTEX_INITIALIZER;
#define coro_pool_lock() pthread_mutex_lock(&coro_pool_mtx)
#define coro_pool_unlock() pthread_mutex_unlock(&coro_pool_mtx)
#endif

static void* coro_pool_alloc(size_t size, void* ud) {
    (void)ud;
    if (size == coro_pool_block_size) {
        coro_pool_lock();
        if (coro_pool_count > 0) {
            void* ptr = coro_pool_slots[--coro_pool_count];
            coro_pool_unlock();
            // Remap to get fresh zeroed pages at the same virtual address.
            void* fresh = mmap(ptr, size, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            return (fresh != MAP_FAILED) ? fresh : NULL;
        }
        coro_pool_unlock();
    }
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr != MAP_FAILED ? ptr : NULL;
}

static void coro_pool_dealloc(void* ptr, size_t size, void* ud) {
    (void)ud;
    if (size == coro_pool_block_size) {
        coro_pool_lock();
        if (coro_pool_count < CORO_POOL_MAX) {
            coro_pool_slots[coro_pool_count++] = ptr;
            coro_pool_unlock();
            return;
        }
        coro_pool_unlock();
    }
    munmap(ptr, size);
}

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
    desc.alloc_cb = coro_pool_alloc;
    desc.dealloc_cb = coro_pool_dealloc;
    if (!coro_pool_block_size) coro_pool_block_size = desc.coro_size;

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
            fprintf(stderr, "\033[31mError:\033[0m coroutine stack overflow (%zuKB limit)\n", wyn_coro_stack_size() / 1024);
            fprintf(stderr, "  Set WYN_CORO_STACK=262144 for larger stacks (256KB).\n");
        }
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
    if (wc->co) mco_destroy(wc->co);
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
