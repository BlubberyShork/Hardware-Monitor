#pragma once
#include <dxgi.h>
#include <vector>
#include <map>
#include "GraphicsProcessor.h"
#include ".\..\third_party\nvapi\nvapi.h"


class LiveGPUHandler
{
private:
	enum class Vendor { AMD, NVIDIA, UNKNOWN } vendor = Vendor::UNKNOWN;
	std::map<NvPhysicalGpuHandle, GPULiveData> all_gpu_live_data;

	void checkAndHandleError(const char* msg, NvAPI_Status status);

	// Temp helpers
	NvS32 avgTemp(NV_GPU_THERMAL_SETTINGS thermal_settings);
	NvS32 hotspotTemp(NV_GPU_THERMAL_SETTINGS thermal_settings);

	// NVIDIA execution
	void fetchLiveNvidiaGPUMetrics();

public:
	LiveGPUHandler();
	virtual ~LiveGPUHandler();

	void fetchCurrentLiveGPUMetrics();
	void outputLiveGPUMetrics();
};
