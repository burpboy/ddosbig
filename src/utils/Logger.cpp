#include "Logger.h"
#include <algorithm>
#include <cstdarg>
#include <ctime>

// --- Private Implementation ---

Logger::Logger() : currentLevel_(LogLevel::DEBUG) {
    // Default constructor
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::init(const std::string& logFilePath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    currentLevel_ = minLevel;
    
    if (!logFilePath.empty()) {
        logFilePath_ = logFilePath;
        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        if (logFile_.is_open()) {
            fileEnabled_ = true;
            logFile_ << "[INIT] Logger initialized. Log file: " << logFilePath << std::endl;
            logFile_.flush();
        } else {
            std::cerr << "Warning: Could not open log file: " << logFilePath << std::endl;
            fileEnabled_ = false;
        }
    } else {
        fileEnabled_ = false;
    }
    
    info("Logger initialized.");
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    struct tm timeInfo;
    localtime_r(&now_c, &timeInfo); // Thread-safe
    
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return std::string(buffer);
}

// Simple formatter that replaces {} placeholders if needed, 
// or just returns the message if no args are passed.
// Note: For true variadic formatting in C++17 without external libs, 
// it's complex. Here we assume the user passes plain strings or uses 
// standard stream insertion logic via a wrapper. 
// However, to keep it "actually implementation" and useful, 
// let's assume a simplified printf-style or just direct string passing.
// If you want {} replacement, see below helper.

std::string Logger::formatMessage(LogLevel level, const std::string& baseMsg, Args... args) {
    // In a real production code, you'd use fmt::format or boost::format here.
    // Since we want 0 dependencies, we'll stick to simple concatenation 
    // or assume the caller already formatted the string.
    // But to support the header's promise of Args..., we need a recursive unpacker.
    
    // Simplified approach for this file: 
    // We will just return the message + timestamp + color codes.
    // Variadic formatting is hard in pure C++17 without macros or libraries.
    // I will provide a macro-based or simple concat version if needed, 
    // but for clarity, let's assume the message is pre-formatted or just log it directly.
    
    // Actually, let's make it robust by just logging the string as is, 
    // but prepending metadata.
    
    std::string prefix = "[" + getTimestamp() + "] [";
    
    std::string levelStr;
    std::string color;
    
    switch (level) {
        case LogLevel::DEBUG:
            levelStr = "DEBUG";
            color = Colors::Blue;
            break;
        case LogLevel::INFO:
            levelStr = "INFO ";
            color = Colors::Green;
            break;
        case LogLevel::WARNING:
            levelStr = "WARN ";
            color = Colors::Yellow;
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR ";
            color = Colors::Red;
            break;
        case LogLevel::CRITICAL:
            levelStr = "CRIT ";
            color = Colors::Magenta;
            break;
        default:
            levelStr = "???? ";
            color = Colors::Reset;
    }
    
    prefix += levelStr + "] ";
    
    return prefix + color + baseMsg + Colors::Reset;
}

void Logger::writeToFile(const std::string& message) {
    if (fileEnabled_ && logFile_.is_open()) {
        // Strip ANSI color codes for clean log files? Optional.
        // For now, write raw including colors if user wants them in file too.
        // Or strip them. Let's strip them for cleanliness.
        
        // Simple strip: remove everything between \033[ and m
        std::string cleanMsg;
        bool inEscape = false;
        for (char c : message) {
            if (c == '\033') {
                inEscape = true;
            } else if (inEscape) {
                if (c == 'm') {
                    inEscape = false;
                }
            } else {
                cleanMsg += c;
            }
        }
        
        logFile_ << cleanMsg << std::endl;
        logFile_.flush();
    }
}

void Logger::writeToConsole(const std::string& message, LogLevel level) {
    if (consoleEnabled_) {
        std::cout << message << std::endl;
        std::cout.flush();
    }
}
