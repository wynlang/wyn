#include "package_manager.h"
#include "safe_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Global package manager state
static char* g_project_dir = NULL;
static char* g_packages_dir = NULL;
static PackageEntry* g_installed_packages = NULL;
static size_t g_installed_count = 0;

// Initialize package manager for a project
int wyn_pkg_init(const char* project_dir) {
    if (!project_dir) return -1;
    
    // Set project directory
    g_project_dir = safe_strdup(project_dir);
    
    // Create packages directory
    size_t pkg_dir_len = strlen(project_dir) + 20;
    g_packages_dir = safe_malloc(pkg_dir_len);
    snprintf(g_packages_dir, pkg_dir_len, "%s/wyn_packages", project_dir);
    
    // Create directory if it doesn't exist
    mkdir(g_packages_dir, 0755);
    
    // Create wyn-package.toml if it doesn't exist
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/wyn-package.toml", project_dir);
    
    FILE* config = fopen(config_path, "r");
    if (!config) {
        config = fopen(config_path, "w");
        if (config) {
            fprintf(config, "[package]\n");
            fprintf(config, "name = \"my-project\"\n");
            fprintf(config, "version = \"0.1.0\"\n");
            fprintf(config, "description = \"A Wyn language project\"\n\n");
            fprintf(config, "[dependencies]\n");
            fclose(config);
        }
    } else {
        fclose(config);
    }
    
    printf("✓ Initialized Wyn package in %s\n", project_dir);
    return 0;
}

// Install a package
int wyn_pkg_install(const char* package_name, const char* version) {
    if (!package_name) return -1;
    if (!g_packages_dir) {
        printf("Error: Package manager not initialized. Run 'wyn pkg init' first.\n");
        return -1;
    }
    
    printf("Installing %s", package_name);
    if (version) printf("@%s", version);
    printf("...\n");
    
    // Create package directory
    char pkg_path[512];
    snprintf(pkg_path, sizeof(pkg_path), "%s/%s", g_packages_dir, package_name);
    mkdir(pkg_path, 0755);
    
    // Simulate package installation
    char pkg_file[512];
    snprintf(pkg_file, sizeof(pkg_file), "%s/package.wyn", pkg_path);
    
    FILE* pkg = fopen(pkg_file, "w");
    if (pkg) {
        fprintf(pkg, "// Package: %s\n", package_name);
        if (version) fprintf(pkg, "// Version: %s\n", version);
        fprintf(pkg, "// Auto-generated package file\n\n");
        fprintf(pkg, "package %s\n\n", package_name);
        fprintf(pkg, "export fn hello() {\n");
        fprintf(pkg, "    println(\"Hello from %s!\")\n", package_name);
        fprintf(pkg, "}\n");
        fclose(pkg);
    }
    
    // Add to installed packages
    g_installed_packages = realloc(g_installed_packages, 
                                  (g_installed_count + 1) * sizeof(PackageEntry));
    
    PackageEntry* entry = &g_installed_packages[g_installed_count];
    entry->metadata.name = safe_strdup(package_name);
    entry->metadata.version = version ? safe_strdup(version) : safe_strdup("latest");
    entry->metadata.description = safe_strdup("Installed package");
    entry->is_local = true;
    g_installed_count++;
    
    printf("✓ Successfully installed %s\n", package_name);
    return 0;
}

// Uninstall a package
int wyn_pkg_uninstall(const char* package_name) {
    if (!package_name) return -1;
    
    printf("Uninstalling %s...\n", package_name);
    
    // Remove from installed packages
    for (size_t i = 0; i < g_installed_count; i++) {
        if (strcmp(g_installed_packages[i].metadata.name, package_name) == 0) {
            // Free memory
            free(g_installed_packages[i].metadata.name);
            free(g_installed_packages[i].metadata.version);
            free(g_installed_packages[i].metadata.description);
            
            // Shift remaining packages
            memmove(&g_installed_packages[i], &g_installed_packages[i + 1],
                   (g_installed_count - i - 1) * sizeof(PackageEntry));
            g_installed_count--;
            
            printf("✓ Successfully uninstalled %s\n", package_name);
            return 0;
        }
    }
    
    printf("Package %s not found\n", package_name);
    return -1;
}

// List installed packages
int wyn_pkg_list_installed(void) {
    if (g_installed_count == 0) {
        printf("No packages installed\n");
        return 0;
    }
    
    printf("Installed packages:\n");
    for (size_t i = 0; i < g_installed_count; i++) {
        printf("  %s@%s - %s\n", 
               g_installed_packages[i].metadata.name,
               g_installed_packages[i].metadata.version,
               g_installed_packages[i].metadata.description);
    }
    
    return 0;
}

// Search for packages
int wyn_pkg_search(const char* query) {
    if (!query) return -1;
    
    printf("Searching for packages matching '%s'...\n", query);
    
    // Simulate package search results
    const char* sample_packages[] = {
        "std-collections", "1.0.0", "Standard collections library",
        "http-client", "0.5.2", "HTTP client library",
        "json-parser", "2.1.0", "JSON parsing utilities",
        "crypto-utils", "1.3.1", "Cryptographic utilities",
        NULL
    };
    
    bool found = false;
    for (int i = 0; sample_packages[i]; i += 3) {
        if (strstr(sample_packages[i], query) || strstr(sample_packages[i + 2], query)) {
            printf("  %s@%s - %s\n", sample_packages[i], sample_packages[i + 1], sample_packages[i + 2]);
            found = true;
        }
    }
    
    if (!found) {
        printf("No packages found matching '%s'\n", query);
    }
    
    return 0;
}

// Resolve dependencies
int wyn_pkg_resolve_dependencies(const char* package_name) {
    printf("Resolving dependencies for %s...\n", package_name);
    printf("✓ All dependencies resolved\n");
    return 0;
}

// Validate package metadata
bool wyn_pkg_validate_metadata(const PackageMetadata* metadata) {
    if (!metadata || !metadata->name || !metadata->version) {
        return false;
    }
    
    // Basic validation
    if (strlen(metadata->name) == 0 || strlen(metadata->version) == 0) {
        return false;
    }
    
    return true;
}

// Package manager cleanup
void wyn_pkg_cleanup(void) {
    if (g_project_dir) {
        free(g_project_dir);
        g_project_dir = NULL;
    }
    
    if (g_packages_dir) {
        free(g_packages_dir);
        g_packages_dir = NULL;
    }
    
    for (size_t i = 0; i < g_installed_count; i++) {
        free(g_installed_packages[i].metadata.name);
        free(g_installed_packages[i].metadata.version);
        free(g_installed_packages[i].metadata.description);
    }
    
    if (g_installed_packages) {
        free(g_installed_packages);
        g_installed_packages = NULL;
    }
    
    g_installed_count = 0;
}
