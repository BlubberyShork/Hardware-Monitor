#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

class PerformanceLogger {
public:
    explicit PerformanceLogger(std::filesystem::path log_file)
        : path_(std::move(log_file)) {
        std::filesystem::create_directories(path_.parent_path());
        stream_.open(path_, std::ios::out | std::ios::trunc);
        if (stream_) {
            stream_ << "timestamp,label,duration_ms\n";
            stream_.flush();
        }
    }

    PerformanceLogger(const PerformanceLogger&) = delete;
    PerformanceLogger& operator=(const PerformanceLogger&) = delete;

    void start(std::string_view label) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_label_ = label;
        start_time_ = std::chrono::steady_clock::now();
    }

    void stop() {
        const auto end = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        if (active_label_.empty()) return;

        const double ms = std::chrono::duration<double, std::milli>(end - start_time_).count();
        const auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        stream_ << epoch_ms << ',' << active_label_ << ',' << ms << '\n';
        stream_.flush();
        active_label_.clear();
    }

private:
    std::filesystem::path path_;
    std::mutex mutex_;
    std::ofstream stream_;
    std::string active_label_;
    std::chrono::steady_clock::time_point start_time_;
};

inline std::filesystem::path defaultPerfLogPath(const std::filesystem::path& project_root,
                                                 std::string_view name) {
    return project_root / "build" / "performance_logs" / (std::string(name) + ".csv");
}
