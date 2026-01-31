#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

// Pre-allocated results
static int* results;

void* compute(void* arg) {
    int idx = *(int*)arg;
    results[idx] = idx * idx;
    return &results[idx];
}

int main() {
    struct timespec start, end;
    int count = 10000000;
    
    results = malloc(sizeof(int) * count);
    
    printf("Spawning 10M tasks (no malloc)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    Future** futures = malloc(sizeof(Future*) * count);
    int* args = malloc(sizeof(int) * count);
    
    for (int i = 0; i < count; i++) {
        args[i] = i;
        futures[i] = wyn_spawn_async(compute, &args[i]);
        if (i % 1000000 == 0) printf("Spawned %dM\n", i/1000000);
    }
    
    printf("Awaiting...\n");
    for (int i = 0; i < count; i++) {
        int* r = (int*)future_get(futures[i]);
        future_free(futures[i]);
        if (i % 1000000 == 0) printf("Awaited %dM\n", i/1000000);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + 
                      (end.tv_nsec - start.tv_nsec);
    
    free(futures);
    free(args);
    free(results);
    
    printf("10M (no malloc): %ld ms\n", elapsed_ns / 1000000);
    return 0;
}
