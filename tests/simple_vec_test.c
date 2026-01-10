#include <stdio.h>
#include <stdlib.h>
#include "../src/collections.h"

int main() {
    printf("Simple Vec test starting...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    if (!vec) {
        printf("Failed to create vector\n");
        return 1;
    }
    
    printf("Vector created successfully\n");
    
    int value = 42;
    if (!wyn_vec_push(vec, &value)) {
        printf("Failed to push value\n");
        wyn_vec_free(vec);
        return 1;
    }
    
    printf("Value pushed successfully\n");
    printf("Vector length: %zu\n", wyn_vec_len(vec));
    
    int retrieved;
    if (wyn_vec_get(vec, 0, &retrieved)) {
        printf("Retrieved value: %d\n", retrieved);
    } else {
        printf("Failed to retrieve value\n");
    }
    
    wyn_vec_free(vec);
    printf("Test completed successfully\n");
    
    return 0;
}
