#pragma once

#include "../OPC_UA/CustomClient.h"
#include "../OPC_UA/ClientQueue.h"
#include "../OPC_UA/utils.h"
#include "../OPC_UA/shared_opc_ua_layout.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class SystemInfoClient : public CustomClient {
public:
    SystemInfoClient(std::string_view client_name,
                      std::filesystem::path project_root,
                      std::shared_ptr<FileLogger> logger,
                      std::shared_ptr<ClientQueue> queue);

    std::vector<opc_ua_utils::TelemetryStore> buildTelemetryPayload(
        const std::vector<TelemetrySnapshot>& drained);

    void addNodes();
    bool sendTelemetryPayload();

private:
    std::shared_ptr<ClientQueue> queue_;

    std::optional<opcua::NodeId>   client_folder_;
    std::optional<opcua::DataType> sensor_dto_type_;
    std::unordered_map<std::string, opc_ua_utils::SnapshotNodeIds> device_nodes_;
};
