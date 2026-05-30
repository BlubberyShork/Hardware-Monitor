#pragma once
#include "..\HardwareDevice.h"
#include ".\..\GPULiveData.h"
#include <string>
#include <vector>
#include ".\..\third_party\adlx\SDK\ADLXHelper\Windows\Cpp\ADLXHelper.h"
#include ".\..\third_party\adlx\SDK\Include\ISystem.h"
#include ".\..\third_party\adlx\SDK\Include\IPerformanceMonitoring.h"
#include <adl_sdk.h> // ADL fallback
#include <adl_structures.h>

class AMDLiveGPUMetrics : public A_HardwareDevice {
public:
    AMDLiveGPUMetrics(LUID luid, std::wstring dxgi_name){}
    ~AMDLiveGPUMetrics() override;

    void fetchMetrics() override;

private:
    LUID luid;
    std::wstring dxgi_name;

// ============================================================
// ADLX
// ============================================================
    bool initialized = false;

    // Backend SDK selection
    enum class Backend { ADLX, ADL };
    Backend active_backend = Backend::ADL;

    // ADLX state 
    ADLXHelper                                  adlx_helper;
    adlx::IADLXSystem* adlx_system = nullptr;
    adlx::IADLXPerformanceMonitoringServices*   perf_monitoring = nullptr;
    std::vector<adlx::IADLXGPUPtr>              gpus;

    void initSystem();
    void initPerfMonitoring();
    void enumerateGPUs();
    void fetchGPUMetrics(adlx::IADLXGPU* gpu);

// ============================================================
// ADL
// ============================================================
    // ADL fallback state
    ADL_CONTEXT_HANDLE  _context            = nullptr;
    HMODULE             adl_module          = nullptr;
    int                 adl_adapter_cnt     = 0;
    int                 overdrive_version   = 0;
    int                 overdrive_supported = 0;
    int                 overdrive_enabled   = 0;
   
    // Adapter index list for active AMD adapters only
    std::vector<int>         adl_adapter_indices;

    // ============================================================
    // Typedefs
    // 
    //   Experiment with adding:
    //      PowerControl functions
    //      Voltage/Voltage Control
    // ============================================================
    // Context / lifecycle
    typedef int (*ADL2_MAIN_CONTROL_CREATE)              (ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE);
    typedef int (*ADL2_MAIN_CONTROL_DESTROY)             (ADL_CONTEXT_HANDLE);

    // Adapter enumeration -> Adapter funcs are not gated behind any particular Overdrive version, use their own adapter_..._caps funcs to check support
    typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)     (ADL_CONTEXT_HANDLE, int*);
    typedef int (*ADL2_ADAPTER_ADAPTERINFO_GET)          (ADL_CONTEXT_HANDLE, LPAdapterInfo, int);
    typedef int (*ADL2_ADAPTER_ID_GET)                   (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_ADAPTER_ACTIVE_GET)               (ADL_CONTEXT_HANDLE, int, int*);

    // Memory
    typedef int (*ADL2_ADAPTER_VRAMUSAGE_GET)            (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET)   (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_ADAPTER_MEMORYINFOX4_GET)         (ADL_CONTEXT_HANDLE, int, ADLMemoryInfoX4, ADLMemoryInfoX4*);

    // Frame metrics
    typedef int (*ADL2_ADAPTER_FRAMEMETRICS_CAPS)        (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_ADAPTER_FRAMEMETRICS_GET)         (ADL_CONTEXT_HANDLE, int, int, float*);
    typedef int (*ADL2_ADAPTER_FRAMEMETRICS_START)       (ADL_CONTEXT_HANDLE, int, int);
    typedef int (*ADL2_ADAPTER_FRAMEMETRICS_STOP)        (ADL_CONTEXT_HANDLE, int, int);

    // Overdrive caps / version
    typedef int (*ADL2_OVERDRIVE_CAPS)                   (ADL_CONTEXT_HANDLE, int, int*, int*, int*); // returns od enabled & supported truth values

    // Overdrive 5
    typedef int (*ADL2_OD5_ODPARAMETERS_GET)             (ADL_CONTEXT_HANDLE, int, ADLODParameters*);   // used to check for curractivity_get. Also contains engine/mem clk ranges
    typedef int (*ADL2_OD5_CURRENTACTIVITY_GET)          (ADL_CONTEXT_HANDLE, int, ADLPMActivity*);
    typedef int (*ADL2_OD5_TEMPERATURE_GET)              (ADL_CONTEXT_HANDLE, int, int, ADLTemperature*);   
    typedef int (*ADL2_OD5_FANSPEED_GET)                 (ADL_CONTEXT_HANDLE, int, int, ADLFanSpeedValue*); // curr fan speed
    typedef int (*ADL2_OD5_FANSPEEDINFO_GET)             (ADL_CONTEXT_HANDLE, int, int, ADLFanSpeedInfo*);  // contains min/max

    // Overdrive 6
    typedef int (*ADL2_OD6_CAPABILITIES_GET)             (ADL_CONTEXT_HANDLE, int, ADLOD6Capabilities*);
    typedef int (*ADL2_OD6_CURRENTPOWER_GET)             (ADL_CONTEXT_HANDLE, int, int, int*);
    typedef int (*ADL2_OD6_TEMPERATURE_GET)              (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_OD6_CURRENTSTATUS_GET)            (ADL_CONTEXT_HANDLE, int, ADLOD6CurrentStatus*);
    typedef int (*ADL2_OD6_FANSPEED_GET)                 (ADL_CONTEXT_HANDLE, int, ADLOD6FanSpeedInfo*);

    // Overdrive N
    typedef int (*ADL2_OVERDRIVEN_TEMPERATURE_GET)       (ADL_CONTEXT_HANDLE, int, int, int*);
    typedef int (*ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET) (ADL_CONTEXT_HANDLE, int, ADLODNPerformanceStatus*);

    //PMLog (Handles beyond 17h family, additional add-on at a later date)
    typedef int (*ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET)   (ADL_CONTEXT_HANDLE, int, int*, int**);
    //typedef int (*ADL2_ADAPTER_PMLOG_START)              (ADL_CONTEXT_HANDLE, int, ADLPMLogStartInput*, ADLPMLogStartOutput*, ADL_D3DKMT_HANDLE);
    //typedef int (*ADL2_ADAPTER_PMLOG_STOP)               (ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE);
    // TODO - Change these all to PMLOG_SHARE_MEMORY  funcs like support and shared_start shared_stop
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_START)      (ADL_CONTEXT_HANDLE, int, int, int, int*, ADL_D3DKMT_HANDLE*, void**, int);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_STOP)       (ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE*);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT)    (ADL_CONTEXT_HANDLE, int, int*, int);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_READ)       (ADL_CONTEXT_HANDLE, int, int, int*, void**, ADLPMLogDataOutput*);
    typedef int (*ADL2_DEVICE_PMLOG_DEVICE_CREATE)       (ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE*);
    typedef int (*ADL2_DEVICE_PMLOG_DEVICE_DESTROY)      (ADL_CONTEXT_HANDLE, ADL_D3DKMT_HANDLE);


    // ============================================================
    // Function pointer declarations (nullptr-initialized)
    // ============================================================
    ADL2_MAIN_CONTROL_CREATE              adl2_main_control_create = nullptr;   //
    ADL2_MAIN_CONTROL_DESTROY             adl2_main_control_destroy = nullptr;  //

    ADL2_ADAPTER_NUMBEROFADAPTERS_GET     adl2_adapter_numberofadapters_get = nullptr;  //
    ADL2_ADAPTER_ADAPTERINFO_GET          adl2_adapter_adapterinfo_get = nullptr;       //
    ADL2_ADAPTER_ID_GET                   adl2_adapter_id_get = nullptr;
    ADL2_ADAPTER_ACTIVE_GET               adl2_adapter_active_get = nullptr;            //

    ADL2_ADAPTER_VRAMUSAGE_GET            adl2_adapter_vramusage_get = nullptr;
    ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET   adl2_adapter_dedicatedvramusage_get = nullptr;
    ADL2_ADAPTER_MEMORYINFOX4_GET         adl2_adapter_memoryinfox4_get = nullptr;

    ADL2_ADAPTER_FRAMEMETRICS_CAPS        adl2_adapter_framemetrics_caps = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_GET         adl2_adapter_framemetrics_get = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_START       adl2_adapter_framemetrics_start = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_STOP        adl2_adapter_framemetrics_stop = nullptr;

    ADL2_OVERDRIVE_CAPS                   adl2_overdrive_caps = nullptr;

    ADL2_OD5_ODPARAMETERS_GET             adl2_od5_odparameters_get = nullptr;
    ADL2_OD5_CURRENTACTIVITY_GET          adl2_od5_currentactivity_get = nullptr;   //
    ADL2_OD5_TEMPERATURE_GET              adl2_od5_temperature_get = nullptr;       //
    ADL2_OD5_FANSPEED_GET                 adl2_od5_fanspeed_get = nullptr;          //
    ADL2_OD5_FANSPEEDINFO_GET             adl2_od5_fanspeedinfo_get = nullptr;

    ADL2_OD6_CAPABILITIES_GET             adl2_od6_capabilities_get = nullptr;
    ADL2_OD6_CURRENTPOWER_GET             adl2_od6_currentpower_get = nullptr;
    ADL2_OD6_TEMPERATURE_GET              adl2_od6_temperature_get = nullptr;
    ADL2_OD6_CURRENTSTATUS_GET            adl2_od6_currentstatus_get = nullptr;
    ADL2_OD6_FANSPEED_GET                 adl2_od6_fanspeed_get = nullptr;

    ADL2_OVERDRIVEN_TEMPERATURE_GET       adl2_overdriven_temperature_get = nullptr;
    ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET adl2_overdriven_performancestatus_get = nullptr;

    //ADL2_ADAPTER_PMLOG_SUPPORT_GET        adl2_adapter_pmlog_support_get = nullptr;
    //ADL2_ADAPTER_PMLOG_START              adl2_adapter_pmlog_start = nullptr;
    //ADL2_ADAPTER_PMLOG_STOP               adl2_adapter_pmlog_stop = nullptr;
    // Need: ADL2_Overdrive8_PMLogSenorType_Support_Get() to complete the rest of sharememory, and no Senor isnt a typo on my endm its theirs. Write Senor for that, check the dll to be safe
    
    ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET   adl2_od8_pmlogsenortype_support_get = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_START      adl2_od8_pmlog_sharememory_start = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_STOP       adl2_od8_pmlog_sharememory_stop = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT    adl2_od8_pmlog_sharememory_support = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_READ       adl2_od8_pmlog_sharememory_read = nullptr;
    ADL2_DEVICE_PMLOG_DEVICE_CREATE       adl2_device_pmlog_device_create = nullptr;
    ADL2_DEVICE_PMLOG_DEVICE_DESTROY      adl2_device_pmlog_device_destroy = nullptr;


    void        initADL();
    void        shutdownADL();
    void        enumerateADLAdapters();
    void        fetchADLGPUMetrics(int adapter_idx);
    static void* __stdcall adlMallocCallback(int size);
};