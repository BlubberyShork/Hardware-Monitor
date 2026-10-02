#include "CPULiveMetrics.h"

#include "../../driver_client/DriverClient.h"

#include <string>

// TODO -> Return the actual hardware name
CPULiveMetrics::CPULiveMetrics(DriverClient& driver_client)
    : A_HardwareDevice(Vendor::INTEL, HardwareType::CPU, "CPU")
    , driver_client_(driver_client) {}

void CPULiveMetrics::fetchMetrics() {
    for (const auto& entry : driver_client_.runDriver()) {
        std::string name(entry.name);
        float value = static_cast<float>(entry.value);

        if (entry.unit == UNIT_MILLIWATTS)
            value /= 1000.0f;
        else if (entry.unit == UNIT_MILLIVOLTS)
            value /= 1000.0f;

        switch (entry.type) {
        case METRIC_TEMP:
            addSensor<Sensors::SensorType::TEMPERATURE>(name, value);
            break;
        case METRIC_VOLTAGE:
            addSensor<Sensors::SensorType::VOLTAGE>(name, value);
            break;
        case METRIC_CLOCK_SPEED:
            addSensor<Sensors::SensorType::CLOCK>(name, value);
            break;
        case METRIC_LOAD:
            addSensor<Sensors::SensorType::USAGE>(name, value);
            break;
        case METRIC_POWER:
            addSensor<Sensors::SensorType::POWER>(name, value);
            break;
        default:
            break;
        }
    }
}
