#include "utils.h"
#include "shared_opc_ua_layout.h"

#include <iostream>

namespace opc_ua_utils {
namespace {
// TODO -> Use 
template <typename Fn>
static auto addNodeGuarded(const std::string& what, Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const opcua::BadStatus& e) {
        std::cerr << what << ": status=0x" << std::hex << e.code() << std::dec << "\n";
        throw;
    }
}

} // namespace
  
opcua::DataType buildSensorDtoType(uint16_t ns) {
    return opcua::DataTypeBuilder<SensorDto>::createStructure(
            "SensorDto",
            opcua::NodeId(ns, "SensorDto"),
            opcua::NodeId(ns, "SensorDto_Encoding_Default"))
        .addField<&SensorDto::name>("name")
        .addField<&SensorDto::value>("value")
        .addField<&SensorDto::unit>("unit")
        .addField<&SensorDto::sensor_type>("sensor_type")
        .build();
}
 
opcua::DataType registerSensorDtoType(opcua::Server& server, uint16_t ns) {
    opcua::DataType type = buildSensorDtoType(ns);
    server.config().addCustomDataTypes({type});
    return type;
}
 
TelemetryStore buildTelemetryStore(
    const TelemetrySnapshot& snapshot,
    const opcua::DataType& sensorDtoType
) {
    TelemetryStore store{};
    store.name          = opcua::String(snapshot.name);
    store.vendor        = opcua::String(snapshot.vendor);
    store.hardware_type = opcua::String(snapshot.hardware_type);
 
    const size_t count = snapshot.sensors.size();
    auto* arr = static_cast<UA_ExtensionObject*>(
        UA_Array_new(count, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]));
 
    for (size_t i = 0; i < count; ++i) {
        const SensorSnapshot& sensor = snapshot.sensors[i];
 
        SensorDto dto{};
        dto.name  = opcua::String(sensor.name);
        dto.value = sensor.value;
        dto.unit  = opcua::ByteString(std::string_view(sensor.unit));
 
        auto* decoded = static_cast<SensorDto*>(UA_new(sensorDtoType.handle()));
        UA_copy(&dto, decoded, sensorDtoType.handle());
 
        arr[i].encoding             = UA_EXTENSIONOBJECT_DECODED;
        arr[i].content.decoded.type = sensorDtoType.handle();
        arr[i].content.decoded.data = decoded;
    }
 
    store.dev_sensors      = reinterpret_cast<opcua::ExtensionObject*>(arr);
    store.dev_sensors_size = count;
    return store;
}
 
void freeTelemetryStore(TelemetryStore& store, const opcua::DataType& sensorDtoType) {
    if (store.dev_sensors != nullptr) {
        UA_Array_delete(store.dev_sensors, store.dev_sensors_size, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
        store.dev_sensors = nullptr;
        store.dev_sensors_size = 0;
    }
}
 
static std::optional<opcua::Node<opcua::Client>> tryBrowseChild(
    opcua::Node<opcua::Client>& parent,
    uint16_t ns,
    std::string_view name
) {
    try {
        return parent.browseChild({{ns, std::string(name)}});
    } catch (const opcua::BadStatus& e) {
        if (e.code() == UA_STATUSCODE_BADNOMATCH) {
            return std::nullopt;
        }
        throw;
    }
}
 
opcua::NodeId ensureClientFolder(
    opcua::Client& client,
    const opcua::NodeId& telemetryClientsFolder,
    std::string_view deviceName,
    uint16_t ns
) {
    namespace layout = opc_ua_layout;
 
    opcua::Node<opcua::Client> clients_folder(client, telemetryClientsFolder);
 
    auto existing = tryBrowseChild(clients_folder, ns, deviceName);
    if (existing.has_value()) {
        return existing->id();
    }
 
    opcua::NodeId folder_id(ns, layout::clientFolderId(deviceName));
    return clients_folder.addFolder(folder_id, deviceName).id();
}
 
// Browses for `field` under `parent`; creates a scalar String variable via
// AddNodes otherwise.
static opcua::NodeId ensureStringVar(
    opcua::Node<opcua::Client>& parent,
    std::string_view deviceName,
    std::string_view deviceKey,
    std::string_view field,
    uint16_t ns
) {
    namespace layout = opc_ua_layout;
 
    auto existing = tryBrowseChild(parent, ns, field);
    if (existing.has_value()) {
        return existing->id();
    }
 
    opcua::NodeId id(ns, layout::snapshotFieldId(deviceName, deviceKey, field));
    opcua::VariableAttributes attr;
    attr.setAccessLevel(opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite);
    attr.setDataType(opcua::DataTypeId::String);

    return parent.addVariable(id, std::string(field), attr).id();
}
 
SnapshotNodeIds ensureSnapshotNode(
    opcua::Client& client,
    const opcua::NodeId& clientFolder,
    std::string_view deviceName,
    std::string_view deviceKey,
    uint16_t ns,
    const opcua::DataType& sensorDtoType
) {
    namespace layout = opc_ua_layout;
    opcua::Node<opcua::Client> folder(client, clientFolder);
 
    std::optional<opcua::Node<opcua::Client>> obj = tryBrowseChild(folder, ns, deviceKey);
    if (!obj.has_value()) {
        opcua::NodeId obj_id(ns, layout::snapshotObjectId(deviceName, deviceKey));
        obj = folder.addObject(obj_id, std::string(deviceKey));
    }
 
    SnapshotNodeIds ids;
    ids.object        = obj->id();
    ids.name          = ensureStringVar(*obj, deviceName, deviceKey, "name", ns);
    ids.vendor        = ensureStringVar(*obj, deviceName, deviceKey, "vendor", ns);
    ids.hardware_type = ensureStringVar(*obj, deviceName, deviceKey, "hardware_type", ns);
 
    auto existing_sensors = tryBrowseChild(*obj, ns, "sensors");
    if (existing_sensors.has_value()) {
        ids.sensors = existing_sensors->id();
    } else {
        opcua::NodeId sensors_id(ns, layout::snapshotFieldId(deviceName, deviceKey, "sensors"));

        opcua::VariableAttributes attr;
        attr.setAccessLevel(opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite);
        attr.setDataType(sensorDtoType.typeId());
        attr.setValueRank(opcua::ValueRank::OneDimension);
        attr.setArrayDimensions({0});

        ids.sensors = obj->addVariable(sensors_id, "sensors", attr).id();    
    }
 
    return ids;
}
 
void writeSnapshot(
    opcua::Client& client,
    const SnapshotNodeIds& ids,
    const TelemetryStore& store
) {
    opcua::Node<opcua::Client>(client, ids.name).writeValue(opcua::Variant(store.name));
    opcua::Node<opcua::Client>(client, ids.vendor).writeValue(opcua::Variant(store.vendor));
    opcua::Node<opcua::Client>(client, ids.hardware_type).writeValue(opcua::Variant(store.hardware_type));
    opcua::Node<opcua::Client>(client, ids.sensors).writeValue(opcua::Variant(
        opcua::Span<opcua::ExtensionObject>(store.dev_sensors, store.dev_sensors_size)));
}
 
} // namespace opc_ua_utils
 
