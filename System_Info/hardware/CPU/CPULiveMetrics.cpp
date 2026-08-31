#include "CPULiveMetrics.h"

#include "../../driver_client/DriverClient.h"

#include <string>

CPULiveMetrics::CPULiveMetrics(DriverClient& driver_client)
    : A_HardwareDevice(Vendor::INTEL, HardwareType::CPU, "CPU")
    , driver_client_(driver_client) {}

void CPULiveMetrics::fetchMetrics() {
    for (const auto& cpu : driver_client_.runDriver()) {
        addSensor<Sensors::SensorType::TEMPERATURE>(
            "CPU " + std::to_string(cpu.cpu_id) + " Temperature",
            static_cast<float>(cpu.temp));
    }
}
