// Enhanced Future implementation with combinators
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <errno.h>

typedef enum {
    FUTURE_PENDING,
    FUTURE_READY
} FutureState;

typedef struct Future {
    _Atomic int state;
    void* result;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Future;

// Future pool - bulk allocate
#define FUTURE_CHUNK_SIZE 1024
typedef struct FutureChunk {
    Future futures[FUTURE_CHUNK_SIZE];
    struct FutureChunk* next;
} FutureChunk;

static _Atomic(Future*) future_free_list = NULL;
static _Atomic(FutureChunk*) future_chunk_list = NULL;
static pthread_mutex_t future_alloc_lock = PTHREAD_MUTEX_INITIALIZER;

static void alloc_future_chunk() {
    FutureChunk* chunk = malloc(sizeof(FutureChunk));
    
    // Initialize and link all futures
    for (int i = 0; i < FUTURE_CHUNK_SIZE; i++) {
        pthread_mutex_init(&chunk->futures[i].lock, NULL);
        pthread_cond_init(&chunk->futures[i].cond, NULL);
        if (i < FUTURE_CHUNK_SIZE - 1) {
            chunk->futures[i].result = &chunk->futures[i + 1];
        } else {
            chunk->futures[i].result = NULL;
        }
    }
    
    // Add to free list
    Future* old_head;
    do {
        old_head = atomic_load(&future_free_list);
        chunk->futures[FUTURE_CHUNK_SIZE - 1].result = old_head;
    } while (!atomic_compare_exchange_weak(&future_free_list, &old_head, &chunk->futures[0]));
    
    // Track chunk
    FutureChunk* old_chunk;
    do {
        old_chunk = atomic_load(&future_chunk_list);
        chunk->next = old_chunk;
    } while (!atomic_compare_exchange_weak(&future_chunk_list, &old_chunk, chunk));
}

// Create a new future
Future* future_new() {
    Future* f;
    Future* next;
    do {
        f = atomic_load(&future_free_list);
        if (!f) {
            pthread_mutex_lock(&future_alloc_lock);
            f = atomic_load(&future_free_list);
            if (!f) {
                alloc_future_chunk();
                f = atomic_load(&future_free_list);
            }
            pthread_mutex_unlock(&future_alloc_lock);
            if (!f) {
                f = malloc(sizeof(Future));
                pthread_mutex_init(&f->lock, NULL);
                pthread_cond_init(&f->cond, NULL);
                break;
            }
        }
        next = (Future*)f->result;
    } while (!atomic_compare_exchange_weak(&future_free_list, &f, next));
    
    atomic_store(&f->state, FUTURE_PENDING);
    f->result = NULL;
    return f;
}

// Set future result (called by worker)
void future_set(Future* f, void* result) {
    pthread_mutex_lock(&f->lock);
    f->result = result;
    atomic_store(&f->state, FUTURE_READY);
    pthread_cond_broadcast(&f->cond);
    pthread_mutex_unlock(&f->lock);
}

// Wait for future result (blocking)
void* future_get(Future* f) {
    pthread_mutex_lock(&f->lock);
    while (atomic_load(&f->state) == FUTURE_PENDING) {
        pthread_cond_wait(&f->cond, &f->lock);
    }
    void* result = f->result;
    pthread_mutex_unlock(&f->lock);
    return result;
}

// Wait with timeout (returns NULL on timeout)
void* future_get_timeout(Future* f, int timeout_ms) {
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
    ts.tv_nsec = (tv.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    pthread_mutex_lock(&f->lock);
    while (atomic_load(&f->state) == FUTURE_PENDING) {
        int ret = pthread_cond_timedwait(&f->cond, &f->lock, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&f->lock);
            return NULL;
        }
    }
    void* result = f->result;
    pthread_mutex_unlock(&f->lock);
    return result;
}

// Poll future (non-blocking)
int future_is_ready(Future* f) {
    return atomic_load(&f->state) == FUTURE_READY;
}

// Free future
void future_free(Future* f) {
    // Return to free list (lock-free)
    Future* old_head;
    do {
        old_head = atomic_load(&future_free_list);
        f->result = old_head;  // Reuse result field for next pointer
    } while (!atomic_compare_exchange_weak(&future_free_list, &old_head, f));
}

// === COMBINATORS ===

typedef void* (*MapFunc)(void*);

// Map over future result
Future* future_map(Future* f, MapFunc map_fn) {
    void* input = future_get(f);
    void* output = map_fn(input);
    // Note: caller must free input if needed
    
    Future* result = future_new();
    future_set(result, output);
    return result;
}

// Wait for all futures to complete
Future* future_all(Future** futures, int count) {
    void** results = malloc(sizeof(void*) * count);
    
    for (int i = 0; i < count; i++) {
        results[i] = future_get(futures[i]);
    }
    
    Future* all = future_new();
    future_set(all, results);
    return all;
}

// Race: return first future to complete
Future* future_race(Future** futures, int count) {
    int attempts = 0;
    while (attempts++ < 10000000) {  // Timeout after many attempts
        for (int i = 0; i < count; i++) {
            if (future_is_ready(futures[i])) {
                void* result = future_get(futures[i]);
                Future* winner = future_new();
                future_set(winner, result);
                return winner;
            }
        }
        sched_yield();
    }
    // Timeout - return first future's result
    void* result = future_get(futures[0]);
    Future* winner = future_new();
    future_set(winner, result);
    return winner;
}

// Select: return (result, index) of first ready future
typedef struct {
    void* result;
    int index;
} SelectResult;

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
