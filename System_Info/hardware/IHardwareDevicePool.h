#pragma once

#include <memory>
#include <vector>

class A_HardwareDevice;

class IHardwareDevicePool {
public:
	virtual ~IHardwareDevicePool() = default;

    IHardwareDevicePool() = default;

    IHardwareDevicePool(IHardwareDevicePool&&) = default;
    IHardwareDevicePool& operator=(IHardwareDevicePool&&) = default;

    IHardwareDevicePool(const IHardwareDevicePool&) = delete;
    IHardwareDevicePool& operator=(const IHardwareDevicePool&) = delete;

	virtual void enumerateDevices() = 0;
	virtual const std::vector<std::unique_ptr<A_HardwareDevice>>& getDevices() const = 0;
};
