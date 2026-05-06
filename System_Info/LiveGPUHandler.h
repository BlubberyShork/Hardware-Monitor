#pragma once
#include <dxgi.h>
#include <vector>
#include "GraphicsProcessor.h"
#include ".\..\third_party\nvapi\nvapi.h"

class LiveGPUHandler
{
private:
	enum class Vendor { AMD, NVIDIA, UNKNOWN } vendor = Vendor::UNKNOWN;
	std::vector<GPULiveData> all_gpu_live_data;

	void checkAndHandleError(NvAPI_Status status);

	// Temp helpers
	NvS32 populateThermalData(std::vector<NV_GPU_THERMAL_SETTINGS> v_thermal_settings);
	NvS32 avgTemp(NV_GPU_THERMAL_SETTINGS thermal_settings);
	NvS32 hotspotTemp(NV_GPU_THERMAL_SETTINGS thermal_settings);

	// Utilization helpers

	// Clock speed helpers

	// void AggregateData(std::vector<GPULiveData> live_data), or just put it all in fetchCurrent... Decide later

public:
	LiveGPUHandler();
	virtual ~LiveGPUHandler();

	void fetchCurrentLiveGPUMetrics();
};
