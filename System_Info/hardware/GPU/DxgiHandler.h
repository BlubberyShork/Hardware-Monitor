#pragma once
#include "dxgi.h"
#include <winnt.h>
#include "..\HardwareDevice.h"
#include "..\IHardwareDevicePool.h"
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

	struct Adapter {
		Vendor          vendor;
		LUID            luid;
		std::wstring    name;
		UINT            device_id;
		UINT            vendor_id;

	};

	DxgiHandler();
	~DxgiHandler() = default;

	void createGPUDevices();

private:
	std::vector<std::unique_ptr<IHardwareDevicePool>> pools;
	std::vector<Adapter> detected_adapters;
	void enumerateAdapters();
};

