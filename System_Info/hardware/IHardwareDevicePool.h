#pragma once

class IHardwareDevicePool {
public:
	virtual ~IHardwareDevicePool() = default;

    IHardwareDevicePool() = default;

    IHardwareDevicePool(IHardwareDevicePool&&) = default;
    IHardwareDevicePool& operator=(IHardwareDevicePool&&) = default;

    IHardwareDevicePool(const IHardwareDevicePool&) = delete;
    IHardwareDevicePool& operator=(const IHardwareDevicePool&) = delete;

	virtual void enumerateDevices() = 0;
};