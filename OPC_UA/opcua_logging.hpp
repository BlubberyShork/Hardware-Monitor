#pragma once

#include <string_view>
#include "open62541pp/plugin/log.hpp"

namespace opcua_log {
    void write(opcua::LogLevel level, opcua::LogCategory category, std::string_view msg);
}
