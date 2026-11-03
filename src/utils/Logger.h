#pragma once

#ifndef BRUTAL_LOGGER_H
#define BRUTAL_LOGGER_H

#include <string>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

// Include spdlog headers dari third_party
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/bundled/chrono.h" // Untuk format waktu yang rapi

namespace Brutal {
namespace Log {

    enum class Level {
        Trace = 0,
        Debug = 1,
        Info  = 2,
        Warn  = 3,
        Error = 4,
        Critical = 5
    };

    class Logger {
    public:
        // Singleton Pattern untuk memastikan satu instance logger global
        static Logger& getInstance() {
            static Logger instance;
            return instance;
        }

        // Inisialisasi logger dengan nama aplikasi dan file log
        void init(const std::string& app_name, const std::string& log_file_path = "brutal_ddos.log") {
            if (initialized_) return;

            try {
                // 1. Buat Console Sink dengan Warna (ANSI)
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                
                // Format warna untuk level berbeda
                console_sink->set_pattern("[%H:%M:%S] [%^%l%$] %v"); 

                // 2. Buat File Sink (Basic, tanpa rotasi kompleks untuk performa maksimal)
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true); // true = truncate on start
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

                // 3. Gabungkan sinks
                std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

                // 4. Buat Logger utama
                logger_ = std::make_shared<spdlog::logger>(app_name, sinks.begin(), sinks.end());
                
                // Set level default ke Debug agar semua tercatat
                logger_->set_level(spdlog::level::debug);
                logger_->flush_on(spdlog::level::info); // Flush setiap info
                
                // Register ke global spdlog registry agar bisa diakses via spdlog::get()
                spdlog::register_logger(logger_);
                
                initialized_ = true;
            } catch (const spdlog::spdlog_ex& ex) {
                std::cerr << "Log Init Failed: " << ex.what() << std::endl;
            }
        }

        // Helper methods untuk logging cepat
        inline void trace(const char* fmt, Args&&... args) { log_(spdlog::level::trace, fmt, std::forward<Args>(args)...); }
        inline void debug(const char* fmt, Args&&... args) { log_(spdlog::level::debug, fmt, std::forward<Args>(args)...); }
        inline void info(const char* fmt, Args&&... args) { log_(spdlog::level::info, fmt, std::forward<Args>(args)...); }
        inline void warn(const char* fmt, Args&&... args) { log_(spdlog::level::warn, fmt, std::forward<Args>(args)...); }
        inline void error(const char* fmt, Args&&... args) { log_(spdlog::level::err, fmt, std::forward<Args>(args)...); }
        inline void critical(const char* fmt, Args&&... args) { log_(spdlog::level::critical, fmt, std::forward<Args>(args)...); }

        // Method khusus untuk DDoS: Log serangan besar-besaran tanpa bottleneck
        inline void attack_log(const char* msg) {
            if (logger_) {
                logger_->info("[ATTACK] {}", msg);
            }
        }

    private:
        template<typename... Args>
        void log_(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
            if (logger_) {
                logger_->log(level, fmt, std::forward<Args>(args)...);
            }
        }

        Logger() : logger_(nullptr), initialized_(false) {}
        ~Logger() {
            if (logger_) {
                spdlog::shutdown();
            }
        }

        // Non-copyable
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

    private:
        std::shared_ptr<spdlog::logger> logger_;
        bool initialized_;
    };

} // namespace Log
} // namespace Brutal

// Macro convenience untuk akses global yang lebih cepat
#define LOG_TRACE(...)   Brutal::Log::Logger::getInstance().trace(__VA_ARGS__)
#define LOG_DEBUG(...)   Brutal::Log::Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...)    Brutal::Log::Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARN(...)    Brutal::Log::Logger::getInstance().warn(__VA_ARGS__)
#define LOG_ERROR(...)   Brutal::Log::Logger::getInstance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) Brutal::Log::Logger::getInstance().critical(__VA_ARGS__)

#endif // BRUTAL_LOGGER_H
