#pragma once

#include "../OPC_UA/CustomClient.h"
#include "../OPC_UA/utils.h"
#include "TelemetryRow.h"
#include "../OPC_UA/shared_opc_ua_layout.h"

#include <open62541pp/subscription.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class PerformanceLogger;

class ControlCenterClient : public CustomClient {
public:
    using TelemetrySink = std::function<void(const std::string& client_folder_name, const std::string& device_key, ClientRow row)>;

    ControlCenterClient(std::string_view client_name,
                         std::filesystem::path project_root,
                         std::shared_ptr<FileLogger> logger,
                         TelemetrySink sink);
    ~ControlCenterClient();

    void setPerfLogger(std::shared_ptr<PerformanceLogger> perf_logger);

    void start();
    void tick(std::chrono::milliseconds io_timeout,
              std::chrono::milliseconds poll_interval);

private:
    struct DeviceCache {
        std::string client_folder_name;
        opc_ua_utils::DecodedDevice info;
    };

    void discoverAndSubscribe();

    void handleSensorUpdate(const std::string& device_node_id,
                             const opcua::Variant& value);

    TelemetrySink                      sink_;
    std::optional<opcua::Subscription<opcua::Client>> subscription_;
    opcua::DataType                    sensor_dto_type_;

    std::map<std::string, DeviceCache> device_cache_;

    std::chrono::steady_clock::time_point last_poll_{};
    std::shared_ptr<PerformanceLogger> perf_logger_;
};
