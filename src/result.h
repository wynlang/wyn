#ifndef WYN_RESULT_H
#define WYN_RESULT_H

#include <stdbool.h>
#include <stddef.h>

// Result type representation - similar to Rust's Result<T,E>
typedef struct {
    bool is_ok;
    void* value;
    void* error;
    size_t value_size;
    size_t error_size;
} WynResult;

// Result type constructors
WynResult* wyn_ok(void* value, size_t value_size);
WynResult* wyn_err(void* error, size_t error_size);

// Result type operations
bool wyn_result_is_ok(WynResult* result);
bool wyn_result_is_err(WynResult* result);
void* wyn_result_unwrap(WynResult* result);
void* wyn_result_unwrap_err(WynResult* result);
void* wyn_result_unwrap_or(WynResult* result, void* default_value);

// Error propagation support (? operator)
WynResult* wyn_result_propagate(WynResult* result);

// Pattern matching support
bool wyn_result_match_ok(WynResult* result, void** out_value);
bool wyn_result_match_err(WynResult* result, void** out_error);

// Memory management
void wyn_result_free(WynResult* result);

// Type-specific constructors for common types
WynResult* ok_int(int value);
WynResult* ok_float(float value);
WynResult* ok_string(const char* value);
WynResult* ok_bool(bool value);
WynResult* ok_void(void);

WynResult* err_int(int error);
WynResult* err_float(float error);
WynResult* err_string(const char* error);
WynResult* err_bool(bool error);

// Type-specific unwrappers
int unwrap_result_int(WynResult* result);
float unwrap_result_float(WynResult* result);
const char* unwrap_result_string(WynResult* result);
bool unwrap_result_bool(WynResult* result);

// Error unwrappers
int unwrap_error_int(WynResult* result);
float unwrap_error_float(WynResult* result);
const char* unwrap_error_string(WynResult* result);
bool unwrap_error_bool(WynResult* result);

// Utility functions for error handling
WynResult* wyn_result_map(WynResult* result, void* (*map_fn)(void*), size_t new_size);
WynResult* wyn_result_map_err(WynResult* result, void* (*map_fn)(void*), size_t new_size);
WynResult* wyn_result_and_then(WynResult* result, WynResult* (*then_fn)(void*));

#endif