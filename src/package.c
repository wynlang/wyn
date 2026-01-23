// Basic package manager implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "package.h"
#include "toml.h"

static int ensure_package_dir() {
    // Create ~/.wyn/packages if it doesn't exist
    char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");  // Windows
    if (!home) return -1;
    
    char wyn_dir[1024];
    snprintf(wyn_dir, sizeof(wyn_dir), "%s/.wyn", home);
    #ifdef _WIN32
    mkdir(wyn_dir);
    #else
    mkdir(wyn_dir, 0755);
    #endif
    
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages", home);
    #ifdef _WIN32
    mkdir(pkg_dir);
    #else
    mkdir(pkg_dir, 0755);
    #endif
    
    return 0;
}

int package_install(const char* project_dir) {
    printf("Installing packages for %s...\n", project_dir);
    
    // Ensure package directory exists
    if (ensure_package_dir() != 0) {
        fprintf(stderr, "Error: Could not create package directory\n");
        return 1;
    }
    
    // Read wyn.toml
    char toml_path[1024];
    snprintf(toml_path, sizeof(toml_path), "%s/wyn.toml", project_dir);
    
    WynConfig* config = wyn_config_parse(toml_path);
    if (!config) {
        fprintf(stderr, "Error: Could not read wyn.toml\n");
        return 1;
    }
    
    printf("Project: %s v%s\n", 
           config->project.name ? config->project.name : "unknown",
           config->project.version ? config->project.version : "0.0.0");
    
    if (config->dependency_count == 0) {
        printf("No dependencies to install.\n");
    } else {
        printf("Installing %d dependencies...\n", config->dependency_count);
        // TODO: Actually download and install packages
        printf("(Package installation not yet implemented)\n");
    }
    
    wyn_config_free(config);
    printf("✓ Installation complete\n");
    return 0;
}

int package_list() {
    char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) {
        fprintf(stderr, "Error: Could not determine home directory\n");
        return 1;
    }
    
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages", home);
    
    printf("Installed packages in %s:\n", pkg_dir);
    printf("(No packages installed yet)\n");
    
    return 0;
}
