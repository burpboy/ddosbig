#ifndef BRUTAL_DDOS_RANDOM_UTILS_H
#define BRUTAL_DDOS_RANDOM_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <random>
#include <sstream>
#include <algorithm>
#include <arpa/inet.h> // Untuk inet_ntop/inet_pton jika perlu manipulasi raw
#include <cstring>

namespace BrutalDDoS {
namespace Utils {

class RandomUtils {
public:
    // Konstanta untuk rentang IP yang umum digunakan (bisa disesuaikan)
    static constexpr uint32_t PRIVATE_IP_START = 0x0A000000; // 10.0.0.0/8
    static constexpr uint32_t PRIVATE_IP_END   = 0x0AFFFFFF; // 10.255.255.255
    
    // Constructor: Inisialisasi RNG dengan seed acak dari hardware atau time
    RandomUtils();
    
    // Non-copyable karena memiliki state RNG internal (opsional, tergantung preferensi arsitektur)
    // Jika ingin thread-local, bisa diganti dengan thread_local di dalam method static
    
    // --- IP Spoofing Functions ---

    /**
     * Generate random IPv4 address.
     * @param excludePrivate Jika true, hindari IP private RFC1918.
     * @return String representasi IP "a.b.c.d"
     */
    std::string generateRandomIPv4(bool excludePrivate = true);

    /**
     * Generate random IPv6 address.
     * @return String representasi IPv6 hex compressed format.
     */
    std::string generateRandomIPv6();

    /**
     * Generate random MAC Address.
     * @return String format "XX:XX:XX:XX:XX:XX"
     */
    std::string generateRandomMAC();

    // --- Payload Generation Functions ---

    /**
     * Generate random binary payload.
     * @param size Size in bytes.
     * @return Vector of bytes.
     */
    std::vector<uint8_t> generateRandomPayload(size_t size);

    /**
     * Generate a pseudo-random string containing printable ASCII characters.
     * Useful for HTTP headers or body content that needs to look like text.
     * @param length Length of the string.
     * @param charset Optional custom charset string. Defaults to alphanumeric + symbols.
     * @return Random string.
     */
    std::string generateRandomString(size_t length, const std::string& charset = "");

    /**
     * Generate a random integer within range [min, max].
     */
    int32_t getRandomInt(int32_t min, int32_t max);

    /**
     * Generate a random double within range [min, max].
     */
    double getRandomDouble(double min, double max);

    /**
     * Shuffle a vector in place using Fisher-Yates algorithm.
     */
    template<typename T>
    void shuffleVector(std::vector<T>& vec) {
        if (vec.empty()) return;
        std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
        for (size_t i = vec.size() - 1; i > 0; --i) {
            size_t j = dist(rng_);
            std::swap(vec[i], vec[j]);
        }
    }

private:
    // Thread-local RNG instance untuk performa tinggi tanpa lock mutex global
    // Menggunakan std::mt19937_64 untuk entropi lebih baik
    thread_local static std::mt19937_64 rng_;
    thread_local static std::uniform_int_distribution<int> uniform_dist_;
    
    // Static initialization untuk seed
    static void initSeeds();
};

} // namespace Utils
} // namespace BrutalDDoS

// Implementasi Inline / Header-Only untuk kemudahan kompilasi
// Jika Anda memisahkan ke .cpp, pindahkan definisi di bawah ke RandomUtils.cpp

inline BrutalDDoS::Utils::RandomUtils::RandomUtils() {
    initSeeds();
}

inline void BrutalDDoS::Utils::RandomUtils::initSeeds() {
    // Seed sekali saat library dimuat atau thread pertama kali dibuat
    static std::once_flag flag;
    std::call_once(flag, []() {
        std::random_device rd;
        // Set seed global jika perlu, tapi thread_local sudah cukup aman
    });
}

// Definisi Thread-Local Storage
thread_local std::mt19937_64 BrutalDDoS::Utils::RandomUtils::rng_(std::random_device{}());
thread_local std::uniform_int_distribution<int> BrutalDDoS::Utils::RandomUtils::uniform_dist_(0, 255);

inline std::string BrutalDDoS::Utils::RandomUtils::generateRandomIPv4(bool excludePrivate) {
    uint32_t ip = 0;
    do {
        // Generate 4 byte random
        uint8_t b1 = uniform_dist_(rng_) & 0xFF;
        uint8_t b2 = uniform_dist_(rng_) & 0xFF;
        uint8_t b3 = uniform_dist_(rng_) & 0xFF;
        uint8_t b4 = uniform_dist_(rng_) & 0xFF;
        
        ip = (static_cast<uint32_t>(b1) << 24) | 
             (static_cast<uint32_t>(b2) << 16) | 
             (static_cast<uint32_t>(b3) << 8) | 
             static_cast<uint32_t>(b4);
             
        if (excludePrivate) {
            // Hindari loopback (127.x.x.x) dan private ranges umum
            if ((ip >= 0x7F000000 && ip <= 0x7FFFFFFF) || // 127.0.0.0/8
                (ip >= 0x0A000000 && ip <= 0x0AFFFFFF) || // 10.0.0.0/8
                (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF) || // 192.168.0.0/16
                (ip >= 0xAC100000 && ip <= 0xAC1FFFFF)) {   // 172.16.0.0/12
                continue;
            }
            // Hindari juga IP broadcast/link-local jika mau ekstra bersih
            if (b1 == 169 && b2 == 254) continue; // 169.254.x.x
        }
    } while (false);

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", 
             (ip >> 24) & 0xFF, 
             (ip >> 16) & 0xFF, 
             (ip >> 8) & 0xFF, 
             ip & 0xFF);
    return std::string(buffer);
}

inline std::string BrutalDDoS::Utils::RandomUtils::generateRandomIPv6() {
    uint16_t parts[8];
    for (int i = 0; i < 8; ++i) {
        parts[i] = static_cast<uint16_t>(uniform_dist_(rng_) & 0xFFFF);
    }
    
    // Format standar IPv6 hex
    char buffer[40]; // ::ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff + null
    snprintf(buffer, sizeof(buffer), "%x:%x:%x:%x:%x:%x:%x:%x", 
             parts[0], parts[1], parts[2], parts[3],
             parts[4], parts[5], parts[6], parts[7]);
    return std::string(buffer);
}

inline std::string BrutalDDoS::Utils::RandomUtils::generateRandomMAC() {
    std::string mac = "";
    for (int i = 0; i < 6; ++i) {
        char byte_str[3];
        snprintf(byte_str, sizeof(byte_str), "%02X", uniform_dist_(rng_) & 0xFF);
        mac += byte_str;
        if (i < 5) mac += ":";
    }
    return mac;
}

inline std::vector<uint8_t> BrutalDDoS::Utils::RandomUtils::generateRandomPayload(size_t size) {
    std::vector<uint8_t> payload(size);
    if (size == 0) return payload;
    
    // Fill dengan random bytes langsung
    // Menggunakan RNG untuk setiap byte mungkin lambat untuk ukuran besar (>1MB)
    // Optimasi: Generate block by block jika perlu, tapi untuk DDoS biasanya ukuran paket kecil-sedang.
    for (size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<uint8_t>(uniform_dist_(rng_) & 0xFF);
    }
    return payload;
}

inline std::string BrutalDDoS::Utils::RandomUtils::generateRandomString(size_t length, const std::string& charset) {
    if (length == 0) return "";
    
    std::string default_charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (charset.empty()) {
        charset = default_charset;
    }
    
    if (charset.empty()) return "";
    
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(rng_)];
    }
    return result;
}

inline int32_t BrutalDDoS::Utils::RandomUtils::getRandomInt(int32_t min, int32_t max) {
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int32_t> dist(min, max);
    return dist(rng_);
}

inline double BrutalDDoS::Utils::RandomUtils::getRandomDouble(double min, double max) {
    if (min > max) std::swap(min, max);
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng_);
}

#endif // BRUTAL_DDOS_RANDOM_UTILS_H
