#include "../src/async.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// Test data structures
typedef struct {
    int value;
    bool completed;
} TestFutureData;

// Test future implementations
static WynPollResult test_future_poll(WynFuture* future, WynContext* context) {
    (void)context;  // Suppress unused parameter warning
    
    TestFutureData* data = (TestFutureData*)future->data;
    if (!data) return WYN_POLL_READY;
    
    if (data->completed) {
        return WYN_POLL_READY;
    }
    
    // Simulate some work
    data->value++;
    if (data->value >= 5) {
        data->completed = true;
        return WYN_POLL_READY;
    }
    
    return WYN_POLL_PENDING;
}

static void test_future_cleanup(WynFuture* future) {
    if (future && future->data) {
        free(future->data);
        future->data = NULL;
    }
}

WynFuture* create_test_future(int initial_value) {
    TestFutureData* data = malloc(sizeof(TestFutureData));
    if (!data) return NULL;
    
    data->value = initial_value;
    data->completed = false;
    
    return wyn_future_new(data, test_future_poll, test_future_cleanup);
}

// Test helper functions
static void test_wake_func(void* data) {
    bool* called = (bool*)data;
    *called = true;
}

void test_waker_operations() {
    printf("Testing waker operations...\n");
    
    bool waker_called = false;
    
    WynWaker* waker = wyn_waker_new(test_wake_func, &waker_called);
    assert(waker != NULL);
    
    // Test waker clone
    WynWaker* cloned = wyn_waker_clone(waker);
    assert(cloned != NULL);
    assert(cloned == waker);  // Should be same instance with increased ref count
    
    // Test wake
    assert(!waker_called);
    wyn_waker_wake(waker);
    assert(waker_called);
    
    // Cleanup
    wyn_waker_free(waker);
    wyn_waker_free(cloned);
    
    printf("✓ Waker operation tests passed\n");
}

void test_context_operations() {
    printf("Testing context operations...\n");
    
    bool waker_called = false;
    
    WynWaker* waker = wyn_waker_new(test_wake_func, &waker_called);
    WynContext* context = wyn_context_new(waker);
    
    assert(context != NULL);
    assert(context->waker != NULL);
    
    // Test context waker
    wyn_waker_wake(context->waker);
    assert(waker_called);
    
    // Cleanup
    wyn_context_free(context);
    wyn_waker_free(waker);
    
    printf("✓ Context operation tests passed\n");
}

void test_future_operations() {
    printf("Testing future operations...\n");
    
    // Test future creation
    WynFuture* future = create_test_future(0);
    assert(future != NULL);
    assert(!wyn_future_is_ready(future));
    
    // Test polling
    bool waker_called = false;
    
    WynWaker* waker = wyn_waker_new(test_wake_func, &waker_called);
    WynContext* context = wyn_context_new(waker);
    
    // Poll until ready
    WynPollResult result;
    int poll_count = 0;
    do {
        result = wyn_future_poll(future, context);
        poll_count++;
        assert(poll_count < 10);  // Prevent infinite loop
    } while (result == WYN_POLL_PENDING);
    
    assert(result == WYN_POLL_READY);
    assert(wyn_future_is_ready(future));
    
    // Cleanup
    wyn_context_free(context);
    wyn_waker_free(waker);
    wyn_future_free(future);
    
    printf("✓ Future operation tests passed\n");
}

void test_task_operations() {
    printf("Testing task operations...\n");
    
    WynFuture* future = create_test_future(0);
    WynTask* task = wyn_task_new(future);
    
    assert(task != NULL);
    assert(task->future == future);
    assert(task->state == WYN_TASK_CREATED);
    assert(task->waker != NULL);
    
    // Test task wake
    wyn_task_wake(task);
    assert(task->state == WYN_TASK_SCHEDULED);
    
    // Cleanup
    wyn_task_free(task);
    wyn_future_free(future);
    
    printf("✓ Task operation tests passed\n");
}

void test_task_queue_operations() {
    printf("Testing task queue operations...\n");
    
    WynTaskQueue* queue = wyn_task_queue_new();
    assert(queue != NULL);
    assert(wyn_task_queue_is_empty(queue));
    
    // Create test tasks
    WynFuture* future1 = create_test_future(0);
    WynFuture* future2 = create_test_future(10);
    WynTask* task1 = wyn_task_new(future1);
    WynTask* task2 = wyn_task_new(future2);
    
    // Test push
    wyn_task_queue_push(queue, task1);
    assert(!wyn_task_queue_is_empty(queue));
    
    wyn_task_queue_push(queue, task2);
    
    // Test pop
    WynTask* popped1 = wyn_task_queue_pop(queue);
    assert(popped1 == task1);
    
    WynTask* popped2 = wyn_task_queue_pop(queue);
    assert(popped2 == task2);
    
    assert(wyn_task_queue_is_empty(queue));
    
    // Cleanup
    wyn_task_free(task1);
    wyn_task_free(task2);
    wyn_future_free(future1);
    wyn_future_free(future2);
    wyn_task_queue_free(queue);
    
    printf("✓ Task queue operation tests passed\n");
}

void test_executor_operations() {
    printf("Testing executor operations...\n");
    
    WynExecutor* executor = wyn_executor_new(2);  // 2 worker threads
    assert(executor != NULL);
    
    // Test spawn
    WynFuture* future = create_test_future(0);
    wyn_executor_spawn(executor, future);
    
    // Give some time for execution
    wyn_thread_sleep_ms(100);
    
    // Test block_on with a simple future
    WynFuture* ready_future = wyn_future_ready((void*)42);
    void* result = wyn_executor_block_on(executor, ready_future);
    assert(result == (void*)42);
    
    // Cleanup
    wyn_future_free(ready_future);
    wyn_executor_free(executor);
    
    printf("✓ Executor operation tests passed\n");
}

void test_reactor_operations() {
    printf("Testing reactor operations...\n");
    
    WynReactor* reactor = wyn_reactor_new();
    assert(reactor != NULL);
    
    // Test async operation registration (simplified)
    WynAsyncOp op = {
        .op_type = WYN_ASYNC_READ,
        .fd = 0,
        .buffer = NULL,
        .buffer_size = 0,
        .waker = NULL,
        .completed = false,
        .result = 0
    };
    
    wyn_reactor_register_async_op(reactor, &op);
    
    // Cleanup
    wyn_reactor_free(reactor);
    
    printf("✓ Reactor operation tests passed\n");
}

void test_runtime_operations() {
    printf("Testing runtime operations...\n");
    
    WynRuntime* runtime = wyn_runtime_new();
    assert(runtime != NULL);
    assert(runtime->executor != NULL);
    assert(runtime->reactor != NULL);
    
    // Test spawn
    WynFuture* future = create_test_future(0);
    wyn_runtime_spawn(runtime, future);
    
    // Test block_on
    WynFuture* ready_future = wyn_future_ready((void*)123);
    void* result = wyn_runtime_block_on(runtime, ready_future);
    assert(result == (void*)123);
    
    // Cleanup
    wyn_future_free(ready_future);
    wyn_runtime_free(runtime);
    
    printf("✓ Runtime operation tests passed\n");
}

void test_builtin_futures() {
    printf("Testing built-in futures...\n");
    
    // Test delay future
    uint64_t start_time = wyn_async_current_time_ms();
    WynFuture* delay_future = wyn_future_delay(50);  // 50ms delay
    assert(delay_future != NULL);
    
    WynRuntime* runtime = wyn_runtime_new();
    wyn_runtime_block_on(runtime, delay_future);
    
    uint64_t end_time = wyn_async_current_time_ms();
    uint64_t elapsed = end_time - start_time;
    assert(elapsed >= 45 && elapsed <= 100);  // Allow some variance
    
    // Test ready future
    WynFuture* ready_future = wyn_future_ready((void*)456);
    void* result = wyn_runtime_block_on(runtime, ready_future);
    assert(result == (void*)456);
    
    // Cleanup
    wyn_future_free(delay_future);
    wyn_future_free(ready_future);
    wyn_runtime_free(runtime);
    
    printf("✓ Built-in future tests passed\n");
}

void test_global_runtime() {
    printf("Testing global runtime...\n");
    
    // Test global runtime access
    WynRuntime* global1 = wyn_async_global_runtime();
    assert(global1 != NULL);
    
    WynRuntime* global2 = wyn_async_global_runtime();
    assert(global1 == global2);  // Should be same instance
    
    // Test setting custom global runtime
    WynRuntime* custom = wyn_runtime_new();
    wyn_async_set_global_runtime(custom);
    
    WynRuntime* global3 = wyn_async_global_runtime();
    assert(global3 == custom);
    
    // Cleanup will happen automatically when setting NULL or new runtime
    wyn_async_set_global_runtime(NULL);
    
    printf("✓ Global runtime tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test current time
    uint64_t time1 = wyn_async_current_time_ms();
    wyn_thread_sleep_ms(10);
    uint64_t time2 = wyn_async_current_time_ms();
    assert(time2 > time1);
    assert((time2 - time1) >= 8);  // Allow some variance
    
    // Test yield
    wyn_async_yield_now();  // Should not crash
    
    printf("✓ Utility function tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    assert(wyn_waker_new(NULL, NULL) == NULL);
    assert(wyn_future_new(NULL, NULL, NULL) == NULL);
    assert(wyn_task_new(NULL) == NULL);
    
    wyn_waker_free(NULL);  // Should not crash
    wyn_future_free(NULL); // Should not crash
    wyn_task_free(NULL);   // Should not crash
    
    // Test executor with 0 workers (should use hardware concurrency)
    WynExecutor* executor = wyn_executor_new(0);
    assert(executor != NULL);
    assert(executor->worker_count > 0);
    wyn_executor_free(executor);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Async/Await System Tests\n");
    printf("================================\n\n");
    
    test_waker_operations();
    test_context_operations();
    test_future_operations();
    test_task_operations();
    test_task_queue_operations();
    test_executor_operations();
    test_reactor_operations();
    test_runtime_operations();
    test_builtin_futures();
    test_global_runtime();
    test_utility_functions();
    test_edge_cases();
    
    printf("\n✅ All async/await system tests passed!\n");
    return 0;
}
