#include "utils.h"

namespace opc_ua_utils {
    inline opcua::DataType buildSensorDtoType(uint16_t ns) {
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

    inline opcua::DataType buildTelemetryStoreType(uint16_t ns) {
        return opcua::DataTypeBuilder<TelemetryStore>::createStructure(
                "TelemetryStore",
                opcua::NodeId(ns, "TelemetryStore"),
                opcua::NodeId(ns, "TelemetryStore_Encoding_Default"))
        .addField<&TelemetryStore::name>("name")
        .addField<&TelemetryStore::vendor>("vendor")
        .addField<&TelemetryStore::hardware_type>("hardware_type")
        .addField<&TelemetryStore::dev_sensors>("dev_sensors")
        .build();
    }

    TelemetryStore buildTelemetryStore(
        const opcua::String& name,
        const opcua::String& vendor,
        const opcua::String& hardware_type,
        const std::vector<std::unique_ptr<Sensors::IDeviceSensor>>& sensors,
        const opcua::DataType& sensorDtoType
    ) {
        TelemetryStore store{};
        store.name          = name;
        store.vendor        = vendor;
        store.hardware_type = hardware_type;
        updateSensors(store, sensors, sensorDtoType);
        return store;
    }

    namespace {
        opcua::String sensorTypeToString(Sensors::SensorType type) {
            switch (type) {
                case Sensors::SensorType::CLOCK:       return opcua::String("CLOCK");
                case Sensors::SensorType::USAGE:       return opcua::String("USAGE");
                case Sensors::SensorType::TEMPERATURE: return opcua::String("TEMPERATURE");
                case Sensors::SensorType::POWER:       return opcua::String("POWER");
                case Sensors::SensorType::MEMORY:      return opcua::String("MEMORY");
                case Sensors::SensorType::VOLTAGE:     return opcua::String("VOLTAGE");
                case Sensors::SensorType::FAN_SPEED:   return opcua::String("FAN_SPEED");
            }
            return opcua::String("UNKNOWN");
        }
    } // local private helper namespace

    void updateSensors(
        TelemetryStore& store,
        const std::vector<std::unique_ptr<Sensors::IDeviceSensor>>& sensors,
        const opcua::DataType& sensorDtoType
    ) {
        // Release the existing array before rebuilding it
        if (store.dev_sensors != nullptr) {
            UA_Array_delete(store.dev_sensors, store.dev_sensors_size, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
            store.dev_sensors = nullptr;
            store.dev_sensors_size = 0;
        }

        const size_t count = sensors.size();
        auto* arr = static_cast<UA_ExtensionObject*>(
            UA_Array_new(count, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]));

        for (size_t i = 0; i < count; ++i) {
            const auto& sensor = sensors[i];

            SensorDto dto{};
            dto.name        = opcua::String(sensor->getName());
            dto.value       = sensor->getValue();
            dto.unit        = opcua::ByteString(sensor->getUnit());
            dto.sensor_type = opc_ua_utils::sensorTypeToString(sensor->getType());

            auto* decoded = static_cast<SensorDto*>(UA_new(sensorDtoType.handle()));
            UA_copy(&dto, decoded, sensorDtoType.handle());

            arr[i].encoding             = UA_EXTENSIONOBJECT_DECODED;
            arr[i].content.decoded.type = sensorDtoType.handle();
            arr[i].content.decoded.data = decoded;
        }

        store.dev_sensors      = reinterpret_cast<opcua::ExtensionObject*>(arr);
        store.dev_sensors_size = count;
    }

    void freeTelemetryStore(TelemetryStore& store, const opcua::DataType& sensorDtoType) {
        if (store.dev_sensors != nullptr) {
            UA_Array_delete(
                store.dev_sensors, store.dev_sensors_size, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
            store.dev_sensors = nullptr;
            store.dev_sensors_size = 0;
        }
    }
} // Namespace opc_ua_utils
