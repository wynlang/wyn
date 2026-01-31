#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

static int* results;

void* compute(void* arg) {
    int idx = *(int*)arg;
    results[idx] = idx * idx;
    return &results[idx];
}

void benchmark(int count) {
    struct timespec start, end;
    
    results = malloc(sizeof(int) * count);
    Future** futures = malloc(sizeof(Future*) * count);
    int* args = malloc(sizeof(int) * count);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < count; i++) {
        args[i] = i;
        futures[i] = wyn_spawn_async(compute, &args[i]);
    }
    
    for (int i = 0; i < count; i++) {
        future_get(futures[i]);
        future_free(futures[i]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000L + 
                      (end.tv_nsec - start.tv_nsec) / 1000000L;
    
    printf("Wyn %d: %ld ms\n", count, elapsed_ms);
    
    free(results);
    free(futures);
    free(args);
}

int main() {
    benchmark(10000);
    benchmark(100000);
    benchmark(1000000);
    benchmark(10000000);
    return 0;
}
