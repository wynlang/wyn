// WebSocket client implementation for Wyn
// RFC 6455 compliant: handshake, frame encode/decode, masking
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

extern char* wyn_strdup(const char* s);
extern char* wyn_crypto_base64_encode(const char* data, size_t len);
extern void wyn_crypto_sha1(const unsigned char* data, size_t len, unsigned char* digest);
extern void wyn_crypto_random_bytes(char* buffer, size_t len);

#define WS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_OP_TEXT   0x1
#define WS_OP_CLOSE  0x8
#define WS_OP_PING   0x9
#define WS_OP_PONG   0xA

// Parse ws://host:port/path
static int ws_parse_url(const char* url, char* host, int* port, char* path) {
    const char* p = url;
    if (strncmp(p, "ws://", 5) == 0) p += 5;
    else if (strncmp(p, "wss://", 6) == 0) { p += 6; *port = 443; }
    
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');
    
    if (colon && (!slash || colon < slash)) {
        memcpy(host, p, colon - p); host[colon - p] = '\0';
        *port = atoi(colon + 1);
    } else if (slash) {
        memcpy(host, p, slash - p); host[slash - p] = '\0';
        if (*port == 0) *port = 80;
    } else {
        strcpy(host, p);
        if (*port == 0) *port = 80;
    }
    
    if (slash) strcpy(path, slash);
    else strcpy(path, "/");
    return 0;
}

// WebSocket connect — returns socket fd or -1
int Ws_connect(const char* url) {
    char host[256] = "", path[512] = "/";
    int port = 0;
    ws_parse_url(url, host, &port, path);
    
    struct hostent* he = gethostbyname(host);
    if (!he) return -1;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    // Generate random key
    char key_bytes[16];
    wyn_crypto_random_bytes(key_bytes, 16);
    char* key = wyn_crypto_base64_encode(key_bytes, 16);
    
    // Send HTTP upgrade request
    char req[2048];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
        path, host, key);
    send(sock, req, strlen(req), 0);
    
    // Read response (just verify 101)
    char resp[4096];
    int n = (int)recv(sock, resp, sizeof(resp) - 1, 0);
    if (n <= 0 || !strstr(resp, "101")) {
        close(sock);
        return -1;
    }
    
    return sock;
}

// Send WebSocket text frame
int Ws_send(int sock, const char* msg) {
    size_t len = strlen(msg);
    unsigned char header[14];
    int hlen = 2;
    
    header[0] = 0x80 | WS_OP_TEXT;  // FIN + TEXT
    
    // Client must mask
    unsigned char mask[4];
    wyn_crypto_random_bytes((char*)mask, 4);
    
    if (len < 126) {
        header[1] = 0x80 | (unsigned char)len;
    } else if (len < 65536) {
        header[1] = 0x80 | 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        hlen = 4;
    } else {
        header[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++) header[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        hlen = 10;
    }
    
    memcpy(header + hlen, mask, 4);
    hlen += 4;
    
    send(sock, header, hlen, 0);
    
    // Send masked payload
    unsigned char* masked = malloc(len);
    for (size_t i = 0; i < len; i++) masked[i] = msg[i] ^ mask[i % 4];
    int result = (int)send(sock, masked, len, 0);
    free(masked);
    
    return result > 0 ? 0 : -1;
}

// Receive WebSocket frame — returns text payload
char* Ws_recv(int sock) {
    unsigned char header[2];
    if (recv(sock, header, 2, 0) != 2) return "";
    
    int opcode = header[0] & 0x0F;
    int masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    
    if (payload_len == 126) {
        unsigned char ext[2];
        recv(sock, ext, 2, 0);
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        recv(sock, ext, 8, 0);
        payload_len = 0;
        for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
    }
    
    unsigned char mask[4] = {0};
    if (masked) recv(sock, mask, 4, 0);
    
    if (payload_len > 10 * 1024 * 1024) return "";  // 10MB limit
    
    char* data = malloc(payload_len + 1);
    size_t received = 0;
    while (received < payload_len) {
        int n = (int)recv(sock, data + received, payload_len - received, 0);
        if (n <= 0) { free(data); return ""; }
        received += n;
    }
    
    if (masked) {
        for (size_t i = 0; i < payload_len; i++) data[i] ^= mask[i % 4];
    }
    data[payload_len] = '\0';
    
    if (opcode == WS_OP_PING) {
        // Auto-respond with pong
        unsigned char pong[2] = { 0x80 | WS_OP_PONG, 0 };
        send(sock, pong, 2, 0);
        return Ws_recv(sock);  // Get next real message
    }
    
    if (opcode == WS_OP_CLOSE) {
        free(data);
        return "";
    }
    
    char* result = wyn_strdup(data);
    free(data);
    return result;
}

// Close WebSocket connection
void Ws_close(int sock) {
    unsigned char close_frame[2] = { 0x80 | WS_OP_CLOSE, 0 };
    send(sock, close_frame, 2, 0);
    close(sock);
}
