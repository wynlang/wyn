#ifndef FUNCTION_H
#define FUNCTION_H

#include <stddef.h>
#include <stdbool.h>

// T1.5.1: Recursion Stack Protection

// Stack depth tracking
typedef struct {
    size_t current_depth;
    size_t max_depth;
    bool overflow_detected;
} RecursionTracker;

// Global recursion tracker
extern RecursionTracker recursion_tracker;

// Recursion protection functions
void init_recursion_tracker(size_t max_depth);
bool enter_function_call(void);
void exit_function_call(void);
bool check_stack_overflow(void);
void reset_recursion_tracker(void);

// Configuration
void set_recursion_limit(size_t limit);
size_t get_recursion_limit(void);
size_t get_current_depth(void);

#endif
