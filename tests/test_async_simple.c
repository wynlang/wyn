#include "../src/async.h"
#include <stdio.h>
#include <assert.h>

// Simple test helper
static void test_wake_func(void* data) {
    bool* called = (bool*)data;
    *called = true;
}

int main() {
    printf("Running Basic Async System Tests\n");
    printf("===============================\n\n");
    
    // Test 1: Waker operations
    printf("Testing waker operations...\n");
    bool waker_called = false;
    WynWaker* waker = wyn_waker_new(test_wake_func, &waker_called);
    assert(waker != NULL);
    
    wyn_waker_wake(waker);
    assert(waker_called);
    wyn_waker_free(waker);
    printf("✓ Waker tests passed\n");
    
    // Test 2: Ready future
    printf("Testing ready future...\n");
    WynFuture* ready_future = wyn_future_ready((void*)42);
    assert(ready_future != NULL);
    assert(wyn_future_is_ready(ready_future));
    wyn_future_free(ready_future);
    printf("✓ Ready future tests passed\n");
    
    // Test 3: Runtime creation
    printf("Testing runtime creation...\n");
    WynRuntime* runtime = wyn_runtime_new();
    assert(runtime != NULL);
    wyn_runtime_free(runtime);
    printf("✓ Runtime creation tests passed\n");
    
    // Test 4: Delay future (short delay)
    printf("Testing delay future...\n");
    WynFuture* delay_future = wyn_future_delay(10);  // 10ms
    assert(delay_future != NULL);
    
    runtime = wyn_runtime_new();
    wyn_runtime_block_on(runtime, delay_future);
    
    wyn_future_free(delay_future);
    wyn_runtime_free(runtime);
    printf("✓ Delay future tests passed\n");
    
    printf("\n✅ All basic async tests passed!\n");
    return 0;
}
