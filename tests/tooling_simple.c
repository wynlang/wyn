#include "../src/tooling.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("Testing basic tooling manager creation...\n");
    
    WynToolingManager* manager = wyn_tooling_manager_new();
    if (!manager) {
        printf("Failed to create tooling manager\n");
        return 1;
    }
    
    printf("✓ Tooling manager created\n");
    
    bool init_result = wyn_tooling_manager_initialize(manager, "/workspace");
    if (!init_result) {
        printf("Failed to initialize tooling manager\n");
        wyn_tooling_manager_free(manager);
        return 1;
    }
    
    printf("✓ Tooling manager initialized\n");
    printf("Workspace: %s\n", manager->workspace_path);
    
    bool setup_result = wyn_tooling_manager_setup_toolchain(manager);
    if (!setup_result) {
        printf("Failed to setup toolchain\n");
        wyn_tooling_manager_free(manager);
        return 1;
    }
    
    printf("✓ Toolchain setup completed\n");
    
    wyn_tooling_manager_free(manager);
    printf("✓ Tooling manager freed\n");
    
    printf("Basic tooling test passed!\n");
    return 0;
}
