#ifndef BRUTAL_DDOS_FUZZER_H
#define BRUTAL_DDOS_FUZZER_H

#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <memory>
#include <mutex>
#include <algorithm>
#include <cstring>

// Forward declarations to avoid circular dependencies if needed
namespace BrutalDDoS {
    namespace Network {
        class SocketManager;
    }
}

namespace BrutalDDoS {
namespace Core {

/**
 * @class Fuzzer
 * @brief Modul untuk menghasilkan fuzzing pada header dan payload paket jaringan.
 * 
 * Fitur Utama:
 * - Thread-safe Random Number Generation (per-thread instance)
 * - Support untuk fuzzing string HTTP headers, TCP flags, UDP payloads
 * - Deterministic mode untuk debugging/reproducibility
 * - Efficient memory allocation using pre-allocated buffers
 */
class Fuzzer {
public:
    // Konfigurasi default fuzzing
    struct Config {
        bool enable_header_fuzzing = true;
        bool enable_payload_fuzzing = true;
        size_t min_payload_size = 64;   // Byte
        size_t max_payload_size = 8192; // Byte (8KB)
        double fuzz_probability = 0.5;  // Probabilitas setiap byte di-fuzz (0.0 - 1.0)
        
        // Khusus HTTP
        std::vector<std::string> custom_headers;
        bool inject_random_user_agent = true;
        bool inject_random_accept_lang = true;
    };

    /**
     * @brief Konstruktor Fuzzer
     * @param config Konfigurasi fuzzing. Jika kosong, menggunakan default.
     */
    explicit Fuzzer(const Config& config = Config());

    ~Fuzzer();

    // Non-copyable, movable
    Fuzzer(const Fuzzer&) = delete;
    Fuzzer& operator=(const Fuzzer&) = delete;
    Fuzzer(Fuzzer&&) noexcept;
    Fuzzer& operator=(Fuzzer&&) noexcept;

    /**
     * @brief Menghasilkan buffer payload acak dengan ukuran tertentu
     * @param size Ukuran buffer yang diinginkan
     * @return std::unique_ptr<uint8_t[]> Buffer yang dialokasikan secara dinamis
     */
    std::unique_ptr<uint8_t[]> generatePayload(size_t size) const;

    /**
     * @brief Menghasilkan string header HTTP yang telah di-fuzz
     * @param originalHeader Header asli (misal: "User-Agent")
     * @param originalValue Nilai asli (misal: "Mozilla/5.0")
     * @return std::string Header yang sudah difuzz (bisa berupa nilai acak total atau modifikasi kecil)
     */
    std::string fuzzHttpHeader(const std::string& originalHeader, const std::string& originalValue) const;

    /**
     * @brief Menghasilkan TCP Flags yang tidak umum (untuk SYN Flood variant)
     * @return uint8_t TCP flags value (kombinasi SYN, ACK, FIN, RST, PSH, URG yang unik)
     */
    uint8_t generateUnusualTcpFlags() const;

    /**
     * @brief Menghasilkan UDP Payload dengan pola anomali
     * @param size Ukuran payload
     * @return std::vector<uint8_t> Payload UDP
     */
    std::vector<uint8_t> generateAnomalousUdpPayload(size_t size) const;

    /**
     * @brief Mengambil seed random saat ini (untuk logging/debugging)
     * @return uint64_t Seed
     */
    uint64_t getCurrentSeed() const;

private:
    // Struktur internal untuk thread-local storage
    struct ThreadLocalState {
        mutable std::mt19937_64 rng;
        mutable std::uniform_int_distribution<int> distribution;
        
        ThreadLocalState(uint64_t seed);
    };

    // Helper methods
    void ensureThreadLocalInitialized() const;
    
    // Akses thread-local state
    static thread_local ThreadLocalState* tls_state;
    static std::once_flag tls_init_flag;

    Config config_;
    uint64_t global_seed_;
    mutable std::mutex mutex_; // Untuk inisialisasi thread-local jika diperlukan

    // Helper untuk menghasilkan byte acak dalam range
    uint8_t getRandomByteInRange(uint8_t min, uint8_t max) const;
    
    // Helper untuk memilih karakter printable ASCII
    char getRandomPrintableChar() const;
};

} // namespace Core
} // namespace BrutalDDoS

#endif // BRUTAL_DDOS_FUZZER_H
