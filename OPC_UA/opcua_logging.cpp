#include "opcua_logging.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace opcua_log {
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

std::string timestampFolderName() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

std::ofstream& logFile() {
    static std::ofstream stream = [] {
    const auto dir = std::filesystem::current_path() / "logs" / timestampFolderName();
        std::filesystem::create_directories(dir);
        std::ofstream f(dir / "session.log", std::ios::app);
        if (!f)
            throw std::runtime_error("Failed to open OPC UA log file in " + dir.string());
        return f;
    }();
    return stream;
}

std::mutex logMutex;

} // namespace

void write(opcua::LogLevel level, opcua::LogCategory category, std::string_view msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile() << "[" << toString(level) << "] "
              << "[" << toString(category) << "] "
              << msg << std::endl;
}

} // namespace opcua_log
