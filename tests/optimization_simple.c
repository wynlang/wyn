#include "../src/optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("Testing basic optimization manager creation...\n");
    
    WynOptimizationManager* manager = wyn_optimization_manager_new();
    if (!manager) {
        printf("Failed to create optimization manager\n");
        return 1;
    }
    
    printf("✓ Optimization manager created\n");
    printf("Current level: %s\n", wyn_optimization_level_name(manager->current_level));
    
    bool init_result = wyn_optimization_manager_initialize(manager);
    if (!init_result) {
        printf("Failed to initialize optimization manager\n");
        wyn_optimization_manager_free(manager);
        return 1;
    }
    
    printf("✓ Optimization manager initialized\n");
    printf("Optimization passes: %zu\n", manager->pass_count);
    
    bool level_result = wyn_optimization_manager_set_level(manager, WYN_OPT_SPEED);
    if (!level_result) {
        printf("Failed to set optimization level\n");
        wyn_optimization_manager_free(manager);
        return 1;
    }
    
    printf("✓ Optimization level set to %s\n", wyn_optimization_level_name(manager->current_level));
    
    wyn_optimization_manager_free(manager);
    printf("✓ Optimization manager freed\n");
    
    printf("Basic optimization test passed!\n");
    return 0;
}
