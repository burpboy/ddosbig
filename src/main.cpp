/**
 * main.cpp - Entry point for BrutalDDoS
 * 
 * Parses command-line arguments, validates them, and initializes the global
 * configuration and attack engine.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <atomic>

// Include local headers (assuming standard relative paths)
#include "config/ConfigParser.h"
#include "core/AttackEngine.h"
#include "network/SocketManager.h"
#include "utils/Logger.h"
#include "utils/Timer.h"

// Forward declarations of helper functions
static void print_usage(const char* program_name);
static bool parse_arguments(int argc, char* argv[], Config& config);
static void initialize_global_resources(Config& config);
static void cleanup_global_resources();

// Global instance of the attack engine (singleton-like pattern for simplicity)
static AttackEngine* g_attack_engine = nullptr;
static Logger* g_logger = nullptr;
static std::atomic<bool> g_running(false);

int main(int argc, char* argv[]) {
    // 1. Initialize Logging first thing
    g_logger = new Logger("brutaldos.log", true); // true = verbose/console output
    LOG_INFO << "BrutalDDoS v1.0 Starting...";

    // 2. Parse Arguments
    Config config;
    if (!parse_arguments(argc, argv, config)) {
        LOG_ERROR << "Failed to parse arguments.";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 3. Validate Configuration
    if (config.target_ip.empty() || config.target_port == 0) {
        LOG_ERROR << "Target IP and Port are required.";
        return EXIT_FAILURE;
    }

    if (config.mode != MODE_SYN && config.mode != MODE_UDP && config.mode != MODE_HTTP && config.mode != MODE_ALL) {
        LOG_ERROR << "Invalid attack mode: " << config.mode_str;
        return EXIT_FAILURE;
    }

    LOG_INFO << "Configuration Validated:";
    LOG_INFO << "  Target: " << config.target_ip << ":" << config.target_port;
    LOG_INFO << "  Mode: " << config.mode_str;
    LOG_INFO << "  Threads: " << config.thread_count;
    LOG_INFO << "  SYN Rate: " << config.syn_rate;
    LOG_INFO << "  UDP BW: " << config.udp_bandwidth_mbps << " Mbps";
    LOG_INFO << "  HTTP RPS: " << config.http_rps;
    LOG_INFO << "  HTTP Method: " << config.http_method;
    LOG_INFO << "  Payload Size: " << config.payload_size_kb << " KB";

    // 4. Initialize Global Resources
    try {
        initialize_global_resources(config);
        
        // 5. Start the Attack
        LOG_INFO << "Launching Attack Engine...";
        g_attack_engine->start();
        
        // Keep the main thread alive until interrupted
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Optional: Display stats periodically
            if (g_logger->get_verbose()) {
                LOG_INFO << "Attack running... Stats updated.";
            }
        }

        LOG_INFO << "Stopping Attack Engine...";
        g_attack_engine->stop();
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Fatal error during execution: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR << "Unknown fatal error occurred.";
        return EXIT_FAILURE;
    } finally {
        // 6. Cleanup
        cleanup_global_resources();
        LOG_INFO << "BrutalDDoS exited cleanly.";
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Parses command-line arguments into the Config object.
 * 
 * Supports flags:
 *   -t <ip>       : Target IP address
 *   -p <port>     : Target port
 *   -m <mode>     : Attack mode (syn, udp, http, all)
 *   -c <count>    : Number of threads
 *   --syn-rate <n>: SYN flood packets per second
 *   --udp-bandwidth <mbps>: UDP bandwidth in Mbps
 *   --http-rps <n>: HTTP requests per second
 *   --http-method <method>: HTTP method (GET, POST)
 *   --payload-size <kb>: Payload size in kilobytes
 *   --verbose     : Enable verbose logging
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @param config Reference to Config object to populate
 * @return true on success, false on failure
 */
bool parse_arguments(int argc, char* argv[], Config& config) {
    if (argc < 2) {
        return false;
    }

    // Defaults
    config.target_ip = "";
    config.target_port = 80;
    config.mode = MODE_HTTP; // Default mode
    config.mode_str = "http";
    config.thread_count = 4;
    config.syn_rate = 10000;
    config.udp_bandwidth_mbps = 100;
    config.http_rps = 1000;
    config.http_method = "POST";
    config.payload_size_kb = 1;
    config.verbose = false;

    int i = 1; // Skip program name
    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "-t" && i + 1 < argc) {
            config.target_ip = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            config.target_port = std::stoi(argv[++i]);
        } else if (arg == "-m" && i + 1 < argc) {
            std::string mode_str = argv[++i];
            if (mode_str == "syn") {
                config.mode = MODE_SYN;
                config.mode_str = "syn";
            } else if (mode_str == "udp") {
                config.mode = MODE_UDP;
                config.mode_str = "udp";
            } else if (mode_str == "http") {
                config.mode = MODE_HTTP;
                config.mode_str = "http";
            } else if (mode_str == "all") {
                config.mode = MODE_ALL;
                config.mode_str = "all";
            } else {
                LOG_WARN << "Unknown mode: " << mode_str;
                return false;
            }
        } else if (arg == "-c" && i + 1 < argc) {
            config.thread_count = std::stoi(argv[++i]);
        } else if (arg == "--syn-rate" && i + 1 < argc) {
            config.syn_rate = std::stoi(argv[++i]);
        } else if (arg == "--udp-bandwidth" && i + 1 < argc) {
            config.udp_bandwidth_mbps = std::stoi(argv[++i]);
        } else if (arg == "--http-rps" && i + 1 < argc) {
            config.http_rps = std::stoi(argv[++i]);
        } else if (arg == "--http-method" && i + 1 < argc) {
            config.http_method = argv[++i];
        } else if (arg == "--payload-size" && i + 1 < argc) {
            config.payload_size_kb = std::stoi(argv[++i]);
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else {
            LOG_WARN << "Unknown argument: " << arg;
            return false;
        }
        ++i;
    }

    return true;
}

/**
 * @brief Initializes global resources like sockets, timers, and the attack engine.
 * 
 * @param config The parsed configuration.
 */
void initialize_global_resources(Config& config) {
    // Initialize Timer for performance monitoring
    Timer::init();

    // Initialize Socket Manager
    SocketManager::init();

    // Create and configure the Attack Engine
    g_attack_engine = new AttackEngine(config);
    
    // Set global running flag
    g_running.store(true);
}

/**
 * @brief Cleans up global resources.
 */
void cleanup_global_resources() {
    if (g_attack_engine) {
        delete g_attack_engine;
        g_attack_engine = nullptr;
    }
    
    SocketManager::cleanup();
    Timer::cleanup();
    
    if (g_logger) {
        delete g_logger;
        g_logger = nullptr;
    }
    
    g_running.store(false);
}

/**
 * @brief Prints usage information to stdout.
 * 
 * @param program_name The name of the executable.
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -t <ip>          Target IP address\n"
              << "  -p <port>        Target port (default: 80)\n"
              << "  -m <mode>        Attack mode: syn, udp, http, all (default: http)\n"
              << "  -c <count>       Number of threads (default: 4)\n"
              << "  --syn-rate <n>   SYN flood packets per second (default: 10000)\n"
              << "  --udp-bandwidth <mbps> UDP bandwidth in Mbps (default: 100)\n"
              << "  --http-rps <n>   HTTP requests per second (default: 1000)\n"
              << "  --http-method <method> HTTP method: GET, POST (default: POST)\n"
              << "  --payload-size <kb> Payload size in KB (default: 1)\n"
              << "  --verbose        Enable verbose logging\n"
              << "  -h, --help       Show this help message\n";
}
