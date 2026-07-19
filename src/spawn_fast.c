// M:N Scheduler for Wyn — coroutine-based (v1.9)
// Architecture: N OS threads (processors), M coroutines (spawns)
// Per-processor local deque + global queue + work-stealing
// Each spawn creates a stackful coroutine via minicoro.h

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "future.h"
#include "coroutine.h"
#include "io_loop.h"

#ifdef _WIN32
// Windows: stub implementation — spawn runs synchronously
typedef void (*TaskFunc)(void*);
void wyn_io_park(void) {}
void* wyn_current_task(void) { return NULL; }
void* wyn_current_task_future(void) { return NULL; }  // no coroutines on Windows
int wyn_spawn_origin_line(void) { return 0; }
const char* wyn_spawn_origin_file(void) { return ""; }
long wyn_spawn_origin_id(void) { return 0; }
void wyn_spawn_fast(TaskFunc func, void* arg) { func(arg); }
void wyn_spawn_fast_traced(TaskFunc func, void* arg, const char* file, int line) { (void)file; (void)line; func(arg); }
void wyn_sched_enqueue(void* task_ptr) { (void)task_ptr; }
void wyn_spawn_wait(void) {}
Future* wyn_spawn_async(void* (*func)(void*), void* arg) {
    Future* future = future_new();
    void* result = func(arg);
    future_set(future, result);
    return future;
}
Future* wyn_spawn_async_traced(void* (*func)(void*), void* arg, const char* f, int l) {
    (void)f; (void)l;
    return wyn_spawn_async(func, arg);
}
_Atomic int ws_blocked = 0;
int pool_try_run_one(void) { return 0; }
int wyn_sched_pump_one(void) { return 0; }  // no scheduler on Windows (spawns run inline)
#else
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#define sched_yield() SwitchToThread()
static long sysconf(int name) { (void)name; SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors; }
#else
#include <unistd.h>
#include <sched.h>
#endif

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
    _Atomic int running;     // 1 = currently being executed by a processor
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

// Heap-allocated at init, sized to the real core count. The old static
// `processors[MAX_PROCESSORS]` embedded 64 × 4096-entry atomic deques =
// ~2.1MB of BSS zerofill in EVERY compiled binary (hello world included).
// All access paths loop to num_processors (0 before init), so a NULL
// pointer is never dereferenced before init_scheduler allocates it.
static Processor* processors = NULL;
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
            return;  // Wake one — it will steal and wake others if needed
        }
    }
}

// Wake all parked processors — used when multiple tasks are enqueued at once
static inline void wake_all_processors(void) {
    int n = atomic_load_explicit(&num_processors, memory_order_relaxed);
    for (int i = 0; i < n; i++) {
        if (atomic_load_explicit(&processors[i].parked, memory_order_acquire)) {
            pthread_mutex_lock(&processors[i].park_lock);
            pthread_cond_signal(&processors[i].park_cond);
            pthread_mutex_unlock(&processors[i].park_lock);
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

// S4: the Future backing the current coroutine task (NULL for fire-and-forget or
// no task). Lets future.c query cancellation without knowing the Task layout.
void* wyn_current_task_future(void) { return current_task ? current_task->future : NULL; }

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
    // Prevent double-resume: only one processor can run this task at a time
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&task->running, &expected, 1,
            memory_order_acq_rel, memory_order_relaxed)) {
        // Another processor is already running this task — re-enqueue for later
        enqueue_task(task);
        return;
    }
    // Save/restore the per-thread current_task + io_parked around the resume.
    // On a worker this is just NULL↔task, but the main-thread await pump (M1)
    // can call execute_task from INSIDE a coroutine that another execute_task is
    // already running — without save/restore the inner return would clear the
    // outer coroutine's identity (and its io_parked), corrupting its next
    // yield/park. Saving makes execute_task safely reentrant.
    Task* saved_task = current_task;
    int saved_io = io_parked;
    current_task = task;
    io_parked = 0;
    bool alive = wyn_coro_resume(coro);
    current_task = saved_task;
    int this_io = io_parked;
    io_parked = saved_io;
    atomic_store_explicit(&task->running, 0, memory_order_release);
    if (alive) {
        if (this_io) {
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
    
    int cpus;
    cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 4;
    if (cpus > MAX_PROCESSORS) cpus = MAX_PROCESSORS;
    
    processors = calloc((size_t)cpus, sizeof(Processor));
    if (!processors) { fprintf(stderr, "wyn: scheduler alloc failed\n"); exit(1); }

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
#define SA_POOL_SIZE (64 * 1024)
static SpawnArgs sa_pool[SA_POOL_SIZE];
static int sa_pool_head = 0;
static SpawnArgs* sa_free_arr[SA_POOL_SIZE];
static int sa_free_top = 0;
// The old lock-free index CAS was UNSOUND: alloc CAS'd sa_free_top then read
// sa_free_arr[top-1], but a concurrent dealloc could CAS the index back up and
// overwrite that same slot in between — handing the same SpawnArgs to two
// coroutines (or a torn pointer). That dropped fire-and-forget tasks (~5-9/20).
// A dedicated mutex makes the free-list obviously correct; SpawnArgs churn is
// not hot enough for the CAS micro-optimization to matter.
static pthread_mutex_t sa_lock = PTHREAD_MUTEX_INITIALIZER;

static SpawnArgs* sa_alloc(void) {
    pthread_mutex_lock(&sa_lock);
    if (sa_free_top > 0) {
        SpawnArgs* sa = sa_free_arr[--sa_free_top];
        pthread_mutex_unlock(&sa_lock);
        return sa;
    }
    if (sa_pool_head < SA_POOL_SIZE) {
        SpawnArgs* sa = &sa_pool[sa_pool_head++];
        pthread_mutex_unlock(&sa_lock);
        return sa;
    }
    pthread_mutex_unlock(&sa_lock);
    return malloc(sizeof(SpawnArgs));
}

static void sa_dealloc(SpawnArgs* sa) {
    if (sa >= sa_pool && sa < sa_pool + SA_POOL_SIZE) {
        pthread_mutex_lock(&sa_lock);
        if (sa_free_top < SA_POOL_SIZE) {
            sa_free_arr[sa_free_top++] = sa;
            pthread_mutex_unlock(&sa_lock);
            return;
        }
        pthread_mutex_unlock(&sa_lock);
        return;  // pool full — drop (came from the fixed arena, can't free())
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

// W8 S1: awaited spawn as a COROUTINE on the M:N scheduler (instead of a pool
// thread), so cooperative I/O / cooperative Time::sleep engage inside awaited
// tasks too. Mirrors wyn_spawn_fast_traced but attaches a Future the body
// resolves. Behind WYN_ASYNC_CORO (see wyn_spawn_async_traced). Returns the
// Future; falls back to running inline on any allocation failure.
static Future* wyn_spawn_async_coro(TaskFuncWithReturn func, void* arg,
                                    const char* file, int line) {
    init_scheduler();
    Future* future = future_new();
    atomic_fetch_add(&total_spawned, 1);

    SpawnAsyncArgs* sa = (SpawnAsyncArgs*)malloc(sizeof(SpawnAsyncArgs));
    if (!sa) { future_set(future, func(arg)); atomic_fetch_add(&total_completed, 1); return future; }
    sa->func = func; sa->arg = arg; sa->future = future;

    WynCoroutine* coro = wyn_coro_create(spawn_async_coro_body, sa);
    if (!coro) { free(sa); future_set(future, func(arg)); atomic_fetch_add(&total_completed, 1); return future; }

    Task* t = alloc_task();
    if (!t) { wyn_coro_destroy(coro); free(sa); future_set(future, func(arg)); atomic_fetch_add(&total_completed, 1); return future; }
    t->coro = coro;
    t->future = future;
    t->spawn_file = file;
    t->spawn_line = line;
    atomic_store_explicit(&t->running, 0, memory_order_relaxed);
    t->spawn_id = 0;
    enqueue_task(t);
    wake_processor();
    return future;
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
    atomic_store_explicit(&t->running, 0, memory_order_relaxed);
    
    long spawned = atomic_fetch_add(&total_spawned, 1);
    t->spawn_id = spawned + 1;
    enqueue_task(t);
    wake_processor();  // Always wake — ensures tasks run in parallel
}

Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg) {
    return wyn_spawn_async_traced(func, arg, NULL, 0);
}

// === Thread Pool for spawn_async ===
// Fixed pool of worker threads that pick up tasks from a shared queue.
// Eliminates pthread_create overhead per spawn.

typedef struct { TaskFuncWithReturn func; void* arg; Future* future; } PoolTask;

#define POOL_QUEUE_SIZE 4096
static PoolTask pool_queue[POOL_QUEUE_SIZE];
static _Atomic int pool_head = 0;
static _Atomic int pool_tail = 0;
static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_cond = PTHREAD_COND_INITIALIZER;
static _Atomic int pool_started = 0;
static _Atomic int pool_shutdown = 0;
static int pool_thread_count = 0;
_Atomic int ws_blocked = 0; // Threads blocked in future_get
#define POOL_MAX_THREADS 32
static pthread_t pool_threads[POOL_MAX_THREADS];

// Dequeue and run one task. Called from future_get to prevent deadlock.
int pool_try_run_one(void) {
    pthread_mutex_lock(&pool_lock);
    if (atomic_load(&pool_head) == atomic_load(&pool_tail)) {
        pthread_mutex_unlock(&pool_lock);
        return 0;
    }
    int idx = atomic_load(&pool_head) % POOL_QUEUE_SIZE;
    PoolTask task = pool_queue[idx];
    atomic_fetch_add(&pool_head, 1);
    pthread_mutex_unlock(&pool_lock);
    void* result = task.func(task.arg);
    future_set(task.future, result);
    atomic_fetch_add(&total_completed, 1);
    return 1;
}

static void* pool_worker(void* arg) {
    (void)arg;
    while (!atomic_load(&pool_shutdown)) {
        pthread_mutex_lock(&pool_lock);
        while (atomic_load(&pool_head) == atomic_load(&pool_tail) && !atomic_load(&pool_shutdown)) {
            pthread_cond_wait(&pool_cond, &pool_lock);
        }
        if (atomic_load(&pool_shutdown)) { pthread_mutex_unlock(&pool_lock); break; }
        int idx = atomic_load(&pool_head) % POOL_QUEUE_SIZE;
        PoolTask task = pool_queue[idx];
        atomic_fetch_add(&pool_head, 1);
        pthread_mutex_unlock(&pool_lock);
        
        void* result = task.func(task.arg);
        future_set(task.future, result);
        atomic_fetch_add(&total_completed, 1);
    }
    return NULL;
}

static void shutdown_pool(void) {
    atomic_store(&pool_shutdown, 1);
    pthread_mutex_lock(&pool_lock);
    pthread_cond_broadcast(&pool_cond);
    pthread_mutex_unlock(&pool_lock);
    for (int i = 0; i < pool_thread_count; i++) {
        pthread_join(pool_threads[i], NULL);
    }
}

static void init_pool(void) {
    if (atomic_exchange(&pool_started, 1)) return;
    int cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 2) cpus = 2;
    if (cpus > POOL_MAX_THREADS) cpus = POOL_MAX_THREADS;
    pool_thread_count = cpus;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 512 * 1024);
    for (int i = 0; i < pool_thread_count; i++) {
        pthread_create(&pool_threads[i], &attr, pool_worker, NULL);
    }
    pthread_attr_destroy(&attr);
    atexit(shutdown_pool);
}

Future* wyn_spawn_async_traced(TaskFuncWithReturn func, void* arg, const char* file, int line) {
    // W8 S1/S2: with WYN_ASYNC_CORO set, run awaited spawns as coroutines on the
    // M:N scheduler (cooperative I/O everywhere) instead of the thread pool. Read
    // the env once. main-thread await pumps the scheduler (see future_get), so
    // this is safe on any core count; but if for some reason no worker processors
    // exist AND we're on the main thread, keep the pool (its worker threads are
    // the only executors then).
    // S3: the coroutine scheduler is now the DEFAULT executor for awaited work
    // (cooperative I/O everywhere). WYN_ASYNC_POOL=1 forces the legacy thread pool
    // as an escape hatch. WYN_ASYNC_CORO is still honored (=0 also selects the
    // pool) for backward compatibility with existing scripts/CI.
    static int async_coro = -1;
    if (async_coro < 0) {
        const char* pool = getenv("WYN_ASYNC_POOL");
        const char* coro = getenv("WYN_ASYNC_CORO");
        if (pool && pool[0] == '1') async_coro = 0;
        else if (coro && coro[0] == '0') async_coro = 0;
        else async_coro = 1;   // default: coroutine scheduler
    }
    if (async_coro) return wyn_spawn_async_coro(func, arg, file, line);

    (void)file; (void)line;
    init_pool();

    Future* future = future_new();
    atomic_fetch_add(&total_spawned, 1);
    
    pthread_mutex_lock(&pool_lock);
    int tail = atomic_load(&pool_tail);
    // If queue is full, run inline
    if (tail - atomic_load(&pool_head) >= POOL_QUEUE_SIZE) {
        pthread_mutex_unlock(&pool_lock);
        void* result = func(arg);
        future_set(future, result);
        atomic_fetch_add(&total_completed, 1);
        return future;
    }
    pool_queue[tail % POOL_QUEUE_SIZE] = (PoolTask){ func, arg, future };
    atomic_fetch_add(&pool_tail, 1);
    pthread_cond_signal(&pool_cond);
    pthread_mutex_unlock(&pool_lock);
    
    return future;
}

// W8 S2 (M1 pump): run ONE scheduler task on the calling thread + poll I/O.
// Called by future_get's main-thread branch when awaiting a coroutine-backed
// future, so `main` (which can't yield) still drives the M:N scheduler and
// resolves awaited coro tasks even with no/idle worker processors — this is what
// makes WYN_ASYNC_CORO correct on every core count. execute_task is reentrant
// (saves/restores current_task + io_parked), so pumping from inside main is safe.
// Returns 1 if a task ran, 0 if the queue was empty (caller should yield/poll).
int wyn_sched_pump_one(void) {
    wyn_io_poll();  // wake any I/O/timer-ready coroutines first
    Task* task = global_queue_pop();
    if (!task) task = try_steal(0);
    if (task) { execute_task(task); return 1; }
    return 0;
}

// Re-enqueue a task from the I/O loop (called when fd becomes ready)
void wyn_sched_enqueue(void* task_ptr) {
    if (!task_ptr) return;
    Task* task = (Task*)task_ptr;
    global_queue_push(task);
    wake_processor();
}

void wyn_spawn_wait(void) {
    // Drain outstanding fire-and-forget tasks. Pump the I/O loop ourselves each
    // iteration: a task may be I/O-parked (e.g. cooperative Time::sleep / socket
    // wait), and if every worker processor is also parked, nothing else would
    // advance the reactor — the old pure sched_yield() spin could hang there.
    while (atomic_load(&total_completed) < atomic_load(&total_spawned)) {
        wyn_io_poll();     // wake any tasks whose timers/fds are ready
        wake_all_processors();
        sched_yield();
    }
    // Belt-and-suspenders: flush stdout/stderr here, before main returns. Most
    // output already flushed (println_str self-flushes), but the numeric/bool/
    // array println variants leave data in the buffer; a fire-and-forget task
    // that produced them last could otherwise have its tail dropped if libc's
    // atexit flush raced the drain. Flushing on the joining (main) thread after
    // parity is observed makes the last task's output deterministic.
    fflush(stdout);
    fflush(stderr);
}

// Inline spawn: run function directly without creating a coroutine.
// Used for spawned functions that have no yield points (no await/channel ops).
// Much cheaper: no mmap, no coroutine struct, no scheduler overhead.
Future* wyn_spawn_inline(TaskFuncWithReturn func, void* arg) {
    return wyn_spawn_async(func, arg);
}

#endif // !_WIN32
