// Enhanced stdlib functions for Wyn

#include <stdlib.h>
#include <string.h>

// Get array length (stored as first element in Wyn arrays)
int wyn_array_length(int* arr) {
    if (!arr) return 0;
    return arr[-1];  // Length stored before array data
}

// Get string length
int wyn_string_length(const char* str) {
    if (!str) return 0;
    return (int)strlen(str);
}

// String to uppercase
char* wyn_string_upper(const char* str) {
    if (!str) return NULL;
    int len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (int i = 0; i < len; i++) {
        result[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 32 : str[i];
    }
    result[len] = '\0';
    return result;
}

// String to lowercase
char* wyn_string_lower(const char* str) {
    if (!str) return NULL;
    int len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (int i = 0; i < len; i++) {
        result[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
    }
    result[len] = '\0';
    return result;
}

// Min function
int wyn_min(int a, int b) {
    return a < b ? a : b;
}

// Max function
int wyn_max(int a, int b) {
    return a > b ? a : b;
}

// Abs function
int wyn_abs(int x) {
    return x < 0 ? -x : x;
}
