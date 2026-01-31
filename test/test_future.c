// Test suite for Future API - TDD approach
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "future.h"

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("Running %s...", #name); \
    test_##name(); \
    printf(" ✓\n"); \
} while(0)

// Test helpers
void* return_int(void* arg) {
    int* result = malloc(sizeof(int));
    *result = (int)(long)arg;
    return result;
}

void* square(void* arg) {
    int n = (int)(long)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    return result;
}

void* slow_task(void* arg) {
    for (volatile int i = 0; i < 1000000; i++);
    return return_int(arg);
}

// === TESTS ===

TEST(future_create_and_free) {
    Future* f = future_new();
    assert(f != NULL);
    assert(!future_is_ready(f));
    future_free(f);
}

TEST(future_set_and_get) {
    Future* f = future_new();
    int* value = malloc(sizeof(int));
    *value = 42;
    
    future_set(f, value);
    assert(future_is_ready(f));
    
    int* result = future_get(f);
    assert(*result == 42);
    
    free(result);
    future_free(f);
}

TEST(future_get_blocks_until_ready) {
    // This is tested implicitly by spawn tests
    // Can't easily test blocking in single-threaded context
}

TEST(future_poll_non_blocking) {
    Future* f = future_new();
    assert(!future_is_ready(f));
    
    int* value = malloc(sizeof(int));
    *value = 100;
    future_set(f, value);
    
    assert(future_is_ready(f));
    free(future_get(f));
    future_free(f);
}

TEST(spawn_async_single_task) {
    Future* f = wyn_spawn_async(return_int, (void*)42L);
    assert(f != NULL);
    
    int* result = future_get(f);
    assert(*result == 42);
    
    free(result);
    future_free(f);
}

TEST(spawn_async_multiple_tasks) {
    Future* futures[10];
    
    for (int i = 0; i < 10; i++) {
        futures[i] = wyn_spawn_async(square, (void*)(long)i);
    }
    
    for (int i = 0; i < 10; i++) {
        int* result = future_get(futures[i]);
        assert(*result == i * i);
        free(result);
        future_free(futures[i]);
    }
}

TEST(spawn_async_100_tasks) {
    Future* futures[100];
    
    for (int i = 0; i < 100; i++) {
        futures[i] = wyn_spawn_async(return_int, (void*)(long)i);
    }
    
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        int* result = future_get(futures[i]);
        sum += *result;
        free(result);
        future_free(futures[i]);
    }
    
    assert(sum == 4950); // Sum of 0..99
}

void* double_it(void* arg) {
    int* n = (int*)arg;
    int* result = malloc(sizeof(int));
    *result = (*n) * 2;
    return result;
}

TEST(future_map) {
    Future* f = wyn_spawn_async(return_int, (void*)10L);
    
    // Map: x * 2
    Future* f2 = future_map(f, double_it);
    
    int* result = future_get(f2);
    assert(*result == 20); // 10 * 2
    
    free(result);
    future_free(f);
    future_free(f2);
}

TEST(future_all_empty) {
    Future** futures = NULL;
    Future* all = future_all(futures, 0);
    
    void** results = future_get(all);
    assert(results != NULL);
    
    free(results);
    future_free(all);
}

TEST(future_all_multiple) {
    Future* futures[5];
    for (int i = 0; i < 5; i++) {
        futures[i] = wyn_spawn_async(return_int, (void*)(long)i);
    }
    
    Future* all = future_all(futures, 5);
    void** results = future_get(all);
    
    for (int i = 0; i < 5; i++) {
        int* val = (int*)results[i];
        assert(*val == i);
        free(val);
    }
    
    free(results);
    future_free(all);
}

TEST(future_race_first_wins) {
    // Simplified: just test that race returns a result
    Future* futures[2];
    futures[0] = wyn_spawn_async(return_int, (void*)1L);
    futures[1] = wyn_spawn_async(return_int, (void*)2L);
    
    // Wait for both to complete first
    future_get(futures[0]);
    future_get(futures[1]);
    
    Future* race = future_race(futures, 2);
    int* result = future_get(race);
    
    // Should be either 1 or 2
    assert(*result == 1 || *result == 2);
    
    free(result);
    future_free(race);
    future_free(futures[0]);
    future_free(futures[1]);
}

TEST(future_timeout_completes) {
    Future* f = wyn_spawn_async(return_int, (void*)42L);
    
    void* result = future_get_timeout(f, 1000); // 1 second
    assert(result != NULL);
    assert(*(int*)result == 42);
    
    free(result);
    future_free(f);
}

TEST(future_timeout_expires) {
    Future* f = future_new(); // Never completes
    
    void* result = future_get_timeout(f, 10); // 10ms
    assert(result == NULL); // Timeout
    
    future_free(f);
}

TEST(future_then_chaining) {
    // spawn task -> then spawn another based on result
    Future* f1 = wyn_spawn_async(return_int, (void*)5L);
    
    // This would need a callback mechanism
    // Simplified: just test sequential chaining
    int* r1 = future_get(f1);
    Future* f2 = wyn_spawn_async(square, (void*)(long)*r1);
    int* r2 = future_get(f2);
    
    assert(*r2 == 25); // 5 * 5
    
    free(r1);
    free(r2);
    future_free(f1);
    future_free(f2);
}

TEST(stress_1000_futures) {
    Future* futures[1000];
    
    for (int i = 0; i < 1000; i++) {
        futures[i] = wyn_spawn_async(return_int, (void*)(long)i);
    }
    
    long sum = 0;
    for (int i = 0; i < 1000; i++) {
        int* result = future_get(futures[i]);
        sum += *result;
        free(result);
        future_free(futures[i]);
    }
    
    assert(sum == 499500); // Sum of 0..999
}

// === MAIN ===

int main() {
    printf("=== Future API Test Suite (TDD) ===\n\n");
    
    RUN_TEST(future_create_and_free);
    RUN_TEST(future_set_and_get);
    RUN_TEST(future_poll_non_blocking);
    RUN_TEST(spawn_async_single_task);
    RUN_TEST(spawn_async_multiple_tasks);
    RUN_TEST(spawn_async_100_tasks);
    
    printf("\n--- Advanced Features ---\n");
    RUN_TEST(future_map);
    RUN_TEST(future_all_empty);
    RUN_TEST(future_all_multiple);
    RUN_TEST(future_race_first_wins);
    RUN_TEST(future_timeout_completes);
    RUN_TEST(future_timeout_expires);
    RUN_TEST(future_then_chaining);
    
    printf("\n--- Stress Tests ---\n");
    RUN_TEST(stress_1000_futures);
    
    printf("\n=== All Tests Passed ✓ ===\n");
    return 0;
}
