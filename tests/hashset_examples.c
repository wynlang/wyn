// Example usage of Wyn HashSet
// This demonstrates the HashSet API in a C context

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/collections.h"

// Example: Working with integer sets
void example_int_hashset() {
    printf("=== Integer HashSet Example ===\n");
    
    // Create a new HashSet for integers
    WynHashSet* numbers = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add some numbers
    printf("Inserting numbers:\n");
    for (int i = 1; i <= 5; i++) {
        bool was_new = wyn_hashset_insert(numbers, &i);
        printf("  %d -> %s\n", i, was_new ? "inserted" : "already exists");
    }
    
    // Try to insert duplicates
    printf("\nTrying to insert duplicates:\n");
    int duplicates[] = {3, 5, 7};
    for (int i = 0; i < 3; i++) {
        bool was_new = wyn_hashset_insert(numbers, &duplicates[i]);
        printf("  %d -> %s\n", duplicates[i], was_new ? "inserted" : "already exists");
    }
    
    printf("\nHashSet contents (length: %zu):\n", wyn_hashset_len(numbers));
    WynHashSetIterator* iter = wyn_hashset_iter(numbers);
    int item;
    while (wyn_hashset_iter_next(iter, &item)) {
        printf("  %d\n", item);
    }
    wyn_hashset_iter_free(iter);
    
    // Check membership
    printf("\nMembership tests:\n");
    for (int i = 0; i <= 8; i++) {
        bool contains = wyn_hashset_contains(numbers, &i);
        printf("  %d: %s\n", i, contains ? "in set" : "not in set");
    }
    
    wyn_hashset_free(numbers);
}

// Example: Working with string sets
void example_string_hashset() {
    printf("\n=== String HashSet Example ===\n");
    
    // Create HashSet for strings
    WynHashSet* words = wyn_hashset_new(sizeof(char*), wyn_hash_string, wyn_equals_string);
    
    // Add programming languages
    char* languages[] = {"C", "Rust", "Go", "Python", "JavaScript", "Wyn"};
    int lang_count = sizeof(languages) / sizeof(languages[0]);
    
    printf("Adding programming languages:\n");
    for (int i = 0; i < lang_count; i++) {
        wyn_hashset_insert(words, &languages[i]);
        printf("  Added: %s\n", languages[i]);
    }
    
    printf("\nLanguages in set:\n");
    WynHashSetIterator* iter = wyn_hashset_iter(words);
    char* word;
    while (wyn_hashset_iter_next(iter, &word)) {
        printf("  - %s\n", word);
    }
    wyn_hashset_iter_free(iter);
    
    // Remove some languages
    printf("\nRemoving some languages:\n");
    char* to_remove[] = {"JavaScript", "Python"};
    for (int i = 0; i < 2; i++) {
        bool removed = wyn_hashset_remove(words, &to_remove[i]);
        printf("  %s: %s\n", to_remove[i], removed ? "removed" : "not found");
    }
    
    printf("\nRemaining languages (count: %zu):\n", wyn_hashset_len(words));
    iter = wyn_hashset_iter(words);
    while (wyn_hashset_iter_next(iter, &word)) {
        printf("  - %s\n", word);
    }
    wyn_hashset_iter_free(iter);
    
    wyn_hashset_free(words);
}

// Example: Set operations
void example_set_operations() {
    printf("\n=== Set Operations Example ===\n");
    
    // Create two sets
    WynHashSet* set_a = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* set_b = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Populate set A: {1, 2, 3, 4}
    printf("Set A: ");
    for (int i = 1; i <= 4; i++) {
        wyn_hashset_insert(set_a, &i);
        printf("%d ", i);
    }
    printf("\n");
    
    // Populate set B: {3, 4, 5, 6}
    printf("Set B: ");
    for (int i = 3; i <= 6; i++) {
        wyn_hashset_insert(set_b, &i);
        printf("%d ", i);
    }
    printf("\n");
    
    // Union: A ∪ B
    WynHashSet* union_set = wyn_hashset_union(set_a, set_b);
    printf("\nUnion (A ∪ B): ");
    WynHashSetIterator* iter = wyn_hashset_iter(union_set);
    int item;
    while (wyn_hashset_iter_next(iter, &item)) {
        printf("%d ", item);
    }
    printf("(size: %zu)\n", wyn_hashset_len(union_set));
    wyn_hashset_iter_free(iter);
    
    // Intersection: A ∩ B
    WynHashSet* intersection_set = wyn_hashset_intersection(set_a, set_b);
    printf("Intersection (A ∩ B): ");
    iter = wyn_hashset_iter(intersection_set);
    while (wyn_hashset_iter_next(iter, &item)) {
        printf("%d ", item);
    }
    printf("(size: %zu)\n", wyn_hashset_len(intersection_set));
    wyn_hashset_iter_free(iter);
    
    // Difference: A - B
    WynHashSet* difference_set = wyn_hashset_difference(set_a, set_b);
    printf("Difference (A - B): ");
    iter = wyn_hashset_iter(difference_set);
    while (wyn_hashset_iter_next(iter, &item)) {
        printf("%d ", item);
    }
    printf("(size: %zu)\n", wyn_hashset_len(difference_set));
    wyn_hashset_iter_free(iter);
    
    // Cleanup
    wyn_hashset_free(set_a);
    wyn_hashset_free(set_b);
    wyn_hashset_free(union_set);
    wyn_hashset_free(intersection_set);
    wyn_hashset_free(difference_set);
}

// Example: Set relationships
void example_set_relationships() {
    printf("\n=== Set Relationships Example ===\n");
    
    // Create sets
    WynHashSet* small_set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* large_set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    WynHashSet* disjoint_set = wyn_hashset_new(sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Small set: {1, 2}
    for (int i = 1; i <= 2; i++) {
        wyn_hashset_insert(small_set, &i);
    }
    
    // Large set: {1, 2, 3, 4, 5}
    for (int i = 1; i <= 5; i++) {
        wyn_hashset_insert(large_set, &i);
    }
    
    // Disjoint set: {6, 7, 8}
    for (int i = 6; i <= 8; i++) {
        wyn_hashset_insert(disjoint_set, &i);
    }
    
    printf("Small set: {1, 2} (size: %zu)\n", wyn_hashset_len(small_set));
    printf("Large set: {1, 2, 3, 4, 5} (size: %zu)\n", wyn_hashset_len(large_set));
    printf("Disjoint set: {6, 7, 8} (size: %zu)\n", wyn_hashset_len(disjoint_set));
    
    // Test subset relationships
    printf("\nSubset relationships:\n");
    printf("  Small ⊆ Large: %s\n", wyn_hashset_is_subset(small_set, large_set) ? "true" : "false");
    printf("  Large ⊆ Small: %s\n", wyn_hashset_is_subset(large_set, small_set) ? "true" : "false");
    printf("  Small ⊆ Small: %s\n", wyn_hashset_is_subset(small_set, small_set) ? "true" : "false");
    
    // Test disjoint relationships
    printf("\nDisjoint relationships:\n");
    printf("  Small ∩ Large = ∅: %s\n", wyn_hashset_is_disjoint(small_set, large_set) ? "true" : "false");
    printf("  Small ∩ Disjoint = ∅: %s\n", wyn_hashset_is_disjoint(small_set, disjoint_set) ? "true" : "false");
    printf("  Large ∩ Disjoint = ∅: %s\n", wyn_hashset_is_disjoint(large_set, disjoint_set) ? "true" : "false");
    
    wyn_hashset_free(small_set);
    wyn_hashset_free(large_set);
    wyn_hashset_free(disjoint_set);
}

// Example: Performance and capacity
void example_hashset_performance() {
    printf("\n=== HashSet Performance Example ===\n");
    
    // Start with small capacity
    WynHashSet* set = wyn_hashset_with_capacity(sizeof(int), 4, wyn_hash_int, wyn_equals_int);
    
    printf("Initial state: capacity=%zu, length=%zu, load=%.2f\n",
           wyn_hashset_capacity(set), wyn_hashset_len(set), wyn_hashset_load_factor(set));
    
    // Add many items to trigger resizing
    printf("\nAdding items (watch capacity grow):\n");
    for (int i = 0; i < 20; i++) {
        wyn_hashset_insert(set, &i);
        
        if (i % 5 == 4) {  // Print every 5 insertions
            printf("  After %d items: capacity=%zu, length=%zu, load=%.2f\n",
                   i + 1, wyn_hashset_capacity(set), wyn_hashset_len(set), wyn_hashset_load_factor(set));
        }
    }
    
    // Verify all items are accessible
    printf("\nVerifying all items are accessible:\n");
    bool all_found = true;
    for (int i = 0; i < 20; i++) {
        if (!wyn_hashset_contains(set, &i)) {
            printf("  ERROR: Item %d not found\n", i);
            all_found = false;
        }
    }
    
    if (all_found) {
        printf("  ✓ All 20 items found in set\n");
    }
    
    wyn_hashset_free(set);
}

int main() {
    printf("Wyn HashSet Examples\n");
    printf("====================\n");
    
    example_int_hashset();
    example_string_hashset();
    example_set_operations();
    example_set_relationships();
    example_hashset_performance();
    
    printf("\n✅ All HashSet examples completed successfully!\n");
    return 0;
}
