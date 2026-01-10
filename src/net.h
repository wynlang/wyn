#ifndef WYN_NET_H
#define WYN_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynSocket WynSocket;
typedef struct WynTcpListener WynTcpListener;
typedef struct WynTcpStream WynTcpStream;
typedef struct WynUdpSocket WynUdpSocket;
typedef struct WynSocketAddr WynSocketAddr;

// Network error types
typedef enum {
    WYN_NET_SUCCESS = 0,
    WYN_NET_ERROR_INVALID_ADDR,
    WYN_NET_ERROR_CONNECTION_REFUSED,
    WYN_NET_ERROR_TIMEOUT,
    WYN_NET_ERROR_NETWORK_UNREACHABLE,
    WYN_NET_ERROR_ADDRESS_IN_USE,
    WYN_NET_ERROR_PERMISSION_DENIED,
    WYN_NET_ERROR_INTERRUPTED,
    WYN_NET_ERROR_WOULD_BLOCK,
    WYN_NET_ERROR_UNKNOWN
} WynNetError;

// Socket types
typedef enum {
    WYN_SOCKET_TCP,
    WYN_SOCKET_UDP
} WynSocketType;

// Address family
typedef enum {
    WYN_AF_IPV4,
    WYN_AF_IPV6
} WynAddressFamily;

// Socket address structure
typedef struct WynSocketAddr {
    WynAddressFamily family;
    char ip[46];  // Max IPv6 string length
    uint16_t port;
} WynSocketAddr;

// Base socket structure
typedef struct WynSocket {
    int fd;
    WynSocketType type;
    WynSocketAddr local_addr;
    bool is_connected;
    bool is_listening;
    bool is_blocking;
} WynSocket;

// TCP Listener
typedef struct WynTcpListener {
    WynSocket* socket;
    WynSocketAddr addr;
} WynTcpListener;

// TCP Stream
typedef struct WynTcpStream {
    WynSocket* socket;
    WynSocketAddr peer_addr;
} WynTcpStream;

// UDP Socket
typedef struct WynUdpSocket {
    WynSocket* socket;
} WynUdpSocket;

// Socket address operations
WynSocketAddr* wyn_socket_addr_new(const char* ip, uint16_t port);
WynSocketAddr* wyn_socket_addr_parse(const char* addr_str);
void wyn_socket_addr_free(WynSocketAddr* addr);
char* wyn_socket_addr_to_string(const WynSocketAddr* addr);

// Base socket operations
WynSocket* wyn_socket_new(WynSocketType type, WynNetError* error);
WynNetError wyn_socket_bind(WynSocket* socket, const WynSocketAddr* addr);
WynNetError wyn_socket_connect(WynSocket* socket, const WynSocketAddr* addr);
WynNetError wyn_socket_listen(WynSocket* socket, int backlog);
WynSocket* wyn_socket_accept(WynSocket* socket, WynSocketAddr* peer_addr, WynNetError* error);
WynNetError wyn_socket_send(WynSocket* socket, const void* data, size_t len, size_t* sent);
WynNetError wyn_socket_recv(WynSocket* socket, void* buffer, size_t len, size_t* received);
WynNetError wyn_socket_sendto(WynSocket* socket, const void* data, size_t len, const WynSocketAddr* addr, size_t* sent);
WynNetError wyn_socket_recvfrom(WynSocket* socket, void* buffer, size_t len, WynSocketAddr* addr, size_t* received);
WynNetError wyn_socket_close(WynSocket* socket);
void wyn_socket_free(WynSocket* socket);

// Socket configuration
WynNetError wyn_socket_set_blocking(WynSocket* socket, bool blocking);
WynNetError wyn_socket_set_reuse_addr(WynSocket* socket, bool reuse);
WynNetError wyn_socket_set_timeout(WynSocket* socket, int timeout_ms);

// TCP Listener operations
WynTcpListener* wyn_tcp_listener_bind(const char* addr, WynNetError* error);
WynTcpStream* wyn_tcp_listener_accept(WynTcpListener* listener, WynNetError* error);
WynNetError wyn_tcp_listener_close(WynTcpListener* listener);
void wyn_tcp_listener_free(WynTcpListener* listener);

// TCP Stream operations
WynTcpStream* wyn_tcp_stream_connect(const char* addr, WynNetError* error);
WynNetError wyn_tcp_stream_send(WynTcpStream* stream, const void* data, size_t len, size_t* sent);
WynNetError wyn_tcp_stream_recv(WynTcpStream* stream, void* buffer, size_t len, size_t* received);
WynNetError wyn_tcp_stream_send_all(WynTcpStream* stream, const void* data, size_t len);
char* wyn_tcp_stream_recv_line(WynTcpStream* stream, WynNetError* error);
WynNetError wyn_tcp_stream_close(WynTcpStream* stream);
void wyn_tcp_stream_free(WynTcpStream* stream);

// UDP Socket operations
WynUdpSocket* wyn_udp_socket_bind(const char* addr, WynNetError* error);
WynNetError wyn_udp_socket_send_to(WynUdpSocket* socket, const void* data, size_t len, const char* addr, size_t* sent);
WynNetError wyn_udp_socket_recv_from(WynUdpSocket* socket, void* buffer, size_t len, char** from_addr, size_t* received);
WynNetError wyn_udp_socket_close(WynUdpSocket* socket);
void wyn_udp_socket_free(WynUdpSocket* socket);

// Utility functions
const char* wyn_net_error_string(WynNetError error);
WynNetError wyn_net_init(void);
void wyn_net_cleanup(void);

// DNS resolution
char** wyn_resolve_hostname(const char* hostname, WynNetError* error);
void wyn_free_hostname_list(char** addresses);

#endif // WYN_NET_H
