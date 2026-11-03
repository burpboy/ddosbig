#pragma once

#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace BrutalDDoS {
namespace Utils {

/**
 * @class Timer
 * @brief High-resolution timer for precise latency and duration monitoring.
 * 
 * Supports single-shot timing and continuous accumulation for statistical analysis.
 * Thread-safe for concurrent use across multiple attack threads.
 */
class Timer {
public:
    /**
     * @brief Construct a new Timer object
     * @param auto_reset If true, resets the timer after stop() is called. Default: false.
     */
    explicit Timer(bool auto_reset = false);

    ~Timer();

    // Non-copyable to prevent accidental sharing of state
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /**
     * @brief Start the timer.
     * If the timer is already running, it continues from the current elapsed time.
     */
    void start();

    /**
     * @brief Stop the timer and accumulate the elapsed time.
     * @return The elapsed duration in microseconds as double.
     */
    double stop();

    /**
     * @brief Get the current elapsed time without stopping.
     * @return Elapsed time in microseconds.
     */
    double getElapsedUs() const;

    /**
     * @brief Reset the timer to zero.
     */
    void reset();

    /**
     * @brief Check if the timer is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Get formatted string of total accumulated time.
     * @return Human-readable string (e.g., "1m 23s 456ms").
     */
    std::string getFormattedTotalTime() const;

    /**
     * @brief Get formatted string of last single interval time.
     * @return Human-readable string.
     */
    std::string getFormattedLastInterval() const;

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    mutable std::mutex mtx_;
    TimePoint start_time_;
    bool is_running_;
    
    // Accumulated total time in microseconds
    std::atomic<double> total_elapsed_us_;
    double last_interval_us_;
    
    bool auto_reset_;
};

} // namespace Utils
} // namespace BrutalDDoS

#endif // TIMER_H
