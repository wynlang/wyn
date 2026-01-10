#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Convert system errno to WynNetError
static WynNetError errno_to_net_error(int err) {
    switch (err) {
        case ECONNREFUSED: return WYN_NET_ERROR_CONNECTION_REFUSED;
        case ETIMEDOUT: return WYN_NET_ERROR_TIMEOUT;
        case ENETUNREACH: return WYN_NET_ERROR_NETWORK_UNREACHABLE;
        case EADDRINUSE: return WYN_NET_ERROR_ADDRESS_IN_USE;
        case EACCES: return WYN_NET_ERROR_PERMISSION_DENIED;
        case EINTR: return WYN_NET_ERROR_INTERRUPTED;
        case EWOULDBLOCK: return WYN_NET_ERROR_WOULD_BLOCK;
        default: return WYN_NET_ERROR_UNKNOWN;
    }
}

// Socket address operations
WynSocketAddr* wyn_socket_addr_new(const char* ip, uint16_t port) {
    if (!ip) return NULL;
    
    WynSocketAddr* addr = malloc(sizeof(WynSocketAddr));
    if (!addr) return NULL;
    
    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
    addr->ip[sizeof(addr->ip) - 1] = '\0';
    addr->port = port;
    
    // Determine address family
    if (strchr(ip, ':')) {
        addr->family = WYN_AF_IPV6;
    } else {
        addr->family = WYN_AF_IPV4;
    }
    
    return addr;
}

WynSocketAddr* wyn_socket_addr_parse(const char* addr_str) {
    if (!addr_str) return NULL;
    
    char* addr_copy = strdup(addr_str);
    if (!addr_copy) return NULL;
    
    char* colon = strrchr(addr_copy, ':');
    if (!colon) {
        free(addr_copy);
        return NULL;
    }
    
    *colon = '\0';
    char* ip = addr_copy;
    char* port_str = colon + 1;
    
    // Handle IPv6 addresses in brackets
    if (ip[0] == '[') {
        ip++;
        char* bracket = strchr(ip, ']');
        if (bracket) *bracket = '\0';
    }
    
    uint16_t port = (uint16_t)atoi(port_str);
    WynSocketAddr* result = wyn_socket_addr_new(ip, port);
    
    free(addr_copy);
    return result;
}

void wyn_socket_addr_free(WynSocketAddr* addr) {
    free(addr);
}

char* wyn_socket_addr_to_string(const WynSocketAddr* addr) {
    if (!addr) return NULL;
    
    char* result = malloc(64);
    if (!result) return NULL;
    
    if (addr->family == WYN_AF_IPV6) {
        snprintf(result, 64, "[%s]:%u", addr->ip, addr->port);
    } else {
        snprintf(result, 64, "%s:%u", addr->ip, addr->port);
    }
    
    return result;
}

// Base socket operations
WynSocket* wyn_socket_new(WynSocketType type, WynNetError* error) {
    WynSocket* sock = malloc(sizeof(WynSocket));
    if (!sock) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        return NULL;
    }
    
    int sock_type = (type == WYN_SOCKET_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    sock->fd = socket(AF_INET, sock_type, 0);
    
    if (sock->fd < 0) {
        if (error) *error = errno_to_net_error(errno);
        free(sock);
        return NULL;
    }
    
    sock->type = type;
    sock->is_connected = false;
    sock->is_listening = false;
    sock->is_blocking = true;
    memset(&sock->local_addr, 0, sizeof(sock->local_addr));
    
    if (error) *error = WYN_NET_SUCCESS;
    return sock;
}

WynNetError wyn_socket_bind(WynSocket* socket, const WynSocketAddr* addr) {
    if (!socket || !addr) return WYN_NET_ERROR_INVALID_ADDR;
    
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &sock_addr.sin_addr) <= 0) {
        return WYN_NET_ERROR_INVALID_ADDR;
    }
    
    if (bind(socket->fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        return errno_to_net_error(errno);
    }
    
    socket->local_addr = *addr;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_connect(WynSocket* socket, const WynSocketAddr* addr) {
    if (!socket || !addr) return WYN_NET_ERROR_INVALID_ADDR;
    
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &sock_addr.sin_addr) <= 0) {
        return WYN_NET_ERROR_INVALID_ADDR;
    }
    
    if (connect(socket->fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        return errno_to_net_error(errno);
    }
    
    socket->is_connected = true;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_listen(WynSocket* socket, int backlog) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    if (listen(socket->fd, backlog) < 0) {
        return errno_to_net_error(errno);
    }
    
    socket->is_listening = true;
    return WYN_NET_SUCCESS;
}

WynSocket* wyn_socket_accept(WynSocket* socket, WynSocketAddr* peer_addr, WynNetError* error) {
    if (!socket || !socket->is_listening) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(socket->fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (error) *error = errno_to_net_error(errno);
        return NULL;
    }
    
    WynSocket* client_socket = malloc(sizeof(WynSocket));
    if (!client_socket) {
        close(client_fd);
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        return NULL;
    }
    
    client_socket->fd = client_fd;
    client_socket->type = socket->type;
    client_socket->is_connected = true;
    client_socket->is_listening = false;
    client_socket->is_blocking = true;
    
    if (peer_addr) {
        inet_ntop(AF_INET, &client_addr.sin_addr, peer_addr->ip, sizeof(peer_addr->ip));
        peer_addr->port = ntohs(client_addr.sin_port);
        peer_addr->family = WYN_AF_IPV4;
    }
    
    if (error) *error = WYN_NET_SUCCESS;
    return client_socket;
}

WynNetError wyn_socket_send(WynSocket* socket, const void* data, size_t len, size_t* sent) {
    if (!socket || !data) return WYN_NET_ERROR_INVALID_ADDR;
    
    ssize_t result = send(socket->fd, data, len, 0);
    if (result < 0) {
        return errno_to_net_error(errno);
    }
    
    if (sent) *sent = (size_t)result;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_recv(WynSocket* socket, void* buffer, size_t len, size_t* received) {
    if (!socket || !buffer) return WYN_NET_ERROR_INVALID_ADDR;
    
    ssize_t result = recv(socket->fd, buffer, len, 0);
    if (result < 0) {
        return errno_to_net_error(errno);
    }
    
    if (received) *received = (size_t)result;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_sendto(WynSocket* socket, const void* data, size_t len, const WynSocketAddr* addr, size_t* sent) {
    if (!socket || !data || !addr) return WYN_NET_ERROR_INVALID_ADDR;
    
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &sock_addr.sin_addr) <= 0) {
        return WYN_NET_ERROR_INVALID_ADDR;
    }
    
    ssize_t result = sendto(socket->fd, data, len, 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (result < 0) {
        return errno_to_net_error(errno);
    }
    
    if (sent) *sent = (size_t)result;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_recvfrom(WynSocket* socket, void* buffer, size_t len, WynSocketAddr* addr, size_t* received) {
    if (!socket || !buffer) return WYN_NET_ERROR_INVALID_ADDR;
    
    struct sockaddr_in sock_addr;
    socklen_t addr_len = sizeof(sock_addr);
    
    ssize_t result = recvfrom(socket->fd, buffer, len, 0, (struct sockaddr*)&sock_addr, &addr_len);
    if (result < 0) {
        return errno_to_net_error(errno);
    }
    
    if (addr) {
        inet_ntop(AF_INET, &sock_addr.sin_addr, addr->ip, sizeof(addr->ip));
        addr->port = ntohs(sock_addr.sin_port);
        addr->family = WYN_AF_IPV4;
    }
    
    if (received) *received = (size_t)result;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_close(WynSocket* socket) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    if (close(socket->fd) < 0) {
        return errno_to_net_error(errno);
    }
    
    socket->is_connected = false;
    socket->is_listening = false;
    return WYN_NET_SUCCESS;
}

void wyn_socket_free(WynSocket* socket) {
    if (!socket) return;
    
    if (socket->fd >= 0) {
        wyn_socket_close(socket);
    }
    
    free(socket);
}

// Socket configuration
WynNetError wyn_socket_set_blocking(WynSocket* socket, bool blocking) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    int flags = fcntl(socket->fd, F_GETFL, 0);
    if (flags < 0) return errno_to_net_error(errno);
    
    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    
    if (fcntl(socket->fd, F_SETFL, flags) < 0) {
        return errno_to_net_error(errno);
    }
    
    socket->is_blocking = blocking;
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_set_reuse_addr(WynSocket* socket, bool reuse) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    int opt = reuse ? 1 : 0;
    if (setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return errno_to_net_error(errno);
    }
    
    return WYN_NET_SUCCESS;
}

WynNetError wyn_socket_set_timeout(WynSocket* socket, int timeout_ms) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(socket->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        return errno_to_net_error(errno);
    }
    
    if (setsockopt(socket->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        return errno_to_net_error(errno);
    }
    
    return WYN_NET_SUCCESS;
}

// TCP Listener operations
WynTcpListener* wyn_tcp_listener_bind(const char* addr, WynNetError* error) {
    WynSocketAddr* socket_addr = wyn_socket_addr_parse(addr);
    if (!socket_addr) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    WynSocket* socket = wyn_socket_new(WYN_SOCKET_TCP, error);
    if (!socket) {
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    // Set SO_REUSEADDR
    wyn_socket_set_reuse_addr(socket, true);
    
    WynNetError bind_error = wyn_socket_bind(socket, socket_addr);
    if (bind_error != WYN_NET_SUCCESS) {
        if (error) *error = bind_error;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynNetError listen_error = wyn_socket_listen(socket, 128);
    if (listen_error != WYN_NET_SUCCESS) {
        if (error) *error = listen_error;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynTcpListener* listener = malloc(sizeof(WynTcpListener));
    if (!listener) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    listener->socket = socket;
    listener->addr = *socket_addr;
    wyn_socket_addr_free(socket_addr);
    
    if (error) *error = WYN_NET_SUCCESS;
    return listener;
}

WynTcpStream* wyn_tcp_listener_accept(WynTcpListener* listener, WynNetError* error) {
    if (!listener) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    WynSocketAddr peer_addr;
    WynSocket* client_socket = wyn_socket_accept(listener->socket, &peer_addr, error);
    if (!client_socket) {
        return NULL;
    }
    
    WynTcpStream* stream = malloc(sizeof(WynTcpStream));
    if (!stream) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        wyn_socket_free(client_socket);
        return NULL;
    }
    
    stream->socket = client_socket;
    stream->peer_addr = peer_addr;
    
    return stream;
}

WynNetError wyn_tcp_listener_close(WynTcpListener* listener) {
    if (!listener) return WYN_NET_ERROR_INVALID_ADDR;
    return wyn_socket_close(listener->socket);
}

void wyn_tcp_listener_free(WynTcpListener* listener) {
    if (!listener) return;
    wyn_socket_free(listener->socket);
    free(listener);
}

// TCP Stream operations
WynTcpStream* wyn_tcp_stream_connect(const char* addr, WynNetError* error) {
    WynSocketAddr* socket_addr = wyn_socket_addr_parse(addr);
    if (!socket_addr) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    WynSocket* socket = wyn_socket_new(WYN_SOCKET_TCP, error);
    if (!socket) {
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynNetError connect_error = wyn_socket_connect(socket, socket_addr);
    if (connect_error != WYN_NET_SUCCESS) {
        if (error) *error = connect_error;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynTcpStream* stream = malloc(sizeof(WynTcpStream));
    if (!stream) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    stream->socket = socket;
    stream->peer_addr = *socket_addr;
    wyn_socket_addr_free(socket_addr);
    
    if (error) *error = WYN_NET_SUCCESS;
    return stream;
}

WynNetError wyn_tcp_stream_send(WynTcpStream* stream, const void* data, size_t len, size_t* sent) {
    if (!stream) return WYN_NET_ERROR_INVALID_ADDR;
    return wyn_socket_send(stream->socket, data, len, sent);
}

WynNetError wyn_tcp_stream_recv(WynTcpStream* stream, void* buffer, size_t len, size_t* received) {
    if (!stream) return WYN_NET_ERROR_INVALID_ADDR;
    return wyn_socket_recv(stream->socket, buffer, len, received);
}

WynNetError wyn_tcp_stream_send_all(WynTcpStream* stream, const void* data, size_t len) {
    if (!stream || !data) return WYN_NET_ERROR_INVALID_ADDR;
    
    size_t total_sent = 0;
    const char* ptr = (const char*)data;
    
    while (total_sent < len) {
        size_t sent;
        WynNetError error = wyn_tcp_stream_send(stream, ptr + total_sent, len - total_sent, &sent);
        if (error != WYN_NET_SUCCESS) {
            return error;
        }
        total_sent += sent;
    }
    
    return WYN_NET_SUCCESS;
}

char* wyn_tcp_stream_recv_line(WynTcpStream* stream, WynNetError* error) {
    if (!stream) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    char* line = malloc(1024);
    if (!line) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        return NULL;
    }
    
    size_t pos = 0;
    char ch;
    
    while (pos < 1023) {
        size_t received;
        WynNetError recv_error = wyn_tcp_stream_recv(stream, &ch, 1, &received);
        
        if (recv_error != WYN_NET_SUCCESS) {
            if (error) *error = recv_error;
            free(line);
            return NULL;
        }
        
        if (received == 0) break;  // Connection closed
        
        if (ch == '\n') break;
        if (ch != '\r') {
            line[pos++] = ch;
        }
    }
    
    line[pos] = '\0';
    if (error) *error = WYN_NET_SUCCESS;
    return line;
}

WynNetError wyn_tcp_stream_close(WynTcpStream* stream) {
    if (!stream) return WYN_NET_ERROR_INVALID_ADDR;
    return wyn_socket_close(stream->socket);
}

void wyn_tcp_stream_free(WynTcpStream* stream) {
    if (!stream) return;
    wyn_socket_free(stream->socket);
    free(stream);
}

// UDP Socket operations
WynUdpSocket* wyn_udp_socket_bind(const char* addr, WynNetError* error) {
    WynSocketAddr* socket_addr = wyn_socket_addr_parse(addr);
    if (!socket_addr) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    WynSocket* socket = wyn_socket_new(WYN_SOCKET_UDP, error);
    if (!socket) {
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynNetError bind_error = wyn_socket_bind(socket, socket_addr);
    if (bind_error != WYN_NET_SUCCESS) {
        if (error) *error = bind_error;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    WynUdpSocket* udp_socket = malloc(sizeof(WynUdpSocket));
    if (!udp_socket) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        wyn_socket_free(socket);
        wyn_socket_addr_free(socket_addr);
        return NULL;
    }
    
    udp_socket->socket = socket;
    wyn_socket_addr_free(socket_addr);
    
    if (error) *error = WYN_NET_SUCCESS;
    return udp_socket;
}

WynNetError wyn_udp_socket_send_to(WynUdpSocket* socket, const void* data, size_t len, const char* addr, size_t* sent) {
    if (!socket || !addr) return WYN_NET_ERROR_INVALID_ADDR;
    
    WynSocketAddr* dest_addr = wyn_socket_addr_parse(addr);
    if (!dest_addr) return WYN_NET_ERROR_INVALID_ADDR;
    
    WynNetError result = wyn_socket_sendto(socket->socket, data, len, dest_addr, sent);
    wyn_socket_addr_free(dest_addr);
    
    return result;
}

WynNetError wyn_udp_socket_recv_from(WynUdpSocket* socket, void* buffer, size_t len, char** from_addr, size_t* received) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    
    WynSocketAddr addr;
    WynNetError result = wyn_socket_recvfrom(socket->socket, buffer, len, &addr, received);
    
    if (result == WYN_NET_SUCCESS && from_addr) {
        *from_addr = wyn_socket_addr_to_string(&addr);
    }
    
    return result;
}

WynNetError wyn_udp_socket_close(WynUdpSocket* socket) {
    if (!socket) return WYN_NET_ERROR_INVALID_ADDR;
    return wyn_socket_close(socket->socket);
}

void wyn_udp_socket_free(WynUdpSocket* socket) {
    if (!socket) return;
    wyn_socket_free(socket->socket);
    free(socket);
}

// Utility functions
const char* wyn_net_error_string(WynNetError error) {
    switch (error) {
        case WYN_NET_SUCCESS: return "Success";
        case WYN_NET_ERROR_INVALID_ADDR: return "Invalid address";
        case WYN_NET_ERROR_CONNECTION_REFUSED: return "Connection refused";
        case WYN_NET_ERROR_TIMEOUT: return "Operation timed out";
        case WYN_NET_ERROR_NETWORK_UNREACHABLE: return "Network unreachable";
        case WYN_NET_ERROR_ADDRESS_IN_USE: return "Address already in use";
        case WYN_NET_ERROR_PERMISSION_DENIED: return "Permission denied";
        case WYN_NET_ERROR_INTERRUPTED: return "Operation interrupted";
        case WYN_NET_ERROR_WOULD_BLOCK: return "Operation would block";
        default: return "Unknown network error";
    }
}

WynNetError wyn_net_init(void) {
    // On Unix systems, no special initialization needed
    return WYN_NET_SUCCESS;
}

void wyn_net_cleanup(void) {
    // On Unix systems, no special cleanup needed
}

// DNS resolution
char** wyn_resolve_hostname(const char* hostname, WynNetError* error) {
    if (!hostname) {
        if (error) *error = WYN_NET_ERROR_INVALID_ADDR;
        return NULL;
    }
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 for now
    hints.ai_socktype = SOCK_STREAM;
    
    int status = getaddrinfo(hostname, NULL, &hints, &result);
    if (status != 0) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        return NULL;
    }
    
    // Count addresses
    int count = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        count++;
    }
    
    char** addresses = malloc((count + 1) * sizeof(char*));
    if (!addresses) {
        if (error) *error = WYN_NET_ERROR_UNKNOWN;
        freeaddrinfo(result);
        return NULL;
    }
    
    // Extract addresses
    int i = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)rp->ai_addr;
        addresses[i] = malloc(INET_ADDRSTRLEN);
        if (addresses[i]) {
            inet_ntop(AF_INET, &addr_in->sin_addr, addresses[i], INET_ADDRSTRLEN);
            i++;
        }
    }
    addresses[i] = NULL;
    
    freeaddrinfo(result);
    if (error) *error = WYN_NET_SUCCESS;
    return addresses;
}

void wyn_free_hostname_list(char** addresses) {
    if (!addresses) return;
    
    for (int i = 0; addresses[i] != NULL; i++) {
        free(addresses[i]);
    }
    free(addresses);
}
