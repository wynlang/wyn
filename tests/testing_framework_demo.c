#include "test.h"
#include "safe_memory.h"
#include <stdio.h>

// Demo: Complete Testing Framework Integration
int main() {
    printf("🎉 Wyn Testing Framework - Complete Integration Demo\n");
    printf("====================================================\n\n");
    
    // 1. Register some functions for coverage tracking
    printf("1️⃣ Registering functions for coverage tracking...\n");
    register_function("calculate_sum", 42);
    register_function("validate_input", 58);
    register_function("process_data", 73);
    
    // 2. Mark some functions as tested
    printf("2️⃣ Marking functions as tested...\n");
    mark_function_tested("calculate_sum");
    mark_function_tested("validate_input");
    
    // 3. Demonstrate assertion library
    printf("3️⃣ Testing assertion library...\n");
    assert_equal(42, 42, "Basic equality test");
    assert_string_equal("hello", "hello", "String comparison test");
    assert_float_equal(3.14159, 3.14160, 0.001, "Float comparison with epsilon");
    assert_true(true, "Boolean assertion test");
    
    // 4. Generate and display coverage report
    printf("\n4️⃣ Generating coverage report...\n");
    CoverageReport* report = generate_coverage_report();
    if (report) {
        print_coverage_warnings(report);
        
        printf("\n5️⃣ Checking coverage requirements...\n");
        check_coverage_requirements(report, 70.0); // Should pass (66.7% >= 70% will fail)
        check_coverage_requirements(report, 60.0); // Should pass
        
        free_coverage_report(report);
    }
    
    // 6. Demonstrate test runner (simplified)
    printf("\n6️⃣ Test runner demonstration...\n");
    TestRunnerStats* stats = run_all_tests();
    if (stats) {
        print_test_results(stats);
        free_test_results(stats);
    }
    
    printf("\n====================================================\n");
    printf("✅ Testing Framework Integration Demo Complete!\n");
    printf("🎯 Ready for Test-Driven Development in Wyn!\n");
    
    return 0;
}
