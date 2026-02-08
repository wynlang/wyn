#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "future.h"

Future* wyn_spawn_async(void* (*func)(void*), void* arg);

void* compute(void* arg) {
    int n = *(int*)arg;
    int* result = malloc(sizeof(int));
    *result = n * n;
    return result;
}

int main() {
    printf("Test 1: 10 futures\n");
    Future* f[10];
    int args[10];
    for (int i = 0; i < 10; i++) {
        args[i] = i;
        f[i] = wyn_spawn_async(compute, &args[i]);
    }
    for (int i = 0; i < 10; i++) {
        int* r = (int*)future_get(f[i]);
        assert(*r == i * i);
        free(r);
        future_free(f[i]);
    }
    printf("✅ PASS\n");
    
    printf("Test 2: 100 futures\n");
    Future* f2[100];
    int args2[100];
    for (int i = 0; i < 100; i++) {
        args2[i] = i;
        f2[i] = wyn_spawn_async(compute, &args2[i]);
    }
    for (int i = 0; i < 100; i++) {
        int* r = (int*)future_get(f2[i]);
        assert(*r == i * i);
        free(r);
        future_free(f2[i]);
    }
    printf("✅ PASS\n");
    
    printf("Test 3: 1000 futures\n");
    Future** f3 = malloc(sizeof(Future*) * 1000);
    int* args3 = malloc(sizeof(int) * 1000);
    for (int i = 0; i < 1000; i++) {
        args3[i] = i;
        f3[i] = wyn_spawn_async(compute, &args3[i]);
    }
    for (int i = 0; i < 1000; i++) {
        int* r = (int*)future_get(f3[i]);
        assert(*r == i * i);
        free(r);
        future_free(f3[i]);
    }
    free(f3);
    free(args3);
    printf("✅ PASS\n");
    
    printf("All tests passed!\n");
    return 0;
}
