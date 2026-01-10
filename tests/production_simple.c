#include "../src/production.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("Testing basic production manager creation...\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    if (!manager) {
        printf("Failed to create production manager\n");
        return 1;
    }
    
    printf("✓ Production manager created\n");
    
    bool init_result = wyn_production_manager_initialize(manager, "1.0.0");
    if (!init_result) {
        printf("Failed to initialize production manager\n");
        wyn_production_manager_free(manager);
        return 1;
    }
    
    printf("✓ Production manager initialized\n");
    
    wyn_production_manager_free(manager);
    printf("✓ Production manager freed\n");
    
    printf("Basic test passed!\n");
    return 0;
}
