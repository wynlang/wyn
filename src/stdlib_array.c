#include <stdlib.h>
#include <string.h>

// Array module - comprehensive array manipulation functions

// NOTE: map/filter/reduce are now implemented in generated code using WynArray
// These old versions are kept for reference but not used

/*
// Map: apply function to each element
int* wyn_array_map(int* arr, int len, int (*fn)(int)) {
    int* result = malloc(sizeof(int) * len);
    for (int i = 0; i < len; i++) {
        result[i] = fn(arr[i]);
    }
    return result;
}

// Filter: keep elements that match predicate
int* wyn_array_filter(int* arr, int len, int (*pred)(int), int* out_len) {
    int* temp = malloc(sizeof(int) * len);
    int count = 0;
    for (int i = 0; i < len; i++) {
        if (pred(arr[i])) {
            temp[count++] = arr[i];
        }
    }
    *out_len = count;
    return temp;
}

// Reduce: fold array into single value
int wyn_array_reduce(int* arr, int len, int (*fn)(int, int), int initial) {
    int result = initial;
    for (int i = 0; i < len; i++) {
        result = fn(result, arr[i]);
    }
    return result;
}
*/

// Find: return first element matching predicate
int wyn_array_find(int* arr, int len, int (*pred)(int), int* found) {
    for (int i = 0; i < len; i++) {
        if (pred(arr[i])) {
            *found = 1;
            return arr[i];
        }
    }
    *found = 0;
    return 0;
}

// Any: check if any element matches predicate
int wyn_array_any(int* arr, int len, int (*pred)(int)) {
    for (int i = 0; i < len; i++) {
        if (pred(arr[i])) return 1;
    }
    return 0;
}

// All: check if all elements match predicate
int wyn_array_all(int* arr, int len, int (*pred)(int)) {
    for (int i = 0; i < len; i++) {
        if (!pred(arr[i])) return 0;
    }
    return 1;
}

// Reverse: reverse array in place
void wyn_array_reverse(int* arr, int len) {
    for (int i = 0; i < len / 2; i++) {
        int temp = arr[i];
        arr[i] = arr[len - 1 - i];
        arr[len - 1 - i] = temp;
    }
}

// Sort: quicksort implementation
static void quicksort(int* arr, int low, int high) {
    if (low < high) {
        int pivot = arr[high];
        int i = low - 1;
        for (int j = low; j < high; j++) {
            if (arr[j] < pivot) {
                i++;
                int temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
        int temp = arr[i + 1];
        arr[i + 1] = arr[high];
        arr[high] = temp;
        int pi = i + 1;
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

void wyn_array_sort(int* arr, int len) {
    if (len > 1) {
        quicksort(arr, 0, len - 1);
    }
}

// Contains: check if array contains value
int wyn_array_contains(int* arr, int len, int value) {
    for (int i = 0; i < len; i++) {
        if (arr[i] == value) return 1;
    }
    return 0;
}

// Index of: find index of value
int wyn_array_index_of(int* arr, int len, int value) {
    for (int i = 0; i < len; i++) {
        if (arr[i] == value) return i;
    }
    return -1;
}

// Last index of: find last index of value
int wyn_array_last_index_of(int* arr, int len, int value) {
    for (int i = len - 1; i >= 0; i--) {
        if (arr[i] == value) return i;
    }
    return -1;
}

// Slice: extract subarray
int* wyn_array_slice(int* arr, int start, int end, int* out_len) {
    if (start < 0) start = 0;
    if (end < start) end = start;
    int len = end - start;
    int* result = malloc(sizeof(int) * len);
    memcpy(result, arr + start, sizeof(int) * len);
    *out_len = len;
    return result;
}

// Concat: concatenate two arrays
int* wyn_array_concat(int* arr1, int len1, int* arr2, int len2, int* out_len) {
    int total = len1 + len2;
    int* result = malloc(sizeof(int) * total);
    memcpy(result, arr1, sizeof(int) * len1);
    memcpy(result + len1, arr2, sizeof(int) * len2);
    *out_len = total;
    return result;
}

// Fill: fill array with value
void wyn_array_fill(int* arr, int len, int value) {
    for (int i = 0; i < len; i++) {
        arr[i] = value;
    }
}

// Sum: sum all elements
int wyn_array_sum(int* arr, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

// Min: find minimum value
int wyn_array_min(int* arr, int len) {
    if (len == 0) return 0;
    int min = arr[0];
    for (int i = 1; i < len; i++) {
        if (arr[i] < min) min = arr[i];
    }
    return min;
}

// Max: find maximum value
int wyn_array_max(int* arr, int len) {
    if (len == 0) return 0;
    int max = arr[0];
    for (int i = 1; i < len; i++) {
        if (arr[i] > max) max = arr[i];
    }
    return max;
}

// Average: calculate average
double wyn_array_average(int* arr, int len) {
    if (len == 0) return 0.0;
    return (double)wyn_array_sum(arr, len) / len;
}
