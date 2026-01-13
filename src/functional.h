// TASK-040: Higher-order functions and iterator combinators
#ifndef WYN_FUNCTIONAL_H
#define WYN_FUNCTIONAL_H

#include <stdlib.h>
#include <stdarg.h>

// Function pointer types
typedef int (*IntToInt)(int);
typedef int (*IntPredicate)(int);

// Array structure for functional operations
typedef struct {
    int* data;
    int length;
    int capacity;
} IntArray;

// Create new array
IntArray* array_new_int(int capacity) {
    IntArray* arr = malloc(sizeof(IntArray));
    arr->data = malloc(sizeof(int) * capacity);
    arr->length = 0;
    arr->capacity = capacity;
    return arr;
}

// Add element to array
void array_push_int(IntArray* arr, int value) {
    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = realloc(arr->data, sizeof(int) * arr->capacity);
    }
    arr->data[arr->length++] = value;
}

// Map function: applies function to each element
IntArray* array_map(IntArray* arr, IntToInt func) {
    IntArray* result = array_new_int(arr->length);
    for (int i = 0; i < arr->length; i++) {
        array_push_int(result, func(arr->data[i]));
    }
    return result;
}

// Filter function: keeps elements that satisfy predicate
IntArray* array_filter(IntArray* arr, IntPredicate pred) {
    IntArray* result = array_new_int(arr->length);
    for (int i = 0; i < arr->length; i++) {
        if (pred(arr->data[i])) {
            array_push_int(result, arr->data[i]);
        }
    }
    return result;
}

// Reduce function: combines all elements into single value
int array_reduce(IntArray* arr, int (*func)(int, int), int initial) {
    int result = initial;
    for (int i = 0; i < arr->length; i++) {
        result = func(result, arr->data[i]);
    }
    return result;
}

// Helper function to create array from literal values
IntArray* array_from_values(int count, ...) {
    IntArray* arr = array_new_int(count);
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        array_push_int(arr, va_arg(args, int));
    }
    va_end(args);
    return arr;
}

#endif