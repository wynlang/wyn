#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

// Simplified ARC test without full dependencies

#define ARC_MAGIC 0x41524330
#define ARC_WEAK_FLAG 0x80000000
#define ARC_COUNT_MASK 0x7FFFFFFF

typedef enum {
    WYN_TYPE_UNKNOWN = 0,
    WYN_TYPE_INT = 1,
    WYN_TYPE_FLOAT = 2,
    WYN_TYPE_BOOL = 3,
    WYN_TYPE_STRING = 4,
    WYN_TYPE_ARRAY = 5,
    WYN_TYPE_STRUCT = 6,
} WynTypeId;

typedef struct WynObjectHeader {
    uint32_t magic;
    _Atomic uint32_t ref_count;
    uint32_t type_id;
    uint32_t size;
    void (*destructor)(void*);
} WynObjectHeader;

typedef struct WynObject {
    WynObjectHeader header;
    uint8_t data[];
} WynObject;

// Global statistics
static _Atomic size_t g_total_allocations = 0;
static _Atomic size_t g_total_deallocations = 0;
static _Atomic size_t g_current_objects = 0;

// Simple implementations
WynObject* wyn_arc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    if (size == 0) return NULL;
    
    size_t total_size = sizeof(WynObjectHeader) + size;
    WynObject* obj = (WynObject*)malloc(total_size);
    if (!obj) return NULL;
    
    obj->header.magic = ARC_MAGIC;
    atomic_store(&obj->header.ref_count, 1);
    obj->header.type_id = type_id;
    obj->header.size = (uint32_t)size;
    obj->header.destructor = destructor;
    
    memset(obj->data, 0, size);
    
    atomic_fetch_add(&g_total_allocations, 1);
    atomic_fetch_add(&g_current_objects, 1);
    
    return obj;
}

bool wyn_arc_is_valid(WynObject* obj) {
    if (!obj) return false;
    return obj->header.magic == ARC_MAGIC;
}

WynObject* wyn_arc_retain(WynObject* obj) {
    if (!obj || !wyn_arc_is_valid(obj)) return NULL;
    
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    do {
        if ((old_count & ARC_COUNT_MASK) == ARC_COUNT_MASK) {
            printf("ERROR: Reference count overflow\n");
            return obj;
        }
        
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | 
                           ((old_count & ARC_COUNT_MASK) + 1);
        
        if (atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
            break;
        }
    } while (true);
    
    return obj;
}

void wyn_arc_release(WynObject* obj) {
    if (!obj || !wyn_arc_is_valid(obj)) return;
    
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    do {
        uint32_t count_part = old_count & ARC_COUNT_MASK;
        if (count_part == 0) {
            printf("ERROR: Double release detected\n");
            return;
        }
        
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | (count_part - 1);
        
        if (atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
            if ((new_count & ARC_COUNT_MASK) == 0) {
                // Deallocate
                if (obj->header.destructor) {
                    obj->header.destructor(obj->data);
                }
                
                atomic_fetch_add(&g_total_deallocations, 1);
                atomic_fetch_sub(&g_current_objects, 1);
                
                obj->header.magic = 0;
                free(obj);
            }
            break;
        }
    } while (true);
}

uint32_t wyn_arc_get_ref_count(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return 0;
    return atomic_load(&obj->header.ref_count) & ARC_COUNT_MASK;
}

void* wyn_arc_get_data(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return NULL;
    return obj->data;
}

void print_stats() {
    printf("Total allocations: %zu\n", atomic_load(&g_total_allocations));
    printf("Total deallocations: %zu\n", atomic_load(&g_total_deallocations));
    printf("Current objects: %zu\n", atomic_load(&g_current_objects));
}

// Test function
void test_arc_basic() {
    printf("=== Basic ARC Test ===\n");
    
    // Test 1: Basic allocation and deallocation
    printf("Test 1: Basic allocation/deallocation\n");
    WynObject* obj1 = wyn_arc_alloc(100, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(200, WYN_TYPE_ARRAY, NULL);
    
    printf("After allocation:\n");
    print_stats();
    
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("After cleanup:\n");
    print_stats();
    
    // Test 2: Retain/Release cycles
    printf("\nTest 2: Retain/Release cycles\n");
    WynObject* obj3 = wyn_arc_alloc(50, WYN_TYPE_INT, NULL);
    
    printf("Initial ref count: %u\n", wyn_arc_get_ref_count(obj3));
    
    wyn_arc_retain(obj3);
    wyn_arc_retain(obj3);
    wyn_arc_retain(obj3);
    
    printf("After 3 retains: %u\n", wyn_arc_get_ref_count(obj3));
    
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    wyn_arc_release(obj3);
    
    printf("After all releases:\n");
    print_stats();
    
    printf("=== ARC Test Complete ===\n");
}

int main() {
    test_arc_basic();
    return 0;
}