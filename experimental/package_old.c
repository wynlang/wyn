// Basic package manager implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
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
    _mkdir(wyn_dir);
    #else
    mkdir(wyn_dir, 0755);
    #endif
    
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages", home);
    #ifdef _WIN32
    _mkdir(pkg_dir);
    #else
    mkdir(pkg_dir, 0755);
    #endif
    
    return 0;
}

static char* get_package_dir() {
    static char pkg_dir[1024];
    char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) return NULL;
    
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/.wyn/packages", home);
    return pkg_dir;
}

static int install_package(const char* name, const char* version) {
    char* pkg_dir = get_package_dir();
    if (!pkg_dir) return -1;
    
    // Package name is the key, version/path is the value
    // If version looks like a path or URL, use it as source
    const char* source = version;
    
    char install_path[2048];
    snprintf(install_path, sizeof(install_path), "%s/%s", pkg_dir, name);
    
    // Check if already installed
    struct stat st;
    if (stat(install_path, &st) == 0) {
        printf("  %s - already installed\n", name);
        return 0;
    }
    
    // Try to install from git (if source contains ://)
    if (strstr(source, "://")) {
        printf("  Installing %s from %s...\n", name, source);
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "git clone %s %s 2>&1", source, install_path);
        int result = system(cmd);
        if (result == 0) {
            printf("  ✓ %s installed\n", name);
            return 0;
        } else {
            printf("  ✗ Failed to install %s\n", name);
            return -1;
        }
    }
    
    // Try to copy from local path
    if (stat(source, &st) == 0) {
        printf("  Installing %s from %s...\n", name, source);
        char cmd[4096];
        #ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "xcopy /E /I /Y \"%s\" \"%s\" > nul 2>&1", source, install_path);
        #else
        snprintf(cmd, sizeof(cmd), "cp -r \"%s\" \"%s\" 2>&1", source, install_path);
        #endif
        int result = system(cmd);
        if (result == 0) {
            printf("  ✓ %s installed\n", name);
            return 0;
        }
    }
    
    // Package not found
    printf("  ✗ Package %s not found\n", name);
    printf("    Source: %s\n", source);
    printf("\n");
    printf("    Supported formats:\n");
    printf("      - Git URL: https://github.com/user/repo\n");
    printf("      - Local path: /path/to/package\n");
    printf("      - Relative path: ../shared/utils\n");
    printf("\n");
    printf("    Example wyn.toml:\n");
    printf("      [dependencies]\n");
    printf("      mylib = \"https://github.com/user/mylib\"\n");
    printf("      utils = \"/home/user/shared/utils\"\n");
    return -1;
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
        
        int failed = 0;
        for (int i = 0; i < config->dependency_count; i++) {
            if (install_package(config->dependencies[i].name, 
                              config->dependencies[i].version) != 0) {
                failed++;
            }
        }
        
        if (failed > 0) {
            printf("\n✗ %d packages failed to install\n", failed);
            wyn_config_free(config);
            return 1;
        }
    }
    
    wyn_config_free(config);
    printf("✓ Installation complete\n");
    return 0;
}

int package_list() {
    char* pkg_dir = get_package_dir();
    if (!pkg_dir) {
        fprintf(stderr, "Error: Could not determine package directory\n");
        return 1;
    }
    
    printf("Installed packages in %s:\n", pkg_dir);
    
    // List directory contents
    char cmd[2048];
    #ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /B \"%s\" 2>nul", pkg_dir);
    #else
    snprintf(cmd, sizeof(cmd), "ls -1 \"%s\" 2>/dev/null", pkg_dir);
    #endif
    
    int result = system(cmd);
    if (result != 0) {
        printf("(No packages installed yet)\n");
    }
    
    return 0;
}
