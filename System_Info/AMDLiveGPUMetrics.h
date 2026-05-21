#pragma once
#include "LiveGPUHandler.h"
#include "GPULiveData.h"
#include <string>
#include ".\..\third_party\adlx\SDK\ADLXHelper\Windows\Cpp\ADLXHelper.h"
#include ".\..\third_party\adlx\SDK\Include\ISystem.h"
#include ".\..\third_party\adlx\SDK\Include\IPerformanceMonitoring.h"

class AMDLiveGPUMetrics : public ILiveGPUMetrics {
public:
    AMDLiveGPUMetrics();
    ~AMDLiveGPUMetrics() override;

    void fetchMetrics()        override;
    void outputMetrics() const override;

private:
    ADLXHelper adlx_helper;
    adlx::IADLXSystem* adlx_system = nullptr;
    adlx::IADLXPerformanceMonitoringServices* perf_monitoring = nullptr;

    std::vector<adlx::IADLXGPUPtr> gpus;
    std::vector<GPULiveData>       gpu_live_data;

    void initSystem();
    void initPerfMonitoring();
    void enumerateGPUs();

    void fetchGPUMetrics(adlx::IADLXGPU* gpu, GPULiveData& live_data);
};