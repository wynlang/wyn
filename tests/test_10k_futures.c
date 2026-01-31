#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
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
    
    printf("Test: 10,000 futures\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    Future** futures = malloc(sizeof(Future*) * 10000);
    int* args = malloc(sizeof(int) * 10000);
    
    for (int i = 0; i < 10000; i++) {
        args[i] = i;
        futures[i] = wyn_spawn_async(compute, &args[i]);
    }
    
    int errors = 0;
    for (int i = 0; i < 10000; i++) {
        int* r = (int*)future_get(futures[i]);
        if (*r != i * i) errors++;
        free(r);
        future_free(futures[i]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + 
                      (end.tv_nsec - start.tv_nsec);
    
    free(futures);
    free(args);
    
    printf("Completed: %d errors\n", errors);
    printf("Time: %.2f ms\n", elapsed_ns / 1000000.0);
    printf("Per operation: %.2f μs\n", elapsed_ns / 10000.0 / 1000.0);
    
    if (errors == 0) {
        printf("✅ PASS\n");
        return 0;
    } else {
        printf("❌ FAIL\n");
        return 1;
    }
}
