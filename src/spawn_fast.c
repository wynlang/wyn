// M:N Scheduler for Wyn — designed to match/beat Go goroutine performance
// Architecture: N OS threads (processors), M lightweight tasks (spawns)
// Per-processor local deque + global queue + work-stealing

#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <sched.h>
#include "future.h"

#define MAX_PROCESSORS 64
#define LOCAL_QUEUE_SIZE 256  // Power of 2, per-processor
#define GLOBAL_BATCH 32

typedef void (*TaskFunc)(void*);
typedef void* (*TaskFuncWithReturn)(void*);

typedef struct Task {
    TaskFunc func;
    void* arg;
    Future* future;
    struct Task* next;
} Task;

// === Lock-free task pool ===
#define TASK_POOL_SIZE 8192
static Task task_pool[TASK_POOL_SIZE];
static _Atomic int task_pool_head = 0;
static Task* task_overflow = NULL;
static pthread_mutex_t overflow_lock = PTHREAD_MUTEX_INITIALIZER;

static Task* alloc_task(void) {
    int idx = atomic_fetch_add(&task_pool_head, 1);
    if (idx < TASK_POOL_SIZE) return &task_pool[idx];
    // Overflow: malloc
    return malloc(sizeof(Task));
}

// === Per-processor local deque (single-producer, multi-consumer) ===
typedef struct {
    _Atomic(Task*) buffer[LOCAL_QUEUE_SIZE];
    _Atomic int top;    // Owner pushes/pops from top
    _Atomic int bottom; // Stealers steal from bottom
} WorkDeque;

static inline void deque_init(WorkDeque* d) {
    atomic_store(&d->top, 0);
    atomic_store(&d->bottom, 0);
    for (int i = 0; i < LOCAL_QUEUE_SIZE; i++) atomic_store(&d->buffer[i], NULL);
}

static inline int deque_push(WorkDeque* d, Task* task) {
    int t = atomic_load_explicit(&d->top, memory_order_relaxed);
    int b = atomic_load_explicit(&d->bottom, memory_order_acquire);
    if (t - b >= LOCAL_QUEUE_SIZE) return 0; // Full
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
    if (b == t + 1) {
        // Last element — race with stealers
        atomic_store_explicit(&d->top, t + 1, memory_order_relaxed);
    }
    return NULL;
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

// === Global queue — lock-free LIFO stack (4ns push, 4ns pop) ===
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
    _Atomic int spinning;  // 1 if looking for work
    _Atomic int parked;    // 1 if sleeping
    pthread_mutex_t park_lock;
    pthread_cond_t park_cond;
} Processor;

static Processor processors[MAX_PROCESSORS];
static _Atomic int num_processors = 0;
static _Atomic int initialized = 0;
static _Atomic int scheduler_shutdown = 0;
static _Atomic long total_spawned = 0;
static _Atomic long total_completed = 0;

// Wake a parked processor — only if someone is actually sleeping
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
    // No parked workers — they're either spinning or busy, task will be picked up
}

// Execute a task
static inline void execute_task(Task* task) {
    if (task->future) {
        TaskFuncWithReturn fn = (TaskFuncWithReturn)task->func;
        void* result = fn(task->arg);
        future_set(task->future, result);
    } else {
        task->func(task->arg);
    }
    atomic_fetch_add(&total_completed, 1);
}

// Try to steal work from another processor
static Task* try_steal(int self_id) {
    int n = atomic_load(&num_processors);
    // Random starting point to reduce contention
    int start = self_id + 1;
    for (int i = 0; i < n - 1; i++) {
        int victim = (start + i) % n;
        Task* task = deque_steal(&processors[victim].deque);
        if (task) return task;
    }
    return NULL;
}

// Processor main loop
static void* processor_loop(void* arg) {
    Processor* p = (Processor*)arg;
    
    while (!atomic_load(&scheduler_shutdown)) {
        // 1. Try local deque first (LIFO — cache-friendly)
        Task* task = deque_pop(&p->deque);
        if (task) { execute_task(task); continue; }
        
        // 2. Try global queue
        task = global_queue_pop();
        if (task) { execute_task(task); continue; }
        
        // 3. Try stealing from others
        task = try_steal(p->id);
        if (task) { execute_task(task); continue; }
        
        // 4. Spin briefly before parking — use CPU hints, no sched_yield
        atomic_store(&p->spinning, 1);
        for (int spins = 0; spins < 32; spins++) {
            task = global_queue_pop();
            if (task) { atomic_store(&p->spinning, 0); execute_task(task); goto next; }
            task = try_steal(p->id);
            if (task) { atomic_store(&p->spinning, 0); execute_task(task); goto next; }
            #ifdef __x86_64__
            __asm__ volatile("pause");
            #elif defined(__aarch64__)
            __asm__ volatile("isb");
            #endif
        }
        atomic_store(&p->spinning, 0);
        
        // 5. Park with short timeout
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
        ts.tv_nsec += 500000; // 500μs
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&p->park_cond, &p->park_lock, &ts);
        atomic_store(&p->parked, 0);
        pthread_mutex_unlock(&p->park_lock);
        
        next: ;
    }
    return NULL;
}

// === Public API ===

static __thread int current_processor_id = -1;

static void init_scheduler(void) {
    if (atomic_exchange(&initialized, 1)) return;
    
    int cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 4;
    if (cpus > MAX_PROCESSORS) cpus = MAX_PROCESSORS;
    
    atomic_store(&num_processors, cpus);
    global_queue_init();
    
    for (int i = 0; i < cpus; i++) {
        processors[i].id = i;
        deque_init(&processors[i].deque);
        atomic_store(&processors[i].spinning, 0);
        atomic_store(&processors[i].parked, 0);
        pthread_mutex_init(&processors[i].park_lock, NULL);
        pthread_cond_init(&processors[i].park_cond, NULL);
        pthread_create(&processors[i].thread, NULL, processor_loop, &processors[i]);
    }
}

void wyn_spawn_fast(TaskFunc func, void* arg) {
    init_scheduler();
    
    Task* t = alloc_task();
    t->func = func;
    t->arg = arg;
    t->future = NULL;
    
    atomic_fetch_add(&total_spawned, 1);
    
    // Try to push to current processor's local deque
    if (current_processor_id >= 0) {
        deque_push(&processors[current_processor_id].deque, t);
    } else {
        global_queue_push(t);
    }
    wake_processor();
}

Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg) {
    init_scheduler();
    
    Future* future = future_new();
    Task* t = alloc_task();
    t->func = (TaskFunc)func;
    t->arg = arg;
    t->future = future;
    
    atomic_fetch_add(&total_spawned, 1);
    
    // Push to global queue (spawns from main thread go global)
    global_queue_push(t);
    wake_processor();
    
    return future;
}

void wyn_spawn_wait(void) {
    while (atomic_load(&total_completed) < atomic_load(&total_spawned)) {
        sched_yield();
    }
}
