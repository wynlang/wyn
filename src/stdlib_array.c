#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

// Find index: return index of first element matching predicate
int wyn_array_find_index(int* arr, int len, int (*pred)(int)) {
    for (int i = 0; i < len; i++) {
        if (pred(arr[i])) {
            return i;
        }
    }
    return -1;
}

// Unique: remove duplicates from array
int* wyn_array_unique(int* arr, int len, int* out_len) {
    int* result = malloc(sizeof(int) * len);
    int count = 0;
    for (int i = 0; i < len; i++) {
        int is_duplicate = 0;
        for (int j = 0; j < count; j++) {
            if (result[j] == arr[i]) {
                is_duplicate = 1;
                break;
            }
        }
        if (!is_duplicate) {
            result[count++] = arr[i];
        }
    }
    *out_len = count;
    return result;
}

// Join: join array elements into string with separator
char* wyn_array_join(int* arr, int len, const char* separator) {
    if (len == 0) {
        char* result = malloc(1);
        result[0] = '\0';
        return result;
    }
    
    // Calculate required buffer size
    int sep_len = strlen(separator);
    int total_len = 0;
    for (int i = 0; i < len; i++) {
        // Estimate digits needed for each number (max 12 for int)
        total_len += 12;
        if (i < len - 1) total_len += sep_len;
    }
    total_len += 1; // null terminator
    
    char* result = malloc(total_len);
    result[0] = '\0';
    
    for (int i = 0; i < len; i++) {
        char num_str[12];
        sprintf(num_str, "%d", arr[i]);
        strcat(result, num_str);
        if (i < len - 1) {
            strcat(result, separator);
        }
    }
    
    return result;
}

// First: get first element
int wyn_array_first(int* arr, int len, int* found) {
    if (len > 0) {
        *found = 1;
        return arr[0];
    }
    *found = 0;
    return 0;
}

// Last: get last element
int wyn_array_last(int* arr, int len, int* found) {
    if (len > 0) {
        *found = 1;
        return arr[len - 1];
    }
    *found = 0;
    return 0;
}

// Is empty: check if array is empty
int wyn_array_is_empty(int* arr, int len) {
    (void)arr;
    return len == 0;
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
