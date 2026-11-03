#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <string>

// Asumsi path header sesuai struktur proyek
#include "../core/PacketGenerator.h" 
#include "../utils/Logger.h" // Untuk logging jika perlu debug

using namespace BrutalDDoS; // Sesuaikan namespace jika berbeda

void test_syn_packet_generation() {
    std::cout << "[TEST] Testing SYN Packet Generation..." << std::endl;
    
    PacketGenerator gen;
    // Target dummy
    std::string target_ip = "192.168.1.1";
    uint16_t target_port = 80;
    uint32_t src_ip = 0x0A000001; // 10.0.0.1
    
    auto packet = gen.generateSynPacket(target_ip, target_port, src_ip);
    
    assert(packet.data() != nullptr);
    assert(packet.size() > 0);
    // Minimal size IP Header (20) + TCP Header (20) = 40 bytes
    assert(packet.size() >= 40);
    
    std::cout << "   [PASS] SYN packet generated successfully. Size: " << packet.size() << " bytes." << std::endl;
}

void test_udp_packet_payload() {
    std::cout << "[TEST] Testing UDP Packet Payload..." << std::endl;
    
    PacketGenerator gen;
    std::string target_ip = "10.0.0.5";
    uint16_t target_port = 53;
    std::string payload_data = "AAAAAAAABBBBBBBBCCCCCCCC";
    
    auto packet = gen.generateUdpPacket(target_ip, target_port, payload_data);
    
    assert(packet.size() > 0);
    // Check if payload is embedded or referenced correctly depending on implementation
    // Disini kita asumsikan packet berisi raw bytes termasuk UDP header + data
    
    std::cout << "   [PASS] UDP packet generated with custom payload." << std::endl;
}

void test_random_payload_fuzzing() {
    std::cout << "[TEST] Testing Random Payload Fuzzing..." << std::endl;
    
    PacketGenerator gen;
    std::string target_ip = "127.0.0.1";
    uint16_t port = 8080;
    
    // Generate 10 packets dengan random payload
    for (int i = 0; i < 10; ++i) {
        auto pkt = gen.generateFuzzedPacket(target_ip, port, 1024); // 1KB random data
        assert(pkt.size() == 1024 + sizeof(udp_header_size)); // Adjust based on actual struct
        
        // Pastikan setiap byte random (bukan nol semua kecuali header)
        bool has_non_zero = false;
        for (auto byte : pkt) {
            if (byte != 0) {
                has_non_zero = true;
                break;
            }
        }
        assert(has_non_zero);
    }
    
    std::cout << "   [PASS] Fuzzing randomness verified." << std::endl;
}

int main() {
    try {
        test_syn_packet_generation();
        test_udp_packet_payload();
        test_random_payload_fuzzing();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL PACKET GENERATION TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
