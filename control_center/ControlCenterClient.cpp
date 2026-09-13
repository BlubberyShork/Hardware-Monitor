#include "ControlCenterClient.h"

#include <iostream>

namespace layout = opc_ua_layout;

ControlCenterClient::ControlCenterClient(std::string_view client_name,
                                          std::filesystem::path project_root,
                                          TelemetrySink sink)
    : CustomClient(client_name, std::move(project_root)), sink_(std::move(sink)) {}

void ControlCenterClient::start() {
    sensor_dto_type_ = opc_ua_utils::buildSensorDtoType(layout::kTelemetryNamespaceIndex);
    subscription_.emplace(native());
    discoverAndSubscribe();
    last_poll_ = std::chrono::steady_clock::now();
}

void ControlCenterClient::discoverAndSubscribe() {
    opcua::Node objects_folder(client_, opcua::ObjectId::ObjectsFolder);

    opcua::Node<opcua::Client> clients_folder = objects_folder.browseChild(
        {{layout::kTelemetryNamespaceIndex, layout::kTelemetryClientsFolderName}});
    if (clients_folder.id().isNull()) {
        return;
    }

    for (auto& client_folder : clients_folder.browseChildren()) {
        const std::string client_folder_name(client_folder.readBrowseName().name());

        for (auto& device_node : client_folder.browseChildren()) {
            const std::string device_node_id = std::string(device_node.id().toString());

            if (device_cache_.count(device_node_id) != 0) {
                continue;
            }

            opcua::Node<opcua::Client> sensors_node{client_, opcua::NodeId{0, 0}};
            opc_ua_utils::SnapshotNodeIds ids{};
            ids.object = device_node.id();

            bool found_sensors = false;
            for (auto& child : device_node.browseChildren()) {
                const std::string child_name(child.readBrowseName().name());
                if (child_name == "name") {
                    ids.name = child.id();
                } else if (child_name == "vendor") {
                    ids.vendor = child.id();
                } else if (child_name == "hardware_type") {
                    ids.hardware_type = child.id();
                } else if (child_name == "sensors") {
                    ids.sensors = child.id();
                    sensors_node = child;
                    found_sensors = true;
                }
            }

            if (!found_sensors) {
                continue;
            }

            DeviceCache cache;
            cache.client_folder_name = client_folder_name;
            cache.info = opc_ua_utils::readDeviceInfo(client_, ids);

            const std::string device_display_name = cache.info.name;
            device_cache_[device_node_id] = std::move(cache);

            subscription_->subscribeDataChange(
                sensors_node.id(),
                opcua::AttributeId::Value,
                [this, device_node_id](opcua::IntegerId, opcua::IntegerId, const opcua::DataValue& dv) {
                    handleSensorUpdate(device_node_id, dv.value());
                });

            std::cout << "[" << clientName() << "] subscribed to "
                      << client_folder_name << "/" << device_display_name << "\n";
        }
    }
}

void ControlCenterClient::handleSensorUpdate(const std::string& device_node_id,
                                              const opcua::Variant& value) {
    auto it = device_cache_.find(device_node_id);
    if (it == device_cache_.end()) {
        return;
    }

    const DeviceCache& cache = it->second;
    const auto sensors = opc_ua_utils::decodeSensorDtos(value, sensor_dto_type_);

    ClientRow row;
    row.fields.emplace_back(
        cache.info.name + " (" + cache.info.hardware_type + ")", "");

    for (const auto& sensor : sensors) {
        std::string val_str = std::to_string(sensor.value);
        if (!sensor.unit.empty()) {
            val_str += " " + sensor.unit;
        }
        row.fields.emplace_back("  " + sensor.name, val_str);
    }

    sink_(cache.client_folder_name, std::move(row));
}

void ControlCenterClient::tick(std::chrono::milliseconds io_timeout,
                                std::chrono::milliseconds poll_interval) {
    client_.runIterate(static_cast<uint16_t>(io_timeout.count()));

    const auto now = std::chrono::steady_clock::now();
    if (now - last_poll_ >= poll_interval) {
        discoverAndSubscribe();
        last_poll_ = now;
    }
}
