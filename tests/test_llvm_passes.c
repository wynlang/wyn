#include "../src/llvm_passes.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_pass_manager_creation() {
    printf("Testing pass manager creation...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    assert(manager->pass_count == 0);
    assert(manager->opt_level == WYN_OPT_LEVEL_DEFAULT);
    
    wyn_pass_manager_free(manager);
    printf("✓ Pass manager creation test passed\n");
}

void test_optimization_levels() {
    printf("Testing optimization levels...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    
    // Test O0 (no optimization)
    assert(wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_NONE) == true);
    assert(manager->opt_level == WYN_OPT_LEVEL_NONE);
    size_t o0_passes = manager->pass_count;
    
    // Reset for next test
    wyn_pass_manager_free(manager);
    manager = wyn_pass_manager_new();
    
    // Test O1 (basic optimization)
    assert(wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_LESS) == true);
    assert(manager->opt_level == WYN_OPT_LEVEL_LESS);
    size_t o1_passes = manager->pass_count;
    assert(o1_passes > o0_passes);
    
    // Reset for next test
    wyn_pass_manager_free(manager);
    manager = wyn_pass_manager_new();
    
    // Test O2 (default optimization)
    assert(wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_DEFAULT) == true);
    assert(manager->opt_level == WYN_OPT_LEVEL_DEFAULT);
    size_t o2_passes = manager->pass_count;
    assert(o2_passes > o1_passes);
    
    // Reset for next test
    wyn_pass_manager_free(manager);
    manager = wyn_pass_manager_new();
    
    // Test O3 (aggressive optimization)
    assert(wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_AGGRESSIVE) == true);
    assert(manager->opt_level == WYN_OPT_LEVEL_AGGRESSIVE);
    size_t o3_passes = manager->pass_count;
    assert(o3_passes > o2_passes);
    
    wyn_pass_manager_free(manager);
    printf("✓ Optimization levels test passed\n");
}

void test_individual_passes() {
    printf("Testing individual pass creation...\n");
    
    // Test dead code elimination pass
    WynOptimizationPass* dce_pass = wyn_create_dead_code_elimination_pass();
    assert(dce_pass != NULL);
    assert(dce_pass->type == WYN_PASS_DEAD_CODE_ELIMINATION);
    assert(dce_pass->name != NULL);
    assert(dce_pass->description != NULL);
    assert(dce_pass->run != NULL);
    assert(dce_pass->validate != NULL);
    assert(dce_pass->cleanup != NULL);
    
    // Test constant folding pass
    WynOptimizationPass* cf_pass = wyn_create_constant_folding_pass();
    assert(cf_pass != NULL);
    assert(cf_pass->type == WYN_PASS_CONSTANT_FOLDING);
    assert(cf_pass->name != NULL);
    
    // Test function inlining pass
    WynOptimizationPass* inline_pass = wyn_create_function_inlining_pass();
    assert(inline_pass != NULL);
    assert(inline_pass->type == WYN_PASS_FUNCTION_INLINING);
    
    // Test vectorization pass
    WynOptimizationPass* vec_pass = wyn_create_vectorization_pass();
    assert(vec_pass != NULL);
    assert(vec_pass->type == WYN_PASS_VECTORIZATION);
    
    // Cleanup
    dce_pass->cleanup(dce_pass);
    free(dce_pass);
    cf_pass->cleanup(cf_pass);
    free(cf_pass);
    inline_pass->cleanup(inline_pass);
    free(inline_pass);
    vec_pass->cleanup(vec_pass);
    free(vec_pass);
    
    printf("✓ Individual passes test passed\n");
}

void test_pass_execution() {
    printf("Testing pass execution...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    
    // Add some passes
    assert(wyn_pass_manager_add_pass(manager, WYN_PASS_DEAD_CODE_ELIMINATION) == true);
    assert(wyn_pass_manager_add_pass(manager, WYN_PASS_CONSTANT_FOLDING) == true);
    assert(manager->pass_count == 2);
    
    // Mock module for testing
    void* mock_module = (void*)0x12345678;
    
    // Run passes (should succeed with stub implementations)
    assert(wyn_pass_manager_run_passes(manager, mock_module) == true);
    
    wyn_pass_manager_free(manager);
    printf("✓ Pass execution test passed\n");
}

void test_profile_guided_optimization() {
    printf("Testing profile-guided optimization...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    
    // Test enabling PGO
    assert(wyn_pass_manager_enable_pgo(manager, true) == true);
    assert(manager->profile_guided_optimization == true);
    
    // Test disabling PGO
    assert(wyn_pass_manager_enable_pgo(manager, false) == true);
    assert(manager->profile_guided_optimization == false);
    
    // Test profile data creation
    WynProfileData* profile = wyn_profile_data_create("test_function", 1000, 15.5);
    assert(profile != NULL);
    assert(strcmp(profile->function_name, "test_function") == 0);
    assert(profile->call_count == 1000);
    assert(profile->execution_time_ms == 15.5);
    assert(profile->is_hot_function == true); // Should be hot (> 10ms)
    
    wyn_profile_data_free(profile);
    free(profile);
    
    wyn_pass_manager_free(manager);
    printf("✓ Profile-guided optimization test passed\n");
}

void test_link_time_optimization() {
    printf("Testing link-time optimization...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    
    // Test enabling LTO
    assert(wyn_pass_manager_enable_lto(manager, true) == true);
    assert(manager->link_time_optimization == true);
    
    // Test disabling LTO
    assert(wyn_pass_manager_enable_lto(manager, false) == true);
    assert(manager->link_time_optimization == false);
    
    wyn_pass_manager_free(manager);
    printf("✓ Link-time optimization test passed\n");
}

void test_pass_configuration() {
    printf("Testing pass configuration...\n");
    
    // Test pass config creation
    WynPassConfig* config = wyn_pass_config_create(WYN_PASS_FUNCTION_INLINING, true, 10);
    assert(config != NULL);
    assert(config->type == WYN_PASS_FUNCTION_INLINING);
    assert(config->enabled == true);
    assert(config->priority == 10);
    
    wyn_pass_config_free(config);
    
    printf("✓ Pass configuration test passed\n");
}

void test_advanced_optimization_config() {
    printf("Testing advanced optimization configuration...\n");
    
    // Test default config for different levels
    WynAdvancedOptConfig aggressive_config = wyn_get_default_advanced_config(WYN_OPT_LEVEL_AGGRESSIVE);
    assert(aggressive_config.aggressive_inlining == true);
    assert(aggressive_config.cross_module_optimization == true);
    assert(aggressive_config.whole_program_optimization == true);
    assert(aggressive_config.profile_guided_inlining == true);
    assert(aggressive_config.inline_threshold == 1000);
    assert(aggressive_config.unroll_threshold == 8);
    
    WynAdvancedOptConfig default_config = wyn_get_default_advanced_config(WYN_OPT_LEVEL_DEFAULT);
    assert(aggressive_config.inline_threshold > default_config.inline_threshold);
    assert(aggressive_config.unroll_threshold > default_config.unroll_threshold);
    
    printf("✓ Advanced optimization configuration test passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Create test profile data
    WynProfileData profile_data[3];
    profile_data[0].function_name = strdup("hot_function");
    profile_data[0].call_count = 5000;
    profile_data[0].execution_time_ms = 25.0;
    profile_data[0].is_hot_function = true;
    
    profile_data[1].function_name = strdup("cold_function");
    profile_data[1].call_count = 10;
    profile_data[1].execution_time_ms = 1.0;
    profile_data[1].is_hot_function = false;
    
    profile_data[2].function_name = strdup("medium_function");
    profile_data[2].call_count = 500;
    profile_data[2].execution_time_ms = 8.0;
    profile_data[2].is_hot_function = false;
    
    // Test utility functions
    assert(wyn_is_hot_function(&profile_data[0], "hot_function") == true);
    assert(wyn_is_hot_function(&profile_data[1], "cold_function") == false);
    
    double exec_time = wyn_get_function_execution_time(profile_data, 3, "hot_function");
    assert(exec_time == 25.0);
    
    size_t call_count = wyn_get_function_call_count(profile_data, 3, "cold_function");
    assert(call_count == 10);
    
    // Cleanup
    for (int i = 0; i < 3; i++) {
        free(profile_data[i].function_name);
    }
    
    printf("✓ Utility functions test passed\n");
}

void test_pass_statistics() {
    printf("Testing pass statistics...\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    assert(manager != NULL);
    
    // Set up manager with some passes
    wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_DEFAULT);
    wyn_pass_manager_enable_pgo(manager, true);
    wyn_pass_manager_enable_lto(manager, true);
    
    // Print statistics (visual verification)
    printf("--- Pass Statistics Output ---\n");
    wyn_print_pass_statistics(manager);
    printf("--- End Statistics Output ---\n");
    
    wyn_pass_manager_free(manager);
    printf("✓ Pass statistics test passed\n");
}

int main() {
    printf("Running Advanced LLVM Passes Tests\n");
    printf("==================================\n");
    
    test_pass_manager_creation();
    test_optimization_levels();
    test_individual_passes();
    test_pass_execution();
    test_profile_guided_optimization();
    test_link_time_optimization();
    test_pass_configuration();
    test_advanced_optimization_config();
    test_utility_functions();
    test_pass_statistics();
    
    printf("\n✓ All advanced LLVM passes tests passed!\n");
    return 0;
}
