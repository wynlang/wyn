#include "../src/release.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("Testing basic release manager creation...\n");
    
    WynReleaseManager* manager = wyn_release_manager_new("1.0.0", WYN_RELEASE_STABLE);
    if (!manager) {
        printf("Failed to create release manager\n");
        return 1;
    }
    
    printf("✓ Release manager created\n");
    printf("Version: %s\n", manager->version);
    printf("Type: %s\n", wyn_release_type_name(manager->release_type));
    
    bool init_result = wyn_release_manager_initialize(manager);
    if (!init_result) {
        printf("Failed to initialize release manager\n");
        wyn_release_manager_free(manager);
        return 1;
    }
    
    printf("✓ Release manager initialized\n");
    printf("Validations: %zu\n", manager->validation_count);
    
    wyn_release_manager_free(manager);
    printf("✓ Release manager freed\n");
    
    printf("Basic release test passed!\n");
    return 0;
}
