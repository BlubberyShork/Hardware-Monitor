#include "HardwareDevice.h"
#include <iomanip>
#include <iostream>

namespace {
std::string vendorName(A_HardwareDevice::Vendor vendor) {
    switch (vendor) {
    case A_HardwareDevice::Vendor::NVIDIA: return "NVIDIA";
    case A_HardwareDevice::Vendor::AMD: return "AMD";
    case A_HardwareDevice::Vendor::INTEL: return "INTEL";
    }
    return "UNKNOWN";
}

std::string hardwareTypeName(A_HardwareDevice::HardwareType type) {
    switch (type) {
    case A_HardwareDevice::HardwareType::MOTHERBOARD: return "Motherboard";
    case A_HardwareDevice::HardwareType::CPU: return "CPU";
    case A_HardwareDevice::HardwareType::GPU: return "GPU";
    case A_HardwareDevice::HardwareType::PSU: return "PSU";
    case A_HardwareDevice::HardwareType::NETWORK: return "Network";
    case A_HardwareDevice::HardwareType::STORAGE: return "Storage";
    }
    return "Unknown";
}
}

TelemetrySnapshot A_HardwareDevice::snapshot() const {
    TelemetrySnapshot snapshot;
    snapshot.name = name;
    snapshot.vendor = vendorName(vendor);
    snapshot.hardware_type = hardwareTypeName(hw_type);
    snapshot.sensors.reserve(dev_sensors.size());
    for (const auto& sensor : dev_sensors) {
        snapshot.sensors.push_back({sensor->getName(), sensor->getValue(), sensor->getUnit()});
    }
    return snapshot;
}

void A_HardwareDevice::outputMetrics() const {
    OUTPUT_HEADER("Live GPU Metrics")
    std::cout << "-- " << name << " --\n";
    for (const auto& sensor : dev_sensors) {
        std::cout << std::left << std::setw(32) << sensor->getName()
            << sensor->getValue() << " " << sensor->getUnit() << "\n";
    }
    std::cout << "\n";
}
