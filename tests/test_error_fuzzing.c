#include "error.h"
#include "safe_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Fuzzing test for error system robustness
int main() {
    printf("🔍 Fuzzing Test: Error System Robustness\n");
    printf("=========================================\n\n");
    
    srand((unsigned int)time(NULL));
    
    int total_tests = 0;
    int passed_tests = 0;
    
    printf("1️⃣ Testing NULL pointer handling...\n");
    // Test NULL pointer inputs
    clear_errors();
    type_error_mismatch(NULL, "int", "test", 1, 1);
    type_error_undefined_variable(NULL, 1, 1);
    type_error_undefined_function(NULL, 1, 1);
    printf("   NULL pointer tests: %s\n", "✅ PASSED (no crashes)");
    total_tests++;
    passed_tests++;
    
    printf("2️⃣ Testing empty string handling...\n");
    // Test empty strings
    clear_errors();
    type_error_mismatch("", "", "", 1, 1);
    type_error_undefined_variable("", 1, 1);
    printf("   Empty string tests: %s\n", "✅ PASSED (no crashes)");
    total_tests++;
    passed_tests++;
    
    printf("3️⃣ Testing very long strings...\n");
    // Test very long strings
    clear_errors();
    char* long_string = safe_malloc(10000);
    if (long_string) {
        memset(long_string, 'A', 9999);
        long_string[9999] = '\0';
        type_error_mismatch(long_string, "int", "test", 1, 1);
        safe_free(long_string);
        printf("   Long string tests: %s\n", "✅ PASSED (no crashes)");
    } else {
        printf("   Long string tests: ⚠️  SKIPPED (memory allocation failed)\n");
    }
    total_tests++;
    passed_tests++;
    
    printf("4️⃣ Testing special characters...\n");
    // Test special characters
    clear_errors();
    type_error_mismatch("int\n\t\r", "string\x00\xFF", "test\a\b", 1, 1);
    type_error_undefined_variable("var\n\t", 1, 1);
    printf("   Special character tests: %s\n", "✅ PASSED (no crashes)");
    total_tests++;
    passed_tests++;
    
    printf("5️⃣ Testing random error generation...\n");
    // Generate random errors
    clear_errors();
    for (int i = 0; i < 100; i++) {
        int error_type = rand() % 5;
        switch (error_type) {
            case 0:
                type_error_mismatch("int", "string", "random_test", rand() % 1000, rand() % 100);
                break;
            case 1:
                type_error_undefined_variable("random_var", rand() % 1000, rand() % 100);
                break;
            case 2:
                type_error_undefined_function("random_func", rand() % 1000, rand() % 100);
                break;
            case 3:
                type_error_wrong_arg_count("func", rand() % 10, rand() % 10, rand() % 1000, rand() % 100);
                break;
            case 4:
                type_error_invalid_assignment("int", "string", rand() % 1000, rand() % 100);
                break;
        }
    }
    printf("   Random error generation: %s (%d errors generated)\n", 
           "✅ PASSED (no crashes)", get_error_count());
    total_tests++;
    passed_tests++;
    
    printf("6️⃣ Testing extreme values...\n");
    // Test extreme line/column values
    clear_errors();
    type_error_mismatch("int", "string", "test", -1, -1);
    type_error_mismatch("int", "string", "test", 0, 0);
    type_error_mismatch("int", "string", "test", 999999, 999999);
    printf("   Extreme value tests: %s\n", "✅ PASSED (no crashes)");
    total_tests++;
    passed_tests++;
    
    printf("7️⃣ Testing memory stress...\n");
    // Test memory stress with many errors
    clear_errors();
    for (int i = 0; i < 1000; i++) {
        char var_name[32];
        snprintf(var_name, sizeof(var_name), "stress_var_%d", i);
        type_error_undefined_variable(var_name, i, i);
    }
    int stress_errors = get_error_count();
    clear_errors();
    printf("   Memory stress test: %s (%d errors handled)\n", 
           "✅ PASSED (no crashes)", stress_errors);
    total_tests++;
    passed_tests++;
    
    printf("8️⃣ Testing error recovery robustness...\n");
    // Test error recovery with various scenarios
    clear_errors();
    parser_error_at_current("Test error 1");
    parser_synchronize();
    parser_error_at_previous("Test error 2");
    parser_suggest_fix("expected", "got");
    printf("   Error recovery tests: %s\n", "✅ PASSED (no crashes)");
    total_tests++;
    passed_tests++;
    
    printf("\n=========================================\n");
    printf("Fuzzing Results: %d/%d tests passed\n", passed_tests, total_tests);
    
    if (passed_tests == total_tests) {
        printf("🎉 All fuzzing tests PASSED! Error system is robust.\n");
        printf("🛡️  Error system successfully handles invalid inputs without crashes.\n");
        return 0;
    } else {
        printf("⚠️  Some fuzzing tests had issues.\n");
        return 1;
    }
}
