#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>

#include "../network/SocketManager.h"
#include "../network/AsyncIO.h" // Opsional, jika SocketManager menggunakannya

using namespace BrutalDDoS::network;

void test_socket_creation_ipv4() {
    std::cout << "[TEST] Testing IPv4 Socket Creation..." << std::endl;
    
    SocketManager sm;
    int sock_fd = sm.createSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    assert(sock_fd > 0);
    std::cout << "   [PASS] IPv4 TCP Socket created. FD: " << sock_fd << std::endl;
    
    // Cleanup
    close(sock_fd);
}

void test_socket_creation_udp() {
    std::cout << "[TEST] Testing UDP Socket Creation..." << std::endl;
    
    SocketManager sm;
    int sock_fd = sm.createSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    assert(sock_fd > 0);
    std::cout << "   [PASS] IPv4 UDP Socket created. FD: " << sock_fd << std::endl;
    
    close(sock_fd);
}

void test_bind_and_listen() {
    std::cout << "[TEST] Testing Bind and Listen..." << std::endl;
    
    SocketManager sm;
    int sock_fd = sm.createSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(9999);
    address.sin_addr.s_addr = INADDR_ANY;
    
    bool bound = sm.bind(sock_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    assert(bound == true);
    
    bool listened = sm.listen(sock_fd, 10);
    assert(listened == true);
    
    std::cout << "   [PASS] Socket bound to port 9999 and listening." << std::endl;
    
    close(sock_fd);
}

void test_send_to_target() {
    std::cout << "[TEST] Testing Send To Target (UDP)..." << std::endl;
    
    SocketManager sm;
    int sock_fd = sm.createSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(53); // DNS Port
    inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);
    
    std::string data = "TEST_PACKET";
    ssize_t sent = sm.sendTo(sock_fd, data.c_str(), data.length(), 
                             reinterpret_cast<struct sockaddr*>(&target), sizeof(target));
    
    assert(sent > 0);
    assert(static_cast<size_t>(sent) == data.length());
    
    std::cout << "   [PASS] Sent " << sent << " bytes to 8.8.8.8:53" << std::endl;
    
    close(sock_fd);
}

int main() {
    try {
        test_socket_creation_ipv4();
        test_socket_creation_udp();
        test_bind_and_listen();
        test_send_to_target();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL SOCKET MANAGER TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
