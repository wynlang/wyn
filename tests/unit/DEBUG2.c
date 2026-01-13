#include <stdio.h>
#include <stdlib.h>
#include "arc_runtime.h"

typedef struct {
    WynTypeId type;
    union {
        int int_val;
        double float_val;
        const char* string_val;
        struct WynArray* array_val;
    } data;
} WynValue;

typedef struct WynArray { WynValue* data; int count; int capacity; } WynArray;

WynArray array_new() { WynArray arr = {0}; return arr; }

void array_push_int(WynArray* arr, int value) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    arr->data[arr->count].type = WYN_TYPE_INT;
    arr->data[arr->count].data.int_val = value;
    printf("Pushed %d at index %d\n", value, arr->count);
    arr->count++;
}

int array_get_int(WynArray arr, int index) {
    printf("array_get_int: count=%d, index=%d\n", arr.count, index);
    if (index < 0 || index >= arr.count) {
        printf("Index out of bounds\n");
        return 0;
    }
    printf("type=%d, int_val=%d\n", arr.data[index].type, arr.data[index].data.int_val);
    if (arr.data[index].type == WYN_TYPE_INT) return arr.data[index].data.int_val;
    return 0;
}

int main() {
    WynArray arr = ({ WynArray __arr_0 = array_new(); array_push_int(&__arr_0, 100); array_push_int(&__arr_0, 200); array_push_int(&__arr_0, 300); __arr_0; });
    printf("Final arr.count = %d\n", arr.count);
    int result = array_get_int(arr, 2);
    printf("Result = %d\n", result);
    return result;
}
