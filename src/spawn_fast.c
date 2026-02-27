// M:N Scheduler for Wyn — coroutine-based (v1.9)
// Architecture: N OS threads (processors), M coroutines (spawns)
// Per-processor local deque + global queue + work-stealing
// Each spawn creates a stackful coroutine via minicoro.h

#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <sched.h>
#include "future.h"
#include "coroutine.h"
#include "io_loop.h"

#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 58
#endif

#define MAX_PROCESSORS 64
#define LOCAL_QUEUE_SIZE 4096  // Power of 2, per-processor

typedef void (*TaskFunc)(void*);
typedef void* (*TaskFuncWithReturn)(void*);

// A Task now wraps a coroutine
typedef struct Task {
    _Atomic(WynCoroutine*) coro;
    Future* future;          // NULL for fire-and-forget
    struct Task* next;       // free list / global queue linkage
    const char* spawn_file;  // Source file that created this spawn
    int spawn_line;          // Line number of the spawn call
    long spawn_id;           // Unique spawn ID for debugging
} Task;

// === Lock-free task pool with recycling ===
#define TASK_POOL_SIZE (64 * 1024)
static Task task_pool[TASK_POOL_SIZE];
static _Atomic int task_pool_head = 0;
static _Atomic(Task*) task_free_list = NULL;

static Task* alloc_task(void) {
    Task* t = atomic_load_explicit(&task_free_list, memory_order_acquire);
    while (t) {
        if (atomic_compare_exchange_weak_explicit(&task_free_list, &t, t->next,
                memory_order_acq_rel, memory_order_acquire))
            return t;
    }
    int idx = atomic_fetch_add(&task_pool_head, 1);
    if (idx < TASK_POOL_SIZE) return &task_pool[idx];
    return malloc(sizeof(Task));
}

static void recycle_task(Task* t) {
    // Don't recycle — the lock-free global queue uses task->next for linkage,
    // and recycling while the task might still be referenced causes ABA problems.
    // The slab allocator provides enough tasks (64K) for most workloads.
    (void)t;
}

// === Per-processor local deque (single-producer, multi-consumer) ===
typedef struct {
    _Atomic(Task*) buffer[LOCAL_QUEUE_SIZE];
    _Atomic int top;
    _Atomic int bottom;
} WorkDeque;

static inline void deque_init(WorkDeque* d) {
    atomic_store(&d->top, 0);
    atomic_store(&d->bottom, 0);
    for (int i = 0; i < LOCAL_QUEUE_SIZE; i++) atomic_store(&d->buffer[i], NULL);
}

static inline int deque_push(WorkDeque* d, Task* task) {
    int t = atomic_load_explicit(&d->top, memory_order_relaxed);
    int b = atomic_load_explicit(&d->bottom, memory_order_acquire);
    if (t - b >= LOCAL_QUEUE_SIZE) return 0;
    atomic_store_explicit(&d->buffer[t & (LOCAL_QUEUE_SIZE - 1)], task, memory_order_relaxed);
    atomic_store_explicit(&d->top, t + 1, memory_order_release);
    return 1;
}

static inline Task* deque_pop(WorkDeque* d) {
    int t = atomic_load_explicit(&d->top, memory_order_relaxed) - 1;
    atomic_store_explicit(&d->top, t, memory_order_seq_cst);
    int b = atomic_load_explicit(&d->bottom, memory_order_seq_cst);
    if (b <= t) {
        return atomic_load_explicit(&d->buffer[t & (LOCAL_QUEUE_SIZE - 1)], memory_order_relaxed);
    }
    // Deque might have exactly one element (b == t) or be empty (b > t)
    Task* task = NULL;
    if (b == t) {
        task = atomic_load_explicit(&d->buffer[t & (LOCAL_QUEUE_SIZE - 1)], memory_order_relaxed);
        int expected = t;
        if (!atomic_compare_exchange_strong_explicit(&d->bottom, &expected, t + 1,
                memory_order_seq_cst, memory_order_relaxed)) {
            task = NULL;  // Lost race with stealer
        }
    }
    atomic_store_explicit(&d->top, t + 1, memory_order_relaxed);
    return task;
}

static inline Task* deque_steal(WorkDeque* d) {
    int b = atomic_load_explicit(&d->bottom, memory_order_acquire);
    int t = atomic_load_explicit(&d->top, memory_order_seq_cst);
    if (b >= t) return NULL;
    Task* task = atomic_load_explicit(&d->buffer[b & (LOCAL_QUEUE_SIZE - 1)], memory_order_relaxed);
    if (atomic_compare_exchange_strong_explicit(&d->bottom, &b, b + 1,
            memory_order_seq_cst, memory_order_relaxed)) {
        return task;
    }
    return NULL;
}

// === Global queue ===
typedef struct {
    _Atomic(Task*) top;
    _Atomic long count;
} GlobalQueue;

static GlobalQueue global_q;

static void global_queue_init(void) {
    atomic_store(&global_q.top, NULL);
    atomic_store(&global_q.count, 0);
}

static void global_queue_push(Task* task) {
    Task* old_top;
    do {
        old_top = atomic_load_explicit(&global_q.top, memory_order_relaxed);
        task->next = old_top;
    } while (!atomic_compare_exchange_weak_explicit(&global_q.top, &old_top, task,
             memory_order_release, memory_order_relaxed));
    atomic_fetch_add_explicit(&global_q.count, 1, memory_order_relaxed);
}

static Task* global_queue_pop(void) {
    Task* top;
    Task* next;
    do {
        top = atomic_load_explicit(&global_q.top, memory_order_acquire);
        if (!top) return NULL;
        next = top->next;
    } while (!atomic_compare_exchange_weak_explicit(&global_q.top, &top, next,
             memory_order_release, memory_order_relaxed));
    atomic_fetch_sub_explicit(&global_q.count, 1, memory_order_relaxed);
    return top;
}

// === Processor (OS thread) ===
typedef struct {
    int id;
    WorkDeque deque;
    pthread_t thread;
    _Atomic int spinning;
    _Atomic int parked;
    int steal_hits;
    _Atomic(Task*) runnext;
    pthread_mutex_t park_lock;
    pthread_cond_t park_cond;
} Processor;

static Processor processors[MAX_PROCESSORS];
static _Atomic int num_processors = 0;
static _Atomic int initialized = 0;
static _Atomic int scheduler_shutdown = 0;
static _Atomic long total_spawned = 0;
static _Atomic long total_completed = 0;

static inline void wake_processor(void) {
    int n = atomic_load_explicit(&num_processors, memory_order_relaxed);
    for (int i = 0; i < n; i++) {
        if (atomic_load_explicit(&processors[i].parked, memory_order_acquire)) {
            pthread_mutex_lock(&processors[i].park_lock);
            pthread_cond_signal(&processors[i].park_cond);
            pthread_mutex_unlock(&processors[i].park_lock);
            return;
        }
    }
}

// Forward declaration for re-enqueue
static void enqueue_task(Task* task);

// Thread-local: current task being executed (for I/O loop integration)
#ifdef __TINYC__
static Task* current_task = NULL;
static int io_parked = 0;  // set by yield points to prevent re-enqueue
#else
static __thread Task* current_task = NULL;
static __thread int io_parked = 0;
#endif

// Public API: get current task pointer (for I/O loop registration)
void* wyn_current_task(void) { return current_task; }

// Public API: mark current coroutine as I/O-parked (don't re-enqueue on yield)
void wyn_io_park(void) { io_parked = 1; }

// Public API: get spawn origin info for current coroutine (for error messages)
const char* wyn_spawn_origin_file(void) {
    return current_task ? current_task->spawn_file : NULL;
}
int wyn_spawn_origin_line(void) {
    return current_task ? current_task->spawn_line : 0;
}
long wyn_spawn_origin_id(void) {
    return current_task ? current_task->spawn_id : 0;
}

// Execute a task: resume its coroutine. If it yields, re-enqueue. If done, complete.
static inline void execute_task(Task* task) {
    WynCoroutine* coro = atomic_load_explicit(&task->coro, memory_order_acquire);
    if (!coro) return;  // already completed (race guard)
    current_task = task;
    io_parked = 0;
    bool alive = wyn_coro_resume(coro);
    current_task = NULL;
    if (alive) {
        if (io_parked) {
            // Coroutine is waiting on I/O — don't re-enqueue.
            // The I/O loop will call wyn_sched_enqueue(task) when fd is ready.
        } else {
            enqueue_task(task);
        }
    } else {
        wyn_coro_destroy(coro);
        atomic_store_explicit(&task->coro, NULL, memory_order_release);
        atomic_fetch_add(&total_completed, 1);
        recycle_task(task);
    }
}

static Task* try_steal(int self_id) {
    int n = atomic_load(&num_processors);
    int start = self_id + 1;
    for (int i = 0; i < n - 1; i++) {
        int victim = (start + i) % n;
        Task* task = deque_steal(&processors[victim].deque);
        if (task) {
            if (self_id >= 0) {
                for (int b = 0; b < 31; b++) {
                    Task* extra = deque_steal(&processors[victim].deque);
                    if (!extra) break;
                    if (!deque_push(&processors[self_id].deque, extra)) {
                        global_queue_push(extra);
                        break;
                    }
                }
            }
            return task;
        }
    }
    return NULL;
}

// Processor main loop
static void* processor_loop(void* arg) {
    Processor* p = (Processor*)arg;
    
    while (!atomic_load(&scheduler_shutdown)) {
        Task* task = atomic_exchange_explicit(&p->runnext, NULL, memory_order_acquire);
        if (task) { execute_task(task); continue; }
        
        task = deque_pop(&p->deque);
        if (task) { execute_task(task); continue; }
        
        task = global_queue_pop();
        if (task) { execute_task(task); continue; }
        
        task = try_steal(p->id);
        if (task) { p->steal_hits = (p->steal_hits < 8) ? p->steal_hits + 1 : 8; execute_task(task); continue; }
        
        // Poll I/O loop — re-enqueues tasks whose fds are ready
        wyn_io_poll();
        
        atomic_store(&p->spinning, 1);
        int spin_limit = 16 + p->steal_hits * 16;
        for (int spins = 0; spins < spin_limit; spins++) {
            task = global_queue_pop();
            if (task) { atomic_store(&p->spinning, 0); execute_task(task); goto next; }
            task = try_steal(p->id);
            if (task) { atomic_store(&p->spinning, 0); execute_task(task); goto next; }
            if ((spins & 15) == 0) wyn_io_poll();  // poll periodically during spin
            #ifdef __x86_64__
            __asm__ volatile("pause");
            #elif defined(__aarch64__) && !defined(__TINYC__)
            __asm__ volatile("isb");
            #endif
        }
        atomic_store(&p->spinning, 0);
        
        p->steal_hits = p->steal_hits > 0 ? p->steal_hits - 1 : 0;
        pthread_mutex_lock(&p->park_lock);
        atomic_store(&p->parked, 1);
        task = global_queue_pop();
        if (task) {
            atomic_store(&p->parked, 0);
            pthread_mutex_unlock(&p->park_lock);
            execute_task(task);
            continue;
        }
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&p->park_cond, &p->park_lock, &ts);
        atomic_store(&p->parked, 0);
        pthread_mutex_unlock(&p->park_lock);
        
        next: ;
    }
    return NULL;
}

// === Public API ===

#ifdef __TINYC__
static int current_processor_id = -1;
#else
static __thread int current_processor_id = -1;
#endif

#include <signal.h>

static void init_scheduler(void) {
    if (atomic_exchange(&initialized, 1)) return;
    
    int cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 4;
    if (cpus > MAX_PROCESSORS) cpus = MAX_PROCESSORS;
    
    atomic_store(&num_processors, cpus);
    global_queue_init();
    
    processors[0].id = 0;
    deque_init(&processors[0].deque);
    atomic_store(&processors[0].spinning, 0);
    atomic_store(&processors[0].parked, 0);
    atomic_store(&processors[0].runnext, NULL);
    pthread_mutex_init(&processors[0].park_lock, NULL);
    pthread_cond_init(&processors[0].park_cond, NULL);
    current_processor_id = 0;
    
    for (int i = 1; i < cpus; i++) {
        processors[i].id = i;
        deque_init(&processors[i].deque);
        atomic_store(&processors[i].spinning, 0);
        atomic_store(&processors[i].parked, 0);
        atomic_store(&processors[i].runnext, NULL);
        pthread_mutex_init(&processors[i].park_lock, NULL);
        pthread_cond_init(&processors[i].park_cond, NULL);
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, 256 * 1024);
        pthread_create(&processors[i].thread, &attr, processor_loop, &processors[i]);
        pthread_attr_destroy(&attr);
    }
}

// Enqueue a task to the current processor's deque or global queue
static void enqueue_task(Task* task) {
    if (current_processor_id > 0) {
        // Worker threads: use runnext fast slot (LIFO)
        Task* old = atomic_exchange_explicit(&processors[current_processor_id].runnext, task, memory_order_release);
        if (old) {
            if (!deque_push(&processors[current_processor_id].deque, old))
                global_queue_push(old);
        }
    } else {
        // Main thread or unknown: use global queue (workers will pick it up)
        global_queue_push(task);
    }
    wake_processor();
}

// Coroutine body for fire-and-forget spawn
typedef struct { TaskFunc func; void* arg; } SpawnArgs;

// SpawnArgs pool — avoid malloc per spawn
#define SA_POOL_SIZE 4096
static SpawnArgs sa_pool[SA_POOL_SIZE];
static _Atomic int sa_pool_head = 0;
static SpawnArgs* sa_free_arr[SA_POOL_SIZE];
static _Atomic int sa_free_top = 0;

static SpawnArgs* sa_alloc(void) {
    int top = atomic_load_explicit(&sa_free_top, memory_order_relaxed);
    while (top > 0) {
        if (atomic_compare_exchange_weak_explicit(&sa_free_top, &top, top - 1,
                memory_order_acq_rel, memory_order_relaxed))
            return sa_free_arr[top - 1];
    }
    int idx = atomic_fetch_add(&sa_pool_head, 1);
    if (idx < SA_POOL_SIZE) return &sa_pool[idx];
    return malloc(sizeof(SpawnArgs));
}

static void sa_dealloc(SpawnArgs* sa) {
    if (sa >= sa_pool && sa < sa_pool + SA_POOL_SIZE) {
        int top = atomic_load_explicit(&sa_free_top, memory_order_relaxed);
        while (top < SA_POOL_SIZE) {
            if (atomic_compare_exchange_weak_explicit(&sa_free_top, &top, top + 1,
                    memory_order_acq_rel, memory_order_relaxed)) {
                sa_free_arr[top] = sa;
                return;
            }
        }
    }
    free(sa);
}

static void spawn_coro_body(void* ctx) {
    SpawnArgs* sa = (SpawnArgs*)ctx;
    sa->func(sa->arg);
    sa_dealloc(sa);
}

// Coroutine body for spawn-with-future (async)
typedef struct { TaskFuncWithReturn func; void* arg; Future* future; } SpawnAsyncArgs;

static void spawn_async_coro_body(void* ctx) {
    SpawnAsyncArgs* sa = (SpawnAsyncArgs*)ctx;
    void* result = sa->func(sa->arg);
    future_set(sa->future, result);
    free(sa);  // SpawnAsyncArgs not pooled (less frequent)
}

void wyn_spawn_fast(TaskFunc func, void* arg) {
    wyn_spawn_fast_traced(func, arg, NULL, 0);
}

void wyn_spawn_fast_traced(TaskFunc func, void* arg, const char* file, int line) {
    init_scheduler();
    
    SpawnArgs* sa = sa_alloc();
    if (!sa) {
        func(arg);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return;
    }
    sa->func = func;
    sa->arg = arg;
    
    WynCoroutine* coro = wyn_coro_create(spawn_coro_body, sa);
    if (!coro) {
        func(arg);
        sa_dealloc(sa);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return;
    }
    
    Task* t = alloc_task();
    if (!t) {
        func(arg);
        wyn_coro_destroy(coro);
        sa_dealloc(sa);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return;
    }
    t->coro = coro;
    t->future = NULL;
    t->spawn_file = file;
    t->spawn_line = line;
    
    long spawned = atomic_fetch_add(&total_spawned, 1);
    t->spawn_id = spawned + 1;
    enqueue_task(t);
    if (spawned == 0 || (spawned & 63) == 0) wake_processor();
}

Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg) {
    return wyn_spawn_async_traced(func, arg, NULL, 0);
}

Future* wyn_spawn_async_traced(TaskFuncWithReturn func, void* arg, const char* file, int line) {
    init_scheduler();
    
    Future* future = future_new();
    
    SpawnAsyncArgs* sa = malloc(sizeof(SpawnAsyncArgs));
    if (!sa) {
        // OOM fallback: run directly
        void* result = func(arg);
        future_set(future, result);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return future;
    }
    sa->func = func;
    sa->arg = arg;
    sa->future = future;
    
    WynCoroutine* coro = wyn_coro_create(spawn_async_coro_body, sa);
    if (!coro) {
        // OOM fallback: run directly on calling thread
        void* result = func(arg);
        future_set(future, result);
        free(sa);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return future;
    }
    
    Task* t = alloc_task();
    if (!t) {
        // OOM: run directly (don't resume coro — it would free sa, then we'd double-free)
        void* result = func(arg);
        future_set(future, result);
        wyn_coro_destroy(coro);
        free(sa);
        atomic_fetch_add(&total_spawned, 1);
        atomic_fetch_add(&total_completed, 1);
        return future;
    }
    t->coro = coro;
    t->future = future;
    t->spawn_file = file;
    t->spawn_line = line;
    
    long spawned = atomic_fetch_add(&total_spawned, 1);
    t->spawn_id = spawned + 1;
    enqueue_task(t);
    if (spawned == 0 || (spawned & 63) == 0) wake_processor();
    
    return future;
}

// Re-enqueue a task from the I/O loop (called when fd becomes ready)
void wyn_sched_enqueue(void* task_ptr) {
    if (!task_ptr) return;
    Task* task = (Task*)task_ptr;
    global_queue_push(task);
    wake_processor();
}

void wyn_spawn_wait(void) {
    while (atomic_load(&total_completed) < atomic_load(&total_spawned)) {
        sched_yield();
    }
}
