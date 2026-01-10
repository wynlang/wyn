#include "../src/threading.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

// Test data structures
typedef struct {
    int value;
    int iterations;
} ThreadTestData;

// Test thread functions
void* simple_thread_func(void* arg) {
    int* result = malloc(sizeof(int));
    *result = *(int*)arg * 2;
    return result;
}

void* counter_thread_func(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;
    for (int i = 0; i < data->iterations; i++) {
        data->value++;
        wyn_thread_yield();
    }
    return NULL;
}

void* mutex_counter_func(void* arg) {
    WynMutex** mutex_ptr = (WynMutex**)arg;
    WynMutex* mutex = *mutex_ptr;
    
    for (int i = 0; i < 1000; i++) {
        WynMutexGuard* guard = wyn_mutex_lock(mutex);
        // Simulate some work
        wyn_thread_yield();
        wyn_mutex_guard_unlock(guard);
    }
    
    return NULL;
}

void* channel_sender_func(void* arg) {
    WynChannel* channel = (WynChannel*)arg;
    
    for (int i = 0; i < 5; i++) {
        int* data = malloc(sizeof(int));
        *data = i;
        wyn_channel_send(channel, data);
    }
    
    return NULL;
}

void* atomic_increment_func(void* arg) {
    WynAtomic* atomic = (WynAtomic*)arg;
    
    for (int i = 0; i < 1000; i++) {
        wyn_atomic_fetch_add(atomic, 1);
    }
    
    return NULL;
}

void test_basic_threading() {
    printf("Testing basic threading...\n");
    
    int input = 42;
    WynThread* thread = wyn_thread_spawn(simple_thread_func, &input);
    assert(thread != NULL);
    assert(wyn_thread_status(thread) == WYN_THREAD_RUNNING);
    
    void* result = wyn_thread_join(thread);
    assert(result != NULL);
    assert(*(int*)result == 84);
    assert(wyn_thread_status(thread) == WYN_THREAD_JOINED);
    
    free(result);
    wyn_thread_free(thread);
    
    printf("✓ Basic threading tests passed\n");
}

void test_detached_threads() {
    printf("Testing detached threads...\n");
    
    int input = 10;
    WynThread* thread = wyn_thread_spawn_detached(simple_thread_func, &input);
    assert(thread != NULL);
    
    // Give thread time to complete
    wyn_thread_sleep_ms(100);
    
    // Cannot join detached thread
    void* result = wyn_thread_join(thread);
    assert(result == NULL);
    
    wyn_thread_free(thread);
    
    printf("✓ Detached thread tests passed\n");
}

void test_mutex_operations() {
    printf("Testing mutex operations...\n");
    
    WynMutex* mutex = wyn_mutex_new();
    assert(mutex != NULL);
    
    // Test basic locking
    WynMutexGuard* guard1 = wyn_mutex_lock(mutex);
    assert(guard1 != NULL);
    
    // Test try_lock (should fail while locked)
    WynMutexGuard* guard2;
    bool try_result = wyn_mutex_try_lock(mutex, &guard2);
    assert(!try_result);
    assert(guard2 == NULL);
    
    // Unlock and try again
    wyn_mutex_guard_unlock(guard1);
    
    try_result = wyn_mutex_try_lock(mutex, &guard2);
    assert(try_result);
    assert(guard2 != NULL);
    
    wyn_mutex_guard_unlock(guard2);
    
    // Test with multiple threads
    WynThread* threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = wyn_thread_spawn(mutex_counter_func, &mutex);
        assert(threads[i] != NULL);
    }
    
    for (int i = 0; i < 4; i++) {
        wyn_thread_join(threads[i]);
        wyn_thread_free(threads[i]);
    }
    
    wyn_mutex_free(mutex);
    
    printf("✓ Mutex operation tests passed\n");
}

void test_rwlock_operations() {
    printf("Testing RwLock operations...\n");
    
    WynRwLock* rwlock = wyn_rwlock_new();
    assert(rwlock != NULL);
    
    // Test read lock
    WynRwLockReadGuard* read_guard1 = wyn_rwlock_read(rwlock);
    assert(read_guard1 != NULL);
    
    // Multiple read locks should work
    WynRwLockReadGuard* read_guard2 = wyn_rwlock_read(rwlock);
    assert(read_guard2 != NULL);
    
    wyn_rwlock_read_guard_unlock(read_guard1);
    wyn_rwlock_read_guard_unlock(read_guard2);
    
    // Test write lock
    WynRwLockWriteGuard* write_guard = wyn_rwlock_write(rwlock);
    assert(write_guard != NULL);
    
    // Try read while write locked (should fail)
    WynRwLockReadGuard* read_guard3;
    bool try_read = wyn_rwlock_try_read(rwlock, &read_guard3);
    assert(!try_read);
    assert(read_guard3 == NULL);
    
    wyn_rwlock_write_guard_unlock(write_guard);
    
    wyn_rwlock_free(rwlock);
    
    printf("✓ RwLock operation tests passed\n");
}

void test_channel_operations() {
    printf("Testing channel operations...\n");
    
    // Test unbounded channel
    WynChannel* channel = wyn_channel_new(0);
    assert(channel != NULL);
    assert(wyn_channel_len(channel) == 0);
    assert(!wyn_channel_is_closed(channel));
    
    // Test send and receive
    int data1 = 42;
    bool send_result = wyn_channel_send(channel, &data1);
    assert(send_result);
    assert(wyn_channel_len(channel) == 1);
    
    void* received_data;
    bool recv_result = wyn_channel_recv(channel, &received_data);
    assert(recv_result);
    assert(*(int*)received_data == 42);
    assert(wyn_channel_len(channel) == 0);
    
    // Test try_recv on empty channel
    recv_result = wyn_channel_try_recv(channel, &received_data);
    assert(!recv_result);
    
    wyn_channel_free(channel);
    
    // Test bounded channel
    WynChannel* bounded_channel = wyn_channel_new(2);
    assert(bounded_channel != NULL);
    assert(wyn_channel_capacity(bounded_channel) == 2);
    
    // Fill the channel
    int data2 = 1, data3 = 2;
    assert(wyn_channel_try_send(bounded_channel, &data2));
    assert(wyn_channel_try_send(bounded_channel, &data3));
    assert(wyn_channel_len(bounded_channel) == 2);
    
    // Should fail to send more
    int data4 = 3;
    assert(!wyn_channel_try_send(bounded_channel, &data4));
    
    wyn_channel_free(bounded_channel);
    
    printf("✓ Channel operation tests passed\n");
}

void test_channel_threading() {
    printf("Testing channel with multiple threads...\n");
    
    WynChannel* channel = wyn_channel_new(10);
    assert(channel != NULL);
    
    // Start sender thread
    WynThread* sender = wyn_thread_spawn(channel_sender_func, channel);
    assert(sender != NULL);
    
    // Receive messages
    for (int i = 0; i < 5; i++) {
        void* data;
        bool received = wyn_channel_recv(channel, &data);
        assert(received);
        assert(*(int*)data == i);
        free(data);
    }
    
    wyn_thread_join(sender);
    wyn_thread_free(sender);
    wyn_channel_free(channel);
    
    printf("✓ Channel threading tests passed\n");
}

void test_atomic_operations() {
    printf("Testing atomic operations...\n");
    
    WynAtomic* atomic = wyn_atomic_new(0);
    assert(atomic != NULL);
    assert(wyn_atomic_load(atomic) == 0);
    
    // Test store and load
    wyn_atomic_store(atomic, 42);
    assert(wyn_atomic_load(atomic) == 42);
    
    // Test fetch_add
    int64_t old_value = wyn_atomic_fetch_add(atomic, 10);
    assert(old_value == 42);
    assert(wyn_atomic_load(atomic) == 52);
    
    // Test fetch_sub
    old_value = wyn_atomic_fetch_sub(atomic, 5);
    assert(old_value == 52);
    assert(wyn_atomic_load(atomic) == 47);
    
    // Test compare_exchange
    int64_t expected = 47;
    bool exchanged = wyn_atomic_compare_exchange(atomic, &expected, 100);
    assert(exchanged);
    assert(wyn_atomic_load(atomic) == 100);
    
    // Test failed compare_exchange
    expected = 50;  // Wrong expected value
    exchanged = wyn_atomic_compare_exchange(atomic, &expected, 200);
    assert(!exchanged);
    assert(expected == 100);  // Should be updated with actual value
    assert(wyn_atomic_load(atomic) == 100);
    
    // Test exchange
    old_value = wyn_atomic_exchange(atomic, 300);
    assert(old_value == 100);
    assert(wyn_atomic_load(atomic) == 300);
    
    wyn_atomic_free(atomic);
    
    printf("✓ Atomic operation tests passed\n");
}

void test_atomic_threading() {
    printf("Testing atomic operations with multiple threads...\n");
    
    WynAtomic* atomic = wyn_atomic_new(0);
    assert(atomic != NULL);
    
    // Start multiple threads incrementing the atomic
    WynThread* threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = wyn_thread_spawn(atomic_increment_func, atomic);
        assert(threads[i] != NULL);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < 4; i++) {
        wyn_thread_join(threads[i]);
        wyn_thread_free(threads[i]);
    }
    
    // Should have incremented 4000 times total
    assert(wyn_atomic_load(atomic) == 4000);
    
    wyn_atomic_free(atomic);
    
    printf("✓ Atomic threading tests passed\n");
}

void test_threadsafe_collections() {
    printf("Testing thread-safe collections...\n");
    
    WynThreadSafeVec* vec = wyn_threadsafe_vec_new();
    assert(vec != NULL);
    assert(wyn_threadsafe_vec_len(vec) == 0);
    
    // Test push and get
    int data1 = 42, data2 = 84;
    assert(wyn_threadsafe_vec_push(vec, &data1));
    assert(wyn_threadsafe_vec_push(vec, &data2));
    assert(wyn_threadsafe_vec_len(vec) == 2);
    
    void* retrieved;
    assert(wyn_threadsafe_vec_get(vec, 0, &retrieved));
    assert(*(int*)retrieved == 42);
    
    assert(wyn_threadsafe_vec_get(vec, 1, &retrieved));
    assert(*(int*)retrieved == 84);
    
    // Test bounds checking
    assert(!wyn_threadsafe_vec_get(vec, 2, &retrieved));
    
    // Test pop
    assert(wyn_threadsafe_vec_pop(vec, &retrieved));
    assert(*(int*)retrieved == 84);
    assert(wyn_threadsafe_vec_len(vec) == 1);
    
    assert(wyn_threadsafe_vec_pop(vec, &retrieved));
    assert(*(int*)retrieved == 42);
    assert(wyn_threadsafe_vec_len(vec) == 0);
    
    // Test pop on empty
    assert(!wyn_threadsafe_vec_pop(vec, &retrieved));
    
    wyn_threadsafe_vec_free(vec);
    
    printf("✓ Thread-safe collection tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test hardware concurrency
    size_t concurrency = wyn_thread_hardware_concurrency();
    assert(concurrency >= 1);
    printf("  Hardware concurrency: %zu\n", concurrency);
    
    // Test current thread ID
    uint64_t thread_id = wyn_thread_current_id();
    assert(thread_id != 0);
    printf("  Current thread ID: %llu\n", (unsigned long long)thread_id);
    
    // Test sleep
    struct timeval start, end;
    gettimeofday(&start, NULL);
    wyn_thread_sleep_ms(100);
    gettimeofday(&end, NULL);
    
    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    assert(elapsed_ms >= 90 && elapsed_ms <= 200);  // Allow some variance
    
    printf("✓ Utility function tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    assert(wyn_thread_spawn(NULL, NULL) == NULL);
    assert(wyn_thread_join(NULL) == NULL);
    assert(wyn_mutex_new() != NULL);
    wyn_mutex_free(NULL);  // Should not crash
    
    // Test channel closure
    WynChannel* channel = wyn_channel_new(1);
    int data = 42;
    wyn_channel_send(channel, &data);
    wyn_channel_close(channel);
    assert(wyn_channel_is_closed(channel));
    
    // Should not be able to send to closed channel
    assert(!wyn_channel_send(channel, &data));
    
    // Should still be able to receive existing data
    void* received;
    assert(wyn_channel_recv(channel, &received));
    assert(*(int*)received == 42);
    
    // No more data available
    assert(!wyn_channel_recv(channel, &received));
    
    wyn_channel_free(channel);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Threading System Tests\n");
    printf("==============================\n\n");
    
    test_basic_threading();
    test_detached_threads();
    test_mutex_operations();
    test_rwlock_operations();
    test_channel_operations();
    test_channel_threading();
    test_atomic_operations();
    test_atomic_threading();
    test_threadsafe_collections();
    test_utility_functions();
    test_edge_cases();
    
    printf("\n✅ All threading system tests passed!\n");
    return 0;
}
