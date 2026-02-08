// Practical Future Examples - Working Demo
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "future.h"

void* compute(void* arg) {
    int n = (int)(long)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    return result;
}

void* process_chunk(void* arg) {
    int chunk_id = (int)(long)arg;
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += chunk_id * i;
    }
    int* result = malloc(sizeof(int));
    *result = sum;
    return result;
}

int main() {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║         Future API - Practical Examples                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    // Example 1: Single async task
    printf("Example 1: Single async task\n");
    Future* f1 = wyn_spawn_async(compute, (void*)10L);
    int* r1 = future_get(f1);
    printf("  compute(10) = %d\n", *r1);
    free(r1);
    future_free(f1);
    printf("  ✓ Complete\n\n");
    
    // Example 2: Multiple tasks
    printf("Example 2: Parallel computation (10 tasks)\n");
    Future* futures[10];
    for (int i = 0; i < 10; i++) {
        futures[i] = wyn_spawn_async(compute, (void*)(long)i);
    }
    
    printf("  Results: ");
    for (int i = 0; i < 10; i++) {
        int* result = future_get(futures[i]);
        printf("%d ", *result);
        free(result);
        future_free(futures[i]);
    }
    printf("\n  ✓ Complete\n\n");
    
    // Example 3: Chunked work (map-reduce pattern)
    printf("Example 3: Chunked work (20 chunks)\n");
    Future* chunks[20];
    for (int i = 0; i < 20; i++) {
        chunks[i] = wyn_spawn_async(process_chunk, (void*)(long)i);
    }
    
    long total = 0;
    for (int i = 0; i < 20; i++) {
        int* chunk_result = future_get(chunks[i]);
        total += *chunk_result;
        free(chunk_result);
        future_free(chunks[i]);
    }
    printf("  Total from 20 chunks: %ld\n", total);
    printf("  ✓ Complete\n\n");
    
    // Example 4: Future.all()
    printf("Example 4: Future.all() combinator\n");
    Future* batch[5];
    for (int i = 0; i < 5; i++) {
        batch[i] = wyn_spawn_async(compute, (void*)(long)(i + 1));
    }
    
    Future* all = future_all(batch, 5);
    void** results = future_get(all);
    
    printf("  Results: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", *(int*)results[i]);
        free(results[i]);
        future_free(batch[i]);
    }
    printf("\n  ✓ Complete\n\n");
    free(results);
    future_free(all);
    
    // Example 5: Timeout
    printf("Example 5: Timeout handling\n");
    Future* f5 = wyn_spawn_async(compute, (void*)100L);
    void* r5 = future_get_timeout(f5, 1000);
    if (r5) {
        printf("  Result: %d (completed in time)\n", *(int*)r5);
        free(r5);
    } else {
        printf("  Timeout!\n");
    }
    future_free(f5);
    printf("  ✓ Complete\n\n");
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              ✅ All Examples Complete ✅                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
