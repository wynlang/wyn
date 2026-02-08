#include <stdio.h>
#include <stdlib.h>
#include "future.h"

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
    printf("Spawning 1000 futures...\n");
    
    Future* futures[1000];
    int args[1000];
    
    for (int i = 0; i < 1000; i++) {
        args[i] = i;
        futures[i] = wyn_spawn_async(compute_wrapper, &args[i]);
    }
    
    printf("Awaiting all...\n");
    
    int errors = 0;
    for (int i = 0; i < 1000; i++) {
        int* result = (int*)future_get(futures[i]);
        if (*result != i * i) {
            errors++;
        }
        free(result);
        future_free(futures[i]);
    }
    
    printf("Completed: %d errors\n", errors);
    return errors;
}
