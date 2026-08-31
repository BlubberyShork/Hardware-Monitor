#include "../System_Info/hardware/DeviceSensor.h"

#include <open62541pp/types.hpp>
#include <open62541pp/datatype.hpp>

/** 
 * TODO - Documentation
 * */
namespace opc_ua_utils {
    struct TelemetryStore {
        opcua::String           name;
        opcua::String           vendor;
        opcua::String           hardware_type;
        size_t                  dev_sensors_size{0};   // must precede the array member
        opcua::ExtensionObject *dev_sensors{nullptr};  // array of ExtensionObject
};

struct SensorDto {
    opcua::String     name;
    float             value{};
    opcua::ByteString unit;
    opcua::String     sensor_type{}; // Sensors::SensorType as int32
};

// DataType construction
opcua::DataType buildSensorDtoType(uint16_t ns);
opcua::DataType buildTelemetryStoreType(uint16_t ns);

// Read-only construction/update from existing sensor data
TelemetryStore buildTelemetryStore(
    const opcua::String& name,
    const opcua::String& vendor,
    const opcua::String& hardware_type,
    const std::vector<std::unique_ptr<Sensors::IDeviceSensor>>& sensors,
    const opcua::DataType& sensorDtoType);

void updateSensors(
    TelemetryStore& store,
    const std::vector<std::unique_ptr<Sensors::IDeviceSensor>>& sensors,
    const opcua::DataType& sensorDtoType);

void freeTelemetryStore(TelemetryStore& store, const opcua::DataType& sensorDtoType);
} // namespace opc_ua_utils
