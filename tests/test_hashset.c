#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/collections.h"

void test_hashset_creation() {
    printf("Testing HashSet creation...\n");
    
    // Test new HashSet
    WynHashSet* set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    assert(set != NULL);
    assert(wyn_hashset_len(set) == 0);
    assert(wyn_hashset_capacity(set) == 16);  // Default capacity
    assert(wyn_hashset_is_empty(set) == true);
    wyn_hashset_free(set);
    
    // Test HashSet with capacity
    set = wyn_hashset_with_capacity(sizeof(int), 32, wyn_hash_int, wyn_equals_int);
    assert(set != NULL);
    assert(wyn_hashset_len(set) == 0);
    assert(wyn_hashset_capacity(set) == 32);
    assert(wyn_hashset_is_empty(set) == true);
    wyn_hashset_free(set);
    
    printf("✓ HashSet creation test passed\n");
}

void test_hashset_insert_contains() {
    printf("Testing HashSet insert/contains operations...\n");
    
    WynHashSet* set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Test insert operations
    for (int i = 0; i < 10; i++) {
        bool was_new = wyn_hashset_insert(set, &i);
        assert(was_new == true);  // Should be new insertion
        assert(wyn_hashset_len(set) == (size_t)(i + 1));
        assert(wyn_hashset_is_empty(set) == false);
    }
    
    // Test duplicate insertion
    int duplicate = 5;
    bool was_new = wyn_hashset_insert(set, &duplicate);
    assert(was_new == false);  // Should not be new
    assert(wyn_hashset_len(set) == 10);  // Length should not change
    
    // Test contains operations
    for (int i = 0; i < 10; i++) {
        assert(wyn_hashset_contains(set, &i) == true);
    }
    
    // Test contains non-existent item
    int non_existent = 999;
    assert(wyn_hashset_contains(set, &non_existent) == false);
    
    wyn_hashset_free(set);
    
    printf("✓ HashSet insert/contains test passed\n");
}

void test_hashset_remove() {
    printf("Testing HashSet remove operations...\n");
    
    WynHashSet* set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add some items
    for (int i = 0; i < 5; i++) {
        wyn_hashset_insert(set, &i);
    }
    
    assert(wyn_hashset_len(set) == 5);
    
    // Remove middle item
    int item = 2;
    assert(wyn_hashset_remove(set, &item) == true);
    assert(wyn_hashset_len(set) == 4);
    assert(wyn_hashset_contains(set, &item) == false);
    
    // Remove non-existent item
    item = 999;
    assert(wyn_hashset_remove(set, &item) == false);
    assert(wyn_hashset_len(set) == 4);
    
    // Remove remaining items
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;  // Already removed
        assert(wyn_hashset_remove(set, &i) == true);
    }
    
    assert(wyn_hashset_is_empty(set) == true);
    
    wyn_hashset_free(set);
    
    printf("✓ HashSet remove test passed\n");
}

void test_hashset_iterator() {
    printf("Testing HashSet iterator...\n");
    
    WynHashSet* set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items
    int expected_sum = 0;
    for (int i = 0; i < 5; i++) {
        wyn_hashset_insert(set, &i);
        expected_sum += i;
    }
    
    // Test iterator
    WynHashSetIterator* iter = wyn_hashset_iter(set);
    assert(iter != NULL);
    
    int actual_sum = 0;
    int count = 0;
    int item;
    
    while (wyn_hashset_iter_next(iter, &item)) {
        actual_sum += item;
        count++;
        
        // Verify item is in expected range
        assert(item >= 0 && item < 5);
    }
    
    assert(count == 5);
    assert(actual_sum == expected_sum);
    
    // Test iterator exhaustion
    assert(wyn_hashset_iter_next(iter, &item) == false);
    
    wyn_hashset_iter_free(iter);
    wyn_hashset_free(set);
    
    printf("✓ HashSet iterator test passed\n");
}

void test_hashset_string_items() {
    printf("Testing HashSet with string items...\n");
    
    WynHashSet* set = wyn_hashset_new(sizeof(char*), wyn_hash_string, wyn_equals_string);
    
    // Add string items
    char* items[] = {"apple", "banana", "cherry", "date", "elderberry"};
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashset_insert(set, &items[i]) == true);
    }
    
    assert(wyn_hashset_len(set) == 5);
    
    // Test contains
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashset_contains(set, &items[i]) == true);
    }
    
    char* non_item = "grape";
    assert(wyn_hashset_contains(set, &non_item) == false);
    
    // Test duplicate insertion
    char* duplicate = "apple";
    assert(wyn_hashset_insert(set, &duplicate) == false);
    assert(wyn_hashset_len(set) == 5);
    
    wyn_hashset_free(set);
    
    printf("✓ HashSet string items test passed\n");
}

void test_hashset_union() {
    printf("Testing HashSet union operation...\n");
    
    WynHashSet* set1 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set2 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items to set1: {1, 2, 3}
    for (int i = 1; i <= 3; i++) {
        wyn_hashset_insert(set1, &i);
    }
    
    // Add items to set2: {3, 4, 5}
    for (int i = 3; i <= 5; i++) {
        wyn_hashset_insert(set2, &i);
    }
    
    // Union should be {1, 2, 3, 4, 5}
    WynHashSet* union_set = wyn_hashset_union(set1, set2);
    assert(union_set != NULL);
    assert(wyn_hashset_len(union_set) == 5);
    
    // Verify all items are present
    for (int i = 1; i <= 5; i++) {
        assert(wyn_hashset_contains(union_set, &i) == true);
    }
    
    wyn_hashset_free(set1);
    wyn_hashset_free(set2);
    wyn_hashset_free(union_set);
    
    printf("✓ HashSet union test passed\n");
}

void test_hashset_intersection() {
    printf("Testing HashSet intersection operation...\n");
    
    WynHashSet* set1 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set2 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items to set1: {1, 2, 3, 4}
    for (int i = 1; i <= 4; i++) {
        wyn_hashset_insert(set1, &i);
    }
    
    // Add items to set2: {3, 4, 5, 6}
    for (int i = 3; i <= 6; i++) {
        wyn_hashset_insert(set2, &i);
    }
    
    // Intersection should be {3, 4}
    WynHashSet* intersection_set = wyn_hashset_intersection(set1, set2);
    assert(intersection_set != NULL);
    assert(wyn_hashset_len(intersection_set) == 2);
    
    // Verify intersection items
    int three = 3, four = 4;
    assert(wyn_hashset_contains(intersection_set, &three) == true);
    assert(wyn_hashset_contains(intersection_set, &four) == true);
    
    // Verify non-intersection items are not present
    int one = 1, five = 5;
    assert(wyn_hashset_contains(intersection_set, &one) == false);
    assert(wyn_hashset_contains(intersection_set, &five) == false);
    
    wyn_hashset_free(set1);
    wyn_hashset_free(set2);
    wyn_hashset_free(intersection_set);
    
    printf("✓ HashSet intersection test passed\n");
}

void test_hashset_difference() {
    printf("Testing HashSet difference operation...\n");
    
    WynHashSet* set1 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set2 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items to set1: {1, 2, 3, 4}
    for (int i = 1; i <= 4; i++) {
        wyn_hashset_insert(set1, &i);
    }
    
    // Add items to set2: {3, 4, 5, 6}
    for (int i = 3; i <= 6; i++) {
        wyn_hashset_insert(set2, &i);
    }
    
    // Difference (set1 - set2) should be {1, 2}
    WynHashSet* diff_set = wyn_hashset_difference(set1, set2);
    assert(diff_set != NULL);
    assert(wyn_hashset_len(diff_set) == 2);
    
    // Verify difference items
    int one = 1, two = 2;
    assert(wyn_hashset_contains(diff_set, &one) == true);
    assert(wyn_hashset_contains(diff_set, &two) == true);
    
    // Verify common items are not present
    int three = 3, four = 4;
    assert(wyn_hashset_contains(diff_set, &three) == false);
    assert(wyn_hashset_contains(diff_set, &four) == false);
    
    wyn_hashset_free(set1);
    wyn_hashset_free(set2);
    wyn_hashset_free(diff_set);
    
    printf("✓ HashSet difference test passed\n");
}

void test_hashset_subset() {
    printf("Testing HashSet subset operation...\n");
    
    WynHashSet* set1 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set2 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items to set1: {1, 2}
    for (int i = 1; i <= 2; i++) {
        wyn_hashset_insert(set1, &i);
    }
    
    // Add items to set2: {1, 2, 3, 4}
    for (int i = 1; i <= 4; i++) {
        wyn_hashset_insert(set2, &i);
    }
    
    // set1 should be subset of set2
    assert(wyn_hashset_is_subset(set1, set2) == true);
    
    // set2 should not be subset of set1
    assert(wyn_hashset_is_subset(set2, set1) == false);
    
    // set1 should be subset of itself
    assert(wyn_hashset_is_subset(set1, set1) == true);
    
    // Empty set should be subset of any set
    WynHashSet* empty_set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    assert(wyn_hashset_is_subset(empty_set, set1) == true);
    assert(wyn_hashset_is_subset(empty_set, set2) == true);
    
    wyn_hashset_free(set1);
    wyn_hashset_free(set2);
    wyn_hashset_free(empty_set);
    
    printf("✓ HashSet subset test passed\n");
}

void test_hashset_disjoint() {
    printf("Testing HashSet disjoint operation...\n");
    
    WynHashSet* set1 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set2 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set3 = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items to set1: {1, 2}
    for (int i = 1; i <= 2; i++) {
        wyn_hashset_insert(set1, &i);
    }
    
    // Add items to set2: {3, 4}
    for (int i = 3; i <= 4; i++) {
        wyn_hashset_insert(set2, &i);
    }
    
    // Add items to set3: {2, 3}
    for (int i = 2; i <= 3; i++) {
        wyn_hashset_insert(set3, &i);
    }
    
    // set1 and set2 should be disjoint
    assert(wyn_hashset_is_disjoint(set1, set2) == true);
    assert(wyn_hashset_is_disjoint(set2, set1) == true);
    
    // set1 and set3 should not be disjoint (share element 2)
    assert(wyn_hashset_is_disjoint(set1, set3) == false);
    assert(wyn_hashset_is_disjoint(set3, set1) == false);
    
    // set2 and set3 should not be disjoint (share element 3)
    assert(wyn_hashset_is_disjoint(set2, set3) == false);
    
    // Empty set should be disjoint with any set
    WynHashSet* empty_set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    assert(wyn_hashset_is_disjoint(empty_set, set1) == true);
    assert(wyn_hashset_is_disjoint(set1, empty_set) == true);
    
    wyn_hashset_free(set1);
    wyn_hashset_free(set2);
    wyn_hashset_free(set3);
    wyn_hashset_free(empty_set);
    
    printf("✓ HashSet disjoint test passed\n");
}

void test_hashset_clear() {
    printf("Testing HashSet clear operation...\n");
    
    WynHashSet* set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add items
    for (int i = 0; i < 5; i++) {
        wyn_hashset_insert(set, &i);
    }
    
    assert(wyn_hashset_len(set) == 5);
    assert(wyn_hashset_is_empty(set) == false);
    
    // Clear set
    wyn_hashset_clear(set);
    
    assert(wyn_hashset_len(set) == 0);
    assert(wyn_hashset_is_empty(set) == true);
    
    // Verify all items are gone
    for (int i = 0; i < 5; i++) {
        assert(wyn_hashset_contains(set, &i) == false);
    }
    
    wyn_hashset_free(set);
    
    printf("✓ HashSet clear test passed\n");
}

void test_hashset_edge_cases() {
    printf("Testing HashSet edge cases...\n");
    
    // Test NULL operations
    assert(wyn_hashset_len(NULL) == 0);
    assert(wyn_hashset_capacity(NULL) == 0);
    assert(wyn_hashset_is_empty(NULL) == true);
    assert(wyn_hashset_load_factor(NULL) == 0.0);
    
    // Test invalid parameters
    WynHashSet* set = wyn_hashset_new(0, wyn_hash_int, wyn_equals_int);
    assert(set == NULL);
    
    set = wyn_hashset_new(sizeof(int), NULL, wyn_equals_int);
    assert(set == NULL);
    
    set = wyn_hashset_new(sizeof(int), wyn_hash_int, NULL);
    assert(set == NULL);
    
    // Test operations on empty set
    set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    int item = 1;
    
    assert(wyn_hashset_contains(set, &item) == false);
    assert(wyn_hashset_remove(set, &item) == false);
    
    wyn_hashset_free(set);
    
    printf("✓ HashSet edge cases test passed\n");
}

int main() {
    printf("Running HashSet Tests...\n\n");
    
    test_hashset_creation();
    test_hashset_insert_contains();
    test_hashset_remove();
    test_hashset_iterator();
    test_hashset_string_items();
    test_hashset_union();
    test_hashset_intersection();
    test_hashset_difference();
    test_hashset_subset();
    test_hashset_disjoint();
    test_hashset_clear();
    test_hashset_edge_cases();
    
    printf("\n✅ All HashSet tests passed!\n");
    return 0;
}
