#pragma once
#include "dxgi.h"
#include <winnt.h>
#include "..\HardwareDevice.h"
#include "..\IHardwareDevicePool.h"
#include <set>
#include <stdexcept>

class DxgiHandler
{
public:
	enum class Vendor {
		NVIDIA,
		AMD,
		INTEL,
		UNKNOWN
	};

	DxgiHandler();
	~DxgiHandler() = default;

	void createGPUDevices();

private:
	std::vector<std::unique_ptr<IHardwareDevicePool>> pools;

	// Since we use pools, we only need to detect which vendors exist, we need one pool per vendor
	std::set<Vendor> detected_vendors;

	void enumerateAdapters();
};

