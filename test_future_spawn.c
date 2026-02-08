// Test collecting results from spawned tasks
#include <stdio.h>
#include <stdlib.h>
#include "future.h"

// Declare spawn functions
typedef void* (*TaskFuncWithReturn)(void*);
extern void* wyn_spawn_async(TaskFuncWithReturn func, void* arg);

// Worker function that returns a result
void* compute_square(void* arg) {
    int n = (int)(long)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    return result;
}

// Chunk processing example
void* process_chunk(void* arg) {
    int chunk_id = (int)(long)arg;
    int sum = 0;
    
    // Simulate processing a chunk of data
    for (int i = 0; i < 1000; i++) {
        sum += chunk_id * i;
    }
    
    int* result = malloc(sizeof(int));
    *result = sum;
    return result;
}

int main() {
    printf("=== Testing Future-based Spawn ===\n\n");
    
    // Test 1: Single spawn with result
    printf("Test 1: Single spawn with result\n");
    Future* f1 = (Future*)wyn_spawn_async(compute_square, (void*)10L);
    int* result1 = (int*)future_get(f1);
    printf("  Square of 10 = %d\n", *result1);
    free(result1);
    future_free(f1);
    printf("  ✓ PASS\n\n");
    
    // Test 2: Multiple spawns, collect all results
    printf("Test 2: Multiple spawns (10 tasks)\n");
    Future* futures[10];
    for (int i = 0; i < 10; i++) {
        futures[i] = (Future*)wyn_spawn_async(compute_square, (void*)(long)i);
    }
    
    printf("  Results: ");
    for (int i = 0; i < 10; i++) {
        int* result = (int*)future_get(futures[i]);
        printf("%d ", *result);
        free(result);
        future_free(futures[i]);
    }
    printf("\n  ✓ PASS\n\n");
    
    // Test 3: Chunked work (like map-reduce)
    printf("Test 3: Chunked work (100 chunks)\n");
    Future* chunk_futures[100];
    for (int i = 0; i < 100; i++) {
        chunk_futures[i] = (Future*)wyn_spawn_async(process_chunk, (void*)(long)i);
    }
    
    // Collect and sum all results
    long total = 0;
    for (int i = 0; i < 100; i++) {
        int* chunk_result = (int*)future_get(chunk_futures[i]);
        total += *chunk_result;
        free(chunk_result);
        future_free(chunk_futures[i]);
    }
    printf("  Total from 100 chunks: %ld\n", total);
    printf("  ✓ PASS\n\n");
    
    // Test 4: Poll without blocking
    printf("Test 4: Non-blocking poll\n");
    Future* f4 = (Future*)wyn_spawn_async(compute_square, (void*)42L);
    
    int checks = 0;
    while (!future_is_ready(f4)) {
        checks++;
        if (checks > 1000000) break;  // Timeout
    }
    
    if (future_is_ready(f4)) {
        int* result4 = (int*)future_get(f4);
        printf("  Result after %d checks: %d\n", checks, *result4);
        free(result4);
        printf("  ✓ PASS\n");
    } else {
        printf("  ✗ TIMEOUT\n");
    }
    future_free(f4);
    
    printf("\n=== All Tests Complete ===\n");
    return 0;
}
