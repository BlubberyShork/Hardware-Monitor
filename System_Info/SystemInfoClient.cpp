#include "SystemInfoClient.h"

#include "../user_space_common/PerformanceLogger.h"

#include <iostream>

SystemInfoClient::SystemInfoClient(std::string_view client_name,
                                    std::filesystem::path project_root,
                                    std::shared_ptr<FileLogger> logger,
                                    std::shared_ptr<ClientQueue> queue)
    : CustomClient(client_name, std::move(project_root), std::move(logger)),
      queue_(std::move(queue)) {}

void SystemInfoClient::setPerfLogger(std::shared_ptr<PerformanceLogger> perf_logger) {
    perf_logger_ = std::move(perf_logger);
}

std::vector<opc_ua_utils::TelemetryStore> SystemInfoClient::buildTelemetryPayload(
    const std::vector<TelemetrySnapshot>& drained) {
    std::vector<opc_ua_utils::TelemetryStore> stores;
    stores.reserve(drained.size());
    for (const auto& snapshot : drained) {
        stores.push_back(opc_ua_utils::buildTelemetryStore(snapshot, *sensor_dto_type_));
    }
    return stores;
}

void SystemInfoClient::addNodes() {
    namespace layout = opc_ua_layout;

    if (!sensor_dto_type_.has_value()) {
        sensor_dto_type_ = opc_ua_utils::buildSensorDtoType(layout::kTelemetryNamespaceIndex);
    }

    opcua::Node objects_folder(client_, opcua::ObjectId::ObjectsFolder);
    opcua::Node clients_folder = objects_folder.browseChild(
        {{layout::kTelemetryNamespaceIndex, layout::kTelemetryClientsFolderName}});

    auto browse_name = clients_folder.readBrowseName();
    std::cout << "clients_folder ns_index: " << browse_name.namespaceIndex() << "\n";
    std::cout << "clients_folder browse_name: " << browse_name.name() << "\n";

    if (clients_folder.id().isNull()) {
        std::cerr << "SystemInfoClient::addNodes: server TelemetryClients folder does not exist\n";
    }

    client_folder_ = opc_ua_utils::ensureClientFolder(
        client_, clients_folder.id(), client_name_, layout::kTelemetryNamespaceIndex);
}

bool SystemInfoClient::sendTelemetryPayload() {
    if (!client_folder_.has_value()) {
        std::cerr << "SystemInfoClient::sendTelemetryPayload: addNodes() was never called\n";
        return false;
    }

    const std::vector<TelemetrySnapshot> drained = queue_->drain();
    if (drained.empty()) {
        return true;
    }

    std::vector<opc_ua_utils::TelemetryStore> stores = buildTelemetryPayload(drained);

    for (size_t i = 0; i < drained.size(); ++i) {
        const std::string& device_key = drained[i].name;
        auto it = device_nodes_.find(device_key);
        if (it == device_nodes_.end()) {
            it = device_nodes_.emplace(
                device_key,
                opc_ua_utils::ensureSnapshotNode(
                    client_, *client_folder_, client_name_, device_key,
                    opc_ua_layout::kTelemetryNamespaceIndex, *sensor_dto_type_)
            ).first;
        }
        if (perf_logger_) perf_logger_->start("opc_ua_client_write");
        opc_ua_utils::writeSnapshot(client_, it->second, stores[i]);
        if (perf_logger_) perf_logger_->stop();
    }

    for (auto& store : stores) {
        opc_ua_utils::freeTelemetryStore(store, *sensor_dto_type_);
    }
    return true;
}
