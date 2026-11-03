#include "Timer.h"

namespace BrutalDDoS {
namespace Utils {

Timer::Timer(bool auto_reset)
    : is_running_(false),
      total_elapsed_us_(0.0),
      last_interval_us_(0.0),
      auto_reset_(auto_reset) {
    // Initialize start time but don't start yet
    start_time_ = Clock::now();
}

Timer::~Timer() {
    // Ensure timer is stopped on destruction to avoid inaccurate totals
    if (is_running_) {
        stop();
    }
}

void Timer::start() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!is_running_) {
        start_time_ = Clock::now();
        is_running_ = true;
    }
}

double Timer::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (is_running_) {
        auto end_time = Clock::now();
        double delta_us = std::chrono::duration<double, std::micro>(end_time - start_time_).count();
        
        total_elapsed_us_.fetch_add(delta_us, std::memory_order_relaxed);
        last_interval_us_ = delta_us;
        
        is_running_ = false;
        
        if (auto_reset_) {
            start_time_ = Clock::now();
        }
        
        return delta_us;
    }
    return 0.0;
}

double Timer::getElapsedUs() const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (is_running_) {
        auto now = Clock::now();
        return std::chrono::duration<double, std::micro>(now - start_time_).count();
    }
    return 0.0;
}

void Timer::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    start_time_ = Clock::now();
    is_running_ = false;
    total_elapsed_us_.store(0.0, std::memory_order_relaxed);
    last_interval_us_ = 0.0;
}

bool Timer::isRunning() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return is_running_;
}

std::string Timer::getFormattedTotalTime() const {
    std::lock_guard<std::mutex> lock(mtx_);
    double total_s = total_elapsed_us_.load(std::memory_order_relaxed) / 1000000.0;
    
    long long seconds = static_cast<long long>(total_s);
    long long milliseconds = static_cast<long long>((total_s - seconds) * 1000);
    long long microseconds = static_cast<long long>((total_s - seconds - milliseconds / 1000.0) * 1000000);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    
    if (seconds >= 3600) {
        oss << (seconds / 3600) << "h ";
        seconds %= 3600;
    }
    if (seconds >= 60) {
        oss << (seconds / 60) << "m ";
        seconds %= 60;
    }
    oss << seconds << "s ";
    oss << milliseconds << "ms ";
    oss << microseconds << "us";
    
    return oss.str();
}

std::string Timer::getFormattedLastInterval() const {
    std::lock_guard<std::mutex> lock(mtx_);
    double last_s = last_interval_us_ / 1000000.0;
    
    long long milliseconds = static_cast<long long>(last_s * 1000);
    long long microseconds = static_cast<long long>((last_s * 1000000) - (milliseconds * 1000));

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << milliseconds << "ms " << microseconds << "us";
    
    return oss.str();
}

} // namespace Utils
} // namespace BrutalDDoS
