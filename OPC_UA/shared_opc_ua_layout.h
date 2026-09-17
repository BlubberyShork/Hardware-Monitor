#pragma once
 
#include <cstdint>
#include <string>
#include <string_view>
 
namespace opc_ua_layout {
 
constexpr uint16_t kTelemetryNamespaceIndex = 1; // default server namespace
 
constexpr const char* kTelemetryClientsFolderName = "TelemetryClients";
 
// ObjectsFolder/TelemetryClients/<device_name>
inline std::string clientFolderId(std::string_view device_name) {
    return std::string(device_name);
}
 
// .../<device_name>/<device_key>  (one TelemetrySnapshot Object per device)
inline std::string snapshotObjectId(std::string_view device_name, std::string_view device_key) {
    return std::string(device_name) + "." + std::string(device_key);
}
 
// .../<device_name>/<device_key>/<field>
inline std::string snapshotFieldId(std::string_view device_name, std::string_view device_key, std::string_view field) {
    return std::string(device_name) + "." + std::string(device_key) + "." + std::string(field);
}
 
} // namespace opc_ua_layout
 
