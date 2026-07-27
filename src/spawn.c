// Spawn runtime implementation
#include "spawn.h"
#include "coroutine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
// Windows: stub implementations - spawn runs synchronously
WynScheduler* global_scheduler = NULL;
WynScheduler* wyn_scheduler_init(int n) { (void)n; return NULL; }
void wyn_scheduler_start(WynScheduler* s) { (void)s; }
void wyn_scheduler_shutdown(WynScheduler* s) { (void)s; }
void wyn_scheduler_enqueue(WynScheduler* s, WynSpawnFunc f, void* a) { (void)s; f(a); }
void wyn_spawn(WynSpawnFunc f, void* a) { f(a); }
void wyn_yield(void) {}
WynTask* wyn_task_new(int cap) {
    // Guard against a 0/negative capacity: calloc(0,..) is implementation-
    // defined and a 0-capacity ring buffer can never deliver. Task.channel
    // already rejects cap < 1; clamp here too so no path allocates a 0-slot ring.
    if (cap < 1) cap = 1;
    WynTask* t = calloc(1, sizeof(WynTask));
    t->capacity = cap; t->buffer = calloc(cap, sizeof(void*));
    return t;
}
void wyn_task_send(WynTask* t, void* v) { if (t->size < t->capacity) t->buffer[t->write_pos++ % t->capacity] = v, t->size++; }
void* wyn_task_recv(WynTask* t) { if (t->size > 0) { t->size--; return t->buffer[t->read_pos++ % t->capacity]; } return NULL; }
void wyn_task_close(WynTask* t) { t->closed = 1; }
void wyn_task_free(WynTask* t) { free(t->buffer); free(t); }
#else

#define _DEFAULT_SOURCE
#include <unistd.h>
#include <sched.h>
#ifdef __APPLE__
#ifndef __TINYC__
#include <sys/sysctl.h>
#endif
#endif

WynScheduler* global_scheduler = NULL;

// Spawn pool for memory reuse (per-worker to reduce contention)
#define SPAWN_POOL_SIZE 256
typedef struct {
    WynSpawn* pool[SPAWN_POOL_SIZE];
    _Atomic int count;
} SpawnPool;

static SpawnPool worker_pools[64];  // Max 64 workers

// Allocate spawn (from thread-local pool)
static inline WynSpawn* alloc_spawn(int worker_id) {
    if (worker_id < 0 || worker_id >= 64) {
        return malloc(sizeof(WynSpawn));
    }
    
    SpawnPool* pool = &worker_pools[worker_id];
    int count = atomic_load(&pool->count);
    
    if (count > 0) {
        int idx = atomic_fetch_sub(&pool->count, 1) - 1;
        if (idx >= 0 && idx < SPAWN_POOL_SIZE) {
            return pool->pool[idx];
        }
    }
    return malloc(sizeof(WynSpawn));
}

// Free spawn (return to thread-local pool)
static inline void free_spawn(WynSpawn* spawn, int worker_id) {
    if (worker_id < 0 || worker_id >= 64) {
        free(spawn);
        return;
    }
    
    SpawnPool* pool = &worker_pools[worker_id];
    int count = atomic_load(&pool->count);
    
    if (count < SPAWN_POOL_SIZE) {
        int idx = atomic_fetch_add(&pool->count, 1);
        if (idx < SPAWN_POOL_SIZE) {
            pool->pool[idx] = spawn;
            return;
        }
    }
    free(spawn);
}

// Initialize scheduler with N worker threads
WynScheduler* wyn_scheduler_init(int num_workers) {
    WynScheduler* sched = malloc(sizeof(WynScheduler));
    sched->num_workers = num_workers;
    sched->running = 1;
    
    // Allocate per-worker queues and locks
    sched->queues = calloc(num_workers, sizeof(WynSpawn*));
    sched->locks = malloc(num_workers * sizeof(pthread_mutex_t));
    sched->workers = malloc(num_workers * sizeof(pthread_t));
    
    pthread_mutex_init(&sched->global_lock, NULL);
    
    for (int i = 0; i < num_workers; i++) {
        pthread_mutex_init(&sched->locks[i], NULL);
    }
    
    return sched;
}

// Worker thread function
static void* worker_thread(void* arg) {
    WynScheduler* sched = (WynScheduler*)arg;
    int worker_id = 0;
    
    // Find our worker ID
    for (int i = 0; i < sched->num_workers; i++) {
        if (pthread_equal(sched->workers[i], pthread_self())) {
            worker_id = i;
            break;
        }
    }
    
    int idle_spins = 0;
    
    while (sched->running) {
        WynSpawn* spawn = NULL;
        
        // Try to get spawn from our queue (fast path)
        pthread_mutex_lock(&sched->locks[worker_id]);
        if (sched->queues[worker_id]) {
            spawn = sched->queues[worker_id];
            sched->queues[worker_id] = spawn->next;
        }
        pthread_mutex_unlock(&sched->locks[worker_id]);
        
        // Work stealing: try other queues
        if (!spawn) {
            for (int i = 1; i < sched->num_workers; i++) {
                int target = (worker_id + i) % sched->num_workers;
                
                pthread_mutex_lock(&sched->locks[target]);
                if (sched->queues[target]) {
                    spawn = sched->queues[target];
                    sched->queues[target] = spawn->next;
                    pthread_mutex_unlock(&sched->locks[target]);
                    break;
                }
                pthread_mutex_unlock(&sched->locks[target]);
            }
        }
        
        // Execute spawn
        if (spawn) {
            spawn->func(spawn->arg);
            free_spawn(spawn, worker_id);  // Return to worker's pool
            idle_spins = 0;
        } else {
            // Adaptive backoff: spin → yield → sleep
            if (idle_spins < 100) {
                // Spin (no syscall) - architecture-specific
                for (int i = 0; i < 10; i++) {
                    #if defined(__x86_64__) || defined(__i386__)
                    __asm__ __volatile__("pause" ::: "memory");
                    #elif (defined(__aarch64__) || defined(__arm__)) && !defined(__TINYC__)
                    __asm__ __volatile__("yield" ::: "memory");
                    #else
                    // Fallback for other architectures
                    volatile int dummy = 0; (void)dummy;
                    #endif
                }
                idle_spins++;
            } else if (idle_spins < 200) {
                // Yield to other threads
                sched_yield();
                idle_spins++;
            } else {
                // Sleep (longer idle)
                usleep(100);  // 0.1ms
                idle_spins = 0;
            }
        }
    }
    
    return NULL;
}

// Start worker threads
void wyn_scheduler_start(WynScheduler* sched) {
    for (int i = 0; i < sched->num_workers; i++) {
        pthread_create(&sched->workers[i], NULL, worker_thread, sched);
    }
}

// Enqueue a spawn (optimized)
void wyn_scheduler_enqueue(WynScheduler* sched, WynSpawnFunc func, void* arg) {
    // Round-robin assignment to workers
    static _Atomic int next_worker = 0;
    int worker_id = atomic_fetch_add(&next_worker, 1) % sched->num_workers;
    
    WynSpawn* spawn = alloc_spawn(worker_id);  // Use worker's pool
    spawn->func = func;
    spawn->arg = arg;
    spawn->next = NULL;
    
    pthread_mutex_lock(&sched->locks[worker_id]);
    spawn->next = sched->queues[worker_id];
    sched->queues[worker_id] = spawn;
    pthread_mutex_unlock(&sched->locks[worker_id]);
}

// Shutdown scheduler
void wyn_scheduler_shutdown(WynScheduler* sched) {
    sched->running = 0;
    
    // Wait for workers
    for (int i = 0; i < sched->num_workers; i++) {
        pthread_join(sched->workers[i], NULL);
    }
    
    // Cleanup
    for (int i = 0; i < sched->num_workers; i++) {
        pthread_mutex_destroy(&sched->locks[i]);
    }
    pthread_mutex_destroy(&sched->global_lock);
    
    free(sched->queues);
    free(sched->locks);
    free(sched->workers);
    free(sched);
}

// User-facing API
void wyn_spawn(WynSpawnFunc func, void* arg) {
    if (!global_scheduler) {
        // Lazy init with CPU count workers
        int num_cpus = 4;  // Default
        #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_cpus = sysinfo.dwNumberOfProcessors;
        #elif defined(__APPLE__)
        size_t len = sizeof(num_cpus);
        sysctlbyname("hw.ncpu", &num_cpus, &len, NULL, 0);
        #else
        num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        #endif
        if (num_cpus < 1) num_cpus = 4;
        global_scheduler = wyn_scheduler_init(num_cpus);
        wyn_scheduler_start(global_scheduler);
    }
    
    wyn_scheduler_enqueue(global_scheduler, func, arg);
}

void wyn_yield() {
    sched_yield();
}

// Scheduler hooks used to park/wake blocked coroutine channel operations
// instead of busy-spinning on wyn_coro_yield() (the spawn_10k livelock: excess
// senders re-enqueued themselves every yield, pegging all workers ~forever).
extern void* wyn_current_task(void);      // scheduler Task* for the running coro
extern void  wyn_io_park(void);           // don't re-enqueue the running coro on yield
extern void  wyn_sched_enqueue(void* t);  // re-enqueue a parked Task* to the scheduler

// FIFO push/pop of a parked-coroutine waiter. Caller holds task->mutex.
static void waiter_push(WynWaiter** head, WynWaiter** tail, void* sched_task) {
    WynWaiter* w = malloc(sizeof(WynWaiter));
    if (!w) return;  // OOM: fall back to spin (the coro will re-check on next yield)
    w->task = sched_task;
    w->next = NULL;
    if (*tail) (*tail)->next = w; else *head = w;
    *tail = w;
}
// Pop one waiter's Task* (or NULL). Caller holds task->mutex.
static void* waiter_pop(WynWaiter** head, WynWaiter** tail) {
    WynWaiter* w = *head;
    if (!w) return NULL;
    *head = w->next;
    if (!*head) *tail = NULL;
    void* t = w->task;
    free(w);
    return t;
}

// Task coordinator implementation
WynTask* wyn_task_new(int capacity) {
    // Guard against a 0/negative capacity: malloc(0) is implementation-defined
    // and a 0-capacity ring buffer can never deliver (size < capacity is never
    // true → permanent send/recv hang). Task.channel already rejects cap < 1;
    // clamp here too so no internal path can allocate a 0-slot ring.
    if (capacity < 1) capacity = 1;
    WynTask* task = malloc(sizeof(WynTask));
    task->capacity = capacity;
    task->size = 0;
    task->read_pos = 0;
    task->write_pos = 0;
    task->closed = 0;
    task->buffer = malloc(capacity * sizeof(void*));
    task->send_head = task->send_tail = NULL;
    task->recv_head = task->recv_tail = NULL;

    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->not_empty, NULL);
    pthread_cond_init(&task->not_full, NULL);

    return task;
}

void wyn_task_send(WynTask* task, void* value) {
    // Inside a coroutine: try-send, and if the channel is FULL, PARK on the
    // send-waiter list (don't re-enqueue) until a receiver frees a slot. The
    // old code yield()ed, which re-enqueued the coroutine immediately - with
    // more senders than capacity every worker busy-spun on the full channel
    // (~900% CPU, spawn_10k never finished). Parking makes a blocked sender
    // consume zero CPU until woken by the exact receiver that made room.
    if (wyn_coro_current()) {
        for (;;) {
            pthread_mutex_lock(&task->mutex);
            if (task->closed) { pthread_mutex_unlock(&task->mutex); return; }
            if (task->size < task->capacity) {
                task->buffer[task->write_pos] = value;
                task->write_pos = (task->write_pos + 1) % task->capacity;
                task->size++;
                // Wake one receiver parked on an empty channel (if any).
                void* woken = waiter_pop(&task->recv_head, &task->recv_tail);
                pthread_cond_signal(&task->not_empty);
                pthread_mutex_unlock(&task->mutex);
                if (woken) wyn_sched_enqueue(woken);
                return;
            }
            // Full: park this coroutine on the send-waiter list, then yield.
            void* self = wyn_current_task();
            if (self) {
                waiter_push(&task->send_head, &task->send_tail, self);
                wyn_io_park();  // scheduler won't re-enqueue us on yield
                pthread_mutex_unlock(&task->mutex);
                wyn_coro_yield();  // suspend until a receiver wakes us
            } else {
                // No task identity (shouldn't happen in a coroutine): fall back
                // to the old cooperative yield so we never hang here.
                pthread_mutex_unlock(&task->mutex);
                wyn_coro_yield();
            }
        }
    }
    // Main thread / OS thread: poll + pump instead of blocking forever, so a
    // send on a full channel that no live task can ever drain is reported as a
    // deadlock rather than hanging silently (mirrors Task_select_n's backstop).
    extern long wyn_sched_inflight(void);
    extern int  wyn_sched_pump_one(void);
    for (;;) {
        pthread_mutex_lock(&task->mutex);
        if (task->closed) { pthread_mutex_unlock(&task->mutex); return; }
        if (task->size < task->capacity) {
            task->buffer[task->write_pos] = value;
            task->write_pos = (task->write_pos + 1) % task->capacity;
            task->size++;
            void* woken = waiter_pop(&task->recv_head, &task->recv_tail);
            pthread_cond_signal(&task->not_empty);
            pthread_mutex_unlock(&task->mutex);
            if (woken) wyn_sched_enqueue(woken);
            return;
        }
        pthread_mutex_unlock(&task->mutex);
        int did = wyn_sched_pump_one();
        if (!did && wyn_sched_inflight() == 0) {
            // Double-check: one more pump + rescan before declaring death, to
            // dodge a last-instant receiver enqueue.
            wyn_sched_pump_one();
            pthread_mutex_lock(&task->mutex);
            int ready = task->size < task->capacity || task->closed;
            pthread_mutex_unlock(&task->mutex);
            if (!ready && wyn_sched_inflight() == 0) {
                fprintf(stderr, "wyn: deadlock - send() on a full channel with no receiver and no live tasks (nothing can ever receive)\n");
                exit(1);
            }
        }
        if (!did) sched_yield();
    }
}

void* wyn_task_recv(WynTask* task) {
    // Inside a coroutine: try-recv, and if EMPTY, PARK on the recv-waiter list
    // (don't re-enqueue) until a sender delivers. Symmetric to wyn_task_send's
    // parking - no busy-spin while blocked.
    if (wyn_coro_current()) {
        for (;;) {
            pthread_mutex_lock(&task->mutex);
            if (task->size > 0) {
                void* value = task->buffer[task->read_pos];
                task->read_pos = (task->read_pos + 1) % task->capacity;
                task->size--;
                // Wake one sender parked on a full channel (if any).
                void* woken = waiter_pop(&task->send_head, &task->send_tail);
                pthread_cond_signal(&task->not_full);
                pthread_mutex_unlock(&task->mutex);
                if (woken) wyn_sched_enqueue(woken);
                return value;
            }
            if (task->closed) { pthread_mutex_unlock(&task->mutex); return NULL; }
            // Empty: park this coroutine on the recv-waiter list, then yield.
            void* self = wyn_current_task();
            if (self) {
                waiter_push(&task->recv_head, &task->recv_tail, self);
                wyn_io_park();
                pthread_mutex_unlock(&task->mutex);
                wyn_coro_yield();
            } else {
                pthread_mutex_unlock(&task->mutex);
                wyn_coro_yield();
            }
        }
    }
    // Main thread / OS thread: poll + pump instead of blocking forever, so a
    // recv on a channel that no live task can ever feed is reported as a
    // deadlock rather than hanging silently (mirrors Task_select_n's backstop).
    extern long wyn_sched_inflight(void);
    extern int  wyn_sched_pump_one(void);
    for (;;) {
        pthread_mutex_lock(&task->mutex);
        if (task->size > 0) {
            void* value = task->buffer[task->read_pos];
            task->read_pos = (task->read_pos + 1) % task->capacity;
            task->size--;
            // A main-thread receive also frees a slot: wake a parked sender so
            // it can make progress (this is the wake that drives spawn_10k).
            void* woken = waiter_pop(&task->send_head, &task->send_tail);
            pthread_cond_signal(&task->not_full);
            pthread_mutex_unlock(&task->mutex);
            if (woken) wyn_sched_enqueue(woken);
            return value;
        }
        if (task->closed) { pthread_mutex_unlock(&task->mutex); return NULL; }
        pthread_mutex_unlock(&task->mutex);
        int did = wyn_sched_pump_one();
        if (!did && wyn_sched_inflight() == 0) {
            // Double-check: one more pump + rescan before declaring death, to
            // dodge a last-instant sender enqueue.
            wyn_sched_pump_one();
            pthread_mutex_lock(&task->mutex);
            int ready = task->size > 0 || task->closed;
            pthread_mutex_unlock(&task->mutex);
            if (!ready && wyn_sched_inflight() == 0) {
                fprintf(stderr, "wyn: deadlock - recv() on a channel with no sender and no live tasks (nothing can ever send)\n");
                exit(1);
            }
        }
        if (!did) sched_yield();
    }
}

void wyn_task_close(WynTask* task) {
    pthread_mutex_lock(&task->mutex);
    task->closed = 1;
    // Wake every parked sender/receiver so they observe `closed` and return
    // (a blocked op on a now-closed channel must not stay parked forever).
    void* woken;
    // Collect under the lock, enqueue after unlocking.
    WynWaiter* to_wake_head = NULL; WynWaiter* to_wake_tail = NULL;
    while ((woken = waiter_pop(&task->send_head, &task->send_tail)))
        waiter_push(&to_wake_head, &to_wake_tail, woken);
    while ((woken = waiter_pop(&task->recv_head, &task->recv_tail)))
        waiter_push(&to_wake_head, &to_wake_tail, woken);
    pthread_cond_broadcast(&task->not_empty);
    pthread_cond_broadcast(&task->not_full);
    pthread_mutex_unlock(&task->mutex);
    while ((woken = waiter_pop(&to_wake_head, &to_wake_tail)))
        wyn_sched_enqueue(woken);
}

void wyn_task_free(WynTask* task) {
    // Drain any leftover waiter nodes (normally none by free time).
    while (waiter_pop(&task->send_head, &task->send_tail)) {}
    while (waiter_pop(&task->recv_head, &task->recv_tail)) {}
    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->not_empty);
    pthread_cond_destroy(&task->not_full);
    free(task->buffer);
    free(task);
}

#endif // !_WIN32
