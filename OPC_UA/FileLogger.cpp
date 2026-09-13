#include "FileLogger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string_view toString(opcua::LogLevel level) {
    switch (level) {
        case opcua::LogLevel::Trace:   return "trace";
        case opcua::LogLevel::Debug:   return "debug";
        case opcua::LogLevel::Info:    return "info";
        case opcua::LogLevel::Warning: return "warning";
        case opcua::LogLevel::Error:   return "error";
        case opcua::LogLevel::Fatal:   return "fatal";
        default:                       return "unknown";
    }
}

std::string_view toString(opcua::LogCategory category) {
    switch (category) {
        case opcua::LogCategory::Network:        return "network";
        case opcua::LogCategory::SecureChannel:  return "channel";
        case opcua::LogCategory::Session:        return "session";
        case opcua::LogCategory::Server:         return "server";
        case opcua::LogCategory::Client:         return "client";
        case opcua::LogCategory::Userland:       return "userland";
        case opcua::LogCategory::SecurityPolicy: return "securitypolicy";
        default:                                 return "unknown";
    }
}

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << now_ms.count();
    return oss.str();
}

} // namespace

FileLogger::FileLogger(std::filesystem::path log_file, opcua::LogLevel min_level)
    : path_(std::move(log_file)), min_level_(min_level) {
    std::filesystem::create_directories(path_.parent_path());
    stream_.open(path_, std::ios::out | std::ios::trunc);
    if (!stream_) {
        throw std::runtime_error("Failed to open log file: " + path_.string());
    }
}

void FileLogger::write(opcua::LogLevel level, opcua::LogCategory category, std::string_view msg) {
    if (level < min_level_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    stream_ << timestampNow() << " [" << toString(level) << "] [" << toString(category) << "] "
            << msg << '\n';
    stream_.flush();
}

void FileLogger::write(std::string_view msg) {
    write(opcua::LogLevel::Info, opcua::LogCategory::Userland, msg);
}

opcua::LogFunction FileLogger::asLogFunction() {
    return [this](opcua::LogLevel level, opcua::LogCategory category, std::string_view msg) {
        write(level, category, msg);
    };
}

std::filesystem::path defaultLogPath(const std::filesystem::path& project_root,
                                      std::string_view client_name) {
    return project_root / "build" / "logs" / (std::string(client_name) + ".log");
}
