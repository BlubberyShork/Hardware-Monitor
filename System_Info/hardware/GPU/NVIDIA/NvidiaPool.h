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

private:
	std::vector<std::unique_ptr<NvidiaLiveGPUMetrics>> nvidia_adapters;
};

