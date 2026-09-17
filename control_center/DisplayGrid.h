#pragma once

#include "TelemetryRow.h"

#include <mutex>
#include <map>
#include <string>

class DisplayGrid {
public:
    void update(const std::string& client_name, const std::string& device_key, ClientRow row);
    bool renderIfDirty();

private:
    std::string buildFrame() const;

    mutable std::mutex mutex_;
    std::map<std::string, std::map<std::string, ClientRow>> rows_;
    bool dirty_ = false;
};
