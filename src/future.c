// Lightweight Future — spin-then-yield for fast await
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <sched.h>
#include <sys/time.h>
#include <errno.h>

typedef enum { FUTURE_PENDING, FUTURE_READY } FutureState;

typedef struct Future {
    _Atomic int state;
    void* result;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Future;

// === Future pool — lock-free slab allocator ===
#define FUTURE_SLAB_SIZE 4096
static Future future_slab[FUTURE_SLAB_SIZE];
static _Atomic int future_slab_idx = 0;

Future* future_new(void) {
    int idx = atomic_fetch_add(&future_slab_idx, 1);
    Future* f;
    if (idx < FUTURE_SLAB_SIZE) {
        f = &future_slab[idx];
    } else {
        f = malloc(sizeof(Future));
    }
    atomic_store_explicit(&f->state, FUTURE_PENDING, memory_order_relaxed);
    f->result = NULL;
    pthread_mutex_init(&f->lock, NULL);
    pthread_cond_init(&f->cond, NULL);
    return f;
}

// Set result — called by worker thread
void future_set(Future* f, void* result) {
    f->result = result;
    atomic_store_explicit(&f->state, FUTURE_READY, memory_order_release);
    // Wake any waiter
    pthread_mutex_lock(&f->lock);
    pthread_cond_signal(&f->cond);
    pthread_mutex_unlock(&f->lock);
}

// Await — adaptive spin with CPU hints, no sched_yield on fast path
void* future_get(Future* f) {
    if (!f) return NULL; // Guard against NULL future (e.g., spawn on module function)
    // Fast path: already ready
    if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY)
        return f->result;
    
    // Spin phase: tight loop with CPU yield hint (~100-300ns total)
    for (int i = 0; i < 128; i++) {
        if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY)
            return f->result;
        #ifdef __x86_64__
        __asm__ volatile("pause");
        #elif defined(__aarch64__)
        __asm__ volatile("isb"); // instruction synchronization barrier — faster than yield
        #endif
    }
    
    // Medium path: yield a few times (only if spin didn't catch it)
    for (int i = 0; i < 8; i++) {
        if (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY)
            return f->result;
        sched_yield();
    }
    
    // Slow path: condvar
    pthread_mutex_lock(&f->lock);
    while (atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_PENDING)
        pthread_cond_wait(&f->cond, &f->lock);
    void* result = f->result;
    pthread_mutex_unlock(&f->lock);
    return result;
}

void* future_get_timeout(Future* f, int timeout_ms) {
    if (atomic_load(&f->state) == FUTURE_READY) return f->result;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    
    pthread_mutex_lock(&f->lock);
    while (atomic_load(&f->state) == FUTURE_PENDING) {
        if (pthread_cond_timedwait(&f->cond, &f->lock, &ts) == ETIMEDOUT) {
            pthread_mutex_unlock(&f->lock);
            return NULL;
        }
    }
    void* result = f->result;
    pthread_mutex_unlock(&f->lock);
    return result;
}

int future_is_ready(Future* f) {
    return atomic_load_explicit(&f->state, memory_order_acquire) == FUTURE_READY;
}

void future_free(Future* f) {
    // No-op for slab-allocated futures
    // For malloc'd futures, we'd need tracking
}

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
