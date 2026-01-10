#include "../src/package.h"
#include "../src/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void example_package_manager_setup() {
    printf("=== Package Manager Setup ===\n");
    
    // Create package manager
    WynPackageManager* manager = wyn_pkg_manager_new();
    printf("Created package manager\n");
    
    // Initialize configuration
    WynPackageError error = wyn_pkg_manager_init_config(manager);
    if (error == WYN_PKG_OK) {
        printf("Initialized package manager configuration\n");
        printf("Default registry added: %s\n", WYN_PKG_DEFAULT_REGISTRY);
    }
    
    // Add custom registry
    error = wyn_pkg_add_registry(manager, "local", "file:///usr/local/wyn/packages");
    if (error == WYN_PKG_OK) {
        printf("Added local registry\n");
    }
    
    // List registries
    printf("Available registries:\n");
    for (size_t i = 0; i < manager->registry_count; i++) {
        WynRegistry* reg = manager->registries[i];
        printf("  %s: %s%s\n", reg->name, reg->url, reg->is_default ? " (default)" : "");
    }
    
    wyn_pkg_manager_free(manager);
    printf("\n");
}

void example_version_management() {
    printf("=== Version Management ===\n");
    
    // Parse versions
    WynVersion* v1 = wyn_version_parse("1.2.3");
    WynVersion* v2 = wyn_version_parse("2.0.0");
    WynVersion* v3 = wyn_version_parse("1.5.0");
    
    printf("Parsed versions:\n");
    char* v1_str = wyn_version_to_string(v1);
    char* v2_str = wyn_version_to_string(v2);
    char* v3_str = wyn_version_to_string(v3);
    
    printf("  v1: %s\n", v1_str);
    printf("  v2: %s\n", v2_str);
    printf("  v3: %s\n", v3_str);
    
    // Version comparisons
    printf("Version comparisons:\n");
    printf("  %s < %s: %s\n", v1_str, v2_str, wyn_version_compare(v1, v2) < 0 ? "true" : "false");
    printf("  %s > %s: %s\n", v2_str, v1_str, wyn_version_compare(v2, v1) > 0 ? "true" : "false");
    printf("  %s == %s: %s\n", v1_str, v1_str, wyn_version_compare(v1, v1) == 0 ? "true" : "false");
    
    // Version constraints
    printf("Version constraints:\n");
    WynVersion* constraint = wyn_version_parse("1.0.0");
    printf("  %s satisfies ^%s: %s\n", v3_str, "1.0.0", 
           wyn_version_satisfies(v3, constraint, WYN_VERSION_COMPATIBLE) ? "true" : "false");
    printf("  %s satisfies ~%s: %s\n", v3_str, "1.0.0",
           wyn_version_satisfies(v3, constraint, WYN_VERSION_TILDE) ? "true" : "false");
    
    // Cleanup
    free(v1_str);
    free(v2_str);
    free(v3_str);
    wyn_version_free(v1);
    wyn_version_free(v2);
    wyn_version_free(v3);
    wyn_version_free(constraint);
    
    printf("\n");
}

void example_package_creation() {
    printf("=== Package Creation ===\n");
    
    // Create a new package
    WynPackage* package = wyn_pkg_new("my-awesome-lib", "1.0.0");
    if (package) {
        printf("Created package: %s v%s\n", package->name, wyn_version_to_string(package->version));
        
        // Add metadata
        package->description = strdup("An awesome library for Wyn");
        package->license = strdup("MIT");
        package->repository = strdup("https://github.com/user/my-awesome-lib");
        
        printf("Package metadata:\n");
        printf("  Description: %s\n", package->description);
        printf("  License: %s\n", package->license);
        printf("  Repository: %s\n", package->repository);
        
        // Add dependencies
        printf("Adding dependencies...\n");
        wyn_pkg_add_dependency(package, "http", "2.1.0");
        wyn_pkg_add_dependency(package, "json", "1.5.0");
        wyn_pkg_add_dependency(package, "utils", "0.8.0");
        
        // List dependencies
        printf("Dependencies:\n");
        WynDependency* dep = package->dependencies;
        while (dep) {
            char* dep_version = wyn_version_to_string(dep->version);
            printf("  %s: %s\n", dep->name, dep_version);
            free(dep_version);
            dep = dep->next;
        }
        
        wyn_pkg_free(package);
    }
    
    printf("\n");
}

void example_project_workflow() {
    printf("=== Project Workflow ===\n");
    
    const char* project_path = "/tmp/wyn_example_project";
    
    // Initialize new project
    printf("Initializing new project at %s...\n", project_path);
    WynPackageError error = wyn_pkg_init_project(project_path, "example-app");
    
    if (error == WYN_PKG_OK) {
        printf("Project initialized successfully\n");
        
        // Check if manifest was created
        char* manifest_path = wyn_sys_join_path(project_path, "wyn.toml");
        if (wyn_sys_file_exists(manifest_path)) {
            printf("Manifest file created: %s\n", manifest_path);
            
            // Load the project
            WynPackage* project;
            error = wyn_pkg_load_manifest(project_path, &project);
            if (error == WYN_PKG_OK && project) {
                printf("Loaded project: %s\n", project->name);
                
                // Add some dependencies
                wyn_pkg_add_dependency(project, "cli", "1.0.0");
                wyn_pkg_add_dependency(project, "config", "0.5.0");
                
                // Save updated manifest
                error = wyn_pkg_save_manifest(project, project_path);
                if (error == WYN_PKG_OK) {
                    printf("Updated manifest with dependencies\n");
                }
                
                wyn_pkg_free(project);
            }
        }
        free(manifest_path);
        
        // Cleanup
        wyn_sys_remove_file(wyn_sys_join_path(project_path, "wyn.toml"));
        wyn_sys_remove_directory(project_path);
    } else {
        printf("Failed to initialize project: %s\n", wyn_pkg_error_string(error));
    }
    
    printf("\n");
}

void example_dependency_management() {
    printf("=== Dependency Management ===\n");
    
    WynPackage* package = wyn_pkg_new("web-server", "0.1.0");
    
    // Add various types of dependencies
    printf("Adding dependencies with different constraints...\n");
    
    // Exact version
    wyn_pkg_add_dependency(package, "http", "2.0.0");
    printf("  http = \"2.0.0\" (exact)\n");
    
    // Compatible version (would be ^1.5.0 in real implementation)
    wyn_pkg_add_dependency(package, "json", "1.5.0");
    printf("  json = \"^1.5.0\" (compatible)\n");
    
    // Tilde version (would be ~0.8.0 in real implementation)
    wyn_pkg_add_dependency(package, "utils", "0.8.0");
    printf("  utils = \"~0.8.0\" (tilde)\n");
    
    // List all dependencies
    printf("Current dependencies:\n");
    WynDependency* dep = package->dependencies;
    int count = 0;
    while (dep) {
        count++;
        char* version_str = wyn_version_to_string(dep->version);
        printf("  %d. %s: %s\n", count, dep->name, version_str);
        free(version_str);
        dep = dep->next;
    }
    
    // Remove a dependency
    printf("Removing 'utils' dependency...\n");
    WynPackageError error = wyn_pkg_remove_dependency(package, "utils");
    if (error == WYN_PKG_OK) {
        printf("Successfully removed 'utils'\n");
    }
    
    // Verify removal
    if (!wyn_pkg_find_dependency(package, "utils")) {
        printf("Confirmed: 'utils' is no longer in dependencies\n");
    }
    
    wyn_pkg_free(package);
    printf("\n");
}

void example_validation_and_errors() {
    printf("=== Validation and Error Handling ===\n");
    
    // Test package name validation
    printf("Package name validation:\n");
    const char* names[] = {"valid-package", "ValidPackage123", "123invalid", "invalid.name", ""};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        bool valid = wyn_pkg_is_valid_name(names[i]);
        printf("  '%s': %s\n", names[i], valid ? "valid" : "invalid");
    }
    
    // Test version validation
    printf("Version validation:\n");
    const char* versions[] = {"1.0.0", "0.1.0", "10.20.30", "1.0", "invalid", ""};
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        bool valid = wyn_pkg_is_valid_version(versions[i]);
        printf("  '%s': %s\n", versions[i], valid ? "valid" : "invalid");
    }
    
    // Test error handling
    printf("Error handling:\n");
    WynPackageError errors[] = {
        WYN_PKG_OK, WYN_PKG_NOT_FOUND, WYN_PKG_INVALID_VERSION,
        WYN_PKG_DEPENDENCY_ERROR, WYN_PKG_NETWORK_ERROR
    };
    
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        printf("  %d: %s\n", errors[i], wyn_pkg_error_string(errors[i]));
    }
    
    printf("\n");
}

void example_cache_and_config() {
    printf("=== Cache and Configuration ===\n");
    
    // Get cache and config directories
    char* cache_dir = wyn_pkg_get_cache_dir();
    char* config_dir = wyn_pkg_get_config_dir();
    
    printf("Package manager directories:\n");
    if (cache_dir) {
        printf("  Cache: %s\n", cache_dir);
        free(cache_dir);
    }
    
    if (config_dir) {
        printf("  Config: %s\n", config_dir);
        free(config_dir);
    }
    
    // Show default constants
    printf("Default settings:\n");
    printf("  Manifest file: %s\n", WYN_PKG_MANIFEST_FILE);
    printf("  Lock file: %s\n", WYN_PKG_LOCK_FILE);
    printf("  Default registry: %s\n", WYN_PKG_DEFAULT_REGISTRY);
    printf("  Cache directory: %s\n", WYN_PKG_CACHE_DIR);
    printf("  Config directory: %s\n", WYN_PKG_CONFIG_DIR);
    
    printf("\n");
}

int main() {
    printf("Wyn Package Manager Foundation Examples\n");
    printf("=======================================\n\n");
    
    example_package_manager_setup();
    example_version_management();
    example_package_creation();
    example_project_workflow();
    example_dependency_management();
    example_validation_and_errors();
    example_cache_and_config();
    
    printf("All package manager examples completed successfully!\n");
    return 0;
}
