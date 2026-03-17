// Spawn runtime for Wyn
// Lightweight spawns (not OS threads) with work-stealing scheduler
#ifndef WYN_SPAWN_H
#define WYN_SPAWN_H

#include <stdint.h>
#include <pthread.h>

// Forward declarations
typedef struct WynSpawn WynSpawn;
typedef struct WynTask WynTask;
typedef struct WynScheduler WynScheduler;

// Spawn function type
typedef void (*WynSpawnFunc)(void*);

struct WynSpawn {
    WynSpawnFunc func;
    void* arg;
    WynSpawn* next;
    int worker_id;
};

// Task coordinator (for communication between spawns)
struct WynTask {
    void** buffer;
    int capacity;
    int size;
    int read_pos;
    int write_pos;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int closed;
};

// Work-stealing scheduler
struct WynScheduler {
    WynSpawn** queues;       // Per-worker queues
    pthread_mutex_t* locks;  // Per-worker locks
    pthread_t* workers;      // Worker threads
    int num_workers;
    int running;
    pthread_mutex_t global_lock;
};

// Global scheduler
extern WynScheduler* global_scheduler;

// Scheduler functions
WynScheduler* wyn_scheduler_init(int num_workers);
void wyn_scheduler_start(WynScheduler* sched);
void wyn_scheduler_shutdown(WynScheduler* sched);
void wyn_scheduler_enqueue(WynScheduler* sched, WynSpawnFunc func, void* arg);

// Spawn functions (user-facing)
void wyn_spawn(WynSpawnFunc func, void* arg);
// wyn_yield defined in wyn_iter.h (static inline)

// Task coordinator functions (communication)
WynTask* wyn_task_new(int capacity);
void wyn_task_send(WynTask* task, void* value);
void* wyn_task_recv(WynTask* task);
void wyn_task_close(WynTask* task);
void wyn_task_free(WynTask* task);

#endif // WYN_SPAWN_H
