#include "HardwareDevice.h"
#include <iomanip>
#include <iostream>

void A_HardwareDevice::outputMetrics() const {
    OUTPUT_HEADER("Live GPU Metrics")

    std::cout << "-- " << name << " --\n";
    for (const auto& sensor : dev_sensors) {
        std::cout << std::left << std::setw(32) << sensor->getName()
            << sensor->getValue() << " " << sensor->getUnit() << "\n";
    }
    std::cout << "\n";
}