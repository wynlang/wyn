#include "test.h"
#include "stdlib_advanced.h"
#include <stdio.h>
#include <string.h>

static int test_vector_operations() {
    WynVector* vec = wyn_vector_new(sizeof(int));
    
    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        wyn_vector_push(vec, &values[i]);
    }
    
    int* retrieved = (int*)wyn_vector_get(vec, 2);
    if (!retrieved || *retrieved != 3) {
        wyn_vector_free(vec);
        return 0;
    }
    
    wyn_vector_free(vec);
    return 1;
}

static int test_async_io() {
    WynAsyncIO* io = wyn_async_io_new("/tmp/test_wyn.txt", "w");
    if (!io) return 0;
    
    const char* test_data = "Hello, Wyn!";
    size_t written = wyn_async_write(io, test_data, strlen(test_data));
    
    wyn_async_io_free(io);
    
    return written == strlen(test_data) ? 1 : 0;
}

static int test_stdlib_stats() {
    wyn_stdlib_reset_stats();
    wyn_stdlib_print_stats();
    return 1;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Advanced Standard Library Tests ===\n");
    
    total++; if (test_vector_operations()) { printf("✓ Vector operations\n"); passed++; } else printf("✗ Vector operations\n");
    total++; if (test_async_io()) { printf("✓ Async I/O\n"); passed++; } else printf("✗ Async I/O\n");
    total++; if (test_stdlib_stats()) { printf("✓ Statistics\n"); passed++; } else printf("✗ Statistics\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All advanced standard library tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
