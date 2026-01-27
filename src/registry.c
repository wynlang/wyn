#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "registry.h"
#include "toml.h"
#include "semver.h"

#define REGISTRY_URL "https://pkg.wynlang.com"

// Note: Package registry server not deployed yet (coming in v1.5.1)
// These are stub implementations. In v1.5.1, they will be replaced with
// a POSIX socket-based HTTP client (no external dependencies).

int registry_search(const char *query) {
    (void)query;  // Unused
    fprintf(stderr, "Package registry not available yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://pkg.wynlang.com for updates\n");
    return 1;
}

int registry_info(const char *package) {
    (void)package;  // Unused
    fprintf(stderr, "Package registry not available yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://pkg.wynlang.com for updates\n");
    return 1;
}

int registry_install(const char *package_spec) {
    (void)package_spec;  // Unused
    fprintf(stderr, "Package registry not available yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://pkg.wynlang.com for updates\n");
    return 1;
}

int registry_versions(const char *package) {
    (void)package;  // Unused
    fprintf(stderr, "Package registry not available yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://pkg.wynlang.com for updates\n");
    return 1;
}

int registry_resolve_version(const char *package, const char *constraint, char *resolved_version, size_t buf_size) {
    (void)package;  // Unused
    (void)constraint;  // Unused
    (void)resolved_version;  // Unused
    (void)buf_size;  // Unused
    fprintf(stderr, "Package registry not available yet (coming in v1.5.1)\n");
    return 1;
}

int registry_publish(int dry_run) {
    // 1. Validate wyn.toml exists
    if (access("wyn.toml", F_OK) != 0) {
        fprintf(stderr, "Error: wyn.toml not found. Run 'wyn init' first.\n");
        return 1;
    }
    
    // 2. Parse wyn.toml
    WynConfig *config = wyn_config_parse("wyn.toml");
    if (!config) {
        fprintf(stderr, "Error: Failed to parse wyn.toml\n");
        return 1;
    }
    
    // 3. Validate required fields
    if (!config->project.name || strlen(config->project.name) == 0) {
        fprintf(stderr, "Error: Missing required field: name\n");
        wyn_config_free(config);
        return 1;
    }
    if (!config->project.version || strlen(config->project.version) == 0) {
        fprintf(stderr, "Error: Missing required field: version\n");
        wyn_config_free(config);
        return 1;
    }
    
    if (dry_run) {
        printf("Publishing %s@%s...\n", config->project.name, config->project.version);
        printf("\n[DRY RUN] Would publish:\n");
        printf("  Name: %s\n", config->project.name);
        printf("  Version: %s\n", config->project.version);
        if (config->project.author) printf("  Author: %s\n", config->project.author);
        if (config->project.description) printf("  Description: %s\n", config->project.description);
        if (config->dependency_count > 0) {
            printf("  Dependencies:\n");
            for (int i = 0; i < config->dependency_count; i++) {
                printf("    - %s@%s\n", config->dependencies[i].name, config->dependencies[i].version);
            }
        }
        printf("\nNote: Registry server not deployed yet (coming in v1.5.1)\n");
        wyn_config_free(config);
        return 0;
    }
    
    printf("Publishing %s@%s...\n", config->project.name, config->project.version);
    fprintf(stderr, "Error: Package registry not available yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://pkg.wynlang.com for updates\n");
    
    wyn_config_free(config);
    return 1;
}
