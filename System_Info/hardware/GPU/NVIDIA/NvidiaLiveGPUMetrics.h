#pragma once
#include "..\..\HardwareDevice.h"
#include ".\..\..\..\third_party\nvapi\nvapi.h"
//#include <winnt.h>

class NvidiaLiveGPUMetrics : public A_HardwareDevice {
public:
    NvidiaLiveGPUMetrics(NvPhysicalGpuHandle& handle);
    ~NvidiaLiveGPUMetrics()    override;

    void fetchMetrics()        override;

private:
    NvPhysicalGpuHandle _handle;

    void checkAndHandleError(const char* msg, NvAPI_Status status);
    int avgTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
    int hotspotTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
};