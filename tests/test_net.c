#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "../src/net.h"

// Test server thread data
typedef struct {
    const char* addr;
    const char* response;
    bool* server_ready;
} ServerThreadData;

// Simple TCP echo server for testing
void* tcp_echo_server(void* arg) {
    ServerThreadData* data = (ServerThreadData*)arg;
    
    WynNetError error;
    WynTcpListener* listener = wyn_tcp_listener_bind(data->addr, &error);
    if (!listener) {
        printf("Server failed to bind: %s\n", wyn_net_error_string(error));
        return NULL;
    }
    
    *data->server_ready = true;
    
    // Accept one connection
    WynTcpStream* stream = wyn_tcp_listener_accept(listener, &error);
    if (stream) {
        // Echo back the response
        wyn_tcp_stream_send_all(stream, data->response, strlen(data->response));
        wyn_tcp_stream_free(stream);
    }
    
    wyn_tcp_listener_free(listener);
    return NULL;
}

void test_socket_addr_operations() {
    printf("Testing socket address operations...\n");
    
    // Test address creation
    WynSocketAddr* addr = wyn_socket_addr_new("127.0.0.1", 8080);
    assert(addr != NULL);
    assert(strcmp(addr->ip, "127.0.0.1") == 0);
    assert(addr->port == 8080);
    assert(addr->family == WYN_AF_IPV4);
    wyn_socket_addr_free(addr);
    
    // Test address parsing
    addr = wyn_socket_addr_parse("192.168.1.1:9000");
    assert(addr != NULL);
    assert(strcmp(addr->ip, "192.168.1.1") == 0);
    assert(addr->port == 9000);
    wyn_socket_addr_free(addr);
    
    // Test address to string
    addr = wyn_socket_addr_new("10.0.0.1", 3000);
    char* addr_str = wyn_socket_addr_to_string(addr);
    assert(strcmp(addr_str, "10.0.0.1:3000") == 0);
    free(addr_str);
    wyn_socket_addr_free(addr);
    
    printf("✓ Socket address operations test passed\n");
}

void test_tcp_client_server() {
    printf("Testing TCP client-server communication...\n");
    
    const char* server_addr = "127.0.0.1:12345";
    const char* test_message = "Hello from server!";
    bool server_ready = false;
    
    // Start server thread
    ServerThreadData server_data = {
        .addr = server_addr,
        .response = test_message,
        .server_ready = &server_ready
    };
    
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_echo_server, &server_data);
    
    // Wait for server to be ready
    while (!server_ready) {
        usleep(10000);  // 10ms
    }
    
    // Connect client
    WynNetError error;
    WynTcpStream* client = wyn_tcp_stream_connect(server_addr, &error);
    assert(client != NULL);
    assert(error == WYN_NET_SUCCESS);
    
    // Receive message from server
    char buffer[256];
    size_t received;
    error = wyn_tcp_stream_recv(client, buffer, sizeof(buffer) - 1, &received);
    assert(error == WYN_NET_SUCCESS);
    assert(received > 0);
    
    buffer[received] = '\0';
    assert(strcmp(buffer, test_message) == 0);
    
    wyn_tcp_stream_free(client);
    pthread_join(server_thread, NULL);
    
    printf("✓ TCP client-server test passed\n");
}

void test_udp_socket() {
    printf("Testing UDP socket operations...\n");
    
    const char* server_addr = "127.0.0.1:12346";
    const char* client_addr = "127.0.0.1:12347";
    const char* test_message = "UDP test message";
    
    WynNetError error;
    
    // Create server socket
    WynUdpSocket* server = wyn_udp_socket_bind(server_addr, &error);
    assert(server != NULL);
    assert(error == WYN_NET_SUCCESS);
    
    // Create client socket
    WynUdpSocket* client = wyn_udp_socket_bind(client_addr, &error);
    assert(client != NULL);
    assert(error == WYN_NET_SUCCESS);
    
    // Send message from client to server
    size_t sent;
    error = wyn_udp_socket_send_to(client, test_message, strlen(test_message), server_addr, &sent);
    assert(error == WYN_NET_SUCCESS);
    assert(sent == strlen(test_message));
    
    // Receive message on server
    char buffer[256];
    char* from_addr;
    size_t received;
    error = wyn_udp_socket_recv_from(server, buffer, sizeof(buffer) - 1, &from_addr, &received);
    assert(error == WYN_NET_SUCCESS);
    assert(received == strlen(test_message));
    
    buffer[received] = '\0';
    assert(strcmp(buffer, test_message) == 0);
    assert(from_addr != NULL);
    
    free(from_addr);
    wyn_udp_socket_free(server);
    wyn_udp_socket_free(client);
    
    printf("✓ UDP socket test passed\n");
}

void test_socket_configuration() {
    printf("Testing socket configuration...\n");
    
    WynNetError error;
    WynSocket* socket = wyn_socket_new(WYN_SOCKET_TCP, &error);
    assert(socket != NULL);
    assert(error == WYN_NET_SUCCESS);
    
    // Test blocking mode
    error = wyn_socket_set_blocking(socket, false);
    assert(error == WYN_NET_SUCCESS);
    assert(socket->is_blocking == false);
    
    error = wyn_socket_set_blocking(socket, true);
    assert(error == WYN_NET_SUCCESS);
    assert(socket->is_blocking == true);
    
    // Test reuse address
    error = wyn_socket_set_reuse_addr(socket, true);
    assert(error == WYN_NET_SUCCESS);
    
    // Test timeout (should not fail)
    error = wyn_socket_set_timeout(socket, 5000);
    assert(error == WYN_NET_SUCCESS);
    
    wyn_socket_free(socket);
    
    printf("✓ Socket configuration test passed\n");
}

void test_dns_resolution() {
    printf("Testing DNS resolution...\n");
    
    WynNetError error;
    
    // Test localhost resolution
    char** addresses = wyn_resolve_hostname("localhost", &error);
    if (addresses && error == WYN_NET_SUCCESS) {
        assert(addresses[0] != NULL);
        printf("  localhost resolves to: %s\n", addresses[0]);
        wyn_free_hostname_list(addresses);
    } else {
        printf("  DNS resolution not available or failed\n");
    }
    
    // Test invalid hostname
    addresses = wyn_resolve_hostname("invalid.hostname.that.does.not.exist", &error);
    if (addresses) {
        wyn_free_hostname_list(addresses);
    }
    // Should fail, but we don't assert since DNS behavior varies
    
    printf("✓ DNS resolution test passed\n");
}

void test_error_handling() {
    printf("Testing network error handling...\n");
    
    WynNetError error;
    
    // Test invalid address parsing
    WynSocketAddr* addr = wyn_socket_addr_parse("invalid_address");
    assert(addr == NULL);
    
    // Test connection to non-existent server
    WynTcpStream* stream = wyn_tcp_stream_connect("127.0.0.1:99999", &error);
    assert(stream == NULL);
    assert(error != WYN_NET_SUCCESS);
    
    // Test binding to invalid address
    WynTcpListener* listener = wyn_tcp_listener_bind("999.999.999.999:8080", &error);
    assert(listener == NULL);
    assert(error != WYN_NET_SUCCESS);
    
    // Test error string conversion
    const char* error_str = wyn_net_error_string(WYN_NET_ERROR_CONNECTION_REFUSED);
    assert(strcmp(error_str, "Connection refused") == 0);
    
    error_str = wyn_net_error_string(WYN_NET_SUCCESS);
    assert(strcmp(error_str, "Success") == 0);
    
    printf("✓ Network error handling test passed\n");
}

void test_tcp_send_all() {
    printf("Testing TCP send_all functionality...\n");
    
    const char* server_addr = "127.0.0.1:12348";
    const char* large_message = "This is a longer message that tests the send_all functionality to ensure all data is transmitted correctly.";
    bool server_ready = false;
    
    // Start server thread
    ServerThreadData server_data = {
        .addr = server_addr,
        .response = large_message,
        .server_ready = &server_ready
    };
    
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_echo_server, &server_data);
    
    // Wait for server to be ready
    while (!server_ready) {
        usleep(10000);  // 10ms
    }
    
    // Connect client
    WynNetError error;
    WynTcpStream* client = wyn_tcp_stream_connect(server_addr, &error);
    assert(client != NULL);
    
    // Receive the large message
    char buffer[512];
    size_t total_received = 0;
    
    while (total_received < strlen(large_message)) {
        size_t received;
        error = wyn_tcp_stream_recv(client, buffer + total_received, 
                                   sizeof(buffer) - total_received - 1, &received);
        if (error != WYN_NET_SUCCESS || received == 0) break;
        total_received += received;
    }
    
    buffer[total_received] = '\0';
    assert(strcmp(buffer, large_message) == 0);
    
    wyn_tcp_stream_free(client);
    pthread_join(server_thread, NULL);
    
    printf("✓ TCP send_all test passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL parameters
    assert(wyn_socket_addr_new(NULL, 8080) == NULL);
    assert(wyn_socket_addr_parse(NULL) == NULL);
    assert(wyn_socket_addr_to_string(NULL) == NULL);
    
    WynNetError error;
    WynTcpStream* stream = wyn_tcp_stream_connect(NULL, &error);
    assert(stream == NULL);
    assert(error == WYN_NET_ERROR_INVALID_ADDR);
    
    WynTcpListener* listener = wyn_tcp_listener_bind(NULL, &error);
    assert(listener == NULL);
    assert(error == WYN_NET_ERROR_INVALID_ADDR);
    
    WynUdpSocket* udp = wyn_udp_socket_bind(NULL, &error);
    assert(udp == NULL);
    assert(error == WYN_NET_ERROR_INVALID_ADDR);
    
    // Test operations on NULL objects
    assert(wyn_tcp_stream_send(NULL, "test", 4, NULL) == WYN_NET_ERROR_INVALID_ADDR);
    assert(wyn_tcp_stream_recv(NULL, NULL, 0, NULL) == WYN_NET_ERROR_INVALID_ADDR);
    assert(wyn_tcp_listener_accept(NULL, &error) == NULL);
    
    printf("✓ Edge cases test passed\n");
}

int main() {
    printf("Running Network I/O Tests...\n\n");
    
    // Initialize network system
    WynNetError init_error = wyn_net_init();
    assert(init_error == WYN_NET_SUCCESS);
    
    test_socket_addr_operations();
    test_tcp_client_server();
    test_udp_socket();
    test_socket_configuration();
    test_dns_resolution();
    test_error_handling();
    test_tcp_send_all();
    test_edge_cases();
    
    // Cleanup network system
    wyn_net_cleanup();
    
    printf("\n✅ All network I/O tests passed!\n");
    return 0;
}
