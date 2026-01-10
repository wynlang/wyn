#include "../src/threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

// Example data structures
typedef struct {
    int start;
    int end;
    int* result;
} SumTask;

typedef struct {
    WynMutex* mutex;
    int* shared_counter;
    int iterations;
} MutexTask;

// Example thread functions
void* sum_range_func(void* arg) {
    SumTask* task = (SumTask*)arg;
    int sum = 0;
    
    for (int i = task->start; i <= task->end; i++) {
        sum += i;
    }
    
    *(task->result) = sum;
    return task->result;
}

void* mutex_increment_func(void* arg) {
    MutexTask* task = (MutexTask*)arg;
    
    for (int i = 0; i < task->iterations; i++) {
        WynMutexGuard* guard = wyn_mutex_lock(task->mutex);
        (*task->shared_counter)++;
        wyn_mutex_guard_unlock(guard);
    }
    
    return NULL;
}

void* producer_func(void* arg) {
    WynChannel* channel = (WynChannel*)arg;
    
    printf("Producer: Starting to send messages...\n");
    
    for (int i = 1; i <= 5; i++) {
        int* message = malloc(sizeof(int));
        *message = i * 10;
        
        printf("Producer: Sending %d\n", *message);
        wyn_channel_send(channel, message);
        
        wyn_thread_sleep_ms(100);  // Simulate work
    }
    
    printf("Producer: Finished sending messages\n");
    return NULL;
}

void* consumer_func(void* arg) {
    WynChannel* channel = (WynChannel*)arg;
    
    printf("Consumer: Starting to receive messages...\n");
    
    while (true) {
        void* data;
        if (wyn_channel_recv(channel, &data)) {
            int message = *(int*)data;
            printf("Consumer: Received %d\n", message);
            free(data);
            
            if (message >= 50) {  // Stop after receiving 50
                break;
            }
        } else {
            printf("Consumer: Channel closed or empty\n");
            break;
        }
    }
    
    printf("Consumer: Finished receiving messages\n");
    return NULL;
}

void* atomic_worker_func(void* arg) {
    WynAtomic* counter = (WynAtomic*)arg;
    
    for (int i = 0; i < 1000; i++) {
        wyn_atomic_fetch_add(counter, 1);
        
        if (i % 100 == 0) {
            int64_t current = wyn_atomic_load(counter);
            printf("Worker: Counter at %lld\n", (long long)current);
        }
    }
    
    return NULL;
}

void example_basic_threading() {
    printf("=== Basic Threading Example ===\n");
    
    // Create tasks for parallel computation
    SumTask task1 = {1, 1000, malloc(sizeof(int))};
    SumTask task2 = {1001, 2000, malloc(sizeof(int))};
    SumTask task3 = {2001, 3000, malloc(sizeof(int))};
    
    printf("Computing sum of 1-3000 using 3 threads...\n");
    
    // Spawn threads
    WynThread* thread1 = wyn_thread_spawn(sum_range_func, &task1);
    WynThread* thread2 = wyn_thread_spawn(sum_range_func, &task2);
    WynThread* thread3 = wyn_thread_spawn(sum_range_func, &task3);
    
    // Wait for results
    void* result1 = wyn_thread_join(thread1);
    void* result2 = wyn_thread_join(thread2);
    void* result3 = wyn_thread_join(thread3);
    
    int total = *(int*)result1 + *(int*)result2 + *(int*)result3;
    printf("Results: %d + %d + %d = %d\n", 
           *(int*)result1, *(int*)result2, *(int*)result3, total);
    
    // Expected: sum of 1-3000 = 3000 * 3001 / 2 = 4,501,500
    printf("Expected: 4,501,500\n");
    
    // Cleanup
    wyn_thread_free(thread1);
    wyn_thread_free(thread2);
    wyn_thread_free(thread3);
    free(task1.result);
    free(task2.result);
    free(task3.result);
    
    printf("\n");
}

void example_mutex_synchronization() {
    printf("=== Mutex Synchronization Example ===\n");
    
    WynMutex* mutex = wyn_mutex_new();
    int shared_counter = 0;
    
    // Create tasks
    MutexTask task1 = {mutex, &shared_counter, 1000};
    MutexTask task2 = {mutex, &shared_counter, 1000};
    MutexTask task3 = {mutex, &shared_counter, 1000};
    
    printf("Incrementing shared counter with 3 threads (1000 each)...\n");
    
    // Spawn threads
    WynThread* thread1 = wyn_thread_spawn(mutex_increment_func, &task1);
    WynThread* thread2 = wyn_thread_spawn(mutex_increment_func, &task2);
    WynThread* thread3 = wyn_thread_spawn(mutex_increment_func, &task3);
    
    // Wait for completion
    wyn_thread_join(thread1);
    wyn_thread_join(thread2);
    wyn_thread_join(thread3);
    
    printf("Final counter value: %d (expected: 3000)\n", shared_counter);
    
    // Cleanup
    wyn_thread_free(thread1);
    wyn_thread_free(thread2);
    wyn_thread_free(thread3);
    wyn_mutex_free(mutex);
    
    printf("\n");
}

void example_rwlock_usage() {
    printf("=== RwLock Usage Example ===\n");
    
    WynRwLock* rwlock = wyn_rwlock_new();
    int shared_data = 42;
    
    printf("Demonstrating read-write lock...\n");
    
    // Multiple readers can access simultaneously
    printf("Acquiring multiple read locks...\n");
    WynRwLockReadGuard* read1 = wyn_rwlock_read(rwlock);
    WynRwLockReadGuard* read2 = wyn_rwlock_read(rwlock);
    WynRwLockReadGuard* read3 = wyn_rwlock_read(rwlock);
    
    printf("All read locks acquired successfully\n");
    printf("Shared data (read): %d\n", shared_data);
    
    // Release read locks
    wyn_rwlock_read_guard_unlock(read1);
    wyn_rwlock_read_guard_unlock(read2);
    wyn_rwlock_read_guard_unlock(read3);
    
    // Exclusive write access
    printf("Acquiring write lock for exclusive access...\n");
    WynRwLockWriteGuard* write_guard = wyn_rwlock_write(rwlock);
    shared_data = 100;
    printf("Shared data updated to: %d\n", shared_data);
    wyn_rwlock_write_guard_unlock(write_guard);
    
    wyn_rwlock_free(rwlock);
    
    printf("\n");
}

void example_channel_communication() {
    printf("=== Channel Communication Example ===\n");
    
    WynChannel* channel = wyn_channel_new(3);  // Bounded channel
    
    printf("Starting producer-consumer communication...\n");
    
    // Start producer and consumer threads
    WynThread* producer = wyn_thread_spawn(producer_func, channel);
    WynThread* consumer = wyn_thread_spawn(consumer_func, channel);
    
    // Let them run for a while
    wyn_thread_sleep_ms(1000);
    
    // Wait for completion
    wyn_thread_join(producer);
    wyn_thread_join(consumer);
    
    printf("Communication completed\n");
    
    // Cleanup
    wyn_thread_free(producer);
    wyn_thread_free(consumer);
    wyn_channel_free(channel);
    
    printf("\n");
}

void example_atomic_operations() {
    printf("=== Atomic Operations Example ===\n");
    
    WynAtomic* counter = wyn_atomic_new(0);
    
    printf("Starting atomic counter with 3 worker threads...\n");
    
    // Start worker threads
    WynThread* worker1 = wyn_thread_spawn(atomic_worker_func, counter);
    WynThread* worker2 = wyn_thread_spawn(atomic_worker_func, counter);
    WynThread* worker3 = wyn_thread_spawn(atomic_worker_func, counter);
    
    // Monitor progress
    for (int i = 0; i < 10; i++) {
        wyn_thread_sleep_ms(50);
        int64_t current = wyn_atomic_load(counter);
        printf("Main: Counter progress: %lld\n", (long long)current);
    }
    
    // Wait for completion
    wyn_thread_join(worker1);
    wyn_thread_join(worker2);
    wyn_thread_join(worker3);
    
    int64_t final_value = wyn_atomic_load(counter);
    printf("Final atomic counter value: %lld (expected: 3000)\n", (long long)final_value);
    
    // Demonstrate compare-and-swap
    printf("Demonstrating compare-and-swap...\n");
    int64_t expected = final_value;
    bool swapped = wyn_atomic_compare_exchange(counter, &expected, 9999);
    printf("CAS result: %s, new value: %lld\n", 
           swapped ? "success" : "failed", (long long)wyn_atomic_load(counter));
    
    // Cleanup
    wyn_thread_free(worker1);
    wyn_thread_free(worker2);
    wyn_thread_free(worker3);
    wyn_atomic_free(counter);
    
    printf("\n");
}

void example_threadsafe_collections() {
    printf("=== Thread-Safe Collections Example ===\n");
    
    WynThreadSafeVec* vec = wyn_threadsafe_vec_new();
    
    printf("Using thread-safe vector...\n");
    
    // Add some data
    int data[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        wyn_threadsafe_vec_push(vec, &data[i]);
        printf("Pushed: %d, length: %zu\n", data[i], wyn_threadsafe_vec_len(vec));
    }
    
    // Read data
    printf("Reading data:\n");
    for (size_t i = 0; i < wyn_threadsafe_vec_len(vec); i++) {
        void* item;
        if (wyn_threadsafe_vec_get(vec, i, &item)) {
            printf("  [%zu]: %d\n", i, *(int*)item);
        }
    }
    
    // Pop data
    printf("Popping data:\n");
    void* item;
    while (wyn_threadsafe_vec_pop(vec, &item)) {
        printf("Popped: %d, remaining: %zu\n", *(int*)item, wyn_threadsafe_vec_len(vec));
    }
    
    wyn_threadsafe_vec_free(vec);
    
    printf("\n");
}

void example_system_info() {
    printf("=== System Information ===\n");
    
    size_t concurrency = wyn_thread_hardware_concurrency();
    uint64_t thread_id = wyn_thread_current_id();
    
    printf("Hardware concurrency: %zu threads\n", concurrency);
    printf("Current thread ID: %llu\n", (unsigned long long)thread_id);
    
    printf("Optimal thread pool size: %zu\n", concurrency);
    printf("Recommended for CPU-bound tasks: %zu\n", concurrency);
    printf("Recommended for I/O-bound tasks: %zu\n", concurrency * 2);
    
    printf("\n");
}

void example_performance_comparison() {
    printf("=== Performance Comparison ===\n");
    
    const int ITERATIONS = 1000000;
    
    // Sequential counter
    printf("Sequential increment (%d iterations)...\n", ITERATIONS);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    int sequential_counter = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        sequential_counter++;
    }
    
    gettimeofday(&end, NULL);
    long sequential_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Sequential result: %d, time: %ld μs\n", sequential_counter, sequential_time);
    
    // Atomic counter with single thread
    printf("Atomic increment (single thread, %d iterations)...\n", ITERATIONS);
    gettimeofday(&start, NULL);
    
    WynAtomic* atomic = wyn_atomic_new(0);
    for (int i = 0; i < ITERATIONS; i++) {
        wyn_atomic_fetch_add(atomic, 1);
    }
    
    gettimeofday(&end, NULL);
    long atomic_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Atomic result: %lld, time: %ld μs\n", 
           (long long)wyn_atomic_load(atomic), atomic_time);
    
    printf("Atomic overhead: %.2fx slower\n", (double)atomic_time / sequential_time);
    
    wyn_atomic_free(atomic);
    
    printf("\n");
}

int main() {
    printf("Wyn Threading System Examples\n");
    printf("============================\n\n");
    
    example_basic_threading();
    example_mutex_synchronization();
    example_rwlock_usage();
    example_channel_communication();
    example_atomic_operations();
    example_threadsafe_collections();
    example_system_info();
    example_performance_comparison();
    
    printf("All threading examples completed successfully!\n");
    return 0;
}
