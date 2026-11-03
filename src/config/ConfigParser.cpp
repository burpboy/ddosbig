#include "ConfigParser.h"
#include "../utils/Logger.h" // Sesuaikan path include sesuai struktur foldermu
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

using namespace BrutalDDoS::Config;

// --- Constructor & Defaults ---
ConfigParser::ConfigParser() {
    resetDefaults();
}

void ConfigParser::resetDefaults() {
    m_config.target_ip = "127.0.0.1";
    m_config.target_port = 80;
    m_config.attack_mode = "SYN";
    m_config.threads_count = 4;
    m_config.duration_seconds = 0;
    m_config.payload_size = 512;
    m_config.random_payload = false;
    m_config.spoof_ip = true;
    m_config.socket_buffer_size_kb = 256;
    m_config.fast_reconnect = true;
    m_config.verbose_log = true;
    m_config.log_file_path = "ddos.log";
}

// --- Public API ---

bool ConfigParser::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!fs::exists(filepath)) {
        Logger::error("Config file not found: " + filepath);
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Cannot open config file: " + filepath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    Format fmt = detectFormat(filepath);
    bool success = false;

    try {
        switch (fmt) {
            case Format::JSON:
                success = parseJSON(content);
                break;
            case Format::YAML:
                success = parseYAML(content);
                break;
            case Format::TXT:
                success = parseTXT(content);
                break;
            default:
                Logger::warn("Unknown format for " + filepath + ", trying JSON first.");
                success = parseJSON(content);
                if (!success) success = parseTXT(content);
                break;
        }
    } catch (const std::exception& e) {
        Logger::error("Error parsing config file " + filepath + ": " + e.what());
        success = false;
    }

    if (success) {
        Logger::info("Configuration loaded successfully from: " + filepath);
    } else {
        Logger::warn("Falling back to default configuration due to parse error.");
        resetDefaults();
    }

    return success;
}

bool ConfigParser::save(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Simple JSON save as default
    nlohmann::json j;
    j["target_ip"] = m_config.target_ip;
    j["target_port"] = m_config.target_port;
    j["attack_mode"] = m_config.attack_mode;
    j["threads_count"] = m_config.threads_count;
    j["duration_seconds"] = m_config.duration_seconds;
    j["payload_size"] = m_config.payload_size;
    j["random_payload"] = m_config.random_payload;
    j["spoof_ip"] = m_config.spoof_ip;
    j["socket_buffer_size_kb"] = m_config.socket_buffer_size_kb;
    j["fast_reconnect"] = m_config.fast_reconnect;
    j["verbose_log"] = m_config.verbose_log;
    j["log_file_path"] = m_config.log_file_path;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Failed to save config to: " + filepath);
        return false;
    }

    file << j.dump(4); // Pretty print with 4 spaces
    file.close();
    
    Logger::info("Configuration saved to: " + filepath);
    return true;
}

void ConfigParser::reload() {
    // If we have a stored path, reload it. Otherwise just reset.
    // For this simple implementation, we assume the user calls load() explicitly 
    // or we store the last loaded path in a member variable.
    // To keep it simple without extra state:
    // Let's assume reload means re-read the DEFAULT config path if exists.
    // Or better: Just reset to defaults if no file is tracked.
    // Here we just reset to defaults to be safe unless you track the last path.
    resetDefaults();
}

const DDoSConfig& ConfigParser::get() const {
    return m_config;
}

// --- Private Parsers ---

bool ConfigParser::parseJSON(const std::string& content) {
    auto json = nlohmann::json::parse(content, nullptr, false);
    if (json.is_discarded()) {
        throw std::runtime_error("Invalid JSON syntax");
    }

    // Helper lambda to safely extract values
    auto safe_get = [&](const std::string& key, auto& dest) {
        if (json.contains(key)) {
            try {
                dest = json[key];
            } catch (...) {
                // Ignore type mismatch, keep default
            }
        }
    };

    safe_get("target_ip", m_config.target_ip);
    safe_get("target_port", m_config.target_port);
    safe_get("attack_mode", m_config.attack_mode);
    safe_get("threads_count", m_config.threads_count);
    safe_get("duration_seconds", m_config.duration_seconds);
    safe_get("payload_size", m_config.payload_size);
    safe_get("random_payload", m_config.random_payload);
    safe_get("spoof_ip", m_config.spoof_ip);
    safe_get("socket_buffer_size_kb", m_config.socket_buffer_size_kb);
    safe_get("fast_reconnect", m_config.fast_reconnect);
    safe_get("verbose_log", m_config.verbose_log);
    safe_get("log_file_path", m_config.log_file_path);

    return true;
}

bool ConfigParser::parseYAML(const std::string& content) {
    // Lightweight YAML parser for flat structures
    // Supports: key: value
    // Ignores comments (#) and empty lines
    
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find(':');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        // Remove quotes if present
        if (value.size() >= 2 && 
            ((value.front() == '"' && value.back() == '"') || 
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // Map keys to config
        if (key == "target_ip") m_config.target_ip = value;
        else if (key == "target_port") m_config.target_port = std::stoi(value);
        else if (key == "attack_mode") m_config.attack_mode = value;
        else if (key == "threads_count") m_config.threads_count = std::stoi(value);
        else if (key == "duration_seconds") m_config.duration_seconds = std::stoull(value);
        else if (key == "payload_size") m_config.payload_size = std::stoull(value);
        else if (key == "random_payload") m_config.random_payload = (value == "true" || value == "1");
        else if (key == "spoof_ip") m_config.spoof_ip = (value == "true" || value == "1");
        else if (key == "socket_buffer_size_kb") m_config.socket_buffer_size_kb = std::stoi(value);
        else if (key == "fast_reconnect") m_config.fast_reconnect = (value == "true" || value == "1");
        else if (key == "verbose_log") m_config.verbose_log = (value == "true" || value == "1");
        else if (key == "log_file_path") m_config.log_file_path = value;
    }

    return true;
}

bool ConfigParser::parseTXT(const std::string& content) {
    // Legacy Key=Value format
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        if (key == "TARGET_IP") m_config.target_ip = value;
        else if (key == "TARGET_PORT") m_config.target_port = std::stoi(value);
        else if (key == "MODE") m_config.attack_mode = value;
        else if (key == "THREADS") m_config.threads_count = std::stoi(value);
        else if (key == "DURATION") m_config.duration_seconds = std::stoull(value);
        else if (key == "PAYLOAD_SIZE") m_config.payload_size = std::stoull(value);
        else if (key == "RANDOM_PAYLOAD") m_config.random_payload = (value == "true" || value == "1");
        else if (key == "SPOOF_IP") m_config.spoof_ip = (value == "true" || value == "1");
        else if (key == "BUFFER_KB") m_config.socket_buffer_size_kb = std::stoi(value);
        else if (key == "FAST_RECONNECT") m_config.fast_reconnect = (value == "true" || value == "1");
        else if (key == "VERBOSE") m_config.verbose_log = (value == "true" || value == "1");
        else if (key == "LOG_FILE") m_config.log_file_path = value;
    }

    return true;
}

// --- Helpers ---

ConfigParser::Format ConfigParser::detectFormat(const std::string& filename) const {
    std::string ext = "";
    size_t dot = filename.find_last_of(".");
    if (dot != std::string::npos) {
        ext = filename.substr(dot + 1);
    }

    if (ext == "json") return Format::JSON;
    if (ext == "yaml" || ext == "yml") return Format::YAML;
    if (ext == "txt" || ext == "conf" || ext == "cfg") return Format::TXT;
    
    // Fallback: Try to detect by content
    std::ifstream f(filename);
    if (f.good()) {
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.find('{') != std::string::npos) return Format::JSON;
        if (content.find('=') != std::string::npos) return Format::TXT;
    }
    
    return Format::TXT; // Default fallback
}

std::string ConfigParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    
    return str.substr(start, end - start + 1);
}
