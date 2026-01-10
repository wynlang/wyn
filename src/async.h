#ifndef WYN_ASYNC_H
#define WYN_ASYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include "threading.h"

// Forward declarations
typedef struct WynFuture WynFuture;
typedef struct WynContext WynContext;
typedef struct WynWaker WynWaker;
typedef struct WynTask WynTask;
typedef struct WynExecutor WynExecutor;
typedef struct WynReactor WynReactor;
typedef struct WynRuntime WynRuntime;

// Poll result enumeration
typedef enum {
    WYN_POLL_READY,
    WYN_POLL_PENDING
} WynPollResult;

// Future state
typedef enum {
    WYN_FUTURE_CREATED,
    WYN_FUTURE_POLLING,
    WYN_FUTURE_READY,
    WYN_FUTURE_CANCELLED
} WynFutureState;

// Task state
typedef enum {
    WYN_TASK_CREATED,
    WYN_TASK_SCHEDULED,
    WYN_TASK_RUNNING,
    WYN_TASK_COMPLETED,
    WYN_TASK_CANCELLED
} WynTaskState;

// Waker function type
typedef void (*WynWakerFunc)(void* data);

// Future poll function type
typedef WynPollResult (*WynFuturePollFunc)(WynFuture* future, WynContext* context);

// Future cleanup function type
typedef void (*WynFutureCleanupFunc)(WynFuture* future);

// Waker structure for task notification
typedef struct WynWaker {
    WynWakerFunc wake_func;
    void* data;
    WynAtomic* ref_count;
} WynWaker;

// Context passed to Future::poll
typedef struct WynContext {
    WynWaker* waker;
} WynContext;

// Future trait implementation
typedef struct WynFuture {
    void* data;                    // Future-specific data
    WynFuturePollFunc poll_func;   // Poll function
    WynFutureCleanupFunc cleanup_func; // Cleanup function
    WynFutureState state;
    WynMutex mutex;               // For thread safety
} WynFuture;

// Task structure for the executor
typedef struct WynTask {
    WynFuture* future;
    WynTaskState state;
    WynWaker* waker;
    struct WynTask* next;         // For task queue
    uint64_t task_id;
} WynTask;

// Task queue for the executor
typedef struct WynTaskQueue {
    WynTask* head;
    WynTask* tail;
    size_t count;
    WynMutex mutex;
    pthread_cond_t not_empty;
} WynTaskQueue;

// Executor for running async tasks
typedef struct WynExecutor {
    WynTaskQueue ready_queue;
    WynThread** worker_threads;
    size_t worker_count;
    WynAtomic* running;
    uint64_t next_task_id;
} WynExecutor;

// Reactor for I/O events (simplified)
typedef struct WynReactor {
    int epoll_fd;                 // Linux epoll file descriptor
    WynThread* reactor_thread;
    WynAtomic* running;
    WynMutex event_mutex;
} WynReactor;

// Runtime combining executor and reactor
typedef struct WynRuntime {
    WynExecutor* executor;
    WynReactor* reactor;
    bool owns_executor;
    bool owns_reactor;
} WynRuntime;

// Async I/O operation types
typedef enum {
    WYN_ASYNC_READ,
    WYN_ASYNC_WRITE,
    WYN_ASYNC_ACCEPT,
    WYN_ASYNC_CONNECT
} WynAsyncOpType;

// Async I/O operation
typedef struct WynAsyncOp {
    WynAsyncOpType op_type;
    int fd;
    void* buffer;
    size_t buffer_size;
    WynWaker* waker;
    bool completed;
    ssize_t result;
} WynAsyncOp;

// Future creation and management
WynFuture* wyn_future_new(void* data, WynFuturePollFunc poll_func, WynFutureCleanupFunc cleanup_func);
void wyn_future_free(WynFuture* future);
WynPollResult wyn_future_poll(WynFuture* future, WynContext* context);
bool wyn_future_is_ready(const WynFuture* future);

// Waker creation and management
WynWaker* wyn_waker_new(WynWakerFunc wake_func, void* data);
WynWaker* wyn_waker_clone(const WynWaker* waker);
void wyn_waker_wake(WynWaker* waker);
void wyn_waker_free(WynWaker* waker);

// Context creation and management
WynContext* wyn_context_new(WynWaker* waker);
void wyn_context_free(WynContext* context);

// Task creation and management
WynTask* wyn_task_new(WynFuture* future);
void wyn_task_free(WynTask* task);
void wyn_task_wake(WynTask* task);

// Task queue operations
WynTaskQueue* wyn_task_queue_new(void);
void wyn_task_queue_free(WynTaskQueue* queue);
void wyn_task_queue_push(WynTaskQueue* queue, WynTask* task);
WynTask* wyn_task_queue_pop(WynTaskQueue* queue);
bool wyn_task_queue_is_empty(const WynTaskQueue* queue);

// Executor operations
WynExecutor* wyn_executor_new(size_t worker_count);
void wyn_executor_free(WynExecutor* executor);
void wyn_executor_spawn(WynExecutor* executor, WynFuture* future);
void* wyn_executor_block_on(WynExecutor* executor, WynFuture* future);
void wyn_executor_shutdown(WynExecutor* executor);

// Reactor operations (simplified)
WynReactor* wyn_reactor_new(void);
void wyn_reactor_free(WynReactor* reactor);
void wyn_reactor_register_async_op(WynReactor* reactor, WynAsyncOp* op);
void wyn_reactor_shutdown(WynReactor* reactor);

// Runtime operations
WynRuntime* wyn_runtime_new(void);
WynRuntime* wyn_runtime_with_config(size_t worker_count);
void wyn_runtime_free(WynRuntime* runtime);
void* wyn_runtime_block_on(WynRuntime* runtime, WynFuture* future);
void wyn_runtime_spawn(WynRuntime* runtime, WynFuture* future);
void wyn_runtime_shutdown(WynRuntime* runtime);

// Built-in future types
typedef struct WynDelayFuture {
    uint64_t delay_ms;
    uint64_t start_time;
    bool completed;
} WynDelayFuture;

typedef struct WynJoinFuture {
    WynFuture** futures;
    size_t future_count;
    void** results;
    size_t completed_count;
} WynJoinFuture;

typedef struct WynSelectFuture {
    WynFuture** futures;
    size_t future_count;
    int completed_index;
    void* result;
} WynSelectFuture;

// Built-in future constructors
WynFuture* wyn_future_delay(uint64_t delay_ms);
WynFuture* wyn_future_ready(void* value);
WynFuture* wyn_future_join_all(WynFuture** futures, size_t count);
WynFuture* wyn_future_select(WynFuture** futures, size_t count);

// Async I/O futures
WynFuture* wyn_future_read(int fd, void* buffer, size_t size);
WynFuture* wyn_future_write(int fd, const void* buffer, size_t size);

// Future combinators
WynFuture* wyn_future_map(WynFuture* future, void* (*map_func)(void*));
WynFuture* wyn_future_then(WynFuture* future, WynFuture* (*then_func)(void*));

// Utility functions
uint64_t wyn_async_current_time_ms(void);
void wyn_async_yield_now(void);

// Global runtime (optional convenience)
WynRuntime* wyn_async_global_runtime(void);
void wyn_async_set_global_runtime(WynRuntime* runtime);

#endif // WYN_ASYNC_H
