#include <stdio.h>
#include <stdlib.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

void* compute(void* arg) {
    int n = *(int*)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    printf("Computed %d\n", *result);
    return result;
}

int main() {
    int arg = 5;
    printf("Spawning...\n");
    Future* f = wyn_spawn_async(compute, &arg);
    printf("Awaiting...\n");
    int* r = (int*)future_get(f);
    printf("Result: %d\n", *r);
    free(r);
    future_free(f);
    return 0;
}
