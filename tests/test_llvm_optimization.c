#include "test.h"
#include "llvm_optimization.h"
#include <stdio.h>
#include <stdlib.h>

static int test_optimization_initialization() {
    printf("Testing LLVM optimization initialization...\n");
    
    int result = wyn_opt_init();
    if (result != 0) {
        printf("  FAIL: Optimization initialization failed\n");
        return 0;
    }
    
    printf("  PASS: Optimization system initialized\n");
    return 1;
}

static int test_optimization_levels() {
    printf("Testing optimization level configuration...\n");
    
    // Test all optimization levels
    for (int level = WYN_OPT_NONE; level <= WYN_OPT_AGGRESSIVE; level++) {
        int result = wyn_opt_set_level((WynOptLevel)level);
        if (result != 0) {
            printf("  FAIL: Failed to set optimization level %d\n", level);
            return 0;
        }
    }
    
    // Test invalid level
    int result = wyn_opt_set_level((WynOptLevel)99);
    if (result == 0) {
        printf("  FAIL: Should reject invalid optimization level\n");
        return 0;
    }
    
    printf("  PASS: Optimization level configuration successful\n");
    return 1;
}

static int test_pass_configuration() {
    printf("Testing optimization pass configuration...\n");
    
    // Test enabling passes
    for (int pass = WYN_PASS_DEAD_CODE_ELIMINATION; pass <= WYN_PASS_REASSOCIATION; pass++) {
        int result = wyn_opt_enable_pass((WynOptPass)pass);
        if (result != 0) {
            printf("  FAIL: Failed to enable pass %d\n", pass);
            return 0;
        }
    }
    
    // Test disabling passes
    for (int pass = WYN_PASS_DEAD_CODE_ELIMINATION; pass <= WYN_PASS_REASSOCIATION; pass++) {
        int result = wyn_opt_disable_pass((WynOptPass)pass);
        if (result != 0) {
            printf("  FAIL: Failed to disable pass %d\n", pass);
            return 0;
        }
    }
    
    printf("  PASS: Pass configuration successful\n");
    return 1;
}

static int test_individual_passes() {
    printf("Testing individual optimization passes...\n");
    
    // Re-enable passes for testing
    wyn_opt_set_level(WYN_OPT_DEFAULT);
    
    // Test dead code elimination
    int result = wyn_opt_run_dead_code_elimination((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Dead code elimination failed\n");
        return 0;
    }
    
    // Test constant folding
    result = wyn_opt_run_constant_folding((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Constant folding failed\n");
        return 0;
    }
    
    // Test function inlining
    result = wyn_opt_run_function_inlining((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Function inlining failed\n");
        return 0;
    }
    
    // Test loop optimization
    result = wyn_opt_run_loop_optimization((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Loop optimization failed\n");
        return 0;
    }
    
    // Test vectorization
    result = wyn_opt_run_vectorization((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Vectorization failed\n");
        return 0;
    }
    
    printf("  PASS: Individual passes successful\n");
    return 1;
}

static int test_optimization_pipeline() {
    printf("Testing complete optimization pipeline...\n");
    
    // Reset stats
    wyn_opt_reset_stats();
    
    // Run complete optimization pipeline
    int result = wyn_opt_run_passes((void*)0x1);
    if (result != 0) {
        printf("  FAIL: Optimization pipeline failed\n");
        return 0;
    }
    
    // Check that statistics were updated
    WynOptStats stats = wyn_opt_get_stats();
    if (stats.optimization_time_ms <= 0) {
        printf("  FAIL: Optimization time not recorded\n");
        return 0;
    }
    
    printf("  PASS: Optimization pipeline successful\n");
    return 1;
}

static int test_lto_configuration() {
    printf("Testing link-time optimization configuration...\n");
    
    // Test enabling LTO
    int result = wyn_opt_enable_lto(true);
    if (result != 0) {
        printf("  FAIL: Failed to enable LTO\n");
        return 0;
    }
    
    // Test LTO execution
    result = wyn_opt_run_lto((void*)0x1);
    if (result != 0) {
        printf("  FAIL: LTO execution failed\n");
        return 0;
    }
    
    // Test disabling LTO
    result = wyn_opt_enable_lto(false);
    if (result != 0) {
        printf("  FAIL: Failed to disable LTO\n");
        return 0;
    }
    
    printf("  PASS: LTO configuration successful\n");
    return 1;
}

static int test_pgo_configuration() {
    printf("Testing profile-guided optimization configuration...\n");
    
    // Test enabling PGO
    int result = wyn_opt_enable_pgo("/tmp/profile.data");
    if (result != 0) {
        printf("  FAIL: Failed to enable PGO\n");
        return 0;
    }
    
    // Test PGO execution
    result = wyn_opt_run_pgo((void*)0x1);
    if (result != 0) {
        printf("  FAIL: PGO execution failed\n");
        return 0;
    }
    
    // Test disabling PGO
    result = wyn_opt_enable_pgo(NULL);
    if (result != 0) {
        printf("  FAIL: Failed to disable PGO\n");
        return 0;
    }
    
    printf("  PASS: PGO configuration successful\n");
    return 1;
}

static int test_optimization_statistics() {
    printf("Testing optimization statistics...\n");
    
    // Reset and run optimizations
    wyn_opt_reset_stats();
    wyn_opt_set_level(WYN_OPT_DEFAULT);
    wyn_opt_run_passes((void*)0x1);
    
    // Get statistics
    WynOptStats stats = wyn_opt_get_stats();
    
    // Verify statistics are reasonable
    if (stats.dead_instructions_removed < 0 || stats.dead_instructions_removed > 1000) {
        printf("  FAIL: Invalid dead instructions count: %d\n", stats.dead_instructions_removed);
        return 0;
    }
    
    if (stats.constants_folded < 0 || stats.constants_folded > 100) {
        printf("  FAIL: Invalid constants folded count: %d\n", stats.constants_folded);
        return 0;
    }
    
    if (stats.optimization_time_ms < 0 || stats.optimization_time_ms > 10000) {
        printf("  FAIL: Invalid optimization time: %.2f ms\n", stats.optimization_time_ms);
        return 0;
    }
    
    // Test statistics printing
    wyn_opt_print_stats();
    
    printf("  PASS: Optimization statistics valid\n");
    return 1;
}

static int test_error_handling() {
    printf("Testing optimization error handling...\n");
    
    // Test with NULL module
    int result = wyn_opt_run_passes(NULL);
    if (result == 0) {
        printf("  FAIL: Should reject NULL module\n");
        return 0;
    }
    
    // Test invalid pass numbers
    result = wyn_opt_enable_pass((WynOptPass)999);
    if (result == 0) {
        printf("  FAIL: Should reject invalid pass number\n");
        return 0;
    }
    
    printf("  PASS: Error handling successful\n");
    return 1;
}

int main() {
    printf("=== T8.3.1: Advanced LLVM Optimization Passes Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_optimization_initialization()) passed_tests++;
    total_tests++; if (test_optimization_levels()) passed_tests++;
    total_tests++; if (test_pass_configuration()) passed_tests++;
    total_tests++; if (test_individual_passes()) passed_tests++;
    total_tests++; if (test_optimization_pipeline()) passed_tests++;
    total_tests++; if (test_lto_configuration()) passed_tests++;
    total_tests++; if (test_pgo_configuration()) passed_tests++;
    total_tests++; if (test_optimization_statistics()) passed_tests++;
    total_tests++; if (test_error_handling()) passed_tests++;
    
    // Cleanup
    wyn_opt_cleanup();
    
    // Print summary
    printf("\n=== LLVM Optimization Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All LLVM optimization tests passed!\n");
        printf("⚡ Advanced optimization passes ready for production\n");
        return 0;
    } else {
        printf("❌ Some LLVM optimization tests failed\n");
        return 1;
    }
}
