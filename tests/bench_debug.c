#include <stdio.h>
#include <stdlib.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

void* compute(void* arg) {
    int* result = malloc(sizeof(int));
    *result = 42;
    return result;
}

int main() {
    printf("Spawning 1000 tasks...\n");
    Future** futures = malloc(sizeof(Future*) * 1000);
    int arg = 0;
    
    for (int i = 0; i < 1000; i++) {
        futures[i] = wyn_spawn_async(compute, &arg);
        if (i % 100 == 0) printf("Spawned %d\n", i);
    }
    
    printf("Awaiting...\n");
    for (int i = 0; i < 1000; i++) {
        int* r = (int*)future_get(futures[i]);
        free(r);
        future_free(futures[i]);
        if (i % 100 == 0) printf("Awaited %d\n", i);
    }
    
    free(futures);
    printf("Done\n");
    return 0;
}
