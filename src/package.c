#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Read package.wyn manifest from module directory
PackageInfo* read_package_manifest(const char* module_path) {
    // Extract directory from module path
    char dir[512];
    const char* last_slash = strrchr(module_path, '/');
    if (last_slash) {
        int len = last_slash - module_path;
        memcpy(dir, module_path, len);
        dir[len] = '\0';
    } else {
        strcpy(dir, ".");
    }
    
    // Check for package.wyn in the same directory
    char package_path[512];
    snprintf(package_path, sizeof(package_path), "%s/package.wyn", dir);
    
    struct stat st;
    if (stat(package_path, &st) != 0) {
        return NULL;  // No package.wyn found
    }
    
    // Read and parse package.wyn
    FILE* f = fopen(package_path, "r");
    if (!f) return NULL;
    
    PackageInfo* info = calloc(1, sizeof(PackageInfo));
    char line[512];
    
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Parse key = value
        char* eq = strchr(line, '=');
        if (!eq) continue;
        
        // Extract key and value
        char key[128], value[256];
        int key_len = eq - line;
        while (key_len > 0 && (line[key_len-1] == ' ' || line[key_len-1] == '\t')) key_len--;
        memcpy(key, line, key_len);
        key[key_len] = '\0';
        
        char* val_start = eq + 1;
        while (*val_start == ' ' || *val_start == '\t' || *val_start == '"') val_start++;
        char* val_end = val_start + strlen(val_start) - 1;
        while (val_end > val_start && (*val_end == '\n' || *val_end == ' ' || *val_end == '\t' || *val_end == '"')) val_end--;
        int val_len = val_end - val_start + 1;
        memcpy(value, val_start, val_len);
        value[val_len] = '\0';
        
        // Store in PackageInfo
        if (strcmp(key, "name") == 0) {
            strncpy(info->name, value, sizeof(info->name) - 1);
        } else if (strcmp(key, "version") == 0) {
            strncpy(info->version, value, sizeof(info->version) - 1);
        } else if (strcmp(key, "description") == 0) {
            strncpy(info->description, value, sizeof(info->description) - 1);
        } else if (strcmp(key, "author") == 0) {
            strncpy(info->author, value, sizeof(info->author) - 1);
        }
    }
    
    fclose(f);
    return info;
}

void free_package_info(PackageInfo* info) {
    if (info) free(info);
}

// CLI command stubs
int package_install(const char* path) {
    printf("Package installation not yet implemented\n");
    return 1;
}

int package_list() {
    printf("Package listing not yet implemented\n");
    return 0;
}
