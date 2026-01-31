#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

void* compute(void* arg) {
    int n = *(int*)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    return result;
}

int main() {
    struct timespec start, end;
    int count = 10000000;
    
    printf("Spawning 10M tasks...\n");
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
        free(r);
        future_free(futures[i]);
        if (i % 1000000 == 0) printf("Awaited %dM\n", i/1000000);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + 
                      (end.tv_nsec - start.tv_nsec);
    
    free(futures);
    free(args);
    
    printf("10M: %ld ms\n", elapsed_ns / 1000000);
    return 0;
}
