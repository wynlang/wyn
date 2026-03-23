// Spawn runtime for Wyn
#ifndef WYN_SPAWN_H
#define WYN_SPAWN_H

#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct WynSpawn WynSpawn;
typedef struct WynTask WynTask;
typedef struct WynScheduler WynScheduler;
typedef void (*WynSpawnFunc)(void*);

struct WynSpawn {
    WynSpawnFunc func;
    void* arg;
    WynSpawn* next;
    int worker_id;
};

struct WynTask {
    void** buffer;
    int capacity;
    int size;
    int read_pos;
    int write_pos;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
#else
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
    int closed;
};

struct WynScheduler {
    WynSpawn** queues;
#ifdef _WIN32
    CRITICAL_SECTION* locks;
    HANDLE* workers;
    CRITICAL_SECTION global_lock;
#else
    pthread_mutex_t* locks;
    pthread_t* workers;
    pthread_mutex_t global_lock;
#endif
    int num_workers;
    int running;
};

extern WynScheduler* global_scheduler;

WynScheduler* wyn_scheduler_init(int num_workers);
void wyn_scheduler_start(WynScheduler* sched);
void wyn_scheduler_shutdown(WynScheduler* sched);
void wyn_scheduler_enqueue(WynScheduler* sched, WynSpawnFunc func, void* arg);
void wyn_spawn(WynSpawnFunc func, void* arg);
WynTask* wyn_task_new(int capacity);
void wyn_task_send(WynTask* task, void* value);
void* wyn_task_recv(WynTask* task);
void wyn_task_close(WynTask* task);
void wyn_task_free(WynTask* task);

#endif
