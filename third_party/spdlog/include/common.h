// spdlog/common.h
#ifndef SPDLOG_COMMON_H
#define SPDLOG_COMMON_H

#include <iostream>
#include <sstream>
#include <string>
#include <memory>

namespace spdlog {
namespace level {
    enum level_enum : uint8_t {
        trace = 0,
        debug = 1,
        info = 2,
        warn = 3,
        err = 4,
        critical = 5,
        off = 6
    };

    inline const char* to_string(level_enum l) {
        switch(l) {
            case trace: return "[TRACE]";
            case debug: return "[DEBUG]";
            case info:  return "[INFO] ";
            case warn:  return "[WARN] ";
            case err:   return "[ERROR]";
            case critical: return "[CRIT] ";
            case off:   return "[OFF]  ";
            default: return "[UNKNOWN]";
        }
    }
}

// Global logger registry helper
inline std::shared_ptr<class logger> create_stdout_color_logger(const std::string& name) {
    // Placeholder for actual sink creation - see sinks/stdout_color_sinks.h
    return nullptr; 
}

} // namespace spdlog

#endif // SPDLOG_COMMON_H
