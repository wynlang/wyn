// Task coordination runtime
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#define MAX_TASKS 1024
#define MAX_SPAWNS_PER_TASK 10000

typedef struct {
    _Atomic int spawn_count;
    _Atomic int completed;
    pthread_mutex_t lock;
    pthread_cond_t done;
    int cancelled;
} Task;

static Task tasks[MAX_TASKS];
static _Atomic int next_task_id = 0;

// Create new task group
int wyn_task_create() {
    int id = atomic_fetch_add(&next_task_id, 1) % MAX_TASKS;
    Task* task = &tasks[id];
    
    atomic_store(&task->spawn_count, 0);
    atomic_store(&task->completed, 0);
    task->cancelled = 0;
    pthread_mutex_init(&task->lock, NULL);
    pthread_cond_init(&task->done, NULL);
    
    return id;
}

// Wait for task group
int wyn_task_wait(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return -1;
    
    Task* task = &tasks[task_id];
    int total = atomic_load(&task->spawn_count);
    
    pthread_mutex_lock(&task->lock);
    while (atomic_load(&task->completed) < total && !task->cancelled) {
        pthread_cond_wait(&task->done, &task->lock);
    }
    pthread_mutex_unlock(&task->lock);
    
    return task->cancelled ? -1 : 0;
}

// Mark spawn complete
void wyn_task_complete(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return;
    
    Task* task = &tasks[task_id];
    int completed = atomic_fetch_add(&task->completed, 1) + 1;
    int total = atomic_load(&task->spawn_count);
    
    if (completed >= total) {
        pthread_mutex_lock(&task->lock);
        pthread_cond_signal(&task->done);
        pthread_mutex_unlock(&task->lock);
    }
}

// Cancel task group
int wyn_task_cancel(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return -1;
    
    Task* task = &tasks[task_id];
    pthread_mutex_lock(&task->lock);
    task->cancelled = 1;
    pthread_cond_signal(&task->done);
    pthread_mutex_unlock(&task->lock);
    
    return 0;
}
