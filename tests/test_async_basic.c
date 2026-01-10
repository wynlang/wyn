#include "../src/async.h"
#include <stdio.h>

int main() {
    printf("Testing async system initialization...\n");
    
    // Test basic waker
    printf("Creating waker...\n");
    bool called = false;
    WynWaker* waker = wyn_waker_new(NULL, &called);
    if (waker) {
        printf("Waker created successfully\n");
        wyn_waker_free(waker);
    } else {
        printf("Failed to create waker\n");
    }
    
    // Test runtime
    printf("Creating runtime...\n");
    WynRuntime* runtime = wyn_runtime_new();
    if (runtime) {
        printf("Runtime created successfully\n");
        wyn_runtime_free(runtime);
    } else {
        printf("Failed to create runtime\n");
    }
    
    printf("Basic async test completed\n");
    return 0;
}
