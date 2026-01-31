#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "future.h"

// Forward declaration
Future* wyn_spawn_async(void* (*func)(void*), void* arg);

int compute(int n) {
    return n * n;
}

void* compute_wrapper(void* arg) {
    int n = *(int*)arg;
    int* result = malloc(sizeof(int));
    *result = compute(n);
    return result;
}

int main() {
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Spawn 10 futures
    int args[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Future* futures[10];
    
    for (int i = 0; i < 10; i++) {
        futures[i] = wyn_spawn_async(compute_wrapper, &args[i]);
    }
    
    // Await all
    for (int i = 0; i < 10; i++) {
        int* result = (int*)future_get(futures[i]);
        free(result);
        future_free(futures[i]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + 
                      (end.tv_nsec - start.tv_nsec);
    
    printf("Total time: %ld ns\n", elapsed_ns);
    printf("Per operation: %ld ns\n", elapsed_ns / 10);
    
    return 0;
}
