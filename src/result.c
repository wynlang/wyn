#include "result.h"
#include "arc_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Generic Result constructors
WynResult* wyn_ok(void* value, size_t value_size) {
    WynResult* result = malloc(sizeof(WynResult));
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for Result\n");
        exit(1);
    }
    
    result->is_ok = true;
    result->value_size = value_size;
    result->error_size = 0;
    result->error = NULL;
    
    if (value && value_size > 0) {
        result->value = malloc(value_size);
        if (!result->value) {
            free(result);
            fprintf(stderr, "Error: Failed to allocate memory for Result value\n");
            exit(1);
        }
        memcpy(result->value, value, value_size);
    } else {
        result->value = NULL;
    }
    
    return result;
}

WynResult* wyn_err(void* error, size_t error_size) {
    WynResult* result = malloc(sizeof(WynResult));
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for Result\n");
        exit(1);
    }
    
    result->is_ok = false;
    result->error_size = error_size;
    result->value_size = 0;
    result->value = NULL;
    
    if (error && error_size > 0) {
        result->error = malloc(error_size);
        if (!result->error) {
            free(result);
            fprintf(stderr, "Error: Failed to allocate memory for Result error\n");
            exit(1);
        }
        memcpy(result->error, error, error_size);
    } else {
        result->error = NULL;
    }
    
    return result;
}

// Result type operations
bool wyn_result_is_ok(WynResult* result) {
    return result && result->is_ok;
}

bool wyn_result_is_err(WynResult* result) {
    return result && !result->is_ok;
}

void* wyn_result_unwrap(WynResult* result) {
    if (!result || !result->is_ok) {
        fprintf(stderr, "Error: Attempted to unwrap Err value\n");
        if (result && result->error) {
            fprintf(stderr, "Error details: %s\n", (char*)result->error);
        }
        exit(1);
    }
    return result->value;
}

void* wyn_result_unwrap_err(WynResult* result) {
    if (!result || result->is_ok) {
        fprintf(stderr, "Error: Attempted to unwrap_err Ok value\n");
        exit(1);
    }
    return result->error;
}

void* wyn_result_unwrap_or(WynResult* result, void* default_value) {
    if (!result || !result->is_ok) {
        return default_value;
    }
    return result->value;
}

// Error propagation support (? operator)
WynResult* wyn_result_propagate(WynResult* result) {
    if (!result) {
        return wyn_err("Null result", 12);
    }
    
    if (result->is_ok) {
        return result; // Continue with Ok value
    } else {
        return result; // Propagate error
    }
}

// Pattern matching support
bool wyn_result_match_ok(WynResult* result, void** out_value) {
    if (result && result->is_ok) {
        if (out_value) *out_value = result->value;
        return true;
    }
    return false;
}

bool wyn_result_match_err(WynResult* result, void** out_error) {
    if (result && !result->is_ok) {
        if (out_error) *out_error = result->error;
        return true;
    }
    return false;
}

// Memory management
void wyn_result_free(WynResult* result) {
    if (result) {
        if (result->value) {
            free(result->value);
        }
        if (result->error) {
            free(result->error);
        }
        free(result);
    }
}

// Type-specific constructors for common types
WynResult* ok_int(int value) {
    return wyn_ok(&value, sizeof(int));
}

WynResult* ok_float(float value) {
    return wyn_ok(&value, sizeof(float));
}

WynResult* ok_string(const char* value) {
    if (!value) return wyn_err("Null string", 12);
    
    WynResult* result = malloc(sizeof(WynResult));
    result->is_ok = true;
    result->value_size = strlen(value) + 1;
    result->error_size = 0;
    result->error = NULL;
    result->value = malloc(result->value_size);
    strcpy((char*)result->value, value);
    return result;
}

WynResult* ok_bool(bool value) {
    return wyn_ok(&value, sizeof(bool));
}

WynResult* ok_void(void) {
    return wyn_ok(NULL, 0);
}

WynResult* err_int(int error) {
    return wyn_err(&error, sizeof(int));
}

WynResult* err_float(float error) {
    return wyn_err(&error, sizeof(float));
}

WynResult* err_string(const char* error) {
    if (!error) error = "Unknown error";
    
    WynResult* result = malloc(sizeof(WynResult));
    result->is_ok = false;
    result->error_size = strlen(error) + 1;
    result->value_size = 0;
    result->value = NULL;
    result->error = malloc(result->error_size);
    strcpy((char*)result->error, error);
    return result;
}

WynResult* err_bool(bool error) {
    return wyn_err(&error, sizeof(bool));
}

// Type-specific unwrappers
int unwrap_result_int(WynResult* result) {
    void* value = wyn_result_unwrap(result);
    return *(int*)value;
}

float unwrap_result_float(WynResult* result) {
    void* value = wyn_result_unwrap(result);
    return *(float*)value;
}

const char* unwrap_result_string(WynResult* result) {
    void* value = wyn_result_unwrap(result);
    return (const char*)value;
}

bool unwrap_result_bool(WynResult* result) {
    void* value = wyn_result_unwrap(result);
    return *(bool*)value;
}

// Error unwrappers
int unwrap_error_int(WynResult* result) {
    void* error = wyn_result_unwrap_err(result);
    return *(int*)error;
}

float unwrap_error_float(WynResult* result) {
    void* error = wyn_result_unwrap_err(result);
    return *(float*)error;
}

const char* unwrap_error_string(WynResult* result) {
    void* error = wyn_result_unwrap_err(result);
    return (const char*)error;
}

bool unwrap_error_bool(WynResult* result) {
    void* error = wyn_result_unwrap_err(result);
    return *(bool*)error;
}

// Utility functions for error handling
WynResult* wyn_result_map(WynResult* result, void* (*map_fn)(void*), size_t new_size) {
    if (!result) return wyn_err("Null result", 12);
    
    if (result->is_ok) {
        void* new_value = map_fn(result->value);
        return wyn_ok(new_value, new_size);
    } else {
        // Return the same error
        return wyn_err(result->error, result->error_size);
    }
}

WynResult* wyn_result_map_err(WynResult* result, void* (*map_fn)(void*), size_t new_size) {
    if (!result) return wyn_err("Null result", 12);
    
    if (!result->is_ok) {
        void* new_error = map_fn(result->error);
        return wyn_err(new_error, new_size);
    } else {
        // Return the same Ok value
        return wyn_ok(result->value, result->value_size);
    }
}

WynResult* wyn_result_and_then(WynResult* result, WynResult* (*then_fn)(void*)) {
    if (!result) return wyn_err("Null result", 12);
    
    if (result->is_ok) {
        return then_fn(result->value);
    } else {
        // Return the same error
        return wyn_err(result->error, result->error_size);
    }
}