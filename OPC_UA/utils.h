#pragma once
 
#include "ClientQueue.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <open62541pp/types.hpp>
#include <open62541pp/datatype.hpp>
#include <open62541pp/node.hpp>
#include <open62541pp/client.hpp>
#include <open62541pp/server.hpp>
 
namespace opc_ua_utils {
 
struct SensorDto {
    opcua::String     name;
    float             value{};
    opcua::ByteString unit;
    opcua::String     sensor_type{};
};
 
// In-memory container for one drained device snapshot, built from
// TelemetrySnapshot/SensorSnapshot before writing to the server.
struct TelemetryStore {
    opcua::String           name;
    opcua::String           vendor;
    opcua::String           hardware_type;
    size_t                  dev_sensors_size{0};
    opcua::ExtensionObject *dev_sensors{nullptr};
};
 
opcua::DataType buildSensorDtoType(uint16_t ns);
opcua::DataType registerSensorDtoType(opcua::Server& server, uint16_t ns);
 
// Deep-copies snapshot's fields. snapshot is read-only and must survive
// this call unmodified (it's a point-in-time hardware snapshot, not a
// resource to consume).
TelemetryStore buildTelemetryStore(
    const TelemetrySnapshot& snapshot,
    const opcua::DataType& sensorDtoType);
 
void freeTelemetryStore(TelemetryStore& store, const opcua::DataType& sensorDtoType);
 
// NodeIds for one device's TelemetrySnapshot subtree:
//   <ClientFolder>/<deviceKey> (Object)
//     name / vendor / hardware_type (String variables)
//     sensors (array variable, DataType: SensorDto)
struct SnapshotNodeIds {
    opcua::NodeId object;
    opcua::NodeId name;
    opcua::NodeId vendor;
    opcua::NodeId hardware_type;
    opcua::NodeId sensors;
};
 
// Browses for this client's folder under telemetryClientsFolder; creates it
// via AddNodes otherwise.
opcua::NodeId ensureClientFolder(
    opcua::Client& client,
    const opcua::NodeId& telemetryClientsFolder,
    std::string_view deviceName,
    uint16_t ns);
 
// Browses for deviceKey's subtree under clientFolder; creates it via AddNodes
// otherwise. Caller should cache the result per deviceKey rather than call
// this every write.
SnapshotNodeIds ensureSnapshotNode(
    opcua::Client& client,
    const opcua::NodeId& clientFolder,
    std::string_view deviceName,
    std::string_view deviceKey,
    uint16_t ns,
    const opcua::DataType& sensorDtoType);
 
// Writes store's fields into an already-created SnapshotNodeIds. Do not free
// store until after this returns.
void writeSnapshot(
    opcua::Client& client,
    const SnapshotNodeIds& ids,
    const TelemetryStore& store);
 
struct DecodedSensor {
    std::string name;
    float       value{};
    std::string unit;
    std::string sensor_type;
};

struct DecodedDevice {
    std::string name;
    std::string vendor;
    std::string hardware_type;
    std::vector<DecodedSensor> sensors;
};

inline std::string readStringNode(opcua::Client& client, const opcua::NodeId& id) {
    opcua::Node<opcua::Client> node(client, id);
    opcua::Variant val = node.readValue();
    const auto* s = static_cast<const UA_String*>(val.data());
    if (s && s->data && s->length > 0) {
        return std::string(reinterpret_cast<const char*>(s->data), s->length);
    }
    return {};
}

inline DecodedDevice readDeviceInfo(
    opcua::Client& client,
    const SnapshotNodeIds& ids) {
    DecodedDevice dev;
    dev.name          = readStringNode(client, ids.name);
    dev.vendor        = readStringNode(client, ids.vendor);
    dev.hardware_type = readStringNode(client, ids.hardware_type);
    return dev;
}

inline std::vector<DecodedSensor> decodeSensorDtos(
    const opcua::Variant& value,
    const opcua::DataType& sensorDtoType) {
    std::vector<DecodedSensor> result;

    if (!value.data()) {
        return result;
    }

    const UA_DataType& dt = *sensorDtoType.handle();
    const size_t count = value.arrayLength();

    auto read_ua_string = [](const uint8_t* base, size_t off) -> std::string {
        const auto* s = reinterpret_cast<const UA_String*>(base + off);
        if (s->data && s->length > 0) {
            return std::string(reinterpret_cast<const char*>(s->data), s->length);
        }
        return {};
    };

    auto extractSensor = [&](const uint8_t* raw) -> DecodedSensor {
        DecodedSensor sensor;
        size_t off = dt.members[0].padding;
        sensor.name = read_ua_string(raw, off);

        off += sizeof(UA_String) + dt.members[1].padding;
        sensor.value = *reinterpret_cast<const float*>(raw + off);

        off += sizeof(UA_Float) + dt.members[2].padding;
        sensor.unit = read_ua_string(raw, off);

        off += sizeof(UA_ByteString) + dt.members[3].padding;
        sensor.sensor_type = read_ua_string(raw, off);
        return sensor;
    };

    if (value.isType(dt)) {
        const auto* raw = static_cast<const uint8_t*>(value.data());
        for (size_t i = 0; i < count; ++i) {
            result.push_back(extractSensor(raw + i * dt.memSize));
        }
        return result;
    }

    const auto* ext_array = static_cast<const UA_ExtensionObject*>(value.data());
    for (size_t i = 0; i < count; ++i) {
        const UA_ExtensionObject& ext = ext_array[i];

        if (ext.encoding == UA_EXTENSIONOBJECT_DECODED ||
            ext.encoding == UA_EXTENSIONOBJECT_DECODED_NODELETE) {
            const auto* raw = static_cast<const uint8_t*>(ext.content.decoded.data);
            if (!raw) continue;
            result.push_back(extractSensor(raw));
        } else if (ext.encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING) {
            void* decoded = UA_new(&dt);
            if (!decoded) continue;
            UA_StatusCode ret = UA_decodeBinary(
                &ext.content.encoded.body, decoded, &dt, nullptr);
            if (ret == UA_STATUSCODE_GOOD) {
                result.push_back(extractSensor(static_cast<const uint8_t*>(decoded)));
            }
            UA_delete(decoded, &dt);
        }
    }

    return result;
}

} // namespace opc_ua_utils
