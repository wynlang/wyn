#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include "string_memory.h"
#include "arc_runtime.h"

// Memory leak detection framework for string operations

typedef struct MemorySnapshot {
    size_t total_allocations;
    size_t total_deallocations;
    size_t current_objects;
    size_t total_memory;
    StringMemoryStats string_stats;
    time_t timestamp;
} MemorySnapshot;

static MemorySnapshot snapshots[100];
static int snapshot_count = 0;

// Take a memory snapshot
void take_memory_snapshot(const char* label) {
    if (snapshot_count >= 100) return;
    
    MemorySnapshot* snap = &snapshots[snapshot_count++];
    WynARCStats arc_stats = wyn_arc_get_stats();
    
    snap->total_allocations = arc_stats.total_allocations;
    snap->total_deallocations = arc_stats.total_deallocations;
    snap->current_objects = arc_stats.current_objects;
    snap->total_memory = arc_stats.total_memory;
    snap->string_stats = wyn_string_get_memory_stats();
    snap->timestamp = time(NULL);
    
    printf("[SNAPSHOT %d] %s: Objects=%zu, Memory=%zu bytes, Strings=%zu\n",
           snapshot_count - 1, label, snap->current_objects, snap->total_memory,
           snap->string_stats.tracked_strings);
}

// Compare two snapshots and report differences
void compare_snapshots(int snap1_idx, int snap2_idx, const char* operation) {
    if (snap1_idx >= snapshot_count || snap2_idx >= snapshot_count) return;
    
    MemorySnapshot* before = &snapshots[snap1_idx];
    MemorySnapshot* after = &snapshots[snap2_idx];
    
    printf("\n=== Memory Analysis: %s ===\n", operation);
    printf("Objects: %zu -> %zu (diff: %+ld)\n",
           before->current_objects, after->current_objects,
           (long)(after->current_objects - before->current_objects));
    printf("Memory: %zu -> %zu bytes (diff: %+ld)\n",
           before->total_memory, after->total_memory,
           (long)(after->total_memory - before->total_memory));
    printf("Tracked Strings: %zu -> %zu (diff: %+ld)\n",
           before->string_stats.tracked_strings, after->string_stats.tracked_strings,
           (long)(after->string_stats.tracked_strings - before->string_stats.tracked_strings));
    printf("Interned Strings: %zu -> %zu (diff: %+ld)\n",
           before->string_stats.interned_strings, after->string_stats.interned_strings,
           (long)(after->string_stats.interned_strings - before->string_stats.interned_strings));
    
    // Check for leaks
    if (after->current_objects > before->current_objects) {
        printf("⚠️  Potential memory leak detected!\n");
    } else if (after->current_objects == before->current_objects) {
        printf("✅ No memory leaks detected\n");
    } else {
        printf("✅ Memory properly released\n");
    }
    printf("=====================================\n\n");
}

// Test string creation and release patterns
void test_string_lifecycle() {
    printf("Testing string lifecycle patterns...\n");
    
    take_memory_snapshot("Initial state");
    
    // Test 1: Simple creation and release
    {
        WynObject* str = WYN_STRING_CREATE("Lifecycle test");
        take_memory_snapshot("After string creation");
        WYN_STRING_RELEASE(str);
        take_memory_snapshot("After string release");
    }
    
    compare_snapshots(0, 2, "Simple creation/release cycle");
    
    // Test 2: Multiple string operations
    {
        WynObject* str1 = WYN_STRING_CREATE("String 1");
        WynObject* str2 = WYN_STRING_CREATE("String 2");
        WynObject* concat = wyn_string_concat_arc(str1, str2);
        take_memory_snapshot("After multiple operations");
        
        WYN_STRING_RELEASE(str1);
        WYN_STRING_RELEASE(str2);
        WYN_STRING_RELEASE(concat);
        take_memory_snapshot("After cleanup");
    }
    
    compare_snapshots(2, 4, "Multiple string operations");
    
    // Test 3: String copying
    {
        WynObject* original = WYN_STRING_CREATE("Original for copying");
        WynObject* copy1 = wyn_string_copy_arc(original);
        WynObject* copy2 = wyn_string_copy_arc(original);
        take_memory_snapshot("After copying operations");
        
        WYN_STRING_RELEASE(original);
        WYN_STRING_RELEASE(copy1);
        WYN_STRING_RELEASE(copy2);
        take_memory_snapshot("After copy cleanup");
    }
    
    compare_snapshots(4, 6, "String copying operations");
}

// Test string interning effectiveness
void test_string_interning_effectiveness() {
    printf("Testing string interning effectiveness...\n");
    
    take_memory_snapshot("Before interning test");
    
    // Create multiple instances of the same strings
    const char* test_strings[] = {
        "Common String 1",
        "Common String 2", 
        "Common String 1", // Duplicate
        "Common String 3",
        "Common String 2", // Duplicate
        "Common String 1"  // Duplicate
    };
    
    WynString* interned[6];
    for (int i = 0; i < 6; i++) {
        interned[i] = wyn_string_intern(test_strings[i]);
    }
    
    take_memory_snapshot("After interning");
    
    // Verify that duplicates point to the same memory
    assert(interned[0] == interned[2]); // "Common String 1"
    assert(interned[0] == interned[5]); // "Common String 1"
    assert(interned[1] == interned[4]); // "Common String 2"
    
    printf("✅ String interning working correctly - duplicates share memory\n");
    
    compare_snapshots(7, 8, "String interning");
}

// Stress test with many string operations
void stress_test_string_operations() {
    printf("Running string operations stress test...\n");
    
    take_memory_snapshot("Before stress test");
    
    const int NUM_OPERATIONS = 1000;
    WynObject* strings[NUM_OPERATIONS];
    
    // Create many strings
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Stress test string %d", i);
        strings[i] = WYN_STRING_CREATE(buffer);
    }
    
    take_memory_snapshot("After creating 1000 strings");
    
    // Perform concatenations
    for (int i = 0; i < NUM_OPERATIONS / 2; i++) {
        WynObject* concat = wyn_string_concat_arc(strings[i], strings[i + NUM_OPERATIONS/2]);
        WYN_STRING_RELEASE(concat);
    }
    
    take_memory_snapshot("After concatenation operations");
    
    // Clean up all strings
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        WYN_STRING_RELEASE(strings[i]);
    }
    
    take_memory_snapshot("After stress test cleanup");
    
    compare_snapshots(9, 12, "Stress test (1000 strings)");
}

// Test for memory leaks in error conditions
void test_error_condition_leaks() {
    printf("Testing memory leaks in error conditions...\n");
    
    take_memory_snapshot("Before error condition tests");
    
    // Test NULL parameter handling
    WynObject* null_result = wyn_string_create_arc(NULL);
    assert(null_result == NULL);
    
    WynObject* null_concat = wyn_string_concat_arc(NULL, NULL);
    assert(null_concat == NULL);
    
    // Test with valid string and NULL
    WynObject* valid_str = WYN_STRING_CREATE("Valid string");
    WynObject* mixed_concat = wyn_string_concat_arc(valid_str, NULL);
    assert(mixed_concat == NULL);
    
    WYN_STRING_RELEASE(valid_str);
    
    take_memory_snapshot("After error condition tests");
    
    compare_snapshots(13, 14, "Error condition handling");
}

// Monitor memory usage over time
void monitor_memory_usage() {
    printf("Monitoring memory usage patterns...\n");
    
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Cycle %d:\n", cycle + 1);
        
        // Create some strings
        WynObject* temp_strings[10];
        for (int i = 0; i < 10; i++) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Temp %d-%d", cycle, i);
            temp_strings[i] = WYN_STRING_CREATE(buffer);
        }
        
        // Do some operations
        for (int i = 0; i < 5; i++) {
            WynObject* concat = wyn_string_concat_arc(temp_strings[i], temp_strings[i+5]);
            WYN_STRING_RELEASE(concat);
        }
        
        // Clean up
        for (int i = 0; i < 10; i++) {
            WYN_STRING_RELEASE(temp_strings[i]);
        }
        
        // Take snapshot
        char label[64];
        snprintf(label, sizeof(label), "End of cycle %d", cycle + 1);
        take_memory_snapshot(label);
        
        // Brief pause to simulate real usage
        usleep(10000); // 10ms
    }
    
    // Compare first and last cycle
    compare_snapshots(15, 19, "Memory stability over cycles");
}

int main() {
    printf("=== String Memory Leak Detection Framework ===\n\n");
    
    // Initialize
    wyn_arc_reset_stats();
    snapshot_count = 0;
    
    // Run tests
    test_string_lifecycle();
    test_string_interning_effectiveness();
    stress_test_string_operations();
    test_error_condition_leaks();
    monitor_memory_usage();
    
    // Final leak check
    printf("\n=== Final Leak Check ===\n");
    wyn_string_check_leaks();
    
    // Print comprehensive statistics
    printf("\n=== Final Memory Statistics ===\n");
    wyn_arc_print_stats();
    wyn_string_print_memory_stats();
    
    // Clean up
    wyn_string_cleanup_intern_table();
    
    printf("\n✅ Memory leak detection framework completed!\n");
    return 0;
}