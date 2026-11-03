#include "SocketManager.h"
#include "../utils/Logger.h" // Asumsi path relatif ke logger

using namespace BrutalDDoS::Network;

// --- Implementation of ISocket Concrete Classes ---

class TcpUdpSocket : public ISocket {
private:
    int fd_;
    SocketType type_;
    SocketStatus status_;
    std::string local_ip_;
    uint16_t local_port_;

    bool initSocket() {
        int domain = AF_INET; // Default IPv4, bisa diperluas ke IPv6 nanti
        int type = 0;
        int protocol = 0;

        if (type_ == SocketType::TCP_STREAM) {
            type = SOCK_STREAM;
            protocol = IPPROTO_TCP;
        } else if (type_ == SocketType::UDP_DGRAM) {
            type = SOCK_DGRAM;
            protocol = IPPROTO_UDP;
        } else {
            return false;
        }

        fd_ = SocketManager::createBaseSocket(domain, type, protocol);
        if (fd_ < 0) {
            LOG_ERROR("Failed to create base socket");
            return false;
        }

        // Set Non-Blocking by default for DDoS efficiency
        if (!SocketManager::setNonBlocking(fd_)) {
            LOG_WARN("Failed to set non-blocking on new socket");
        }

        status_ = SocketStatus::IDLE;
        return true;
    }

public:
    TcpUdpSocket(SocketType type) : fd_(-1), type_(type), status_(SocketStatus::IDLE) {
        if (!initSocket()) {
            if (fd_ >= 0) close(fd_);
        }
    }

    ~TcpUdpSocket() override {
        close();
    }

    bool bind(const std::string& ip, uint16_t port) override {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (ip.empty() || ip == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
                LOG_ERROR("Invalid IP address for bind: " + ip);
                return false;
            }
        }

        int optval = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            LOG_ERROR("Bind failed on " + ip + ":" + std::to_string(port));
            return false;
        }

        local_ip_ = ip;
        local_port_ = port;
        status_ = SocketStatus::LISTENING;
        return true;
    }
