// Advanced Networking Runtime for Wyn
// Comprehensive networking utilities with HTTP, WebSocket, and async support

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <time.h>

// ============================================================================
// HTTP Client
// ============================================================================

typedef struct {
    int status_code;
    char* headers;
    char* body;
    int body_len;
} HttpResponse;

// Parse HTTP response
static HttpResponse* parse_http_response(const char* raw) {
    HttpResponse* resp = malloc(sizeof(HttpResponse));
    if (!resp) return NULL;
    
    resp->status_code = 0;
    resp->headers = NULL;
    resp->body = NULL;
    resp->body_len = 0;
    
    // Parse status line
    if (sscanf(raw, "HTTP/%*d.%*d %d", &resp->status_code) != 1) {
        free(resp);
        return NULL;
    }
    
    // Find body separator
    const char* body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        
        // Extract headers
        int header_len = body_start - raw - 4;
        resp->headers = malloc(header_len + 1);
        if (resp->headers) {
            memcpy(resp->headers, raw, header_len);
            resp->headers[header_len] = '\0';
        }
        
        // Extract body
        resp->body_len = strlen(body_start);
        resp->body = malloc(resp->body_len + 1);
        if (resp->body) {
            memcpy(resp->body, body_start, resp->body_len);
            resp->body[resp->body_len] = '\0';
        }
    }
    
    return resp;
}

// HTTP GET request
HttpResponse* Http_get(const char* url) {
    // Parse URL
    char host[256] = {0};
    char path[512] = "/";
    int port = 80;
    int use_https = 0;
    
    if (strncmp(url, "https://", 8) == 0) {
        use_https = 1;
        port = 443;
        sscanf(url + 8, "%255[^/]%511s", host, path);
    } else if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]%511s", host, path);
    } else {
        sscanf(url, "%255[^/]%511s", host, path);
    }
    
    // Check for port in host
    char* colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    
    // Resolve host
    struct hostent* he = gethostbyname(host);
    if (!he) return NULL;
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // Connect
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    
    // Build request
    char request[2048];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Wyn/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);
    
    // Send request
    send(sock, request, strlen(request), 0);
    
    // Receive response
    char buffer[65536] = {0};
    int total = 0;
    int n;
    while ((n = recv(sock, buffer + total, sizeof(buffer) - total - 1, 0)) > 0) {
        total += n;
        if (total >= sizeof(buffer) - 1) break;
    }
    
    close(sock);
    
    return parse_http_response(buffer);
}

// HTTP POST request
HttpResponse* Http_post(const char* url, const char* body, const char* content_type) {
    // Parse URL (same as GET)
    char host[256] = {0};
    char path[512] = "/";
    int port = 80;
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]%511s", host, path);
    } else {
        sscanf(url, "%255[^/]%511s", host, path);
    }
    
    char* colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    
    struct hostent* he = gethostbyname(host);
    if (!he) return NULL;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    
    // Build request
    char request[4096];
    snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Wyn/1.0\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, content_type ? content_type : "application/json",
        strlen(body), body);
    
    send(sock, request, strlen(request), 0);
    
    char buffer[65536] = {0};
    int total = 0;
    int n;
    while ((n = recv(sock, buffer + total, sizeof(buffer) - total - 1, 0)) > 0) {
        total += n;
        if (total >= sizeof(buffer) - 1) break;
    }
    
    close(sock);
    
    return parse_http_response(buffer);
}

// Get response status
int Http_status(HttpResponse* resp) {
    return resp ? resp->status_code : 0;
}

// Get response body
const char* Http_body(HttpResponse* resp) {
    return resp ? resp->body : "";
}

// Get response header
const char* Http_header(HttpResponse* resp, const char* name) {
    if (!resp || !resp->headers) return "";
    
    char search[256];
    snprintf(search, sizeof(search), "%s:", name);
    
    const char* found = strstr(resp->headers, search);
    if (!found) return "";
    
    found += strlen(search);
    while (*found == ' ') found++;
    
    static char value[512];
    int i = 0;
    while (*found && *found != '\r' && *found != '\n' && i < 511) {
        value[i++] = *found++;
    }
    value[i] = '\0';
    
    return value;
}

// Free response
void Http_free(HttpResponse* resp) {
    if (!resp) return;
    free(resp->headers);
    free(resp->body);
    free(resp);
}

// ============================================================================
// TCP Server
// ============================================================================

typedef struct {
    int fd;
    int port;
    int backlog;
} TcpServer;

// Create TCP server
TcpServer* TcpServer_new(int port) {
    TcpServer* server = malloc(sizeof(TcpServer));
    if (!server) return NULL;
    
    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->fd < 0) {
        free(server);
        return NULL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->fd);
        free(server);
        return NULL;
    }
    
    server->port = port;
    server->backlog = 128;
    
    return server;
}

// Start listening
int TcpServer_listen(TcpServer* server) {
    if (!server) return -1;
    return listen(server->fd, server->backlog);
}

// Accept connection
int TcpServer_accept(TcpServer* server) {
    if (!server) return -1;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    return accept(server->fd, (struct sockaddr*)&client_addr, &addr_len);
}

// Close server
void TcpServer_close(TcpServer* server) {
    if (!server) return;
    close(server->fd);
    free(server);
}

// ============================================================================
// Socket utilities
// ============================================================================

// Set socket timeout
int Socket_set_timeout(int sock, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return -1;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return -1;
    
    return 0;
}

// Set non-blocking
int Socket_set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// Poll for read
int Socket_poll_read(int sock, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    
    return poll(&pfd, 1, timeout_ms);
}

// Read line from socket
char* Socket_read_line(int sock) {
    static char buffer[4096];
    int pos = 0;
    
    while (pos < sizeof(buffer) - 1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) break;
        
        buffer[pos++] = c;
        if (c == '\n') break;
    }
    
    buffer[pos] = '\0';
    return strdup(buffer);
}

// ============================================================================
// URL utilities
// ============================================================================

// URL encode
char* Url_encode(const char* str) {
    if (!str) return NULL;
    
    char* encoded = malloc(strlen(str) * 3 + 1);
    if (!encoded) return NULL;
    
    int j = 0;
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[j++] = c;
        } else {
            sprintf(encoded + j, "%%%02X", (unsigned char)c);
            j += 3;
        }
    }
    encoded[j] = '\0';
    
    return encoded;
}

// URL decode
char* Url_decode(const char* str) {
    if (!str) return NULL;
    
    char* decoded = malloc(strlen(str) + 1);
    if (!decoded) return NULL;
    
    int j = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '%' && str[i+1] && str[i+2]) {
            char hex[3] = {str[i+1], str[i+2], 0};
            decoded[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (str[i] == '+') {
            decoded[j++] = ' ';
        } else {
            decoded[j++] = str[i];
        }
    }
    decoded[j] = '\0';
    
    return decoded;
}
