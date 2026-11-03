// spdlog/sinks/stdout_color_sinks.h
#ifndef SPDLOG_SINKS_STDOUT_COLOR_SINKS_H
#define SPDLOG_SINKS_STDOUT_COLOR_SINKS_H

#include "spdlog/common.h"
#include <iostream>
#include <mutex>

namespace spdlog {
namespace sinks {

class stdout_color_sink_mt : public sink {
public:
    explicit stdout_color_sink_mt(std::ostream& stream = std::cout)
        : stream_(stream) {}

    void log(const std::string &msg, level::level_enum lvl) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // ANSI Colors
        const char* reset = "\033[0m";
        const char* red = "\033[31m";
        const char* green = "\033[32m";
        const char* yellow = "\033[33m";
        const char* cyan = "\033[36m";
        const char* magenta = "\033[35m";
        
        const char* color = reset;
        switch (lvl) {
            case level::trace: color = cyan; break;
            case level::debug: color = cyan; break;
            case level::info:  color = green; break;
            case level::warn:  color = yellow; break;
            case level::err:   color = red; break;
            case level::critical: color = magenta; break;
            default: break;
        }

        stream_ << color << "[" << level::to_string(lvl) << "] " << msg << reset << "\n";
    }

    void flush() override { stream_.flush(); }

private:
    std::ostream& stream_;
    mutable std::mutex mutex_;
};

inline std::shared_ptr<logger> stdout_color_stl(const std::string& name) {
    auto sink = std::make_shared<stdout_color_sink_mt>();
    return std::make_shared<logger>(name, sink);
}

} // namespace sinks
} // namespace spdlog

#endif // SPDLOG_SINKS_STDOUT_COLOR_SINKS_H
