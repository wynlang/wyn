#include "test.h"
#include "package_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int test_package_manager_init() {
    printf("Testing package manager initialization...\n");
    
    // Test initialization
    int result = wyn_pkg_init("/tmp/test_wyn_project");
    if (result != 0) {
        printf("  FAIL: Package manager initialization failed\n");
        return 0;
    }
    
    // Check if directory was created
    struct stat st;
    if (stat("/tmp/test_wyn_project/wyn_packages", &st) != 0) {
        printf("  FAIL: Packages directory not created\n");
        return 0;
    }
    
    // Check if config file was created
    if (stat("/tmp/test_wyn_project/wyn-package.toml", &st) != 0) {
        printf("  FAIL: Package config file not created\n");
        return 0;
    }
    
    printf("  PASS: Package manager initialization successful\n");
    return 1;
}

static int test_package_install() {
    printf("Testing package installation...\n");
    
    // Test package installation
    int result = wyn_pkg_install("test-package", "1.0.0");
    if (result != 0) {
        printf("  FAIL: Package installation failed\n");
        return 0;
    }
    
    // Test installation without version
    result = wyn_pkg_install("another-package", NULL);
    if (result != 0) {
        printf("  FAIL: Package installation without version failed\n");
        return 0;
    }
    
    printf("  PASS: Package installation successful\n");
    return 1;
}

static int test_package_list() {
    printf("Testing package listing...\n");
    
    // Test listing installed packages
    int result = wyn_pkg_list_installed();
    if (result != 0) {
        printf("  FAIL: Package listing failed\n");
        return 0;
    }
    
    printf("  PASS: Package listing successful\n");
    return 1;
}

static int test_package_search() {
    printf("Testing package search...\n");
    
    // Test package search
    int result = wyn_pkg_search("http");
    if (result != 0) {
        printf("  FAIL: Package search failed\n");
        return 0;
    }
    
    // Test search with no results
    result = wyn_pkg_search("nonexistent-package-xyz");
    if (result != 0) {
        printf("  FAIL: Package search with no results failed\n");
        return 0;
    }
    
    printf("  PASS: Package search successful\n");
    return 1;
}

static int test_package_uninstall() {
    printf("Testing package uninstallation...\n");
    
    // Test uninstalling existing package
    int result = wyn_pkg_uninstall("test-package");
    if (result != 0) {
        printf("  FAIL: Package uninstallation failed\n");
        return 0;
    }
    
    // Test uninstalling non-existent package
    result = wyn_pkg_uninstall("non-existent-package");
    if (result == 0) {
        printf("  FAIL: Uninstalling non-existent package should fail\n");
        return 0;
    }
    
    printf("  PASS: Package uninstallation successful\n");
    return 1;
}

static int test_dependency_resolution() {
    printf("Testing dependency resolution...\n");
    
    // Test dependency resolution
    int result = wyn_pkg_resolve_dependencies("test-package");
    if (result != 0) {
        printf("  FAIL: Dependency resolution failed\n");
        return 0;
    }
    
    printf("  PASS: Dependency resolution successful\n");
    return 1;
}

static int test_metadata_validation() {
    printf("Testing metadata validation...\n");
    
    // Test valid metadata
    PackageMetadata valid_metadata = {
        .name = "test-package",
        .version = "1.0.0",
        .description = "Test package",
        .dependencies = NULL,
        .dependency_count = 0,
        .author = "Test Author",
        .license = "MIT"
    };
    
    if (!wyn_pkg_validate_metadata(&valid_metadata)) {
        printf("  FAIL: Valid metadata rejected\n");
        return 0;
    }
    
    // Test invalid metadata (missing name)
    PackageMetadata invalid_metadata = {
        .name = NULL,
        .version = "1.0.0",
        .description = "Test package"
    };
    
    if (wyn_pkg_validate_metadata(&invalid_metadata)) {
        printf("  FAIL: Invalid metadata accepted\n");
        return 0;
    }
    
    printf("  PASS: Metadata validation successful\n");
    return 1;
}

static int test_package_manager_error_handling() {
    printf("Testing error handling...\n");
    
    // Test operations without initialization
    wyn_pkg_cleanup(); // Reset state
    
    int result = wyn_pkg_install("test-package", "1.0.0");
    if (result == 0) {
        printf("  FAIL: Install should fail without initialization\n");
        return 0;
    }
    
    // Test with NULL parameters
    result = wyn_pkg_install(NULL, "1.0.0");
    if (result == 0) {
        printf("  FAIL: Install should fail with NULL package name\n");
        return 0;
    }
    
    printf("  PASS: Error handling successful\n");
    return 1;
}

int main() {
    printf("=== T5.2.2: Package Manager Implementation Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_package_manager_init()) passed_tests++;
    total_tests++; if (test_package_install()) passed_tests++;
    total_tests++; if (test_package_list()) passed_tests++;
    total_tests++; if (test_package_search()) passed_tests++;
    total_tests++; if (test_package_uninstall()) passed_tests++;
    total_tests++; if (test_dependency_resolution()) passed_tests++;
    total_tests++; if (test_metadata_validation()) passed_tests++;
    total_tests++; if (test_package_manager_error_handling()) passed_tests++;
    
    // Cleanup
    wyn_pkg_cleanup();
    system("rm -rf /tmp/test_wyn_project");
    
    // Print summary
    printf("\n=== Package Manager Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All package manager tests passed!\n");
        printf("📦 Package manager ready for production use\n");
        return 0;
    } else {
        printf("❌ Some package manager tests failed\n");
        return 1;
    }
}
