#pragma once
#include <map>
#include <stdexcept>
#include "LiveGPUHandler.h"
#include "GPULiveData.h"
#include ".\..\third_party\nvapi\nvapi.h"

class NvidiaLiveGPUMetrics : public ILiveGPUMetrics {
public:
    NvidiaLiveGPUMetrics();
    ~NvidiaLiveGPUMetrics()    override;

    void fetchMetrics()        override;
    void outputMetrics() const override;

private:
    std::map<NvPhysicalGpuHandle, GPULiveData> all_gpu_live_data;

    static GPUClockDomain   toClockDomain(NV_GPU_PUBLIC_CLOCK_ID nv_id);
    void                    checkAndHandleError(const char* msg, NvAPI_Status status);
    int                     avgTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
    int                     hotspotTemp(const NV_GPU_THERMAL_SETTINGS& ts) const;
};