// spdlog/spdlog.h
// Minimal wrapper to ensure clean API usage in BrutalDDoS

#ifndef SPDLOG_SPDLOG_H
#define SPDLOG_SPDLOG_H

#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/tweakme.h"

namespace spd {
    using Logger = spdlog::logger;
}

#endif // SPDLOG_SPDLOG_H
