// Example usage of Wyn Vec (Dynamic Array)
// This demonstrates the Vec API in a C context

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/collections.h"

// Example: Working with integers
void example_int_vec() {
    printf("=== Integer Vec Example ===\n");
    
    // Create a new vector for integers
    WynVec* numbers = wyn_vec_new(sizeof(int));
    
    // Add some numbers
    for (int i = 1; i <= 5; i++) {
        wyn_vec_push(numbers, &i);
        printf("Pushed %d, length: %zu\n", i, wyn_vec_len(numbers));
    }
    
    // Access elements
    printf("\nVector contents:\n");
    for (size_t i = 0; i < wyn_vec_len(numbers); i++) {
        int value;
        if (wyn_vec_get(numbers, i, &value)) {
            printf("  [%zu] = %d\n", i, value);
        }
    }
    
    // Insert in middle
    int insert_val = 99;
    wyn_vec_insert(numbers, 2, &insert_val);
    printf("\nAfter inserting 99 at index 2:\n");
    for (size_t i = 0; i < wyn_vec_len(numbers); i++) {
        int value;
        wyn_vec_get(numbers, i, &value);
        printf("  [%zu] = %d\n", i, value);
    }
    
    // Pop elements
    printf("\nPopping elements:\n");
    while (!wyn_vec_is_empty(numbers)) {
        int popped;
        if (wyn_vec_pop(numbers, &popped)) {
            printf("Popped: %d, remaining: %zu\n", popped, wyn_vec_len(numbers));
        }
    }
    
    wyn_vec_free(numbers);
}

// Example: Working with strings
void example_string_vec() {
    printf("\n=== String Vec Example ===\n");
    
    // Create vector for string pointers
    WynVec* strings = wyn_vec_new(sizeof(char*));
    
    // Add some strings (using string literals - no need to free)
    char* words[] = {"hello", "world", "from", "wyn", "language"};
    for (int i = 0; i < 5; i++) {
        wyn_vec_push(strings, &words[i]);
    }
    
    printf("String vector contents:\n");
    for (size_t i = 0; i < wyn_vec_len(strings); i++) {
        char* str;
        if (wyn_vec_get(strings, i, &str)) {
            printf("  [%zu] = \"%s\"\n", i, str);
        }
    }
    
    wyn_vec_free(strings);
}

// Example: Using iterator
void example_iterator() {
    printf("\n=== Iterator Example ===\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add squares of numbers
    for (int i = 1; i <= 5; i++) {
        int square = i * i;
        wyn_vec_push(vec, &square);
    }
    
    // Iterate through vector
    printf("Squares using iterator:\n");
    WynVecIterator* iter = wyn_vec_iter(vec);
    int value;
    size_t index = 0;
    while (wyn_vec_iter_next(iter, &value)) {
        printf("  Square[%zu] = %d\n", index++, value);
    }
    
    wyn_vec_iter_free(iter);
    wyn_vec_free(vec);
}

// Example: Memory management and capacity
void example_memory_management() {
    printf("\n=== Memory Management Example ===\n");
    
    // Create vector with initial capacity
    WynVec* vec = wyn_vec_with_capacity(sizeof(int), 100);
    printf("Initial capacity: %zu\n", wyn_vec_capacity(vec));
    
    // Add elements - no reallocation needed
    for (int i = 0; i < 50; i++) {
        wyn_vec_push(vec, &i);
    }
    printf("After adding 50 elements - length: %zu, capacity: %zu\n", 
           wyn_vec_len(vec), wyn_vec_capacity(vec));
    
    // Shrink to fit
    wyn_vec_shrink_to_fit(vec);
    printf("After shrink_to_fit - length: %zu, capacity: %zu\n", 
           wyn_vec_len(vec), wyn_vec_capacity(vec));
    
    // Reserve more space
    wyn_vec_reserve(vec, 50);
    printf("After reserving 50 more - capacity: %zu\n", wyn_vec_capacity(vec));
    
    wyn_vec_free(vec);
}

// Example: Sorting and searching
bool int_eq(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

int int_cmp(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

void example_sorting_searching() {
    printf("\n=== Sorting and Searching Example ===\n");
    
    WynVec* vec = wyn_vec_new(sizeof(int));
    
    // Add random numbers
    int numbers[] = {42, 17, 8, 99, 3, 56, 21};
    for (int i = 0; i < 7; i++) {
        wyn_vec_push(vec, &numbers[i]);
    }
    
    printf("Original vector:\n");
    for (size_t i = 0; i < wyn_vec_len(vec); i++) {
        int value;
        wyn_vec_get(vec, i, &value);
        printf("  %d", value);
    }
    printf("\n");
    
    // Sort the vector
    wyn_vec_sort(vec, int_cmp);
    
    printf("Sorted vector:\n");
    for (size_t i = 0; i < wyn_vec_len(vec); i++) {
        int value;
        wyn_vec_get(vec, i, &value);
        printf("  %d", value);
    }
    printf("\n");
    
    // Search for elements
    int search_val = 42;
    bool found = wyn_vec_contains(vec, &search_val, int_eq);
    printf("Contains 42: %s\n", found ? "yes" : "no");
    
    search_val = 100;
    found = wyn_vec_contains(vec, &search_val, int_eq);
    printf("Contains 100: %s\n", found ? "yes" : "no");
    
    wyn_vec_free(vec);
}

int main() {
    printf("Wyn Dynamic Array (Vec) Examples\n");
    printf("================================\n");
    
    example_int_vec();
    example_string_vec();
    example_iterator();
    example_memory_management();
    example_sorting_searching();
    
    printf("\n✅ All examples completed successfully!\n");
    return 0;
}
