// Spawn runtime implementation
#include "spawn.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

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
                    #elif defined(__aarch64__) || defined(__arm__)
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
        int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cpus < 1) num_cpus = 4;
        global_scheduler = wyn_scheduler_init(num_cpus);
        wyn_scheduler_start(global_scheduler);
    }
    
    wyn_scheduler_enqueue(global_scheduler, func, arg);
}

void wyn_yield() {
    sched_yield();
}

// Task coordinator implementation
WynTask* wyn_task_new(int capacity) {
    WynTask* task = malloc(sizeof(WynTask));
    task->capacity = capacity;
    task->size = 0;
    task->read_pos = 0;
    task->write_pos = 0;
    task->closed = 0;
    task->buffer = malloc(capacity * sizeof(void*));
    
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->not_empty, NULL);
    pthread_cond_init(&task->not_full, NULL);
    
    return task;
}

void wyn_task_send(WynTask* task, void* value) {
    pthread_mutex_lock(&task->mutex);
    
    // Wait if full
    while (task->size == task->capacity && !task->closed) {
        pthread_cond_wait(&task->not_full, &task->mutex);
    }
    
    if (task->closed) {
        pthread_mutex_unlock(&task->mutex);
        return;
    }
    
    // Add to buffer
    task->buffer[task->write_pos] = value;
    task->write_pos = (task->write_pos + 1) % task->capacity;
    task->size++;
    
    pthread_cond_signal(&task->not_empty);
    pthread_mutex_unlock(&task->mutex);
}

void* wyn_task_recv(WynTask* task) {
    pthread_mutex_lock(&task->mutex);
    
    // Wait if empty
    while (task->size == 0 && !task->closed) {
        pthread_cond_wait(&task->not_empty, &task->mutex);
    }
    
    if (task->size == 0 && task->closed) {
        pthread_mutex_unlock(&task->mutex);
        return NULL;
    }
    
    // Get from buffer
    void* value = task->buffer[task->read_pos];
    task->read_pos = (task->read_pos + 1) % task->capacity;
    task->size--;
    
    pthread_cond_signal(&task->not_full);
    pthread_mutex_unlock(&task->mutex);
    
    return value;
}

void wyn_task_close(WynTask* task) {
    pthread_mutex_lock(&task->mutex);
    task->closed = 1;
    pthread_cond_broadcast(&task->not_empty);
    pthread_cond_broadcast(&task->not_full);
    pthread_mutex_unlock(&task->mutex);
}

void wyn_task_free(WynTask* task) {
    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->not_empty);
    pthread_cond_destroy(&task->not_full);
    free(task->buffer);
    free(task);
}
