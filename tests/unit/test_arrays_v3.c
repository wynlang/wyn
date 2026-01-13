#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
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
    arr->count++;
}
int array_get_int(WynArray arr, int index) {
    printf("array_get_int: count=%d, index=%d\n", arr.count, index);
    if (index < 0 || index >= arr.count) return 0;
    printf("type=%d, int_val=%d\n", arr.data[index].type, arr.data[index].data.int_val);
    if (arr.data[index].type == WYN_TYPE_INT) return arr.data[index].data.int_val;
    return 0;
}

int wyn_main() {
    const WynArray arr = ({ WynArray __arr_0 = array_new(); array_push_int(&__arr_0, 1); array_push_int(&__arr_0, 2); array_push_int(&__arr_0, 3); __arr_0; });
    printf("arr.count = %d\n", arr.count);
    return array_get_int(arr, 0);
}

int main() {
    int result = wyn_main();
    printf("result = %d\n", result);
    return result;
}
