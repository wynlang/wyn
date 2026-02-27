#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#define popen_fn popen
#define pclose_fn pclose
#else
#define popen_fn _popen
#define pclose_fn _pclose
#define unlink _unlink
#endif
#include "registry.h"
#include "toml.h"
#include "semver.h"

#define REGISTRY_URL "https://pkg.wynlang.com"
#define BUFFER_SIZE 65536

// Validate a string is safe for use in URLs/paths (alphanumeric, hyphens, underscores, dots, @)
static int is_safe_identifier(const char *s) {
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
              || c == '-' || c == '_' || c == '.' || c == '@' || c == '+')) return 0;
    }
    return 1;
}

// Looser validation for search queries (allows spaces)
static int is_safe_query(const char *s) {
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
              || c == '-' || c == '_' || c == '.' || c == ' ' || c == '+')) return 0;
    }
    return 1;
}

// HTTP GET via curl (supports HTTPS via Cloudflare)
static int http_get(const char *host, const char *path, char **response_body) {
    (void)host;

    // Check curl is available (once)
    static int curl_checked = 0, curl_ok = 0;
    if (!curl_checked) {
        curl_checked = 1;
#ifdef _WIN32
        curl_ok = (system("where curl >nul 2>&1") == 0);
#else
        curl_ok = (system("command -v curl >/dev/null 2>&1") == 0);
#endif
    }
    if (!curl_ok) {
        fprintf(stderr, "Error: 'curl' is required for registry operations but was not found.\n");
#ifdef _WIN32
        fprintf(stderr, "curl ships with Windows 10+. Update Windows or install curl from https://curl.se\n");
#else
        fprintf(stderr, "Install it: brew install curl (macOS) / apt install curl (Linux)\n");
#endif
        return -1;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -sS --max-time 10 '%s%s' 2>/dev/null", REGISTRY_URL, path);

    FILE *p = popen_fn(cmd, "r");
    if (!p) return -1;

    size_t buf_sz = BUFFER_SIZE;
    char *buf = malloc(buf_sz);
    if (!buf) { pclose_fn(p); return -1; }
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, 4096, p)) > 0) {
        total += n;
        if (total + 4096 >= buf_sz) {
            buf_sz *= 2;
            char *nb = realloc(buf, buf_sz);
            if (!nb) break;
            buf = nb;
        }
    }
    buf[total] = '\0';
    int status = pclose_fn(p);

    if (status != 0 || total == 0) { free(buf); return -1; }
    *response_body = buf;
    return 0;
}

// Download file via curl
static int http_download(const char *url, const char *output_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -sS --max-time 30 -o '%s' '%s' 2>/dev/null", output_path, url);
    return system(cmd);
}

int registry_search(const char *query) {
    if (!query || !query[0]) {
        fprintf(stderr, "Usage: wyn pkg search <query>\n");
        return 1;
    }
    if (!is_safe_query(query)) {
        fprintf(stderr, "Error: Invalid search query\n");
        return 1;
    }
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/search?q=%s", query);
    
    if (http_get(NULL, path, &response) != 0) {
        fprintf(stderr, "Error: Failed to connect to package registry\n");
        return 1;
    }
    
    if (response) {
        // Parse and display results
        // Response: {"packages":[{"name":"...","latest":"...","description":"...","downloads":0},...]}
        const char *p = response;
        const char *arr = strstr(p, "\"packages\"");
        if (!arr) { printf("No results\n"); free(response); return 0; }
        
        int count = 0;
        const char *cur = arr;
        while ((cur = strstr(cur, "\"name\":\"")) != NULL) {
            cur += 8;
            const char *end = strchr(cur, '"');
            if (!end) break;
            char name[128]; size_t len = end - cur; if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, cur, len); name[len] = '\0';
            
            // Find version
            char ver[64] = "?";
            const char *vp = strstr(end, "\"latest\":\"");
            if (vp && vp < end + 200) {
                vp += 10; const char *ve = strchr(vp, '"');
                if (ve) { size_t vl = ve - vp; if (vl >= sizeof(ver)) vl = sizeof(ver) - 1; memcpy(ver, vp, vl); ver[vl] = '\0'; }
            }
            
            // Find description
            char desc[256] = "";
            const char *dp = strstr(end, "\"description\":\"");
            if (dp && dp < end + 400) {
                dp += 15; const char *de = strchr(dp, '"');
                if (de) { size_t dl = de - dp; if (dl >= sizeof(desc)) dl = sizeof(desc) - 1; memcpy(desc, dp, dl); desc[dl] = '\0'; }
            }
            
            printf("  %-20s v%-10s %s\n", name, ver, desc);
            count++;
        }
        if (count == 0) printf("No packages found for '%s'\n", query);
        else printf("\n%d package(s) found\n", count);
        free(response);
        return 0;
    }
    
    return 1;
}

int registry_info(const char *package) {
    if (!package || !is_safe_identifier(package)) {
        fprintf(stderr, "Error: Invalid package name\n");
        return 1;
    }
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s", package);
    
    if (http_get(NULL, path, &response) != 0) {
        fprintf(stderr, "Error: Package '%s' not found in registry\n", package);
        return 1;
    }
    
    if (response) {
        // Parse and display fields from {"package":{...}}
        // Helper to extract a string field
        #define SHOW(label, key) do { \
            const char* _p = strstr(response, "\"" key "\":\""); \
            if (_p) { _p += strlen("\"" key "\":\""); const char* _e = strchr(_p, '"'); \
                if (_e) { printf("  %-14s%.*s\n", label, (int)(_e - _p), _p); } } \
        } while(0)
        
        printf("\n");
        SHOW("Name:", "name");
        SHOW("Latest:", "latest");
        SHOW("Author:", "author");
        SHOW("Description:", "description");
        
        // Downloads (numeric)
        const char* dp = strstr(response, "\"downloads\":");
        if (dp) { dp += 12; printf("  %-14s%d\n", "Downloads:", atoi(dp)); }
        
        // Versions array with per-version downloads
        const char* vp = strstr(response, "\"versions\":[");
        if (vp) {
            vp += 12;
            const char* ve = strchr(vp, ']');
            if (ve) {
                printf("  Versions:\n");
                const char* c = vp;
                // Find version_downloads object
                const char* vdl = strstr(response, "\"version_downloads\":{");
                while (c < ve) {
                    if (*c == '"') {
                        c++;
                        const char* end = strchr(c, '"');
                        if (end && end < ve) {
                            char ver[64]; size_t vl = end - c; if (vl >= sizeof(ver)) vl = sizeof(ver)-1;
                            memcpy(ver, c, vl); ver[vl] = '\0';
                            // Find download count for this version
                            int vdl_count = 0;
                            if (vdl) {
                                char needle[80]; snprintf(needle, sizeof(needle), "\"%s\":", ver);
                                const char* vdc = strstr(vdl, needle);
                                if (vdc) { vdc += strlen(needle); vdl_count = atoi(vdc); }
                            }
                            printf("    %-12s %d downloads\n", ver, vdl_count);
                            c = end + 1;
                        } else break;
                    } else c++;
                }
            }
        }
        
        printf("\n  Install:      wyn pkg install %s\n\n", package);
        
        #undef SHOW
        free(response);
        return 0;
    }
    
    return 1;
}

int registry_versions(const char *package) {
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s/versions", package);
    
    if (http_get(NULL, path, &response) != 0) {
        fprintf(stderr, "Error: Package '%s' not found in registry\n", package);
        return 1;
    }
    
    if (response) {
        printf("%s\n", response);
        free(response);
        return 0;
    }
    
    return 1;
}

int registry_install(const char *package_spec) {
    char package[256];
    char version[64] = "latest";
    
    // Parse package@version
    const char *at = strchr(package_spec, '@');
    if (at) {
        size_t pkg_len = at - package_spec;
        if (pkg_len >= sizeof(package)) pkg_len = sizeof(package) - 1;
        memcpy(package, package_spec, pkg_len);
        package[pkg_len] = '\0';
        strncpy(version, at + 1, sizeof(version) - 1);
        version[sizeof(version) - 1] = '\0';
    } else {
        strncpy(package, package_spec, sizeof(package) - 1);
        package[sizeof(package) - 1] = '\0';
    }

    if (!is_safe_identifier(package) || (strcmp(version, "latest") != 0 && !is_safe_identifier(version))) {
        fprintf(stderr, "Error: Invalid package name or version\n");
        return 1;
    }
    
    printf("Installing %s@%s...\n", package, version);
    
    // Check if already installed (skip if requesting specific version)
    {
        char check_dir[512];
        snprintf(check_dir, sizeof(check_dir), "packages/%s", package);
        struct stat dir_st;
        if (stat(check_dir, &dir_st) == 0 && strcmp(version, "latest") == 0) {
            printf("Package '%s' is already installed in %s/\n", package, check_dir);
            printf("To reinstall, run: wyn pkg uninstall %s && wyn pkg install %s\n", package, package_spec);
            return 0;
        }
        if (stat(check_dir, &dir_st) == 0 && strcmp(version, "latest") != 0) {
            // Different version requested — remove old, install new
            char rm_cmd[600];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf 'packages/%s'", package);
            system(rm_cmd);
        }
    }
    
    // Download tarball
    char url[512];
    if (strcmp(version, "latest") == 0) {
        snprintf(url, sizeof(url), "%s/api/download/%s", REGISTRY_URL, package);
    } else {
        snprintf(url, sizeof(url), "%s/api/download/%s/%s", REGISTRY_URL, package, version);
    }
    
    char tarball[512];
    snprintf(tarball, sizeof(tarball), "/tmp/wyn-pkg-%s.tar.gz", package);
    
    if (http_download(url, tarball) != 0) {
        fprintf(stderr, "Error: Failed to download package '%s'\n", package);
        fprintf(stderr, "Use Git URL instead: wyn pkg install github.com/wynlang/%s\n", package);
        return 1;
    }
    
    // Check if we got a valid tarball (not a JSON error)
    FILE *f = fopen(tarball, "rb");
    if (f) {
        int c = fgetc(f);
        fclose(f);
        if (c == '{') {
            // Got JSON error response, not a tarball
            fprintf(stderr, "Error: Package '%s' version '%s' not found in registry\n", package, version);
            unlink(tarball);
            return 1;
        }
    }
    
    // Create packages directory and extract
    char pkg_dir[512];
    snprintf(pkg_dir, sizeof(pkg_dir), "packages/%s", package);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && tar xzf '%s' -C '%s' 2>/dev/null", pkg_dir, tarball, pkg_dir);
    int rc = system(cmd);
    unlink(tarball);
    
    if (rc != 0) {
        fprintf(stderr, "Error: Failed to extract package\n");
        return 1;
    }
    
    printf("Installed %s@%s → %s/\n", package, version, pkg_dir);
    return 0;
}

int registry_resolve_version(const char *package, const char *constraint, char *resolved_version, size_t buf_size) {
    (void)constraint;
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s", package);
    
    if (http_get(NULL, path, &response) != 0) {
        return -1;
    }
    
    if (response) {
        // Extract "latest" field from response
        const char *lp = strstr(response, "\"latest\":\"");
        if (lp) {
            lp += 10;
            const char *end = strchr(lp, '"');
            if (end) {
                size_t len = end - lp;
                if (len >= buf_size) len = buf_size - 1;
                memcpy(resolved_version, lp, len);
                resolved_version[len] = '\0';
                free(response);
                return 0;
            }
        }
        free(response);
    }
    
    return -1;
}

int registry_publish(int dry_run) {
    // Validate wyn.toml exists
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA("wyn.toml");
    if (attrs == INVALID_FILE_ATTRIBUTES) {
#else
    if (access("wyn.toml", F_OK) != 0) {
#endif
        fprintf(stderr, "Error: wyn.toml not found. Run 'wyn init' first.\n");
        return 1;
    }
    
    // Parse wyn.toml
    WynConfig *config = wyn_config_parse("wyn.toml");
    if (!config) {
        fprintf(stderr, "Error: Failed to parse wyn.toml\n");
        return 1;
    }
    
    // Validate required fields
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
        wyn_config_free(config);
        return 0;
    }
    
    printf("Publishing %s@%s...\n", config->project.name, config->project.version);
    
    // TODO: Create tarball and upload via HTTP POST
    fprintf(stderr, "Error: Publish functionality requires registry server\n");
    fprintf(stderr, "Note: Registry server not deployed yet (coming in v1.5.1)\n");
    fprintf(stderr, "Visit https://github.com/topics/wyn-package for packages\n");
    
    wyn_config_free(config);
    return 1;
}
