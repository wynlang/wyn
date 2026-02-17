#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Read package.wyn manifest from module directory
PackageInfo* read_package_manifest(const char* module_path) {
    char dir[512];
    const char* last_slash = strrchr(module_path, '/');
    if (last_slash) {
        int len = last_slash - module_path;
        memcpy(dir, module_path, len);
        dir[len] = '\0';
    } else {
        strcpy(dir, ".");
    }
    
    char package_path[512];
    snprintf(package_path, sizeof(package_path), "%s/package.wyn", dir);
    
    struct stat st;
    if (stat(package_path, &st) != 0) return NULL;
    
    FILE* f = fopen(package_path, "r");
    if (!f) return NULL;
    
    PackageInfo* info = calloc(1, sizeof(PackageInfo));
    char line[512];
    
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        
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
        
        if (strcmp(key, "name") == 0) strncpy(info->name, value, sizeof(info->name) - 1);
        else if (strcmp(key, "version") == 0) strncpy(info->version, value, sizeof(info->version) - 1);
        else if (strcmp(key, "description") == 0) strncpy(info->description, value, sizeof(info->description) - 1);
        else if (strcmp(key, "author") == 0) strncpy(info->author, value, sizeof(info->author) - 1);
    }
    
    fclose(f);
    return info;
}

void free_package_info(PackageInfo* info) {
    if (info) free(info);
}

// Ensure directory exists
static void ensure_dir(const char* path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

// Get packages directory
static void get_packages_dir(char* buf, size_t size) {
    const char* home = getenv("HOME");
    if (home) {
        snprintf(buf, size, "%s/.wyn/packages", home);
    } else {
        snprintf(buf, size, "./wyn_modules");
    }
}

// Install from local path
static int install_local(const char* name, const char* source_path) {
    char pkg_dir[512];
    get_packages_dir(pkg_dir, sizeof(pkg_dir));
    ensure_dir(pkg_dir);
    
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/%s", pkg_dir, name);
    ensure_dir(dest);
    
    // Copy .wyn files
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cp '%s'/*.wyn '%s/' 2>/dev/null", source_path, dest);
    int result = system(cmd);
    
    if (result == 0) {
        printf("  ✓ Installed %s from %s\n", name, source_path);
        printf("    → %s\n", dest);
        return 0;
    } else {
        // Try single file
        snprintf(cmd, sizeof(cmd), "cp '%s' '%s/%s.wyn' 2>/dev/null", source_path, dest, name);
        result = system(cmd);
        if (result == 0) {
            printf("  ✓ Installed %s from %s\n", name, source_path);
            printf("    → %s/%s.wyn\n", dest, name);
            return 0;
        }
    }
    
    fprintf(stderr, "  ✗ Failed to install from %s\n", source_path);
    return 1;
}

// Install from git URL
static int install_git(const char* name, const char* url) {
    char pkg_dir[512];
    get_packages_dir(pkg_dir, sizeof(pkg_dir));
    ensure_dir(pkg_dir);
    
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/%s", pkg_dir, name);
    
    // Remove existing
    struct stat st;
    if (stat(dest, &st) == 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dest);
        system(cmd);
    }
    
    // Clone
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "git clone --depth 1 '%s' '%s' 2>&1", url, dest);
    printf("  Cloning %s...\n", url);
    int result = system(cmd);
    
    if (result == 0) {
        // Validate: must contain at least one .wyn file
        char check[1024];
        snprintf(check, sizeof(check), "find '%s' -name '*.wyn' -maxdepth 2 | head -1", dest);
        FILE* fp = popen(check, "r");
        char buf[256] = "";
        if (fp) { fgets(buf, sizeof(buf), fp); pclose(fp); }
        if (buf[0] == '\0') {
            fprintf(stderr, "  \033[31m✗\033[0m Not a valid Wyn package: no .wyn files found in %s\n", url);
            char rm[1024]; snprintf(rm, sizeof(rm), "rm -rf '%s'", dest); system(rm);
            return 1;
        }
        // Warn if no wyn.toml
        char toml_path[512];
        snprintf(toml_path, sizeof(toml_path), "%s/wyn.toml", dest);
        struct stat ts;
        if (stat(toml_path, &ts) != 0) {
            printf("  \033[33m⚠\033[0m No wyn.toml found — package may not have version info\n");
        }
        printf("  \033[32m✓\033[0m Installed %s\n", name);
        printf("    → %s\n", dest);
        return 0;
    }
    
    fprintf(stderr, "  ✗ Failed to clone %s\n", url);
    return 1;
}

// Main install command
int package_install(const char* spec) {
    if (!spec || strlen(spec) == 0 || strcmp(spec, ".") == 0) {
        // Install from wyn.toml in current directory
        printf("Installing dependencies from wyn.toml...\n");
        
        struct stat st;
        if (stat("wyn.toml", &st) != 0) {
            fprintf(stderr, "Error: No wyn.toml found in current directory\n");
            return 1;
        }
        
        // Parse wyn.toml for [dependencies]
        FILE* f = fopen("wyn.toml", "r");
        if (!f) return 1;
        
        char line[512];
        int in_deps = 0;
        int installed = 0;
        
        while (fgets(line, sizeof(line), f)) {
            // Trim
            char* trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            char* end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
            
            if (strcmp(trimmed, "[dependencies]") == 0) { in_deps = 1; continue; }
            if (trimmed[0] == '[') { in_deps = 0; continue; }
            if (!in_deps || trimmed[0] == '#' || trimmed[0] == '\0') continue;
            
            // Parse: name = "path_or_url"
            char* eq = strchr(trimmed, '=');
            if (!eq) continue;
            
            char name[128], source[256];
            int nlen = eq - trimmed;
            while (nlen > 0 && trimmed[nlen-1] == ' ') nlen--;
            memcpy(name, trimmed, nlen);
            name[nlen] = '\0';
            
            char* src = eq + 1;
            while (*src == ' ' || *src == '"') src++;
            char* src_end = src + strlen(src) - 1;
            while (src_end > src && (*src_end == '"' || *src_end == ' ')) src_end--;
            int slen = src_end - src + 1;
            memcpy(source, src, slen);
            source[slen] = '\0';
            
            // Determine source type
            if (strstr(source, "github.com") || strstr(source, "git@") || strstr(source, ".git")) {
                install_git(name, source);
            } else {
                install_local(name, source);
            }
            installed++;
        }
        
        fclose(f);
        printf("\n%d package(s) processed\n", installed);
        return 0;
    }
    
    // Install single package by name/path/url
    // Detect if it's a git URL
    if (strstr(spec, "github.com") || strstr(spec, "git@") || strstr(spec, ".git") || strstr(spec, "://")) {
        // Extract name from URL
        const char* name = strrchr(spec, '/');
        if (name) name++; else name = spec;
        char clean_name[128];
        strncpy(clean_name, name, sizeof(clean_name) - 1);
        // Remove .git suffix
        char* dot_git = strstr(clean_name, ".git");
        if (dot_git) *dot_git = '\0';
        
        return install_git(clean_name, spec);
    }
    
    // Check if it's a local path
    struct stat st;
    if (stat(spec, &st) == 0) {
        const char* name = strrchr(spec, '/');
        if (name) name++; else name = spec;
        char clean_name[128];
        strncpy(clean_name, name, sizeof(clean_name) - 1);
        // Remove .wyn suffix
        char* dot_wyn = strstr(clean_name, ".wyn");
        if (dot_wyn) *dot_wyn = '\0';
        
        return install_local(clean_name, spec);
    }
    
    // Try registry (will fail if server not available)
    printf("Package '%s' not found locally. Trying registry...\n", spec);
    extern int registry_install(const char*);
    return registry_install(spec);
}

int package_list() {
    char pkg_dir[512];
    get_packages_dir(pkg_dir, sizeof(pkg_dir));
    
    printf("Installed packages (%s):\n\n", pkg_dir);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ls -1 '%s' 2>/dev/null", pkg_dir);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        printf("  (none)\n");
        return 0;
    }
    
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), pipe)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0) {
            // Check for package.wyn manifest
            char manifest[512];
            snprintf(manifest, sizeof(manifest), "%s/%s/package.wyn", pkg_dir, line);
            struct stat st;
            if (stat(manifest, &st) == 0) {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", pkg_dir, line);
                PackageInfo* info = read_package_manifest(full_path);
                if (info) {
                    printf("  %s@%s — %s\n", info->name, info->version, info->description);
                    free_package_info(info);
                } else {
                    printf("  %s\n", line);
                }
            } else {
                printf("  %s\n", line);
            }
            count++;
        }
    }
    pclose(pipe);
    
    if (count == 0) printf("  (none)\n");
    printf("\n%d package(s) installed\n", count);
    return 0;
}
