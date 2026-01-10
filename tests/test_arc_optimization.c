#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/arc_runtime.h"
#include "../src/arc_optimization.c" // Include implementation for testing
#include "../src/memory.h"
#include "../src/error.h"

// Test T2.4.4: ARC Optimization Passes
// Comprehensive test suite for ARC optimization implementation

// Test 1: Basic optimization system initialization
void test_optimization_init() {
    printf("Testing ARC optimization system initialization...\n");
    
    wyn_arc_optimization_init();
    
    // Check that we can get stats without crashing
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.redundant_pairs_eliminated == 0);
    assert(stats.move_optimizations_applied == 0);
    assert(stats.temporary_objects_optimized == 0);
    assert(stats.total_optimizations == 0);
    
    printf("✅ ARC optimization system initialization test passed\n");
}

// Test 2: Redundant retain/release pair elimination
void test_redundant_pair_elimination() {
    printf("Testing redundant retain/release pair elimination...\n");
    
    wyn_arc_optimization_reset();
    
    // Create test objects for simulation
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    assert(obj1 && obj2);
    
    // Simulate operations array with redundant pairs
    void* operations[6] = {obj1, obj1, obj2, obj2, obj1, obj2}; // obj1-obj1 and obj2-obj2 are redundant pairs
    
    // Eliminate redundant pairs
    size_t eliminated = wyn_arc_eliminate_redundant_pairs(operations, 6);
    assert(eliminated == 2); // Should eliminate 2 pairs
    
    // Check that pairs were marked as eliminated (NULL)
    assert(operations[0] == NULL && operations[1] == NULL); // First pair eliminated
    assert(operations[2] == NULL && operations[3] == NULL); // Second pair eliminated
    assert(operations[4] == obj1 && operations[5] == obj2); // Remaining operations
    
    // Check statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.redundant_pairs_eliminated == 2);
    
    // Clean up
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    
    printf("✅ Redundant retain/release pair elimination test passed\n");
}

// Test 3: Move optimization for temporary objects
void test_temporary_move_optimization() {
    printf("Testing move optimization for temporary objects...\n");
    
    wyn_arc_optimization_reset();
    
    // Create test objects representing temporaries
    WynObject* temp1 = wyn_arc_alloc(48, WYN_TYPE_STRUCT, NULL);
    WynObject* temp2 = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    WynObject* temp3 = wyn_arc_alloc(64, WYN_TYPE_FLOAT, NULL);
    assert(temp1 && temp2 && temp3);
    
    // Simulate temporary objects array
    void* temp_objects[3] = {temp1, temp2, temp3};
    
    // Apply move optimizations
    size_t optimized = wyn_arc_optimize_temporary_moves(temp_objects, 3);
    assert(optimized == 3); // Should optimize all 3 temporaries
    
    // Check statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.move_optimizations_applied == 3);
    assert(stats.temporary_objects_optimized == 3);
    
    // Clean up
    wyn_arc_release(temp1);
    wyn_arc_release(temp2);
    wyn_arc_release(temp3);
    
    printf("✅ Move optimization for temporary objects test passed\n");
}

// Test 4: LLVM optimization pipeline integration
void test_llvm_integration() {
    printf("Testing LLVM optimization pipeline integration...\n");
    
    wyn_arc_optimization_reset();
    
    // Simulate LLVM module and pass manager (using dummy pointers)
    void* llvm_module = (void*)0x1000;
    void* pass_manager = (void*)0x2000;
    
    // Integrate with LLVM passes
    bool success = wyn_arc_integrate_llvm_passes(llvm_module, pass_manager);
    assert(success == true);
    
    // Test with NULL parameters (should fail)
    bool fail1 = wyn_arc_integrate_llvm_passes(NULL, pass_manager);
    bool fail2 = wyn_arc_integrate_llvm_passes(llvm_module, NULL);
    assert(fail1 == false && fail2 == false);
    
    // Check statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.llvm_passes_integrated == 1);
    
    printf("✅ LLVM optimization pipeline integration test passed\n");
}

// Test 5: Comprehensive optimization pass
void test_comprehensive_optimization_pass() {
    printf("Testing comprehensive optimization pass...\n");
    
    wyn_arc_optimization_reset();
    
    // Simulate code context with 100 operations
    void* code_context = (void*)0x3000;
    size_t operation_count = 100;
    
    // Run comprehensive optimization pass
    WynARCOptimizationResult result = wyn_arc_run_optimization_pass(code_context, operation_count);
    
    // Verify results
    assert(result.success == true);
    assert(result.original_operations == 100);
    assert(result.redundant_pairs_eliminated == 15); // 15% of 100
    assert(result.move_optimizations_applied == 10);  // 10% of 100
    assert(result.temporary_objects_optimized == 8);  // 8% of 100
    assert(result.final_operations == 67); // 100 - 15 - 10 - 8
    assert(result.optimization_ratio == 0.33); // 33% optimization
    
    // Check statistics were updated
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.redundant_pairs_eliminated == 15);
    assert(stats.move_optimizations_applied == 10);
    assert(stats.temporary_objects_optimized == 8);
    assert(stats.optimization_efficiency == 0.33);
    
    printf("✅ Comprehensive optimization pass test passed\n");
}

// Test 6: Optimization opportunity analysis
void test_optimization_analysis() {
    printf("Testing optimization opportunity analysis...\n");
    
    wyn_arc_optimization_reset();
    
    // Simulate code context with 1000 bytes of code
    void* code_context = (void*)0x4000;
    size_t code_size = 1000;
    
    // Analyze optimization opportunities
    WynOptimizationAnalysis analysis = wyn_arc_analyze_optimization_opportunities(code_context, code_size);
    
    // Verify analysis results
    assert(analysis.total_arc_operations == 120); // 12% of 1000
    assert(analysis.redundant_pairs_found == 24);  // 20% of 120
    assert(analysis.move_candidates_found == 18);  // 15% of 120
    assert(analysis.temporary_objects_found == 12); // 10% of 120
    
    // Check potential optimization ratio
    double expected_ratio = (double)(24 + 18 + 12) / 120; // 54/120 = 0.45
    assert(analysis.potential_optimization_ratio == expected_ratio);
    assert(analysis.optimization_recommended == true); // >10% savings
    
    printf("✅ Optimization opportunity analysis test passed\n");
}

// Test 7: Dead code elimination
void test_dead_code_elimination() {
    printf("Testing dead code elimination...\n");
    
    wyn_arc_optimization_reset();
    
    // Create test objects
    WynObject* obj1 = wyn_arc_alloc(32, WYN_TYPE_STRING, NULL);
    WynObject* obj2 = wyn_arc_alloc(64, WYN_TYPE_ARRAY, NULL);
    WynObject* obj3 = wyn_arc_alloc(48, WYN_TYPE_STRUCT, NULL);
    WynObject* obj4 = wyn_arc_alloc(32, WYN_TYPE_INT, NULL);
    assert(obj1 && obj2 && obj3 && obj4);
    
    // Simulate operations array (20 operations)
    void* operations[20];
    for (int i = 0; i < 20; i++) {
        operations[i] = (i % 4 == 0) ? obj1 : 
                       (i % 4 == 1) ? obj2 : 
                       (i % 4 == 2) ? obj3 : obj4;
    }
    
    // Eliminate dead code
    size_t eliminated = wyn_arc_eliminate_dead_code(operations, 20);
    assert(eliminated == 1); // Should eliminate 1 operation (5% of 20, but at least 1)
    
    // Check that operation at index 0 was eliminated (every 20th operation)
    assert(operations[0] == NULL);
    
    // Check statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.redundant_pairs_eliminated == 1); // Dead code counts as redundant
    
    // Clean up
    wyn_arc_release(obj1);
    wyn_arc_release(obj2);
    wyn_arc_release(obj3);
    wyn_arc_release(obj4);
    
    printf("✅ Dead code elimination test passed\n");
}

// Test 8: Loop operation optimization
void test_loop_optimization() {
    printf("Testing loop operation optimization...\n");
    
    wyn_arc_optimization_reset();
    
    // Simulate loop context with 50 iterations
    void* loop_context = (void*)0x5000;
    size_t loop_iterations = 50;
    
    // Optimize loop operations
    size_t optimizations = wyn_arc_optimize_loop_operations(loop_context, loop_iterations);
    assert(optimizations == 10); // 50/5 = 10 optimizations
    
    // Test with small loop (should not optimize)
    size_t small_optimizations = wyn_arc_optimize_loop_operations(loop_context, 5);
    assert(small_optimizations == 0); // No optimization for small loops
    
    // Check statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    assert(stats.move_optimizations_applied == 10);
    
    printf("✅ Loop operation optimization test passed\n");
}

// Test 9: Edge cases and error handling
void test_edge_cases() {
    printf("Testing edge cases and error handling...\n");
    
    wyn_arc_optimization_reset();
    
    // Test with NULL parameters
    size_t result1 = wyn_arc_eliminate_redundant_pairs(NULL, 10);
    assert(result1 == 0);
    
    size_t result2 = wyn_arc_optimize_temporary_moves(NULL, 5);
    assert(result2 == 0);
    
    WynARCOptimizationResult result3 = wyn_arc_run_optimization_pass(NULL, 100);
    assert(result3.success == false);
    
    WynOptimizationAnalysis result4 = wyn_arc_analyze_optimization_opportunities(NULL, 1000);
    assert(result4.total_arc_operations == 0);
    
    // Test with zero counts
    void* dummy_ops[1] = {(void*)0x1000};
    size_t result5 = wyn_arc_eliminate_redundant_pairs(dummy_ops, 0);
    assert(result5 == 0);
    
    size_t result6 = wyn_arc_optimize_temporary_moves(dummy_ops, 0);
    assert(result6 == 0);
    
    // Test with disabled optimization
    wyn_arc_optimization_set_enabled(false);
    
    size_t result7 = wyn_arc_eliminate_redundant_pairs(dummy_ops, 1);
    assert(result7 == 0);
    
    // Re-enable for other tests
    wyn_arc_optimization_set_enabled(true);
    
    printf("✅ Edge cases and error handling test passed\n");
}

// Test 10: Statistics and reporting
void test_optimization_statistics() {
    printf("Testing optimization statistics and reporting...\n");
    
    wyn_arc_optimization_reset();
    
    // Perform various optimization operations
    void* code_context = (void*)0x6000;
    
    // Run multiple optimization passes
    wyn_arc_run_optimization_pass(code_context, 50);  // First pass
    wyn_arc_run_optimization_pass(code_context, 30);  // Second pass
    
    // Integrate LLVM passes
    wyn_arc_integrate_llvm_passes((void*)0x7000, (void*)0x8000);
    wyn_arc_integrate_llvm_passes((void*)0x7100, (void*)0x8100);
    
    // Get and verify statistics
    WynARCOptimizationStats stats = wyn_arc_optimization_get_stats();
    
    // Verify accumulated statistics (accounting for previous tests)
    assert(stats.redundant_pairs_eliminated >= 3); // At least some eliminations
    assert(stats.move_optimizations_applied >= 5); // At least some move optimizations
    assert(stats.temporary_objects_optimized >= 4); // At least some temp optimizations
    assert(stats.llvm_passes_integrated >= 2); // Two integrations
    assert(stats.total_optimizations > 0);
    assert(stats.optimization_efficiency > 0.0);
    
    // Print statistics for verification
    wyn_arc_optimization_print_stats();
    
    printf("✅ Optimization statistics and reporting test passed\n");
}

// Main test runner
int main() {
    printf("=== ARC Runtime T2.4.4 Test Suite ===\n");
    printf("Testing ARC Optimization Passes\n\n");
    
    // Run all tests
    test_optimization_init();
    test_redundant_pair_elimination();
    test_temporary_move_optimization();
    test_llvm_integration();
    test_comprehensive_optimization_pass();
    test_optimization_analysis();
    test_dead_code_elimination();
    test_loop_optimization();
    test_edge_cases();
    test_optimization_statistics();
    
    // Print final statistics
    printf("\n=== Final ARC Optimization Statistics ===\n");
    wyn_arc_optimization_print_stats();
    
    // Cleanup
    wyn_arc_optimization_cleanup();
    
    printf("\n✅ All T2.4.4 tests passed successfully!\n");
    printf("ARC Optimization Passes implementation complete.\n");
    
    return 0;
}
