#include "function.h"
#include "error.h"
#include "safe_memory.h"
#include <stdio.h>

// T1.5.1: Recursion Stack Protection Implementation

// Global recursion tracker
RecursionTracker recursion_tracker = {0, 1000, false};

void init_recursion_tracker(size_t max_depth) {
    recursion_tracker.current_depth = 0;
    recursion_tracker.max_depth = max_depth;
    recursion_tracker.overflow_detected = false;
}

bool enter_function_call(void) {
    recursion_tracker.current_depth++;
    
    if (recursion_tracker.current_depth > recursion_tracker.max_depth) {
        recursion_tracker.overflow_detected = true;
        
        // Report stack overflow error
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Stack overflow: recursion depth exceeded limit of %zu", 
                recursion_tracker.max_depth);
        
        report_error(ERR_OUT_OF_MEMORY, "runtime", 0, 0, error_msg);
        
        return false;
    }
    
    return true;
}

void exit_function_call(void) {
    if (recursion_tracker.current_depth > 0) {
        recursion_tracker.current_depth--;
    }
}

bool check_stack_overflow(void) {
    return recursion_tracker.overflow_detected;
}

void reset_recursion_tracker(void) {
    recursion_tracker.current_depth = 0;
    recursion_tracker.overflow_detected = false;
}

void set_recursion_limit(size_t limit) {
    recursion_tracker.max_depth = limit;
}

size_t get_recursion_limit(void) {
    return recursion_tracker.max_depth;
}

size_t get_current_depth(void) {
    return recursion_tracker.current_depth;
}
