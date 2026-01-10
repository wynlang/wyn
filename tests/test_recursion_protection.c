#include "function.h"
#include "error.h"
#include "safe_memory.h"
#include <stdio.h>
#include <assert.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s... ", #name); \
        tests_run++; \
        clear_errors(); \
        reset_recursion_tracker(); \
        if (test_##name()) { \
            printf("✅ PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("❌ FAILED\n"); \
        } \
    } while(0)

// Test recursion tracker initialization
bool test_recursion_tracker_init() {
    init_recursion_tracker(500);
    
    return (get_recursion_limit() == 500) &&
           (get_current_depth() == 0) &&
           (!check_stack_overflow());
}

// Test stack depth tracking
bool test_stack_depth_tracking() {
    init_recursion_tracker(10);
    
    // Test entering function calls
    if (!enter_function_call()) return false;
    if (get_current_depth() != 1) return false;
    
    if (!enter_function_call()) return false;
    if (get_current_depth() != 2) return false;
    
    // Test exiting function calls
    exit_function_call();
    if (get_current_depth() != 1) return false;
    
    exit_function_call();
    if (get_current_depth() != 0) return false;
    
    return true;
}

// Test configurable recursion limit
bool test_configurable_recursion_limit() {
    // Test setting different limits
    set_recursion_limit(5);
    if (get_recursion_limit() != 5) return false;
    
    set_recursion_limit(1000);
    if (get_recursion_limit() != 1000) return false;
    
    set_recursion_limit(1);
    if (get_recursion_limit() != 1) return false;
    
    return true;
}

// Test stack overflow detection
bool test_stack_overflow_detection() {
    init_recursion_tracker(3); // Very low limit for testing
    
    // Should succeed for first 3 calls
    if (!enter_function_call()) return false; // depth 1
    if (!enter_function_call()) return false; // depth 2
    if (!enter_function_call()) return false; // depth 3
    
    // Fourth call should trigger overflow
    if (enter_function_call()) return false; // depth 4 - should fail
    
    // Check that overflow was detected
    if (!check_stack_overflow()) return false;
    
    // Check that error was reported
    if (!has_errors()) return false;
    
    return true;
}

// Test stack overflow error handling
bool test_stack_overflow_error_handling() {
    init_recursion_tracker(2);
    
    // Fill up the stack
    enter_function_call(); // depth 1
    enter_function_call(); // depth 2
    
    // This should trigger error handling
    bool overflow_handled = !enter_function_call(); // depth 3 - should fail
    
    // Verify error was reported
    bool error_reported = has_errors();
    bool overflow_detected = check_stack_overflow();
    
    return overflow_handled && error_reported && overflow_detected;
}

// Test recursion tracker reset
bool test_recursion_tracker_reset() {
    init_recursion_tracker(5);
    
    // Create some depth and overflow
    enter_function_call();
    enter_function_call();
    enter_function_call();
    enter_function_call();
    enter_function_call();
    enter_function_call(); // Should cause overflow
    
    // Reset should clear everything
    reset_recursion_tracker();
    
    return (get_current_depth() == 0) && (!check_stack_overflow());
}

// Test edge cases
bool test_edge_cases() {
    // Test with zero limit
    init_recursion_tracker(0);
    if (enter_function_call()) return false; // Should immediately fail
    
    // Test exit without enter
    reset_recursion_tracker();
    init_recursion_tracker(10);
    exit_function_call(); // Should not crash
    if (get_current_depth() != 0) return false; // Should stay at 0
    
    // Test multiple exits
    enter_function_call();
    exit_function_call();
    exit_function_call(); // Extra exit
    if (get_current_depth() != 0) return false; // Should stay at 0
    
    return true;
}

// Test recursion simulation
bool test_recursion_simulation() {
    printf("\n--- Recursion Simulation Test ---\n");
    
    init_recursion_tracker(5);
    
    // Simulate recursive function calls
    int successful_calls = 0;
    for (int i = 0; i < 10; i++) {
        if (enter_function_call()) {
            successful_calls++;
            printf("   Call %d: depth = %zu\n", i + 1, get_current_depth());
        } else {
            printf("   Call %d: OVERFLOW detected at depth %zu\n", i + 1, get_current_depth());
            break;
        }
    }
    
    printf("   Successful calls: %d\n", successful_calls);
    printf("--- End Recursion Simulation ---\n");
    
    return successful_calls == 5; // Should succeed exactly 5 times
}

// Main test runner
int main() {
    printf("🧪 Testing T1.5.1: Recursion Stack Protection\n");
    printf("=============================================\n\n");
    
    TEST(recursion_tracker_init);
    TEST(stack_depth_tracking);
    TEST(configurable_recursion_limit);
    TEST(stack_overflow_detection);
    TEST(stack_overflow_error_handling);
    TEST(recursion_tracker_reset);
    TEST(edge_cases);
    TEST(recursion_simulation);
    
    printf("\n=============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.5.1 recursion stack protection tests PASSED!\n");
        printf("T1.5.1: Recursion Stack Protection - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.5.1 tests FAILED!\n");
        return 1;
    }
}
