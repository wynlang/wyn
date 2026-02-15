#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#ifdef __linux__
#include <sys/epoll.h>
#else
// For macOS/BSD, we'll use a simplified approach
#define EPOLL_CLOEXEC 0
#endif
#include <errno.h>

// Global runtime instance
static WynRuntime* global_runtime = NULL;
static pthread_mutex_t global_runtime_mutex_raw = PTHREAD_MUTEX_INITIALIZER;
static WynMutex global_runtime_mutex = {.mutex = PTHREAD_MUTEX_INITIALIZER, .initialized = true};

// Utility functions
uint64_t wyn_async_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void wyn_async_yield_now(void) {
    wyn_thread_yield();
}

// Waker implementation
WynWaker* wyn_waker_new(WynWakerFunc wake_func, void* data) {
    if (!wake_func) return NULL;
    
    WynWaker* waker = malloc(sizeof(WynWaker));
    if (!waker) return NULL;
    
    waker->wake_func = wake_func;
    waker->data = data;
    waker->ref_count = wyn_atomic_new(1);
    
    if (!waker->ref_count) {
        free(waker);
        return NULL;
    }
    
    return waker;
}

WynWaker* wyn_waker_clone(const WynWaker* waker) {
    if (!waker) return NULL;
    
    wyn_atomic_fetch_add(waker->ref_count, 1);
    return (WynWaker*)waker;  // Cast away const for clone
}

void wyn_waker_wake(WynWaker* waker) {
    if (!waker || !waker->wake_func) return;
    waker->wake_func(waker->data);
}

void wyn_waker_free(WynWaker* waker) {
    if (!waker) return;
    
    int64_t ref_count = wyn_atomic_fetch_sub(waker->ref_count, 1);
    if (ref_count <= 1) {
        wyn_atomic_free(waker->ref_count);
        free(waker);
    }
}

// Context implementation
WynContext* wyn_context_new(WynWaker* waker) {
    WynContext* context = malloc(sizeof(WynContext));
    if (!context) return NULL;
    
    context->waker = wyn_waker_clone(waker);
    return context;
}

void wyn_context_free(WynContext* context) {
    if (!context) return;
    wyn_waker_free(context->waker);
    free(context);
}

// Future implementation
WynFuture* wyn_future_new(void* data, WynFuturePollFunc poll_func, WynFutureCleanupFunc cleanup_func) {
    if (!poll_func) return NULL;
    
    WynFuture* future = malloc(sizeof(WynFuture));
    if (!future) return NULL;
    
    future->data = data;
    future->poll_func = poll_func;
    future->cleanup_func = cleanup_func;
    future->state = WYN_FUTURE_CREATED;
    
    WynMutex* mutex = wyn_mutex_new();
    if (!mutex) {
        free(future);
        return NULL;
    }
    future->mutex = *mutex;
    free(mutex);
    
    return future;
}

void wyn_future_free(WynFuture* future) {
    if (!future) return;
    
    if (future->cleanup_func) {
        future->cleanup_func(future);
    }
    
    pthread_mutex_destroy(&future->mutex.mutex);
    free(future);
}

WynPollResult wyn_future_poll(WynFuture* future, WynContext* context) {
    if (!future || !context) return WYN_POLL_PENDING;
    
    WynMutexGuard* guard = wyn_mutex_lock(&future->mutex);
    
    if (future->state == WYN_FUTURE_READY) {
        wyn_mutex_guard_unlock(guard);
        return WYN_POLL_READY;
    }
    
    future->state = WYN_FUTURE_POLLING;
    WynPollResult result = future->poll_func(future, context);
    
    if (result == WYN_POLL_READY) {
        future->state = WYN_FUTURE_READY;
    }
    
    wyn_mutex_guard_unlock(guard);
    return result;
}

bool wyn_future_is_ready(const WynFuture* future) {
    return future ? (future->state == WYN_FUTURE_READY) : false;
}

// Task implementation
static void task_waker_func(void* data) {
    WynTask* task = (WynTask*)data;
    if (task) {
        task->state = WYN_TASK_SCHEDULED;
    }
}

WynTask* wyn_task_new(WynFuture* future) {
    if (!future) return NULL;
    
    WynTask* task = malloc(sizeof(WynTask));
    if (!task) return NULL;
    
    task->future = future;
    task->state = WYN_TASK_CREATED;
    task->waker = wyn_waker_new(task_waker_func, task);
    task->next = NULL;
    task->task_id = 0;  // Will be set by executor
    
    if (!task->waker) {
        free(task);
        return NULL;
    }
    
    return task;
}

void wyn_task_free(WynTask* task) {
    if (!task) return;
    wyn_waker_free(task->waker);
    free(task);
}

void wyn_task_wake(WynTask* task) {
    if (!task) return;
    wyn_waker_wake(task->waker);
}

// Task queue implementation
WynTaskQueue* wyn_task_queue_new(void) {
    WynTaskQueue* queue = malloc(sizeof(WynTaskQueue));
    if (!queue) return NULL;
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    WynMutex* mutex = wyn_mutex_new();
    if (!mutex) {
        free(queue);
        return NULL;
    }
    queue->mutex = *mutex;
    free(mutex);
    
    int result = pthread_cond_init(&queue->not_empty, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&queue->mutex.mutex);
        free(queue);
        return NULL;
    }
    
    return queue;
}

void wyn_task_queue_free(WynTaskQueue* queue) {
    if (!queue) return;
    
    // Free remaining tasks
    WynTask* current = queue->head;
    while (current) {
        WynTask* next = current->next;
        wyn_task_free(current);
        current = next;
    }
    
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex.mutex);
    free(queue);
}

void wyn_task_queue_push(WynTaskQueue* queue, WynTask* task) {
    if (!queue || !task) return;
    
    pthread_mutex_lock(&queue->mutex.mutex);
    
    task->next = NULL;
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex.mutex);
}

WynTask* wyn_task_queue_pop(WynTaskQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex.mutex);
    
    // Use a timeout instead of indefinite wait
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;  // 1 second timeout
    
    while (queue->count == 0) {
        int result = pthread_cond_timedwait(&queue->not_empty, &queue->mutex.mutex, &timeout);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&queue->mutex.mutex);
            return NULL;  // Timeout
        }
    }
    
    WynTask* task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->count--;
    
    pthread_mutex_unlock(&queue->mutex.mutex);
    
    return task;
}

bool wyn_task_queue_is_empty(const WynTaskQueue* queue) {
    if (!queue) return true;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->mutex.mutex);
    bool empty = (queue->count == 0);
    pthread_mutex_unlock((pthread_mutex_t*)&queue->mutex.mutex);
    
    return empty;
}

// Executor worker thread function
static void* executor_worker_func(void* arg) {
    WynExecutor* executor = (WynExecutor*)arg;
    
    while (wyn_atomic_load(executor->running)) {
        WynTask* task = wyn_task_queue_pop(&executor->ready_queue);
        if (!task) continue;  // Timeout or no task
        
        task->state = WYN_TASK_RUNNING;
        
        WynContext* context = wyn_context_new(task->waker);
        if (context) {
            WynPollResult result = wyn_future_poll(task->future, context);
            
            if (result == WYN_POLL_READY) {
                task->state = WYN_TASK_COMPLETED;
            } else {
                task->state = WYN_TASK_SCHEDULED;
                // Re-queue the task for later execution
                wyn_task_queue_push(&executor->ready_queue, task);
                task = NULL;  // Don't free it
            }
            
            wyn_context_free(context);
        }
        
        if (task && task->state == WYN_TASK_COMPLETED) {
            wyn_task_free(task);
        }
    }
    
    return NULL;
}

// Executor implementation
WynExecutor* wyn_executor_new(size_t worker_count) {
    if (worker_count == 0) {
        worker_count = wyn_thread_hardware_concurrency();
    }
    
    WynExecutor* executor = malloc(sizeof(WynExecutor));
    if (!executor) return NULL;
    
    // Initialize task queue
    WynTaskQueue* queue = wyn_task_queue_new();
    if (!queue) {
        free(executor);
        return NULL;
    }
    executor->ready_queue = *queue;
    free(queue);
    
    executor->worker_count = worker_count;
    executor->worker_threads = malloc(worker_count * sizeof(WynThread*));
    executor->running = wyn_atomic_new(1);
    executor->next_task_id = 1;
    
    if (!executor->worker_threads || !executor->running) {
        wyn_task_queue_free(&executor->ready_queue);
        free(executor->worker_threads);
        wyn_atomic_free(executor->running);
        free(executor);
        return NULL;
    }
    
    // Start worker threads
    for (size_t i = 0; i < worker_count; i++) {
        executor->worker_threads[i] = wyn_thread_spawn(executor_worker_func, executor);
        if (!executor->worker_threads[i]) {
            // Cleanup on failure
            wyn_executor_shutdown(executor);
            return NULL;
        }
    }
    
    return executor;
}

void wyn_executor_free(WynExecutor* executor) {
    if (!executor) return;
    
    wyn_executor_shutdown(executor);
    
    wyn_task_queue_free(&executor->ready_queue);
    free(executor->worker_threads);
    wyn_atomic_free(executor->running);
    free(executor);
}

void wyn_executor_spawn(WynExecutor* executor, WynFuture* future) {
    if (!executor || !future) return;
    
    WynTask* task = wyn_task_new(future);
    if (!task) return;
    
    task->task_id = executor->next_task_id++;
    task->state = WYN_TASK_SCHEDULED;
    
    wyn_task_queue_push(&executor->ready_queue, task);
}

void* wyn_executor_block_on(WynExecutor* executor, WynFuture* future) {
    if (!executor || !future) return NULL;
    
    // Simple blocking implementation - poll until ready
    WynWaker* waker = wyn_waker_new(task_waker_func, NULL);
    WynContext* context = wyn_context_new(waker);
    
    void* result = NULL;
    
    while (true) {
        WynPollResult poll_result = wyn_future_poll(future, context);
        if (poll_result == WYN_POLL_READY) {
            result = future->data;  // Simplified - would need proper result extraction
            break;
        }
        
        wyn_thread_sleep_ms(1);  // Small delay to avoid busy waiting
    }
    
    wyn_context_free(context);
    wyn_waker_free(waker);
    
    return result;
}

void wyn_executor_shutdown(WynExecutor* executor) {
    if (!executor) return;
    
    wyn_atomic_store(executor->running, 0);
    
    // Wake up all worker threads
    for (size_t i = 0; i < executor->worker_count; i++) {
        if (executor->worker_threads[i]) {
            wyn_thread_join(executor->worker_threads[i]);
            wyn_thread_free(executor->worker_threads[i]);
            executor->worker_threads[i] = NULL;
        }
    }
}

// Reactor implementation (simplified)
WynReactor* wyn_reactor_new(void) {
    WynReactor* reactor = malloc(sizeof(WynReactor));
    if (!reactor) return NULL;
    
#ifdef __linux__
    reactor->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (reactor->epoll_fd == -1) {
        free(reactor);
        return NULL;
    }
#else
    // For non-Linux systems, use a dummy fd
    reactor->epoll_fd = -1;
#endif
    
    reactor->reactor_thread = NULL;
    reactor->running = wyn_atomic_new(1);
    
    WynMutex* mutex = wyn_mutex_new();
    if (!mutex) {
        close(reactor->epoll_fd);
        wyn_atomic_free(reactor->running);
        free(reactor);
        return NULL;
    }
    reactor->event_mutex = *mutex;
    free(mutex);
    
    return reactor;
}

void wyn_reactor_free(WynReactor* reactor) {
    if (!reactor) return;
    
    wyn_reactor_shutdown(reactor);
    
#ifdef __linux__
    if (reactor->epoll_fd != -1) {
        close(reactor->epoll_fd);
    }
#endif
    pthread_mutex_destroy(&reactor->event_mutex.mutex);
    wyn_atomic_free(reactor->running);
    free(reactor);
}

void wyn_reactor_register_async_op(WynReactor* reactor, WynAsyncOp* op) {
    if (!reactor || !op) return;
    
    // Simplified implementation - would register with epoll
    (void)op;  // Suppress unused parameter warning
}

void wyn_reactor_shutdown(WynReactor* reactor) {
    if (!reactor) return;
    
    wyn_atomic_store(reactor->running, 0);
    
    if (reactor->reactor_thread) {
        wyn_thread_join(reactor->reactor_thread);
        wyn_thread_free(reactor->reactor_thread);
        reactor->reactor_thread = NULL;
    }
}

// Runtime implementation
WynRuntime* wyn_runtime_new(void) {
    return wyn_runtime_with_config(0);  // Use default worker count
}

WynRuntime* wyn_runtime_with_config(size_t worker_count) {
    WynRuntime* runtime = malloc(sizeof(WynRuntime));
    if (!runtime) return NULL;
    
    runtime->executor = wyn_executor_new(worker_count);
    runtime->reactor = wyn_reactor_new();
    runtime->owns_executor = true;
    runtime->owns_reactor = true;
    
    if (!runtime->executor || !runtime->reactor) {
        wyn_runtime_free(runtime);
        return NULL;
    }
    
    return runtime;
}

void wyn_runtime_free(WynRuntime* runtime) {
    if (!runtime) return;
    
    if (runtime->owns_executor) {
        wyn_executor_free(runtime->executor);
    }
    if (runtime->owns_reactor) {
        wyn_reactor_free(runtime->reactor);
    }
    
    free(runtime);
}

void* wyn_runtime_block_on(WynRuntime* runtime, WynFuture* future) {
    if (!runtime || !future) return NULL;
    return wyn_executor_block_on(runtime->executor, future);
}

void wyn_runtime_spawn(WynRuntime* runtime, WynFuture* future) {
    if (!runtime || !future) return;
    wyn_executor_spawn(runtime->executor, future);
}

void wyn_runtime_shutdown(WynRuntime* runtime) {
    if (!runtime) return;
    
    wyn_executor_shutdown(runtime->executor);
    wyn_reactor_shutdown(runtime->reactor);
}

// Built-in future types

// Delay future implementation
static WynPollResult delay_future_poll(WynFuture* future, WynContext* context) {
    (void)context;  // Suppress unused parameter warning
    
    WynDelayFuture* delay = (WynDelayFuture*)future->data;
    if (!delay) return WYN_POLL_READY;
    
    if (delay->completed) {
        return WYN_POLL_READY;
    }
    
    uint64_t current_time = wyn_async_current_time_ms();
    if (current_time >= delay->start_time + delay->delay_ms) {
        delay->completed = true;
        return WYN_POLL_READY;
    }
    
    return WYN_POLL_PENDING;
}

static void delay_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* wyn_future_delay(uint64_t delay_ms) {
    WynDelayFuture* delay = malloc(sizeof(WynDelayFuture));
    if (!delay) return NULL;
    
    delay->delay_ms = delay_ms;
    delay->start_time = wyn_async_current_time_ms();
    delay->completed = false;
    
    return wyn_future_new(delay, delay_future_poll, delay_future_cleanup);
}

// Ready future implementation
static WynPollResult ready_future_poll(WynFuture* future, WynContext* context) {
    (void)future;   // Suppress unused parameter warning
    (void)context;  // Suppress unused parameter warning
    return WYN_POLL_READY;
}

WynFuture* wyn_future_ready(void* value) {
    WynFuture* future = wyn_future_new(value, ready_future_poll, NULL);
    if (future) {
        future->state = WYN_FUTURE_READY;  // Mark as ready immediately
    }
    return future;
}

// Join all future implementation
static WynPollResult join_all_future_poll(WynFuture* future, WynContext* context) {
    WynJoinFuture* join_all = (WynJoinFuture*)future->data;
    if (!join_all) return WYN_POLL_READY;
    
    if (join_all->completed_count == join_all->future_count) {
        return WYN_POLL_READY;
    }
    
    size_t completed = 0;
    for (size_t i = 0; i < join_all->future_count; i++) {
        if (join_all->results[i] == NULL) {
            WynPollResult result = wyn_future_poll(join_all->futures[i], context);
            if (result == WYN_POLL_READY) {
                join_all->results[i] = join_all->futures[i]->data;
                completed++;
            }
        } else {
            completed++;
        }
    }
    
    join_all->completed_count = completed;
    
    if (completed == join_all->future_count) {
        return WYN_POLL_READY;
    }
    
    return WYN_POLL_PENDING;
}

static void join_all_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        WynJoinFuture* join_all = (WynJoinFuture*)future->data;
        free(join_all->results);
        free(join_all);
        future->data = NULL;
    }
}

WynFuture* wyn_future_join_all(WynFuture** futures, size_t count) {
    if (!futures || count == 0) return wyn_future_ready(NULL);
    
    WynJoinFuture* join_all = malloc(sizeof(WynJoinFuture));
    if (!join_all) return NULL;
    
    join_all->futures = futures;
    join_all->future_count = count;
    join_all->results = calloc(count, sizeof(void*));
    join_all->completed_count = 0;
    
    if (!join_all->results) {
        free(join_all);
        return NULL;
    }
    
    return wyn_future_new(join_all, join_all_future_poll, join_all_future_cleanup);
}

// Select future implementation
static WynPollResult select_future_poll(WynFuture* future, WynContext* context) {
    WynSelectFuture* select = (WynSelectFuture*)future->data;
    if (!select) return WYN_POLL_READY;
    
    if (select->completed_index >= 0) {
        return WYN_POLL_READY;
    }
    
    for (size_t i = 0; i < select->future_count; i++) {
        WynPollResult result = wyn_future_poll(select->futures[i], context);
        if (result == WYN_POLL_READY) {
            select->completed_index = (int)i;
            select->result = select->futures[i]->data;
            return WYN_POLL_READY;
        }
    }
    
    return WYN_POLL_PENDING;
}

static void select_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* wyn_future_select(WynFuture** futures, size_t count) {
    if (!futures || count == 0) return wyn_future_ready(NULL);
    
    WynSelectFuture* select = malloc(sizeof(WynSelectFuture));
    if (!select) return NULL;
    
    select->futures = futures;
    select->future_count = count;
    select->completed_index = -1;
    select->result = NULL;
    
    return wyn_future_new(select, select_future_poll, select_future_cleanup);
}

// Async I/O implementation
typedef struct {
    int fd;
    void* buffer;
    size_t size;
    ssize_t bytes_read;
    bool completed;
    int error_code;
} WynAsyncReadFuture;

static WynPollResult async_read_future_poll(WynFuture* future, WynContext* context) {
    (void)context;  // Suppress unused parameter warning
    
    WynAsyncReadFuture* read_future = (WynAsyncReadFuture*)future->data;
    if (!read_future) return WYN_POLL_READY;
    
    if (read_future->completed) {
        return WYN_POLL_READY;
    }
    
    // Perform non-blocking read
    ssize_t result = read(read_future->fd, read_future->buffer, read_future->size);
    
    if (result >= 0) {
        read_future->bytes_read = result;
        read_future->completed = true;
        read_future->error_code = 0;
        return WYN_POLL_READY;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Would block, try again later
        return WYN_POLL_PENDING;
    } else {
        // Error occurred
        read_future->bytes_read = -1;
        read_future->completed = true;
        read_future->error_code = errno;
        return WYN_POLL_READY;
    }
}

static void async_read_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* wyn_future_read(int fd, void* buffer, size_t size) {
    if (!buffer || size == 0) return wyn_future_ready(NULL);
    
    WynAsyncReadFuture* read_future = malloc(sizeof(WynAsyncReadFuture));
    if (!read_future) return NULL;
    
    read_future->fd = fd;
    read_future->buffer = buffer;
    read_future->size = size;
    read_future->bytes_read = 0;
    read_future->completed = false;
    read_future->error_code = 0;
    
    return wyn_future_new(read_future, async_read_future_poll, async_read_future_cleanup);
}

typedef struct {
    int fd;
    const void* buffer;
    size_t size;
    ssize_t bytes_written;
    bool completed;
    int error_code;
} WynAsyncWriteFuture;

static WynPollResult async_write_future_poll(WynFuture* future, WynContext* context) {
    (void)context;  // Suppress unused parameter warning
    
    WynAsyncWriteFuture* write_future = (WynAsyncWriteFuture*)future->data;
    if (!write_future) return WYN_POLL_READY;
    
    if (write_future->completed) {
        return WYN_POLL_READY;
    }
    
    // Perform non-blocking write
    ssize_t result = write(write_future->fd, write_future->buffer, write_future->size);
    
    if (result >= 0) {
        write_future->bytes_written = result;
        write_future->completed = true;
        write_future->error_code = 0;
        return WYN_POLL_READY;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Would block, try again later
        return WYN_POLL_PENDING;
    } else {
        // Error occurred
        write_future->bytes_written = -1;
        write_future->completed = true;
        write_future->error_code = errno;
        return WYN_POLL_READY;
    }
}

static void async_write_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* wyn_future_write(int fd, const void* buffer, size_t size) {
    if (!buffer || size == 0) return wyn_future_ready(NULL);
    
    WynAsyncWriteFuture* write_future = malloc(sizeof(WynAsyncWriteFuture));
    if (!write_future) return NULL;
    
    write_future->fd = fd;
    write_future->buffer = buffer;
    write_future->size = size;
    write_future->bytes_written = 0;
    write_future->completed = false;
    write_future->error_code = 0;
    
    return wyn_future_new(write_future, async_write_future_poll, async_write_future_cleanup);
}

// Map future implementation
typedef struct {
    WynFuture* inner_future;
    void* (*map_func)(void*);
    void* result;
    bool completed;
} WynMapFuture;

static WynPollResult map_future_poll(WynFuture* future, WynContext* context) {
    WynMapFuture* map = (WynMapFuture*)future->data;
    if (!map) return WYN_POLL_READY;
    
    if (map->completed) {
        return WYN_POLL_READY;
    }
    
    WynPollResult result = wyn_future_poll(map->inner_future, context);
    if (result == WYN_POLL_READY) {
        map->result = map->map_func(map->inner_future->data);
        map->completed = true;
        return WYN_POLL_READY;
    }
    
    return WYN_POLL_PENDING;
}

static void map_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* wyn_future_map(WynFuture* future, void* (*map_func)(void*)) {
    if (!future || !map_func) return wyn_future_ready(NULL);
    
    WynMapFuture* map = malloc(sizeof(WynMapFuture));
    if (!map) return NULL;
    
    map->inner_future = future;
    map->map_func = map_func;
    map->result = NULL;
    map->completed = false;
    
    return wyn_future_new(map, map_future_poll, map_future_cleanup);
}

// Then future implementation
typedef struct {
    WynFuture* inner_future;
    WynFuture* (*then_func)(void*);
    WynFuture* chained_future;
    void* result;
    bool inner_completed;
    bool completed;
} WynThenFuture;

static WynPollResult then_future_poll(WynFuture* future, WynContext* context) {
    WynThenFuture* then = (WynThenFuture*)future->data;
    if (!then) return WYN_POLL_READY;
    
    if (then->completed) {
        return WYN_POLL_READY;
    }
    
    if (!then->inner_completed) {
        WynPollResult result = wyn_future_poll(then->inner_future, context);
        if (result == WYN_POLL_READY) {
            then->inner_completed = true;
            then->chained_future = then->then_func(then->inner_future->data);
        } else {
            return WYN_POLL_PENDING;
        }
    }
    
    if (then->chained_future) {
        WynPollResult result = wyn_future_poll(then->chained_future, context);
        if (result == WYN_POLL_READY) {
            then->result = then->chained_future->data;
            then->completed = true;
            return WYN_POLL_READY;
        }
    }
    
    return WYN_POLL_PENDING;
}

static void then_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        WynThenFuture* then = (WynThenFuture*)future->data;
        if (then->chained_future) {
            wyn_future_free(then->chained_future);
        }
        free(then);
        future->data = NULL;
    }
}

WynFuture* wyn_future_then(WynFuture* future, WynFuture* (*then_func)(void*)) {
    if (!future || !then_func) return wyn_future_ready(NULL);
    
    WynThenFuture* then = malloc(sizeof(WynThenFuture));
    if (!then) return NULL;
    
    then->inner_future = future;
    then->then_func = then_func;
    then->chained_future = NULL;
    then->result = NULL;
    then->inner_completed = false;
    then->completed = false;
    
    return wyn_future_new(then, then_future_poll, then_future_cleanup);
}

// Global runtime management
WynRuntime* wyn_async_global_runtime(void) {
    pthread_mutex_lock(&global_runtime_mutex.mutex);
    
    if (!global_runtime) {
        global_runtime = wyn_runtime_new();
    }
    
    WynRuntime* runtime = global_runtime;
    pthread_mutex_unlock(&global_runtime_mutex.mutex);
    
    return runtime;
}

void wyn_async_set_global_runtime(WynRuntime* runtime) {
    pthread_mutex_lock(&global_runtime_mutex.mutex);
    
    if (global_runtime && global_runtime != runtime) {
        wyn_runtime_free(global_runtime);
    }
    
    global_runtime = runtime;
    pthread_mutex_unlock(&global_runtime_mutex.mutex);
}
