#include "../src/llvm_passes.h"
#include <stdio.h>
#include <stdlib.h>

// Example: Basic pass manager usage
void example_basic_pass_manager() {
    printf("=== Basic Pass Manager Example ===\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    
    // Set optimization level to O2 (default)
    wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_DEFAULT);
    
    printf("Pass manager created with %zu passes\n", manager->pass_count);
    printf("Optimization level: %d\n", manager->opt_level);
    
    // Print current passes
    wyn_print_pass_statistics(manager);
    
    wyn_pass_manager_free(manager);
}

// Example: Custom optimization pipeline
void example_custom_optimization_pipeline() {
    printf("\n=== Custom Optimization Pipeline Example ===\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    
    // Start with no optimizations
    wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_NONE);
    
    // Add specific passes in custom order
    printf("Adding custom passes:\n");
    
    wyn_pass_manager_add_pass(manager, WYN_PASS_DEAD_CODE_ELIMINATION);
    printf("  + Dead Code Elimination\n");
    
    wyn_pass_manager_add_pass(manager, WYN_PASS_CONSTANT_FOLDING);
    printf("  + Constant Folding\n");
    
    wyn_pass_manager_add_pass(manager, WYN_PASS_FUNCTION_INLINING);
    printf("  + Function Inlining\n");
    
    wyn_pass_manager_add_pass(manager, WYN_PASS_LOOP_UNROLLING);
    printf("  + Loop Unrolling\n");
    
    wyn_pass_manager_add_pass(manager, WYN_PASS_VECTORIZATION);
    printf("  + Vectorization\n");
    
    printf("\nCustom pipeline has %zu passes\n", manager->pass_count);
    
    wyn_pass_manager_free(manager);
}

// Example: Profile-guided optimization
void example_profile_guided_optimization() {
    printf("\n=== Profile-Guided Optimization Example ===\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    
    // Enable profile-guided optimization
    wyn_pass_manager_enable_pgo(manager, true);
    printf("Profile-guided optimization: %s\n", 
           manager->profile_guided_optimization ? "enabled" : "disabled");
    
    // Create sample profile data
    WynProfileData* hot_func = wyn_profile_data_create("compute_heavy", 10000, 150.5);
    WynProfileData* cold_func = wyn_profile_data_create("init_once", 1, 0.1);
    WynProfileData* medium_func = wyn_profile_data_create("process_data", 500, 25.0);
    
    printf("\nProfile data:\n");
    printf("  %s: %zu calls, %.2f ms (hot: %s)\n", 
           hot_func->function_name, hot_func->call_count, 
           hot_func->execution_time_ms, hot_func->is_hot_function ? "yes" : "no");
    
    printf("  %s: %zu calls, %.2f ms (hot: %s)\n", 
           cold_func->function_name, cold_func->call_count, 
           cold_func->execution_time_ms, cold_func->is_hot_function ? "yes" : "no");
    
    printf("  %s: %zu calls, %.2f ms (hot: %s)\n", 
           medium_func->function_name, medium_func->call_count, 
           medium_func->execution_time_ms, medium_func->is_hot_function ? "yes" : "no");
    
    // Check if functions are hot
    printf("\nHot function analysis:\n");
    printf("  compute_heavy is hot: %s\n", 
           wyn_is_hot_function(hot_func, "compute_heavy") ? "yes" : "no");
    printf("  init_once is hot: %s\n", 
           wyn_is_hot_function(cold_func, "init_once") ? "yes" : "no");
    
    // Cleanup
    wyn_profile_data_free(hot_func);
    free(hot_func);
    wyn_profile_data_free(cold_func);
    free(cold_func);
    wyn_profile_data_free(medium_func);
    free(medium_func);
    
    wyn_pass_manager_free(manager);
}

// Example: Optimization level comparison
void example_optimization_level_comparison() {
    printf("\n=== Optimization Level Comparison Example ===\n");
    
    WynOptLevel levels[] = {
        WYN_OPT_LEVEL_NONE,
        WYN_OPT_LEVEL_LESS,
        WYN_OPT_LEVEL_DEFAULT,
        WYN_OPT_LEVEL_AGGRESSIVE,
        WYN_OPT_LEVEL_SIZE
    };
    
    const char* level_names[] = {
        "None (-O0)",
        "Less (-O1)", 
        "Default (-O2)",
        "Aggressive (-O3)",
        "Size (-Os)"
    };
    
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        WynPassManager* manager = wyn_pass_manager_new();
        wyn_pass_manager_set_opt_level(manager, levels[i]);
        
        printf("%s: %zu passes\n", level_names[i], manager->pass_count);
        
        wyn_pass_manager_free(manager);
    }
}

// Example: Advanced optimization configuration
void example_advanced_optimization_config() {
    printf("\n=== Advanced Optimization Configuration Example ===\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    
    // Get default config for aggressive optimization
    WynAdvancedOptConfig config = wyn_get_default_advanced_config(WYN_OPT_LEVEL_AGGRESSIVE);
    
    printf("Aggressive optimization configuration:\n");
    printf("  Aggressive inlining: %s\n", config.aggressive_inlining ? "enabled" : "disabled");
    printf("  Cross-module optimization: %s\n", config.cross_module_optimization ? "enabled" : "disabled");
    printf("  Whole program optimization: %s\n", config.whole_program_optimization ? "enabled" : "disabled");
    printf("  Profile-guided inlining: %s\n", config.profile_guided_inlining ? "enabled" : "disabled");
    printf("  Inline threshold: %zu\n", config.inline_threshold);
    printf("  Unroll threshold: %zu\n", config.unroll_threshold);
    
    // Compare with default config
    WynAdvancedOptConfig default_config = wyn_get_default_advanced_config(WYN_OPT_LEVEL_DEFAULT);
    
    printf("\nDefault optimization configuration:\n");
    printf("  Inline threshold: %zu\n", default_config.inline_threshold);
    printf("  Unroll threshold: %zu\n", default_config.unroll_threshold);
    
    wyn_pass_manager_free(manager);
}

// Example: Link-time optimization
void example_link_time_optimization() {
    printf("\n=== Link-Time Optimization Example ===\n");
    
    WynPassManager* manager = wyn_pass_manager_new();
    
    // Enable link-time optimization
    wyn_pass_manager_enable_lto(manager, true);
    printf("Link-time optimization: %s\n", 
           manager->link_time_optimization ? "enabled" : "disabled");
    
    // Set aggressive optimization for LTO
    wyn_pass_manager_set_opt_level(manager, WYN_OPT_LEVEL_AGGRESSIVE);
    
    printf("LTO with aggressive optimization: %zu passes\n", manager->pass_count);
    
    // In a real implementation, you would run LTO on multiple modules
    printf("Note: LTO would optimize across multiple compilation units\n");
    
    wyn_pass_manager_free(manager);
}

// Example: Pass configuration
void example_pass_configuration() {
    printf("\n=== Pass Configuration Example ===\n");
    
    // Create pass configurations
    WynPassConfig* inline_config = wyn_pass_config_create(WYN_PASS_FUNCTION_INLINING, true, 10);
    WynPassConfig* unroll_config = wyn_pass_config_create(WYN_PASS_LOOP_UNROLLING, true, 5);
    WynPassConfig* vec_config = wyn_pass_config_create(WYN_PASS_VECTORIZATION, false, 1);
    
    printf("Pass configurations:\n");
    printf("  Function Inlining: enabled=%s, priority=%d\n", 
           inline_config->enabled ? "true" : "false", inline_config->priority);
    printf("  Loop Unrolling: enabled=%s, priority=%d\n", 
           unroll_config->enabled ? "true" : "false", unroll_config->priority);
    printf("  Vectorization: enabled=%s, priority=%d\n", 
           vec_config->enabled ? "true" : "false", vec_config->priority);
    
    // Cleanup
    wyn_pass_config_free(inline_config);
    wyn_pass_config_free(unroll_config);
    wyn_pass_config_free(vec_config);
}

// Example: Individual pass creation and properties
void example_individual_passes() {
    printf("\n=== Individual Pass Properties Example ===\n");
    
    WynPassType pass_types[] = {
        WYN_PASS_DEAD_CODE_ELIMINATION,
        WYN_PASS_CONSTANT_FOLDING,
        WYN_PASS_FUNCTION_INLINING,
        WYN_PASS_VECTORIZATION,
        WYN_PASS_ESCAPE_ANALYSIS
    };
    
    const char* pass_names[] = {
        "Dead Code Elimination",
        "Constant Folding", 
        "Function Inlining",
        "Vectorization",
        "Escape Analysis"
    };
    
    printf("Available optimization passes:\n");
    
    for (size_t i = 0; i < sizeof(pass_types) / sizeof(pass_types[0]); i++) {
        WynOptimizationPass* pass = NULL;
        
        switch (pass_types[i]) {
            case WYN_PASS_DEAD_CODE_ELIMINATION:
                pass = wyn_create_dead_code_elimination_pass();
                break;
            case WYN_PASS_CONSTANT_FOLDING:
                pass = wyn_create_constant_folding_pass();
                break;
            case WYN_PASS_FUNCTION_INLINING:
                pass = wyn_create_function_inlining_pass();
                break;
            case WYN_PASS_VECTORIZATION:
                pass = wyn_create_vectorization_pass();
                break;
            case WYN_PASS_ESCAPE_ANALYSIS:
                pass = wyn_create_escape_analysis_pass();
                break;
            default:
                continue;
        }
        
        if (pass) {
            printf("  %s: %s\n", pass->name, pass->description);
            
            // Cleanup
            pass->cleanup(pass);
            free(pass);
        }
    }
}

int main() {
    printf("Wyn Advanced LLVM Passes Examples\n");
    printf("=================================\n\n");
    
    example_basic_pass_manager();
    example_custom_optimization_pipeline();
    example_profile_guided_optimization();
    example_optimization_level_comparison();
    example_advanced_optimization_config();
    example_link_time_optimization();
    example_pass_configuration();
    example_individual_passes();
    
    printf("\n✓ All advanced LLVM passes examples completed!\n");
    return 0;
}
