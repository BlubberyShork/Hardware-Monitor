#pragma once

#include "../HardwareDevice.h"

class DriverClient;

class CPULiveMetrics final : public A_HardwareDevice {
public:
    explicit CPULiveMetrics(DriverClient& driver_client);
    void fetchMetrics() override;

private:
    DriverClient& driver_client_;
};
