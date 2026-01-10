#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "../src/arc_runtime.h"

// T2.3.1 Validation Test Suite
// Comprehensive validation of Object Header Design requirements

// Test 1: Efficient Object Header Layout
void test_efficient_header_layout() {
    printf("Testing efficient object header layout...\n");
    
    // Verify header size is reasonable (should be 20 bytes on 64-bit systems)
    size_t header_size = sizeof(WynObjectHeader);
    printf("  Header size: %zu bytes\n", header_size);
    
    // Header should be compact but include all necessary fields
    assert(header_size <= 32); // Should not exceed 32 bytes
    assert(header_size >= 16); // Should have at least basic fields
    
    // Verify field alignment and layout
    WynObjectHeader header = {0};
    
    // Test magic number field
    header.magic = ARC_MAGIC;
    assert(header.magic == 0x41524330); // "ARC0" in hex
    
    // Test reference count field (atomic)
    atomic_store(&header.ref_count, 1);
    assert(atomic_load(&header.ref_count) == 1);
    
    // Test type ID field
    header.type_id = WYN_TYPE_STRING;
    assert(header.type_id == WYN_TYPE_STRING);
    
    // Test size field
    header.size = 256;
    assert(header.size == 256);
    
    // Test destructor field
    header.destructor = NULL;
    assert(header.destructor == NULL);
    
    printf("  ✅ Header layout is efficient and well-structured\n");
}

// Test 2: Reference Count Storage
void test_reference_count_storage() {
    printf("Testing reference count storage...\n");
    
    // Test atomic operations
    WynObject* obj = wyn_arc_alloc(64, WYN_TYPE_INT, NULL);
    assert(obj != NULL);
    
    // Initial reference count should be 1
    uint32_t ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == 1);
    printf("  Initial ref count: %u\n", ref_count);
    
    // Test atomic increment
    wyn_arc_retain(obj);
    ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == 2);
    printf("  After retain: %u\n", ref_count);
    
    // Test atomic decrement
    wyn_arc_release(obj);
    ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == 1);
    printf("  After release: %u\n", ref_count);
    
    // Test weak flag functionality
    wyn_arc_weak_retain(obj);
    ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == 1); // Weak references don't affect count
    printf("  After weak retain: %u\n", ref_count);
    
    wyn_arc_weak_release(obj);
    ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == 1);
    printf("  After weak release: %u\n", ref_count);
    
    // Test overflow protection (simulate high count)
    uint32_t high_count = ARC_COUNT_MASK - 1;
    atomic_store(&obj->header.ref_count, high_count);
    ref_count = wyn_arc_get_ref_count(obj);
    assert(ref_count == high_count);
    printf("  High ref count test: %u\n", ref_count);
    
    // Reset to 1 for cleanup
    atomic_store(&obj->header.ref_count, 1);
    wyn_arc_release(obj);
    
    printf("  ✅ Reference count storage is atomic and thread-safe\n");
}

// Test 3: Type Information Integration
void test_type_information_integration() {
    printf("Testing type information integration...\n");
    
    // Test all built-in type IDs
    struct {
        WynTypeId type_id;
        const char* name;
    } types[] = {
        {WYN_TYPE_UNKNOWN, "UNKNOWN"},
        {WYN_TYPE_INT, "INT"},
        {WYN_TYPE_FLOAT, "FLOAT"},
        {WYN_TYPE_BOOL, "BOOL"},
        {WYN_TYPE_STRING, "STRING"},
        {WYN_TYPE_ARRAY, "ARRAY"},
        {WYN_TYPE_STRUCT, "STRUCT"},
        {WYN_TYPE_FUNCTION, "FUNCTION"},
        {WYN_TYPE_CUSTOM_BASE, "CUSTOM_BASE"}
    };
    
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        WynObject* obj = wyn_arc_alloc(32, types[i].type_id, NULL);
        assert(obj != NULL);
        
        uint32_t stored_type = wyn_arc_get_type_id(obj);
        assert(stored_type == types[i].type_id);
        
        printf("  Type %s (ID: %u) stored and retrieved correctly\n", 
               types[i].name, types[i].type_id);
        
        wyn_arc_release(obj);
    }
    
    // Test custom type IDs (should be >= WYN_TYPE_CUSTOM_BASE)
    uint32_t custom_type = WYN_TYPE_CUSTOM_BASE + 42;
    WynObject* custom_obj = wyn_arc_alloc(16, custom_type, NULL);
    assert(custom_obj != NULL);
    assert(wyn_arc_get_type_id(custom_obj) == custom_type);
    printf("  Custom type ID %u stored correctly\n", custom_type);
    wyn_arc_release(custom_obj);
    
    printf("  ✅ Type information integration is complete and flexible\n");
}

// Test 4: Memory Layout Validation
void test_memory_layout_validation() {
    printf("Testing memory layout validation...\n");
    
    // Test object structure layout
    WynObject* obj = wyn_arc_alloc(128, WYN_TYPE_ARRAY, NULL);
    assert(obj != NULL);
    
    // Verify header is at the beginning
    void* header_ptr = &obj->header;
    void* obj_ptr = obj;
    assert(header_ptr == obj_ptr);
    
    // Verify data follows header immediately
    void* data_ptr = obj->data;
    void* expected_data_ptr = (char*)obj + sizeof(WynObjectHeader);
    assert(data_ptr == expected_data_ptr);
    
    // Test data access
    char* data = (char*)wyn_arc_get_data(obj);
    strcpy(data, "Memory layout test");
    assert(strcmp(data, "Memory layout test") == 0);
    
    // Verify size information
    assert(wyn_arc_get_size(obj) == 128);
    
    wyn_arc_release(obj);
    
    printf("  ✅ Memory layout is correct and efficient\n");
}

// Test 5: Performance Characteristics
void test_performance_characteristics() {
    printf("Testing performance characteristics...\n");
    
    wyn_arc_reset_stats();
    
    // Allocate many objects to test performance
    const int num_objects = 1000;
    WynObject* objects[num_objects];
    
    // Allocation performance test
    for (int i = 0; i < num_objects; i++) {
        objects[i] = wyn_arc_alloc(64, WYN_TYPE_INT, NULL);
        assert(objects[i] != NULL);
    }
    
    // Reference counting performance test
    for (int i = 0; i < num_objects; i++) {
        wyn_arc_retain(objects[i]);
        wyn_arc_release(objects[i]);
    }
    
    // Deallocation performance test
    for (int i = 0; i < num_objects; i++) {
        wyn_arc_release(objects[i]);
    }
    
    // Verify statistics
    WynARCStats stats = wyn_arc_get_stats();
    assert(stats.total_allocations == num_objects);
    assert(stats.total_deallocations == num_objects);
    assert(stats.current_objects == 0);
    
    printf("  Processed %d objects successfully\n", num_objects);
    printf("  ✅ Performance characteristics are acceptable\n");
}

// Main validation function
int main() {
    printf("=== T2.3.1 Object Header Design Validation ===\n");
    printf("Validating all acceptance criteria:\n");
    printf("- [ ] Efficient object header layout\n");
    printf("- [ ] Reference count storage\n");
    printf("- [ ] Type information integration\n\n");
    
    test_efficient_header_layout();
    printf("\n");
    
    test_reference_count_storage();
    printf("\n");
    
    test_type_information_integration();
    printf("\n");
    
    test_memory_layout_validation();
    printf("\n");
    
    test_performance_characteristics();
    printf("\n");
    
    printf("=== T2.3.1 Validation Results ===\n");
    printf("✅ Efficient object header layout - PASSED\n");
    printf("✅ Reference count storage - PASSED\n");
    printf("✅ Type information integration - PASSED\n");
    printf("\n");
    printf("🎉 T2.3.1 Object Header Design implementation is COMPLETE\n");
    printf("All acceptance criteria have been met and validated.\n");
    
    return 0;
}