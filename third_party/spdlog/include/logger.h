// spdlog/logger.h
#ifndef SPDLOG_LOGGER_H
#define SPDLOG_LOGGER_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "spdlog/common.h"

namespace spdlog {

class sink;

class logger {
public:
    explicit logger(std::string name, std::shared_ptr<sink> sink);
    explicit logger(std::string name, sinks_init_list sinks_list);

    void set_level(level::level_enum log_level);
    level::level_enum level() const;
    void flush_on(level::level_enum log_level);
    void flush();
    bool should_log(level::level_enum log_level) const;
    void log(level::level_enum log_level, char const* msg);
    void log(level::level_enum log_level, char const* fmt, Args&&... args);
    
    // Convenience methods
    void debug(char const* fmt, Args&&... args);
    void info(char const* fmt, Args&&... args);
    void warn(char const* fmt, Args&&... args);
    void error(char const* fmt, Args&&... args);
    void critical(char const* fmt, Args&&... args);

private:
    std::string name_;
    std::shared_ptr<sink> sink_;
    level::level_enum level_;
};

inline logger::logger(std::string name, std::shared_ptr<sink> sink)
    : name_(std::move(name)), sink_(std::move(sink)), level_(level::info) {}

inline logger::logger(std::string name, sinks_init_list sinks_list)
    : name_(std::move(name)) {
    for (auto& sink : sinks_list) {
        sink_ = sink; // Simplified for single-sink demo
    }
    level_ = level::info;
}

inline void logger::set_level(level::level_enum log_level) { level_ = log_level; }
inline level::level_enum logger::level() const { return level_; }
inline bool logger::should_log(level::level_enum log_level) const { return log_level >= level_; }

inline void logger::debug(char const* fmt, Args&&... args) {
    if (should_log(level::debug)) {
        log(level::debug, fmt, std::forward<Args>(args)...);
    }
}

inline void logger::info(char const* fmt, Args&&... args) {
    if (should_log(level::info)) {
        log(level::info, fmt, std::forward<Args>(args)...);
    }
}

inline void logger::warn(char const* fmt, Args&&... args) {
    if (should_log(level::warn)) {
        log(level::warn, fmt, std::forward<Args>(args)...);
    }
}

inline void logger::error(char const* fmt, Args&&... args) {
    if (should_log(level::err)) {
        log(level::err, fmt, std::forward<Args>(args)...);
    }
}

inline void logger::critical(char const* fmt, Args&&... args) {
    if (should_log(level::critical)) {
        log(level::critical, fmt, std::forward<Args>(args)...);
    }
}

} // namespace spdlog

#endif // SPDLOG_LOGGER_H
