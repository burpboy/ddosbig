#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iostream>

// Include library JSON eksternal (harus sudah di-setup di CMakeLists)
#include "nlohmann/json.hpp"

// Forward declaration untuk Logger agar tidak circular dependency
class Logger;

namespace BrutalDDoS {
namespace Config {

    enum class Format {
        JSON,
        YAML, // Kita treat sebagai format key-value sederhana atau subset YAML
        TXT   // Format key=value klasik
    };

    struct DDoSConfig {
        // Target Settings
        std::string target_ip = "127.0.0.1";
        int target_port = 80;
        
        // Attack Settings
        std::string attack_mode = "SYN"; // SYN, UDP, HTTP, DNS, NTP
        int threads_count = 4;
        uint64_t duration_seconds = 0; // 0 = infinite
        
        // Packet Settings
        size_t payload_size = 512;
        bool random_payload = false;
        bool spoof_ip = true;
        
        // Network Settings
        int socket_buffer_size_kb = 256;
        bool fast_reconnect = true;
        
        // Logging
        bool verbose_log = true;
        std::string log_file_path = "ddos.log";
    };

    class ConfigParser {
    public:
        // Singleton Instance
        static ConfigParser& instance() {
            static ConfigParser inst;
            return inst;
        }

        // Load configuration from file
        // Returns true if successful, false otherwise
        bool load(const std::string& filepath);

        // Get the parsed configuration object
        const DDoSConfig& get() const;

        // Helper to save current config back to file (overwrites)
        bool save(const std::string& filepath);

        // Utility: Reload config on the fly
        void reload();

        // Utility: Reset to defaults
        void resetDefaults();

    private:
        ConfigParser();
        ~ConfigParser() = default;
        
        // Prevent copying
        ConfigParser(const ConfigParser&) = delete;
        ConfigParser& operator=(const ConfigParser&) = delete;

        DDoSConfig m_config;
        mutable std::mutex m_mutex;

        // Internal parsers
        bool parseJSON(const std::string& content);
        bool parseYAML(const std::string& content);
        bool parseTXT(const std::string& content);
        
        // Helper to detect format
        Format detectFormat(const std::string& filename) const;
        
        // Helper to trim whitespace
        static std::string trim(const std::string& str);
    };

} // namespace Config
} // namespace BrutalDDoS
