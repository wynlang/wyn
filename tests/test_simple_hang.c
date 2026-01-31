#include <stdio.h>
#include <stdlib.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

void* simple_task(void* arg) {
    int* result = malloc(sizeof(int));
    *result = 42;
    return result;
}

int main() {
    printf("Test 1: Single future\n");
    Future* f = wyn_spawn_async(simple_task, NULL);
    int* r = (int*)future_get(f);
    printf("Result: %d\n", *r);
    free(r);
    future_free(f);
    
    printf("Test 2: Three futures\n");
    Future* f1 = wyn_spawn_async(simple_task, NULL);
    Future* f2 = wyn_spawn_async(simple_task, NULL);
    Future* f3 = wyn_spawn_async(simple_task, NULL);
    
    int* r1 = (int*)future_get(f1);
    int* r2 = (int*)future_get(f2);
    int* r3 = (int*)future_get(f3);
    
    printf("Results: %d %d %d\n", *r1, *r2, *r3);
    
    free(r1); free(r2); free(r3);
    future_free(f1); future_free(f2); future_free(f3);
    
    printf("Success\n");
    return 0;
}
