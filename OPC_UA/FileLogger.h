#pragma once

#include <open62541pp/plugin/log_default.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

// Per-instance log sink. Each CustomClient owns its own FileLogger writing to
// its own file, so multiple clients in the same process never share one log.
class FileLogger {
public:
    explicit FileLogger(std::filesystem::path log_file,
                         opcua::LogLevel min_level = opcua::LogLevel::Info);

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;
    FileLogger(FileLogger&&) = delete;
    FileLogger& operator=(FileLogger&&) = delete;

    void write(opcua::LogLevel level, opcua::LogCategory category, std::string_view msg);
    void write(std::string_view msg);

    // Adapter for opcua::ClientConfig::setLogger()/opcua::ServerConfig::setLogger().
    opcua::LogFunction asLogFunction();

private:
    std::filesystem::path path_;
    opcua::LogLevel        min_level_;
    std::mutex              mutex_;
    std::ofstream           stream_;
};

// project_root/build/logs/<client_name>.log
std::filesystem::path defaultLogPath(const std::filesystem::path& project_root,
                                      std::string_view client_name);
