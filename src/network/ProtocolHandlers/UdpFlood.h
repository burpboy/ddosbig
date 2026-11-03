#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include "Logger.h" // Assuming Logger is available in utils/

class UdpFlood {
public:
    struct Config {
        std::string target_ip;
        uint16_t target_port;
        size_t payload_size = 1024;      // Default 1KB
        size_t bandwidth_limit_mbps = 0; // 0 means unlimited
        size_t threads = 1;
        bool spoof_ips = true;
        bool verbose = false;
    };

    UdpFlood(const Config& config);
    ~UdpFlood();

    void start();
    void stop();
    
    // Stats
    uint64_t get_packets_sent() const;
    double get_bandwidth_mbps() const;

private:
    void worker_thread(int thread_id);
    void generate_payload();
    std::string generate_random_ip();
    uint32_t rand_uint32();

    Config config_;
    std::vector<std::thread> threads_;
    
    // Socket file descriptor
    int sock_fd_ = -1;
    
    // Data buffers
    std::string payload_buffer_;
    std::string ip_buffer_; // For constructing IP header if needed, or just storing random IP string
    
    // Target address structure
    struct sockaddr_in target_addr_;
    
    // Stats
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<double> current_bandwidth_mbps_{0.0};
    
    // Timing for bandwidth control
    std::mutex bw_mutex_;
    std::chrono::steady_clock::time_point last_bw_check_;
    uint64_t bytes_sent_window_ = 0;
    
    // Random state
    std::mt19937 rng_;
    std::uniform_int_distribution<uint32_t> dist_;
};
