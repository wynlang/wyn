#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/collections.h"

// Test helper functions
bool int_eq(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

int int_cmp(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

void* int_clone(const void* item) {
    int* cloned = malloc(sizeof(int));
    *cloned = *(const int*)item;
    return cloned;
}

void test_vec_creation() {
    printf("Testing vector creation...\n");
    
    // Test new vector
    WynVec* vec = wyn_vec_new(sizeof(int));
    assert(vec != NULL);
    assert(wyn_vec_len(vec) == 0);
    assert(wyn_vec_capacity(vec) == 0);
    assert(wyn_vec_is_empty(vec) == true);
    wyn_vec_free(vec);
    
    // Test vector with capacity
    vec = wyn_vec_with_capacity(sizeof(int), 10);
    assert(vec != NULL);
    assert(wyn_vec_len(vec) == 0);
    assert(wyn_vec_capacity(vec) == 10);
    assert(wyn_vec_is_empty(vec) == true);
    wyn_vec_free(vec);
    
    printf("✓ Vector creation test passed\n");
}

void test_vec_push_pop() {
    printf("Testing vector push/pop operations...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Test push operations
    for (int i = 0; i < 10; i++) {
        assert(wyn_vec_push(vec, &i) == true);
        assert(wyn_vec_len(vec) == (size_t)(i + 1));
        assert(wyn_vec_is_empty(vec) == false);
    }
    
    // Test capacity growth
    assert(wyn_vec_capacity(vec) >= 10);
    
    // Test pop operations
    for (int i = 9; i >= 0; i--) {
        int popped;
        assert(wyn_vec_pop(vec, &popped) == true);
        assert(popped == i);
        assert(wyn_vec_len(vec) == (size_t)i);
    }
    
    assert(wyn_vec_is_empty(vec) == true);
    
    // Test pop from empty vector
    int dummy;
    assert(wyn_vec_pop(vec, &dummy) == false);
    
    wyn_vec_free(vec);
    
    printf("✓ Vector push/pop test passed\n");
}

void test_vec_get_set() {
    printf("Testing vector get/set operations...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add some items
    for (int i = 0; i < 5; i++) {
        wyn_vec_push(vec, &i);
    }
    
    // Test get operations
    for (int i = 0; i < 5; i++) {
        int value;
        assert(wyn_vec_get(vec, i, &value) == true);
        assert(value == i);
    }
    
    // Test get out of bounds
    int dummy;
    assert(wyn_vec_get(vec, 10, &dummy) == false);
    
    // Test set operations
    for (int i = 0; i < 5; i++) {
        int new_value = i * 10;
        assert(wyn_vec_set(vec, i, &new_value) == true);
        
        int retrieved;
        assert(wyn_vec_get(vec, i, &retrieved) == true);
        assert(retrieved == new_value);
    }
    
    // Test set out of bounds
    int new_val = 999;
    assert(wyn_vec_set(vec, 10, &new_val) == false);
    
    wyn_vec_free(vec);
    
    printf("✓ Vector get/set test passed\n");
}

void test_vec_insert_remove() {
    printf("Testing vector insert/remove operations...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add initial items: [0, 1, 2, 3, 4]
    for (int i = 0; i < 5; i++) {
        wyn_vec_push(vec, &i);
    }
    
    // Test insert at beginning: [99, 0, 1, 2, 3, 4]
    int insert_val = 99;
    assert(wyn_vec_insert(vec, 0, &insert_val) == true);
    assert(wyn_vec_len(vec) == 6);
    
    int first;
    assert(wyn_vec_get(vec, 0, &first) == true);
    assert(first == 99);
    
    // Test insert in middle: [99, 0, 88, 1, 2, 3, 4]
    insert_val = 88;
    assert(wyn_vec_insert(vec, 2, &insert_val) == true);
    assert(wyn_vec_len(vec) == 7);
    
    int middle;
    assert(wyn_vec_get(vec, 2, &middle) == true);
    assert(middle == 88);
    
    // Test insert at end: [99, 0, 88, 1, 2, 3, 4, 77]
    insert_val = 77;
    assert(wyn_vec_insert(vec, wyn_vec_len(vec), &insert_val) == true);
    assert(wyn_vec_len(vec) == 8);
    
    int last;
    assert(wyn_vec_get(vec, 7, &last) == true);
    assert(last == 77);
    
    // Test remove from beginning
    int removed;
    assert(wyn_vec_remove(vec, 0, &removed) == true);
    assert(removed == 99);
    assert(wyn_vec_len(vec) == 7);
    
    // Test remove from middle
    assert(wyn_vec_remove(vec, 1, &removed) == true);
    assert(removed == 88);
    assert(wyn_vec_len(vec) == 6);
    
    // Test remove from end
    assert(wyn_vec_remove(vec, wyn_vec_len(vec) - 1, &removed) == true);
    assert(removed == 77);
    assert(wyn_vec_len(vec) == 5);
    
    // Test remove out of bounds
    assert(wyn_vec_remove(vec, 10, &removed) == false);
    
    wyn_vec_free(vec);
    
    printf("✓ Vector insert/remove test passed\n");
}

void test_vec_iterator() {
    printf("Testing vector iterator...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add items
    for (int i = 0; i < 5; i++) {
        wyn_vec_push(vec, &i);
    }
    
    // Test iterator
    WynVecIterator* iter = wyn_vec_iter(vec);
    assert(iter != NULL);
    
    int expected = 0;
    int value;
    while (wyn_vec_iter_next(iter, &value)) {
        assert(value == expected);
        expected++;
    }
    
    assert(expected == 5);
    
    // Test iterator exhaustion
    assert(wyn_vec_iter_next(iter, &value) == false);
    
    wyn_vec_iter_free(iter);
    wyn_vec_free(vec);
    
    printf("✓ Vector iterator test passed\n");
}

void test_vec_utility_functions() {
    printf("Testing vector utility functions...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add items: [3, 1, 4, 1, 5]
    int items[] = {3, 1, 4, 1, 5};
    for (int i = 0; i < 5; i++) {
        wyn_vec_push(vec, &items[i]);
    }
    
    // Test contains
    int search_val = 4;
    assert(wyn_vec_contains(vec, &search_val, int_eq) == true);
    
    search_val = 99;
    assert(wyn_vec_contains(vec, &search_val, int_eq) == false);
    
    // Test sort
    wyn_vec_sort(vec, int_cmp);
    
    // Verify sorted order: [1, 1, 3, 4, 5]
    int expected_sorted[] = {1, 1, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        int value;
        assert(wyn_vec_get(vec, i, &value) == true);
        assert(value == expected_sorted[i]);
    }
    
    // Test clone
    WynVec* cloned = wyn_vec_clone(vec, NULL);
    assert(cloned != NULL);
    assert(wyn_vec_len(cloned) == wyn_vec_len(vec));
    
    for (size_t i = 0; i < wyn_vec_len(vec); i++) {
        int orig, clone;
        assert(wyn_vec_get(vec, i, &orig) == true);
        assert(wyn_vec_get(cloned, i, &clone) == true);
        assert(orig == clone);
    }
    
    wyn_vec_free(cloned);
    wyn_vec_free(vec);
    
    printf("✓ Vector utility functions test passed\n");
}

void test_vec_memory_management() {
    printf("Testing vector memory management...\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Test reserve
    assert(wyn_vec_reserve(vec, 100) == true);
    assert(wyn_vec_capacity(vec) >= 100);
    assert(wyn_vec_len(vec) == 0);
    
    // Add some items
    for (int i = 0; i < 10; i++) {
        wyn_vec_push(vec, &i);
    }
    
    // Test shrink to fit
    wyn_vec_shrink_to_fit(vec);
    assert(wyn_vec_capacity(vec) == wyn_vec_len(vec));
    
    // Test clear
    wyn_vec_clear(vec);
    assert(wyn_vec_len(vec) == 0);
    assert(wyn_vec_is_empty(vec) == true);
    
    wyn_vec_free(vec);
    
    printf("✓ Vector memory management test passed\n");
}

void test_vec_edge_cases() {
    printf("Testing vector edge cases...\n");
    
    // Test NULL vector operations
    assert(wyn_vec_len(NULL) == 0);
    assert(wyn_vec_capacity(NULL) == 0);
    assert(wyn_vec_is_empty(NULL) == true);
    assert(wyn_vec_as_ptr(NULL) == NULL);
    assert(wyn_vec_as_const_ptr(NULL) == NULL);
    
    // Test invalid item size
    WynVec* vec = wyn_vec_new(0);
    assert(vec == NULL);
    
    // Test operations on empty vector
    vec = wyn_vec_new(sizeof(int));
    int dummy;
    assert(wyn_vec_pop(vec, &dummy) == false);
    assert(wyn_vec_get(vec, 0, &dummy) == false);
    assert(wyn_vec_set(vec, 0, &dummy) == false);
    assert(wyn_vec_remove(vec, 0, &dummy) == false);
    
    wyn_vec_free(vec);
    
    printf("✓ Vector edge cases test passed\n");
}

int main() {
    printf("Running Dynamic Array (Vec) Tests...\n\n");
    
    test_vec_creation();
    test_vec_push_pop();
    test_vec_get_set();
    test_vec_insert_remove();
    test_vec_iterator();
    test_vec_utility_functions();
    test_vec_memory_management();
    test_vec_edge_cases();
    
    printf("\n✅ All Vec tests passed!\n");
    return 0;
}
