// Optimized scheduler - working version with larger batches
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include "future.h"

#define MAX_WORKERS 64
#define BATCH_SIZE 128

typedef void (*TaskFunc)(void*);
typedef void* (*TaskFuncWithReturn)(void*);

typedef struct Task {
    TaskFunc func;
    void* arg;
    Future* future;
    struct Task* next;
} Task;

// Bulk task allocation
#define TASK_CHUNK_SIZE 4096
typedef struct TaskChunk {
    Task tasks[TASK_CHUNK_SIZE];
    struct TaskChunk* next;
} TaskChunk;

static _Atomic(Task*) task_free_list = NULL;
static _Atomic(TaskChunk*) chunk_list = NULL;
static pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    Task* head;
    Task* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    _Atomic long count;
    _Atomic int waiting_workers;
} GlobalQueue;

static GlobalQueue global_queue;
static pthread_t workers[MAX_WORKERS];
static _Atomic int num_workers = 0;
static _Atomic int initialized = 0;
static _Atomic int shutdown = 0;

static void alloc_task_chunk() {
    TaskChunk* chunk = malloc(sizeof(TaskChunk));
    for (int i = 0; i < TASK_CHUNK_SIZE - 1; i++) {
        chunk->tasks[i].next = &chunk->tasks[i + 1];
    }
    chunk->tasks[TASK_CHUNK_SIZE - 1].next = NULL;
    
    Task* old_head;
    do {
        old_head = atomic_load(&task_free_list);
        chunk->tasks[TASK_CHUNK_SIZE - 1].next = old_head;
    } while (!atomic_compare_exchange_weak(&task_free_list, &old_head, &chunk->tasks[0]));
    
    TaskChunk* old_chunk;
    do {
        old_chunk = atomic_load(&chunk_list);
        chunk->next = old_chunk;
    } while (!atomic_compare_exchange_weak(&chunk_list, &old_chunk, chunk));
}

static Task* alloc_task() {
    Task* t;
    Task* next;
    do {
        t = atomic_load(&task_free_list);
        if (!t) {
            pthread_mutex_lock(&alloc_lock);
            t = atomic_load(&task_free_list);
            if (!t) {
                alloc_task_chunk();
                t = atomic_load(&task_free_list);
            }
            pthread_mutex_unlock(&alloc_lock);
            if (!t) return malloc(sizeof(Task));
        }
        next = t->next;
    } while (!atomic_compare_exchange_weak(&task_free_list, &t, next));
    return t;
}

static void free_task(Task* t) {
    Task* old_head;
    do {
        old_head = atomic_load(&task_free_list);
        t->next = old_head;
    } while (!atomic_compare_exchange_weak(&task_free_list, &old_head, t));
}

static void global_queue_init() {
    global_queue.head = NULL;
    global_queue.tail = NULL;
    atomic_store(&global_queue.count, 0);
    atomic_store(&global_queue.waiting_workers, 0);
    pthread_mutex_init(&global_queue.lock, NULL);
    pthread_cond_init(&global_queue.cond, NULL);
}

static void global_queue_push(Task* task) {
    task->next = NULL;
    pthread_mutex_lock(&global_queue.lock);
    if (global_queue.tail) {
        global_queue.tail->next = task;
    } else {
        global_queue.head = task;
        if (atomic_load(&global_queue.waiting_workers) > 0) {
            pthread_cond_signal(&global_queue.cond);
        }
    }
    global_queue.tail = task;
    atomic_fetch_add(&global_queue.count, 1);
    pthread_mutex_unlock(&global_queue.lock);
}

static Task* global_queue_pop_batch(int* out_count) {
    pthread_mutex_lock(&global_queue.lock);
    
    while (!global_queue.head && !atomic_load(&shutdown)) {
        atomic_fetch_add(&global_queue.waiting_workers, 1);
        pthread_cond_wait(&global_queue.cond, &global_queue.lock);
        atomic_fetch_sub(&global_queue.waiting_workers, 1);
    }
    
    if (!global_queue.head) {
        pthread_mutex_unlock(&global_queue.lock);
        *out_count = 0;
        return NULL;
    }
    
    Task* batch_head = global_queue.head;
    Task* current = batch_head;
    int count = 1;
    
    while (count < BATCH_SIZE && current->next) {
        current = current->next;
        count++;
    }
    
    global_queue.head = current->next;
    if (!global_queue.head) {
        global_queue.tail = NULL;
    }
    current->next = NULL;
    
    atomic_fetch_sub(&global_queue.count, count);
    pthread_mutex_unlock(&global_queue.lock);
    
    *out_count = count;
    return batch_head;
}

static void* worker(void* arg) {
    (void)arg;
    
    while (!atomic_load(&shutdown)) {
        int count;
        Task* batch = global_queue_pop_batch(&count);
        if (!batch) break;
        
        for (int i = 0; i < count; i++) {
            Task* t = batch;
            batch = batch->next;
            
            if (t->future) {
                TaskFuncWithReturn func_ret = (TaskFuncWithReturn)t->func;
                void* result = func_ret(t->arg);
                future_set(t->future, result);
            } else {
                t->func(t->arg);
            }
            
            free_task(t);
        }
    }
    
    return NULL;
}

static void init_scheduler() {
    if (atomic_exchange(&initialized, 1)) return;
    
    int cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 4;
    if (cpus > MAX_WORKERS) cpus = MAX_WORKERS;
    
    atomic_store(&num_workers, cpus);
    global_queue_init();
    
    for (int i = 0; i < cpus; i++) {
        pthread_create(&workers[i], NULL, worker, NULL);
    }
}

void wyn_spawn_fast(TaskFunc func, void* arg) {
    init_scheduler();
    
    Task* t = alloc_task();
    t->func = func;
    t->arg = arg;
    t->future = NULL;
    t->next = NULL;
    
    global_queue_push(t);
}

Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg) {
    init_scheduler();
    
    Future* future = future_new();
    Task* t = alloc_task();
    t->func = (TaskFunc)func;
    t->arg = arg;
    t->future = future;
    t->next = NULL;
    
    global_queue_push(t);
    
    return future;
}

void wyn_spawn_wait() {
    while (atomic_load(&global_queue.count) > 0) {
        usleep(1000);
    }
}
