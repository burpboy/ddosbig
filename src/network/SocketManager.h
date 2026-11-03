#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <cstring>
#include <thread>
#include <atomic>

// Include Logger jika ada di project ini, atau gunakan ini sebagai fallback sementara
// Asumsi: Logger tersedia di utils/Logger.h
#ifdef HAS_LOGGER
    #include "../utils/Logger.h"
#endif

namespace BrutalDDoS {
namespace Network {

/**
 * Enum untuk tipe socket yang ingin dibuat
 */
enum class SocketType {
    TCP_STREAM,
    UDP_DGRAM,
    RAW_IP // Untuk SYN flood atau custom header
};

/**
 * Enum untuk status socket
 */
enum class SocketStatus {
    IDLE,
    CONNECTING,
    CONNECTED,
    LISTENING,
    CLOSED
};

/**
 * Interface dasar untuk semua jenis socket
 */
class ISocket {
public:
    virtual ~ISocket() = default;
    virtual bool bind(const std::string& ip, uint16_t port) = 0;
    virtual bool connect(const std::string& ip, uint16_t port) = 0;
    virtual bool listen(int backlog = 128) = 0;
    virtual int accept(std::string& client_ip, uint16_t& client_port) = 0;
    virtual ssize_t send(const void* buf, size_t len) = 0;
    virtual ssize_t recv(void* buf, size_t len) = 0;
    virtual bool close() = 0;
    virtual int getFd() const = 0;
    virtual SocketType getType() const = 0;
    virtual SocketStatus getStatus() const = 0;
};

/**
 * Manager utama untuk membuat dan mengelola lifecycle socket
 * Bertanggung jawab untuk inisialisasi library network OS dan cleanup
 */
class SocketManager {
public:
    // Singleton pattern untuk manager instance global jika diperlukan
    static SocketManager& getInstance();

    // Factory method untuk membuat socket berdasarkan tipe
    std::unique_ptr<ISocket> createSocket(SocketType type);

    // Utility: Resolve hostname to IP
    static std::string resolveHost(const std::string& hostname);

    // Utility: Set non-blocking flag pada fd
    static bool setNonBlocking(int fd);

    // Utility: Set keep-alive pada fd
    static bool setKeepAlive(int fd);

    // Cleanup global resources jika perlu
    
private:
    SocketManager();
    ~SocketManager();
    
    // Helper internal untuk setup socket dasar
    static int createBaseSocket(int domain, int type, int protocol);
};

} // namespace Network
} // namespace BrutalDDoS
