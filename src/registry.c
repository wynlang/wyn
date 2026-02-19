#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define close closesocket
#else
    #define _POSIX_C_SOURCE 200809L
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

#include <sys/stat.h>
#include "registry.h"
#include "toml.h"
#include "semver.h"

#define REGISTRY_HOST "github.com"
#define REGISTRY_PORT 443  // HTTPS
#define BUFFER_SIZE 8192

// Initialize Winsock on Windows
static int init_sockets(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    return 0;
#endif
}

// Cleanup Winsock on Windows
static void cleanup_sockets(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Simple HTTP client using POSIX sockets (or Winsock2 on Windows)
static int http_get(const char *host, const char *path, char **response_body) {
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd;
    char request[2048];
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    // Initialize sockets (Windows only)
    if (init_sockets() != 0) {
        fprintf(stderr, "Error: Cannot initialize sockets\n");
        return -1;
    }
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error: Cannot create socket\n");
        cleanup_sockets();
        return -1;
    }
    
    // Get host by name
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: Cannot resolve host %s\n", host);
        close(sockfd);
        cleanup_sockets();
        return -1;
    }
    
    // Setup server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(80);  // Use HTTP for now (HTTPS requires SSL/TLS)
    
    // Connect
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error: Cannot connect to %s\n", host);
        close(sockfd);
        cleanup_sockets();
        return -1;
    }
    
    // Build HTTP request
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: wyn-cli/1.5.0\r\n"
             "Accept: application/json\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);
    
    // Send request
    if (send(sockfd, request, strlen(request), 0) < 0) {
        fprintf(stderr, "Error: Cannot send request\n");
        close(sockfd);
        cleanup_sockets();
        return -1;
    }
    
    // Receive response
    char *full_response = malloc(1);
    full_response[0] = '\0';
    size_t total_size = 0;
    
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        char *new_response = realloc(full_response, total_size + bytes_received + 1);
        if (!new_response) {
            free(full_response);
            close(sockfd);
            cleanup_sockets();
            return -1;
        }
        full_response = new_response;
        memcpy(full_response + total_size, buffer, bytes_received);
        total_size += bytes_received;
        full_response[total_size] = '\0';
    }
    
    close(sockfd);
    cleanup_sockets();
    
    // Parse HTTP response - extract body
    char *body_start = strstr(full_response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        *response_body = strdup(body_start);
        free(full_response);
        return 0;
    }
    
    free(full_response);
    return -1;
}

int registry_search(const char *query) {
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/search?q=%s", query);
    
    if (http_get(REGISTRY_HOST, path, &response) != 0) {
        fprintf(stderr, "Error: Failed to connect to package registry\n");
        fprintf(stderr, "Note: Use Git-based packages instead\n");
        fprintf(stderr, "Visit https://github.com/topics/wyn-package for packages\n");
        return 1;
    }
    
    if (response) {
        printf("%s\n", response);
        free(response);
        return 0;
    }
    
    return 1;
}

int registry_info(const char *package) {
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s", package);
    
    if (http_get(REGISTRY_HOST, path, &response) != 0) {
        fprintf(stderr, "Error: Failed to connect to package registry\n");
        fprintf(stderr, "Note: Use Git-based packages instead\n");
        fprintf(stderr, "Visit https://github.com/topics/wyn-package for packages\n");
        return 1;
    }
    
    if (response) {
        printf("%s\n", response);
        free(response);
        return 0;
    }
    
    return 1;
}

int registry_versions(const char *package) {
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s/versions", package);
    
    if (http_get(REGISTRY_HOST, path, &response) != 0) {
        fprintf(stderr, "Error: Failed to connect to package registry\n");
        fprintf(stderr, "Note: Use Git-based packages instead\n");
        fprintf(stderr, "Visit https://github.com/topics/wyn-package for packages\n");
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
    
    printf("Installing %s@%s...\n", package, version);
    
    char path[512];
    char *response = NULL;
    
    if (strcmp(version, "latest") == 0) {
        snprintf(path, sizeof(path), "/api/packages/%s", package);
    } else {
        snprintf(path, sizeof(path), "/api/packages/%s/versions/%s", package, version);
    }
    
    if (http_get(REGISTRY_HOST, path, &response) != 0) {
        fprintf(stderr, "Error: Failed to connect to package registry\n");
        fprintf(stderr, "Note: Use Git-based packages instead\n");
        fprintf(stderr, "Visit https://github.com/topics/wyn-package for packages\n");
        return 1;
    }
    
    if (response) {
        // Registry not yet available
        fprintf(stderr, "Error: Package registry not available yet\n");
        fprintf(stderr, "Use Git URL instead: wyn pkg install github.com/wynlang/%s\n", package);
        free(response);
        return 1;
    }
    
    return 1;
}

int registry_resolve_version(const char *package, const char *constraint, char *resolved_version, size_t buf_size) {
    char path[512];
    char *response = NULL;
    
    snprintf(path, sizeof(path), "/api/packages/%s/versions", package);
    
    if (http_get(REGISTRY_HOST, path, &response) != 0) {
        return -1;
    }
    
    if (response) {
        // TODO: Parse JSON and match constraint using semver
        // For now, just use "latest"
        strncpy(resolved_version, "latest", buf_size - 1);
        resolved_version[buf_size - 1] = '\0';
        free(response);
        return 0;
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
