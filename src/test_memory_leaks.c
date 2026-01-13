#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_runtime.h"

// Memory leak detection test
void test_memory_leaks() {
    printf("=== Memory Leak Detection Test ===\n");
    
    // Reset statistics
    wyn_arc_reset_stats();
    
    // Test 1: Basic allocation and deallocation
    printf("Test 1: Basic allocation/deallocation\n");
    WynObject* obj1 = wyn_arc_alloc(100, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(200, WYN_TYPE_ARRAY, NULL);
    
    wyn_arc_print_stats();
    
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("After cleanup:\n");
    wyn_arc_print_stats();
    
    // Test 2: Retain/Release cycles
    printf("\nTest 2: Retain/Release cycles\n");
    WynObject* obj3 = wyn_arc_alloc(50, WYN_TYPE_INT, NULL);
    
    // Multiple retains
    wyn_arc_retain(obj3);
    wyn_arc_retain(obj3);
    wyn_arc_retain(obj3);
    
    printf("After 3 retains (ref count should be 4):\n");
    printf("Reference count: %u\n", wyn_arc_get_ref_count(obj3));
    
    // Release all
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    
    printf("After all releases:\n");
    wyn_arc_print_stats();
    
    // Test 3: Stress test
    printf("\nTest 3: Stress test (1000 allocations)\n");
    for (int i = 0; i < 1000; i++) {
        WynObject* temp = wyn_arc_alloc(i + 1, WYN_TYPE_STRING, NULL);
        if (i % 2 == 0) {
            wyn_arc_retain(temp);
            wyn_arc_release(temp);
        }
        wyn_arc_release(temp);
    }
    
    printf("After stress test:\n");
    wyn_arc_print_stats();
    
    printf("=== Memory Leak Test Complete ===\n");
}

int main() {
    test_memory_leaks();
    return 0;
}