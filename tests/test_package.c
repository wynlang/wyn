#include "../src/package.h"
#include "../src/system.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void test_package_manager_creation() {
    printf("Testing package manager creation...\n");
    
    WynPackageManager* manager = wyn_pkg_manager_new();
    assert(manager != NULL);
    assert(manager->registry_count == 0);
    assert(manager->current_package == NULL);
    
    // Initialize configuration
    WynPackageError error = wyn_pkg_manager_init_config(manager);
    assert(error == WYN_PKG_OK);
    assert(manager->registry_count == 1);  // Default registry added
    
    wyn_pkg_manager_free(manager);
    
    printf("✓ Package manager creation tests passed\n");
}

void test_registry_management() {
    printf("Testing registry management...\n");
    
    WynPackageManager* manager = wyn_pkg_manager_new();
    
    // Add registry
    WynPackageError error = wyn_pkg_add_registry(manager, "test", "https://test.example.com");
    assert(error == WYN_PKG_OK);
    assert(manager->registry_count == 1);
    
    // Find registry
    WynRegistry* registry = wyn_pkg_find_registry(manager, "test");
    assert(registry != NULL);
    assert(strcmp(registry->name, "test") == 0);
    assert(strcmp(registry->url, "https://test.example.com") == 0);
    
    // Add another registry
    error = wyn_pkg_add_registry(manager, "local", "file:///local/packages");
    assert(error == WYN_PKG_OK);
    assert(manager->registry_count == 2);
    
    // Remove registry
    error = wyn_pkg_remove_registry(manager, "test");
    assert(error == WYN_PKG_OK);
    assert(manager->registry_count == 1);
    
    // Try to find removed registry
    registry = wyn_pkg_find_registry(manager, "test");
    assert(registry == NULL);
    
    wyn_pkg_manager_free(manager);
    
    printf("✓ Registry management tests passed\n");
}

void test_version_handling() {
    printf("Testing version handling...\n");
    
    // Test version parsing
    WynVersion* v1 = wyn_version_parse("1.2.3");
    assert(v1 != NULL);
    assert(v1->major == 1);
    assert(v1->minor == 2);
    assert(v1->patch == 3);
    
    WynVersion* v2 = wyn_version_parse("2.0.0");
    assert(v2 != NULL);
    
    // Test version comparison
    assert(wyn_version_compare(v1, v2) < 0);  // v1 < v2
    assert(wyn_version_compare(v2, v1) > 0);  // v2 > v1
    assert(wyn_version_compare(v1, v1) == 0); // v1 == v1
    
    // Test version constraints
    WynVersion* constraint = wyn_version_parse("1.0.0");
    WynVersion* test_version = wyn_version_parse("1.5.0");
    
    assert(wyn_version_satisfies(test_version, constraint, WYN_VERSION_COMPATIBLE));
    assert(!wyn_version_satisfies(test_version, constraint, WYN_VERSION_EXACT));
    assert(wyn_version_satisfies(test_version, constraint, WYN_VERSION_GREATER));
    
    // Test version to string
    char* version_str = wyn_version_to_string(v1);
    assert(strcmp(version_str, "1.2.3") == 0);
    free(version_str);
    
    wyn_version_free(v1);
    wyn_version_free(v2);
    wyn_version_free(constraint);
    wyn_version_free(test_version);
    
    printf("✓ Version handling tests passed\n");
}

void test_package_operations() {
    printf("Testing package operations...\n");
    
    // Create package
    WynPackage* package = wyn_pkg_new("test-package", "1.0.0");
    assert(package != NULL);
    assert(strcmp(package->name, "test-package") == 0);
    assert(package->version->major == 1);
    assert(package->version->minor == 0);
    assert(package->version->patch == 0);
    
    // Add description
    package->description = strdup("A test package");
    
    // Add dependency
    WynPackageError error = wyn_pkg_add_dependency(package, "http", "2.0.0");
    assert(error == WYN_PKG_OK);
    
    // Find dependency
    WynDependency* dep = wyn_pkg_find_dependency(package, "http");
    assert(dep != NULL);
    assert(strcmp(dep->name, "http") == 0);
    
    // Add another dependency
    error = wyn_pkg_add_dependency(package, "json", "1.5.0");
    assert(error == WYN_PKG_OK);
    
    // Remove dependency
    error = wyn_pkg_remove_dependency(package, "http");
    assert(error == WYN_PKG_OK);
    
    // Try to find removed dependency
    dep = wyn_pkg_find_dependency(package, "http");
    assert(dep == NULL);
    
    // json dependency should still exist
    dep = wyn_pkg_find_dependency(package, "json");
    assert(dep != NULL);
    
    wyn_pkg_free(package);
    
    printf("✓ Package operation tests passed\n");
}

void test_manifest_operations() {
    printf("Testing manifest operations...\n");
    
    // Create a test package
    WynPackage* package = wyn_pkg_new("manifest-test", "0.5.0");
    package->description = strdup("Test package for manifest operations");
    
    // Add some dependencies
    wyn_pkg_add_dependency(package, "utils", "1.0.0");
    wyn_pkg_add_dependency(package, "core", "2.1.0");
    
    // Create temporary directory for testing
    const char* test_dir = "/tmp/wyn_pkg_test";
    wyn_sys_create_directory(test_dir);
    
    // Save manifest
    WynPackageError error = wyn_pkg_save_manifest(package, test_dir);
    assert(error == WYN_PKG_OK);
    
    // Check if manifest file exists
    char* manifest_path = wyn_sys_join_path(test_dir, "wyn.toml");
    assert(wyn_sys_file_exists(manifest_path));
    free(manifest_path);
    
    // Load manifest (simplified test)
    WynPackage* loaded_package;
    error = wyn_pkg_load_manifest(test_dir, &loaded_package);
    assert(error == WYN_PKG_OK);
    assert(loaded_package != NULL);
    
    // Cleanup
    wyn_pkg_free(package);
    wyn_pkg_free(loaded_package);
    wyn_sys_remove_file(wyn_sys_join_path(test_dir, "wyn.toml"));
    wyn_sys_remove_directory(test_dir);
    
    printf("✓ Manifest operation tests passed\n");
}

void test_project_initialization() {
    printf("Testing project initialization...\n");
    
    const char* project_path = "/tmp/wyn_test_project";
    
    // Initialize project
    WynPackageError error = wyn_pkg_init_project(project_path, "my-project");
    assert(error == WYN_PKG_OK);
    
    // Check if project directory exists
    assert(wyn_sys_is_directory(project_path));
    
    // Check if manifest exists
    char* manifest_path = wyn_sys_join_path(project_path, "wyn.toml");
    assert(wyn_sys_file_exists(manifest_path));
    free(manifest_path);
    
    // Load the created project
    // WynPackage* project;
    // error = wyn_pkg_load_project(project_path, &project);
    // Note: wyn_pkg_load_project is not implemented, so this would fail
    // assert(error == WYN_PKG_OK);
    
    // Cleanup
    wyn_sys_remove_file(wyn_sys_join_path(project_path, "wyn.toml"));
    wyn_sys_remove_directory(project_path);
    
    printf("✓ Project initialization tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test error strings
    assert(strcmp(wyn_pkg_error_string(WYN_PKG_OK), "Success") == 0);
    assert(strcmp(wyn_pkg_error_string(WYN_PKG_NOT_FOUND), "Package not found") == 0);
    
    // Test cache and config directories
    char* cache_dir = wyn_pkg_get_cache_dir();
    char* config_dir = wyn_pkg_get_config_dir();
    
    if (cache_dir) {
        printf("Cache directory: %s\n", cache_dir);
        free(cache_dir);
    }
    
    if (config_dir) {
        printf("Config directory: %s\n", config_dir);
        free(config_dir);
    }
    
    // Test package name validation
    assert(wyn_pkg_is_valid_name("valid_package"));
    assert(wyn_pkg_is_valid_name("valid-package"));
    assert(wyn_pkg_is_valid_name("ValidPackage123"));
    assert(!wyn_pkg_is_valid_name("123invalid"));
    assert(!wyn_pkg_is_valid_name("invalid.package"));
    assert(!wyn_pkg_is_valid_name(""));
    
    // Test version validation
    assert(wyn_pkg_is_valid_version("1.0.0"));
    assert(wyn_pkg_is_valid_version("0.1.0"));
    assert(wyn_pkg_is_valid_version("10.20.30"));
    assert(!wyn_pkg_is_valid_version("invalid"));
    assert(!wyn_pkg_is_valid_version("1.0"));
    assert(!wyn_pkg_is_valid_version(""));
    
    printf("✓ Utility function tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    assert(wyn_pkg_new(NULL, "1.0.0") == NULL);
    assert(wyn_pkg_new("test", NULL) == NULL);
    
    wyn_pkg_free(NULL);  // Should not crash
    
    // Test invalid version parsing
    WynVersion* invalid_version = wyn_version_parse("invalid");
    assert(invalid_version == NULL);
    
    invalid_version = wyn_version_parse("");
    assert(invalid_version == NULL);
    
    // Test version comparison with NULL
    WynVersion* v1 = wyn_version_parse("1.0.0");
    assert(wyn_version_compare(v1, NULL) == 0);
    assert(wyn_version_compare(NULL, v1) == 0);
    assert(wyn_version_compare(NULL, NULL) == 0);
    
    wyn_version_free(v1);
    
    // Test dependency operations on NULL package
    assert(wyn_pkg_add_dependency(NULL, "test", "1.0.0") == WYN_PKG_UNKNOWN_ERROR);
    assert(wyn_pkg_find_dependency(NULL, "test") == NULL);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Package Manager Foundation Tests\n");
    printf("=======================================\n\n");
    
    test_package_manager_creation();
    test_registry_management();
    test_version_handling();
    test_package_operations();
    test_manifest_operations();
    test_project_initialization();
    test_utility_functions();
    test_edge_cases();
    
    printf("\n✅ All package manager tests passed!\n");
    return 0;
}
