#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/collections.h"

void test_hashmap_creation() {
    printf("Testing HashMap creation...\n");
    
    // Test new HashMap
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    assert(map != NULL);
    assert(wyn_hashmap_len(map) == 0);
    assert(wyn_hashmap_capacity(map) == 16);  // Default capacity
    assert(wyn_hashmap_is_empty(map) == true);
    wyn_hashmap_free(map);
    
    // Test HashMap with capacity
    map = wyn_hashmap_with_capacity(sizeof(int), sizeof(int), 32, wyn_hash_int, wyn_equals_int);
    assert(map != NULL);
    assert(wyn_hashmap_len(map) == 0);
    assert(wyn_hashmap_capacity(map) == 32);
    assert(wyn_hashmap_is_empty(map) == true);
    wyn_hashmap_free(map);
    
    printf("✓ HashMap creation test passed\n");
}

void test_hashmap_insert_get() {
    printf("Testing HashMap insert/get operations...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Test insert operations
    for (int i = 0; i < 10; i++) {
        int value = i * 10;
        assert(wyn_hashmap_insert(map, &i, &value, NULL) == true);
        assert(wyn_hashmap_len(map) == (size_t)(i + 1));
        assert(wyn_hashmap_is_empty(map) == false);
    }
    
    // Test get operations
    for (int i = 0; i < 10; i++) {
        int retrieved;
        assert(wyn_hashmap_get(map, &i, &retrieved) == true);
        assert(retrieved == i * 10);
    }
    
    // Test get non-existent key
    int key = 999;
    int dummy;
    assert(wyn_hashmap_get(map, &key, &dummy) == false);
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap insert/get test passed\n");
}

void test_hashmap_update() {
    printf("Testing HashMap update operations...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Insert initial value
    int key = 42;
    int value = 100;
    assert(wyn_hashmap_insert(map, &key, &value, NULL) == true);
    assert(wyn_hashmap_len(map) == 1);
    
    // Update with old value retrieval
    int new_value = 200;
    int old_value;
    assert(wyn_hashmap_insert(map, &key, &new_value, &old_value) == true);
    assert(old_value == 100);
    assert(wyn_hashmap_len(map) == 1);  // Length should not change
    
    // Verify updated value
    int retrieved;
    assert(wyn_hashmap_get(map, &key, &retrieved) == true);
    assert(retrieved == 200);
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap update test passed\n");
}

void test_hashmap_remove() {
    printf("Testing HashMap remove operations...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add some items
    for (int i = 0; i < 5; i++) {
        int value = i * 10;
        wyn_hashmap_insert(map, &i, &value, NULL);
    }
    
    assert(wyn_hashmap_len(map) == 5);
    
    // Remove middle item
    int key = 2;
    int removed_value;
    assert(wyn_hashmap_remove(map, &key, &removed_value) == true);
    assert(removed_value == 20);
    assert(wyn_hashmap_len(map) == 4);
    
    // Verify item is gone
    int dummy;
    assert(wyn_hashmap_get(map, &key, &dummy) == false);
    assert(wyn_hashmap_contains_key(map, &key) == false);
    
    // Remove non-existent item
    key = 999;
    assert(wyn_hashmap_remove(map, &key, &dummy) == false);
    assert(wyn_hashmap_len(map) == 4);
    
    // Remove remaining items
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;  // Already removed
        assert(wyn_hashmap_remove(map, &i, NULL) == true);
    }
    
    assert(wyn_hashmap_is_empty(map) == true);
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap remove test passed\n");
}

void test_hashmap_contains() {
    printf("Testing HashMap contains operations...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add some items
    int keys[] = {1, 3, 5, 7, 9};
    for (int i = 0; i < 5; i++) {
        int value = keys[i] * 10;
        wyn_hashmap_insert(map, &keys[i], &value, NULL);
    }
    
    // Test contains for existing keys
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashmap_contains_key(map, &keys[i]) == true);
    }
    
    // Test contains for non-existing keys
    int non_keys[] = {0, 2, 4, 6, 8, 10};
    for (int i = 0; i < 6; i++) {
        assert(wyn_hashmap_contains_key(map, &non_keys[i]) == false);
    }
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap contains test passed\n");
}

void test_hashmap_iterator() {
    printf("Testing HashMap iterator...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items
    int expected_sum = 0;
    for (int i = 0; i < 5; i++) {
        int value = i * 10;
        wyn_hashmap_insert(map, &i, &value, NULL);
        expected_sum += value;
    }
    
    // Test iterator
    WynHashMapIterator* iter = wyn_hashmap_iter(map);
    assert(iter != NULL);
    
    int actual_sum = 0;
    int count = 0;
    int key, value;
    
    while (wyn_hashmap_iter_next(iter, &key, &value)) {
        actual_sum += value;
        count++;
        
        // Verify key-value relationship
        assert(value == key * 10);
    }
    
    assert(count == 5);
    assert(actual_sum == expected_sum);
    
    // Test iterator exhaustion
    assert(wyn_hashmap_iter_next(iter, &key, &value) == false);
    
    wyn_hashmap_iter_free(iter);
    wyn_hashmap_free(map);
    
    printf("✓ HashMap iterator test passed\n");
}

void test_hashmap_string_keys() {
    printf("Testing HashMap with string keys...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(char*), sizeof(int), wyn_hash_string, wyn_equals_string);
    
    // Add string key-value pairs
    char* keys[] = {"apple", "banana", "cherry", "date", "elderberry"};
    int values[] = {1, 2, 3, 4, 5};
    
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashmap_insert(map, &keys[i], &values[i], NULL) == true);
    }
    
    assert(wyn_hashmap_len(map) == 5);
    
    // Test retrieval
    for (int i = 0; i < 5; i++) {
        int retrieved;
        assert(wyn_hashmap_get(map, &keys[i], &retrieved) == true);
        assert(retrieved == values[i]);
    }
    
    // Test contains
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashmap_contains_key(map, &keys[i]) == true);
    }
    
    char* non_key = "grape";
    assert(wyn_hashmap_contains_key(map, &non_key) == false);
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap string keys test passed\n");
}

void test_hashmap_resize() {
    printf("Testing HashMap resize operations...\n");
    
    WynHashMap* map = wyn_hashmap_with_capacity(sizeof(int), sizeof(int), 4, wyn_hash_int, wyn_equals_int);
    
    printf("Initial capacity: %zu, load factor: %.2f\n", 
           wyn_hashmap_capacity(map), wyn_hashmap_load_factor(map));
    
    // Add items to trigger resize
    for (int i = 0; i < 10; i++) {
        int value = i * 10;
        assert(wyn_hashmap_insert(map, &i, &value, NULL) == true);
        
        printf("After inserting %d: len=%zu, cap=%zu, load=%.2f\n", 
               i, wyn_hashmap_len(map), wyn_hashmap_capacity(map), wyn_hashmap_load_factor(map));
    }
    
    // Verify all items are still accessible after resize
    for (int i = 0; i < 10; i++) {
        int retrieved;
        assert(wyn_hashmap_get(map, &i, &retrieved) == true);
        assert(retrieved == i * 10);
    }
    
    assert(wyn_hashmap_len(map) == 10);
    assert(wyn_hashmap_capacity(map) > 4);  // Should have grown
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap resize test passed\n");
}

void test_hashmap_clear() {
    printf("Testing HashMap clear operation...\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items
    for (int i = 0; i < 5; i++) {
        int value = i * 10;
        wyn_hashmap_insert(map, &i, &value, NULL);
    }
    
    assert(wyn_hashmap_len(map) == 5);
    assert(wyn_hashmap_is_empty(map) == false);
    
    // Clear map
    wyn_hashmap_clear(map);
    
    assert(wyn_hashmap_len(map) == 0);
    assert(wyn_hashmap_is_empty(map) == true);
    
    // Verify all keys are gone
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashmap_contains_key(map, &i) == false);
    }
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap clear test passed\n");
}

void test_hashmap_edge_cases() {
    printf("Testing HashMap edge cases...\n");
    
    // Test NULL operations
    assert(wyn_hashmap_len(NULL) == 0);
    assert(wyn_hashmap_capacity(NULL) == 0);
    assert(wyn_hashmap_is_empty(NULL) == true);
    assert(wyn_hashmap_load_factor(NULL) == 0.0);
    
    // Test invalid parameters
    WynHashMap* map = wyn_hashmap_new(0, sizeof(int), wyn_hash_int, wyn_equals_int);
    assert(map == NULL);
    
    map = wyn_hashmap_new(sizeof(int), sizeof(int), NULL, wyn_equals_int);
    assert(map == NULL);
    
    map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, NULL);
    assert(map == NULL);
    
    // Test operations on empty map
    map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    int key = 1, value, dummy;
    
    assert(wyn_hashmap_get(map, &key, &value) == false);
    assert(wyn_hashmap_remove(map, &key, &dummy) == false);
    assert(wyn_hashmap_contains_key(map, &key) == false);
    
    wyn_hashmap_free(map);
    
    printf("✓ HashMap edge cases test passed\n");
}

int main() {
    printf("Running HashMap Tests...\n\n");
    
    test_hashmap_creation();
    test_hashmap_insert_get();
    test_hashmap_update();
    test_hashmap_remove();
    test_hashmap_contains();
    test_hashmap_iterator();
    test_hashmap_string_keys();
    test_hashmap_resize();
    test_hashmap_clear();
    test_hashmap_edge_cases();
    
    printf("\n✅ All HashMap tests passed!\n");
    return 0;
}
