// Example usage of Wyn HashMap
// This demonstrates the HashMap API in a C context

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/collections.h"

// Example: Working with integer keys and values
void example_int_hashmap() {
    printf("=== Integer HashMap Example ===\n");
    
    // Create a new HashMap for int -> int mapping
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(int), wyn_hash_int, wyn_equals_int);
    
    // Add some key-value pairs
    printf("Inserting key-value pairs:\n");
    for (int i = 1; i <= 5; i++) {
        int key = i;
        int value = i * i;  // Square of the key
        wyn_hashmap_insert(map, &key, &value, NULL);
        printf("  %d -> %d\n", key, value);
    }
    
    printf("HashMap length: %zu\n", wyn_hashmap_len(map));
    
    // Retrieve values
    printf("\nRetrieving values:\n");
    for (int i = 1; i <= 5; i++) {
        int value;
        if (wyn_hashmap_get(map, &i, &value)) {
            printf("  map[%d] = %d\n", i, value);
        }
    }
    
    // Update a value
    int key = 3;
    int new_value = 999;
    int old_value;
    if (wyn_hashmap_insert(map, &key, &new_value, &old_value)) {
        printf("\nUpdated key %d: %d -> %d\n", key, old_value, new_value);
    }
    
    // Check if keys exist
    printf("\nChecking key existence:\n");
    for (int i = 0; i <= 6; i++) {
        bool exists = wyn_hashmap_contains_key(map, &i);
        printf("  Key %d exists: %s\n", i, exists ? "yes" : "no");
    }
    
    wyn_hashmap_free(map);
}

// Example: Working with string keys
void example_string_hashmap() {
    printf("\n=== String HashMap Example ===\n");
    
    // Create HashMap for string -> int mapping (word counts)
    WynHashMap* word_counts = wyn_hashmap_new(sizeof(char*), sizeof(int), wyn_hash_string, wyn_equals_string);
    
    // Sample text to count words
    char* words[] = {"hello", "world", "hello", "wyn", "world", "hello", "language"};
    int word_count = sizeof(words) / sizeof(words[0]);
    
    printf("Counting word occurrences:\n");
    for (int i = 0; i < word_count; i++) {
        char* word = words[i];
        int count = 1;
        int existing_count;
        
        // Check if word already exists
        if (wyn_hashmap_get(word_counts, &word, &existing_count)) {
            count = existing_count + 1;
        }
        
        wyn_hashmap_insert(word_counts, &word, &count, NULL);
        printf("  \"%s\" -> %d\n", word, count);
    }
    
    printf("\nFinal word counts:\n");
    char* unique_words[] = {"hello", "world", "wyn", "language"};
    for (int i = 0; i < 4; i++) {
        char* word = unique_words[i];
        int count;
        if (wyn_hashmap_get(word_counts, &word, &count)) {
            printf("  \"%s\": %d occurrences\n", word, count);
        }
    }
    
    wyn_hashmap_free(word_counts);
}

// Example: Using HashMap iterator
void example_hashmap_iterator() {
    printf("\n=== HashMap Iterator Example ===\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(char*), wyn_hash_int, wyn_equals_int);
    
    // Add number -> name mappings
    char* names[] = {"zero", "one", "two", "three", "four"};
    for (int i = 0; i < 5; i++) {
        wyn_hashmap_insert(map, &i, &names[i], NULL);
    }
    
    // Iterate through all key-value pairs
    printf("Iterating through HashMap:\n");
    WynHashMapIterator* iter = wyn_hashmap_iter(map);
    int key;
    char* value;
    
    while (wyn_hashmap_iter_next(iter, &key, &value)) {
        printf("  %d -> \"%s\"\n", key, value);
    }
    
    wyn_hashmap_iter_free(iter);
    wyn_hashmap_free(map);
}

// Example: HashMap performance and resizing
void example_hashmap_performance() {
    printf("\n=== HashMap Performance Example ===\n");
    
    // Start with small capacity to demonstrate resizing
    WynHashMap* map = wyn_hashmap_with_capacity(sizeof(int), sizeof(int), 4, wyn_hash_int, wyn_equals_int);
    
    printf("Initial state: capacity=%zu, length=%zu, load=%.2f\n",
           wyn_hashmap_capacity(map), wyn_hashmap_len(map), wyn_hashmap_load_factor(map));
    
    // Add many items to trigger multiple resizes
    printf("\nAdding items (watch capacity grow):\n");
    for (int i = 0; i < 20; i++) {
        int value = i * 2;
        wyn_hashmap_insert(map, &i, &value, NULL);
        
        if (i % 5 == 4) {  // Print every 5 insertions
            printf("  After %d items: capacity=%zu, length=%zu, load=%.2f\n",
                   i + 1, wyn_hashmap_capacity(map), wyn_hashmap_len(map), wyn_hashmap_load_factor(map));
        }
    }
    
    // Verify all items are still accessible
    printf("\nVerifying all items are accessible:\n");
    bool all_found = true;
    for (int i = 0; i < 20; i++) {
        int value;
        if (!wyn_hashmap_get(map, &i, &value) || value != i * 2) {
            printf("  ERROR: Key %d not found or incorrect value\n", i);
            all_found = false;
        }
    }
    
    if (all_found) {
        printf("  ✓ All 20 items found with correct values\n");
    }
    
    wyn_hashmap_free(map);
}

// Example: HashMap operations
void example_hashmap_operations() {
    printf("\n=== HashMap Operations Example ===\n");
    
    WynHashMap* map = wyn_hashmap_new(sizeof(int), sizeof(char*), wyn_hash_int, wyn_equals_int);
    
    // Insert some data
    char* colors[] = {"red", "green", "blue", "yellow", "purple"};
    for (int i = 0; i < 5; i++) {
        wyn_hashmap_insert(map, &i, &colors[i], NULL);
    }
    
    printf("Initial HashMap contents:\n");
    for (int i = 0; i < 5; i++) {
        char* color;
        if (wyn_hashmap_get(map, &i, &color)) {
            printf("  %d -> \"%s\"\n", i, color);
        }
    }
    
    // Remove some items
    printf("\nRemoving items 1 and 3:\n");
    int keys_to_remove[] = {1, 3};
    for (int i = 0; i < 2; i++) {
        int key = keys_to_remove[i];
        char* removed_value;
        if (wyn_hashmap_remove(map, &key, &removed_value)) {
            printf("  Removed %d -> \"%s\"\n", key, removed_value);
        }
    }
    
    printf("\nRemaining HashMap contents:\n");
    for (int i = 0; i < 5; i++) {
        if (wyn_hashmap_contains_key(map, &i)) {
            char* color;
            wyn_hashmap_get(map, &i, &color);
            printf("  %d -> \"%s\"\n", i, color);
        } else {
            printf("  %d -> (removed)\n", i);
        }
    }
    
    // Clear all items
    printf("\nClearing HashMap...\n");
    wyn_hashmap_clear(map);
    printf("HashMap is now empty: %s\n", wyn_hashmap_is_empty(map) ? "yes" : "no");
    printf("Length: %zu\n", wyn_hashmap_len(map));
    
    wyn_hashmap_free(map);
}

int main() {
    printf("Wyn HashMap Examples\n");
    printf("====================\n");
    
    example_int_hashmap();
    example_string_hashmap();
    example_hashmap_iterator();
    example_hashmap_performance();
    example_hashmap_operations();
    
    printf("\n✅ All HashMap examples completed successfully!\n");
    return 0;
}
