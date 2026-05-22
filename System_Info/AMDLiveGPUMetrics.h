#pragma once
#include "LiveGPUHandler.h"
#include "GPULiveData.h"
#include <string>
#include <vector>
#include ".\..\third_party\adlx\SDK\ADLXHelper\Windows\Cpp\ADLXHelper.h"
#include ".\..\third_party\adlx\SDK\Include\ISystem.h"
#include ".\..\third_party\adlx\SDK\Include\IPerformanceMonitoring.h"
#include <adl_sdk.h> // ADL fallback
#include <adl_structures.h>

class AMDLiveGPUMetrics : public ILiveGPUMetrics {
public:
    AMDLiveGPUMetrics();
    ~AMDLiveGPUMetrics()       override;
    void fetchMetrics()        override;
    void outputMetrics() const override;

private:
    bool initialized = false;

    // Backend SDK selection
    enum class Backend { ADLX, ADL };
    Backend active_backend = Backend::ADLX;

    // ADLX state 
    ADLXHelper                                  adlx_helper;
    adlx::IADLXSystem* adlx_system = nullptr;
    adlx::IADLXPerformanceMonitoringServices*   perf_monitoring = nullptr;
    std::vector<adlx::IADLXGPUPtr>              gpus;
    std::vector<GPULiveData>                    gpu_live_data;

    void initSystem();
    void initPerfMonitoring();
    void enumerateGPUs();
    void fetchGPUMetrics(adlx::IADLXGPU* gpu, GPULiveData& live_data);

    // ADL fallback state
    HMODULE     adl_module = nullptr;
    int         adl_adapter_cnt = 0;

    // ADL function pointer typedefs
    typedef int (*ADL_MAIN_CONTROL_CREATE)          (ADL_MAIN_MALLOC_CALLBACK, int);
    typedef int (*ADL_MAIN_CONTROL_DESTROY)         ();
    typedef int (*ADL_ADAPTER_NUMBEROFADAPTERS_GET)(int*);
    typedef int (*ADL_ADAPTER_ADAPTERINFO_GET)      (LPAdapterInfo, int);
    typedef int (*ADL_ADAPTER_ACTIVE_GET)           (int, int*);
    typedef int (*ADL_OD5_TEMPERATURE_GET)          (int, int, ADLTemperature*);
    typedef int (*ADL_OD5_CURRENTACTIVITY_GET)      (int, ADLPMActivity*);
    typedef int (*ADL_OD5_FANSPEED_GET)             (int, int, ADLFanSpeedValue*);

    ADL_MAIN_CONTROL_CREATE          adl_main_control_create = nullptr;
    ADL_MAIN_CONTROL_DESTROY         adl_main_control_destroy = nullptr;
    ADL_ADAPTER_NUMBEROFADAPTERS_GET adl_adapter_number_get = nullptr;
    ADL_ADAPTER_ADAPTERINFO_GET      adl_adapter_info_get = nullptr;
    ADL_ADAPTER_ACTIVE_GET           adl_adapter_active_get = nullptr;
    ADL_OD5_TEMPERATURE_GET          adl_od5_temperature_get = nullptr;
    ADL_OD5_CURRENTACTIVITY_GET      adl_od5_currentactivity_get = nullptr;
    ADL_OD5_FANSPEED_GET             adl_od5_fanspeed_get = nullptr;

    // Adapter index list for active AMD adapters only
    std::vector<int>         adl_adapter_indices;
    std::vector<GPULiveData> adl_gpu_live_data;

    void        initADL();
    void        shutdownADL();
    void        enumerateADLAdapters();
    void        fetchADLGPUMetrics(int adapter_idx, GPULiveData& live_data);
    static void* __stdcall adlMallocCallback(int size);
};