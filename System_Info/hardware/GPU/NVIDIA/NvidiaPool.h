#pragma once

#include <windows.h>
#include "..\..\..\..\third_party\nvapi\nvapi.h"
#include "NvidiaLiveGPUMetrics.h"
#include "..\..\IHardwareDevicePool.h"
#include <vector>

class NvidiaPool : public IHardwareDevicePool
{
public:
	NvidiaPool();
	~NvidiaPool() override;

	void enumerateDevices() override;

    const std::vector<std::unique_ptr<A_HardwareDevice>>& getDevices() const override { return devices_; }

private:
	std::vector<std::unique_ptr<A_HardwareDevice>> devices_;
};

