#pragma once
#include <map>
#include <stdexcept>
#include "..\HardwareDevice.h"
#include ".\..\GPULiveData.h"
#include ".\..\third_party\nvapi\nvapi.h"
//#include <winnt.h>

class NvidiaLiveGPUMetrics : public A_HardwareDevice {
public:
    NvidiaLiveGPUMetrics(LUID luid, std::wstring dxgi_name);
    ~NvidiaLiveGPUMetrics()    override;

    void fetchMetrics()        override;

private:
    LUID luid;
    std::wstring dxgi_name;

    void                    checkAndHandleError(const char* msg, NvAPI_Status status);
    int                     avgTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
    int                     hotspotTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
};