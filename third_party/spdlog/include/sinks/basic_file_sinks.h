// spdlog/sinks/basic_file_sink.h
#ifndef SPDLOG_SINKS_BASIC_FILE_SINK_H
#define SPDLOG_SINKS_BASIC_FILE_SINK_H

#include "spdlog/common.h"
#include <fstream>
#include <mutex>

namespace spdlog {
namespace sinks {

class basic_file_sink_mt : public sink {
public:
    explicit basic_file_sink_mt(const std::string &filename, bool truncate = false) {
        filename_ = filename;
        of_.open(filename, std::ios::out | std::ios::app | (truncate ? std::ios::trunc : 0));
        if (of_.fail()) throw std::runtime_error("Failed to open log file: " + filename);
    }

    void log(const std::string &msg, level::level_enum lvl) override {
        std::lock_guard<std::mutex> lock(mutex_);
        of_ << "[" << level::to_string(lvl) << "] " << msg << "\n";
        of_.flush();
    }

    void flush() override { of_.flush(); }

private:
    std::string filename_;
    std::ofstream of_;
    mutable std::mutex mutex_;
};

} // namespace sinks
} // namespace spdlog

#endif // SPDLOG_SINKS_BASIC_FILE_SINK_H
