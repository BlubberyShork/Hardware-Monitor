#pragma once

class IHardwareDevicePool {
public:
	virtual ~IHardwareDevicePool() = default;
	virtual void enumerateDevices() = 0;
};