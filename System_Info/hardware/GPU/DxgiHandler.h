#pragma once
#include "dxgi.h"
#include <winnt.h>
#include "..\HardwareDevice.h"
#include <stdexcept>
#include "NvidiaLiveGPUMetrics.h"
#include "AMDLiveGPUMetrics.h"

class DxgiHandler
{
public:
	enum class Vendor {
		NVIDIA,
		AMD,
		INTEL,
		UNKNOWN
	};

	struct Adapter {
		Vendor          vendor;
		LUID            luid;
		std::wstring    name;
		UINT            device_id;
		UINT            vendor_id;

	};

	DxgiHandler();
	~DxgiHandler() = default;

	std::vector<std::unique_ptr<A_HardwareDevice>> createGPUDevices();

private:
	std::vector<Adapter> detected_adapters;
	void enumerateAdapters();
};

