// Example usage of Wyn Network I/O System
// This demonstrates the Network API in a C context

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../src/net.h"

// Example: Simple TCP echo server
void example_tcp_echo_server() {
    printf("=== TCP Echo Server Example ===\n");
    
    const char* server_addr = "127.0.0.1:8080";
    
    WynNetError error;
    WynTcpListener* listener = wyn_tcp_listener_bind(server_addr, &error);
    
    if (!listener) {
        printf("✗ Failed to bind server: %s\n", wyn_net_error_string(error));
        return;
    }
    
    printf("✓ Server listening on %s\n", server_addr);
    printf("Waiting for connections... (will handle one connection)\n");
    
    // Accept one connection
    WynTcpStream* stream = wyn_tcp_listener_accept(listener, &error);
    if (stream) {
        printf("✓ Client connected\n");
        
        // Echo loop
        char buffer[1024];
        size_t received;
        
        while (true) {
            error = wyn_tcp_stream_recv(stream, buffer, sizeof(buffer) - 1, &received);
            if (error != WYN_NET_SUCCESS || received == 0) {
                break;  // Connection closed or error
            }
            
            buffer[received] = '\0';
            printf("Received: %s", buffer);
            
            // Echo back
            wyn_tcp_stream_send_all(stream, buffer, received);
        }
        
        printf("✓ Client disconnected\n");
        wyn_tcp_stream_free(stream);
    } else {
        printf("✗ Failed to accept connection: %s\n", wyn_net_error_string(error));
    }
    
    wyn_tcp_listener_free(listener);
}

// Example: TCP client
void example_tcp_client() {
    printf("\n=== TCP Client Example ===\n");
    
    const char* server_addr = "127.0.0.1:8080";
    const char* message = "Hello from Wyn TCP client!\n";
    
    WynNetError error;
    WynTcpStream* stream = wyn_tcp_stream_connect(server_addr, &error);
    
    if (!stream) {
        printf("✗ Failed to connect to server: %s\n", wyn_net_error_string(error));
        return;
    }
    
    printf("✓ Connected to %s\n", server_addr);
    
    // Send message
    error = wyn_tcp_stream_send_all(stream, message, strlen(message));
    if (error == WYN_NET_SUCCESS) {
        printf("✓ Sent: %s", message);
        
        // Receive echo
        char buffer[1024];
        size_t received;
        error = wyn_tcp_stream_recv(stream, buffer, sizeof(buffer) - 1, &received);
        
        if (error == WYN_NET_SUCCESS && received > 0) {
            buffer[received] = '\0';
            printf("✓ Received echo: %s", buffer);
        }
    } else {
        printf("✗ Failed to send message: %s\n", wyn_net_error_string(error));
    }
    
    wyn_tcp_stream_free(stream);
}

// Example: UDP communication
void example_udp_communication() {
    printf("\n=== UDP Communication Example ===\n");
    
    const char* server_addr = "127.0.0.1:9090";
    const char* client_addr = "127.0.0.1:9091";
    const char* message = "Hello UDP!";
    
    WynNetError error;
    
    // Create server socket
    WynUdpSocket* server = wyn_udp_socket_bind(server_addr, &error);
    if (!server) {
        printf("✗ Failed to create server socket: %s\n", wyn_net_error_string(error));
        return;
    }
    
    // Create client socket
    WynUdpSocket* client = wyn_udp_socket_bind(client_addr, &error);
    if (!client) {
        printf("✗ Failed to create client socket: %s\n", wyn_net_error_string(error));
        wyn_udp_socket_free(server);
        return;
    }
    
    printf("✓ UDP sockets created\n");
    
    // Send message from client to server
    size_t sent;
    error = wyn_udp_socket_send_to(client, message, strlen(message), server_addr, &sent);
    if (error == WYN_NET_SUCCESS) {
        printf("✓ Client sent: %s (%zu bytes)\n", message, sent);
        
        // Receive on server
        char buffer[1024];
        char* from_addr;
        size_t received;
        
        error = wyn_udp_socket_recv_from(server, buffer, sizeof(buffer) - 1, &from_addr, &received);
        if (error == WYN_NET_SUCCESS) {
            buffer[received] = '\0';
            printf("✓ Server received: %s (%zu bytes) from %s\n", buffer, received, from_addr);
            free(from_addr);
        } else {
            printf("✗ Server failed to receive: %s\n", wyn_net_error_string(error));
        }
    } else {
        printf("✗ Client failed to send: %s\n", wyn_net_error_string(error));
    }
    
    wyn_udp_socket_free(server);
    wyn_udp_socket_free(client);
}

// Example: DNS resolution
void example_dns_resolution() {
    printf("\n=== DNS Resolution Example ===\n");
    
    const char* hostnames[] = {"localhost", "google.com", "github.com"};
    int hostname_count = sizeof(hostnames) / sizeof(hostnames[0]);
    
    for (int i = 0; i < hostname_count; i++) {
        printf("Resolving %s:\n", hostnames[i]);
        
        WynNetError error;
        char** addresses = wyn_resolve_hostname(hostnames[i], &error);
        
        if (addresses && error == WYN_NET_SUCCESS) {
            for (int j = 0; addresses[j] != NULL; j++) {
                printf("  %s\n", addresses[j]);
            }
            wyn_free_hostname_list(addresses);
        } else {
            printf("  ✗ Resolution failed: %s\n", wyn_net_error_string(error));
        }
    }
}

// Example: Socket configuration
void example_socket_configuration() {
    printf("\n=== Socket Configuration Example ===\n");
    
    WynNetError error;
    WynSocket* socket = wyn_socket_new(WYN_SOCKET_TCP, &error);
    
    if (!socket) {
        printf("✗ Failed to create socket: %s\n", wyn_net_error_string(error));
        return;
    }
    
    printf("✓ Socket created\n");
    
    // Configure socket options
    printf("Configuring socket options:\n");
    
    // Set non-blocking mode
    error = wyn_socket_set_blocking(socket, false);
    if (error == WYN_NET_SUCCESS) {
        printf("  ✓ Set to non-blocking mode\n");
    } else {
        printf("  ✗ Failed to set non-blocking: %s\n", wyn_net_error_string(error));
    }
    
    // Set reuse address
    error = wyn_socket_set_reuse_addr(socket, true);
    if (error == WYN_NET_SUCCESS) {
        printf("  ✓ Enabled address reuse\n");
    } else {
        printf("  ✗ Failed to set reuse address: %s\n", wyn_net_error_string(error));
    }
    
    // Set timeout
    error = wyn_socket_set_timeout(socket, 5000);  // 5 seconds
    if (error == WYN_NET_SUCCESS) {
        printf("  ✓ Set 5-second timeout\n");
    } else {
        printf("  ✗ Failed to set timeout: %s\n", wyn_net_error_string(error));
    }
    
    wyn_socket_free(socket);
}

// Example: Error handling
void example_error_handling() {
    printf("\n=== Error Handling Example ===\n");
    
    WynNetError error;
    
    // Try to connect to non-existent server
    printf("Attempting to connect to non-existent server:\n");
    WynTcpStream* stream = wyn_tcp_stream_connect("127.0.0.1:99999", &error);
    if (!stream) {
        printf("  ✓ Expected error: %s\n", wyn_net_error_string(error));
    }
    
    // Try to bind to invalid address
    printf("Attempting to bind to invalid address:\n");
    WynTcpListener* listener = wyn_tcp_listener_bind("999.999.999.999:8080", &error);
    if (!listener) {
        printf("  ✓ Expected error: %s\n", wyn_net_error_string(error));
    }
    
    // Try to parse invalid address
    printf("Attempting to parse invalid address:\n");
    WynSocketAddr* addr = wyn_socket_addr_parse("invalid_address");
    if (!addr) {
        printf("  ✓ Expected failure: Invalid address format\n");
    }
    
    // Show all error types
    printf("\nAll network error types:\n");
    WynNetError errors[] = {
        WYN_NET_SUCCESS, WYN_NET_ERROR_INVALID_ADDR, WYN_NET_ERROR_CONNECTION_REFUSED,
        WYN_NET_ERROR_TIMEOUT, WYN_NET_ERROR_NETWORK_UNREACHABLE, WYN_NET_ERROR_ADDRESS_IN_USE,
        WYN_NET_ERROR_PERMISSION_DENIED, WYN_NET_ERROR_INTERRUPTED, WYN_NET_ERROR_WOULD_BLOCK,
        WYN_NET_ERROR_UNKNOWN
    };
    
    for (int i = 0; i < 10; i++) {
        printf("  %d: %s\n", errors[i], wyn_net_error_string(errors[i]));
    }
}

// Example: Address operations
void example_address_operations() {
    printf("\n=== Address Operations Example ===\n");
    
    // Create addresses
    WynSocketAddr* addr1 = wyn_socket_addr_new("192.168.1.100", 8080);
    WynSocketAddr* addr2 = wyn_socket_addr_parse("10.0.0.1:3000");
    WynSocketAddr* addr3 = wyn_socket_addr_parse("[::1]:9000");  // IPv6 localhost
    
    printf("Address examples:\n");
    
    if (addr1) {
        char* addr_str = wyn_socket_addr_to_string(addr1);
        printf("  Address 1: %s (family: %s)\n", addr_str, 
               addr1->family == WYN_AF_IPV4 ? "IPv4" : "IPv6");
        free(addr_str);
        wyn_socket_addr_free(addr1);
    }
    
    if (addr2) {
        char* addr_str = wyn_socket_addr_to_string(addr2);
        printf("  Address 2: %s (family: %s)\n", addr_str,
               addr2->family == WYN_AF_IPV4 ? "IPv4" : "IPv6");
        free(addr_str);
        wyn_socket_addr_free(addr2);
    }
    
    if (addr3) {
        char* addr_str = wyn_socket_addr_to_string(addr3);
        printf("  Address 3: %s (family: %s)\n", addr_str,
               addr3->family == WYN_AF_IPV4 ? "IPv4" : "IPv6");
        free(addr_str);
        wyn_socket_addr_free(addr3);
    }
}

int main() {
    printf("Wyn Network I/O Examples\n");
    printf("========================\n");
    
    // Initialize network system
    WynNetError init_error = wyn_net_init();
    if (init_error != WYN_NET_SUCCESS) {
        printf("✗ Failed to initialize network system: %s\n", wyn_net_error_string(init_error));
        return 1;
    }
    
    // Note: TCP server/client examples are commented out as they require coordination
    // example_tcp_echo_server();  // Run this in one terminal
    // example_tcp_client();       // Run this in another terminal
    
    example_udp_communication();
    example_dns_resolution();
    example_socket_configuration();
    example_error_handling();
    example_address_operations();
    
    // Cleanup network system
    wyn_net_cleanup();
    
    printf("\n✅ All network examples completed successfully!\n");
    printf("\nNote: TCP server/client examples are available but commented out.\n");
    printf("Uncomment them to test TCP communication between separate processes.\n");
    
    return 0;
}
