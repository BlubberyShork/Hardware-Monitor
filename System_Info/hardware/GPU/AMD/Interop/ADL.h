#pragma once
#include <windows.h>
#include <adl_sdk.h>
#include <adl_structures.h>

// ---------------------------------------------------------------------------
// ADLInterop
//
// Owns the ADL DLL lifetime, ADL_CONTEXT_HANDLE, and every resolved function
// pointer.  Constructed once by AMDPool; shared with each
// AMDLiveGPUMetrics instance that was created on the ADL path.
// ---------------------------------------------------------------------------
class ADL {
public:
    ADL() = default;
    ~ADL();

    ADL(const ADL&) = delete;
    ADL& operator=(const ADL&) = delete;

    // Loads atiadlxx.dll, resolves all symbols, calls ADL2_Main_Control_Create.
    // Throws std::runtime_error on any failure.
    void init();

    // ADL2_Main_Control_Destroy + FreeLibrary.  Safe to call multiple times.
    void shutdown();

    // -----------------------------------------------------------------------
    // Live state
    // -----------------------------------------------------------------------
    ADL_CONTEXT_HANDLE  context = nullptr;
    HMODULE             adl_module = nullptr;

    // -----------------------------------------------------------------------
    // Typedefs
    // -----------------------------------------------------------------------
    // Context / lifecycle
    typedef int (*ADL2_MAIN_CONTROL_CREATE)              (ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
    typedef int (*ADL2_MAIN_CONTROL_DESTROY)             (ADL_CONTEXT_HANDLE);

    // Adapter enumeration
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
    typedef int (*ADL2_OVERDRIVE_CAPS)                   (ADL_CONTEXT_HANDLE, int, int*, int*, int*);

    // Overdrive 5
    typedef int (*ADL2_OD5_ODPARAMETERS_GET)             (ADL_CONTEXT_HANDLE, int, ADLODParameters*);
    typedef int (*ADL2_OD5_CURRENTACTIVITY_GET)          (ADL_CONTEXT_HANDLE, int, ADLPMActivity*);
    typedef int (*ADL2_OD5_TEMPERATURE_GET)              (ADL_CONTEXT_HANDLE, int, int, ADLTemperature*);
    typedef int (*ADL2_OD5_FANSPEED_GET)                 (ADL_CONTEXT_HANDLE, int, int, ADLFanSpeedValue*);
    typedef int (*ADL2_OD5_FANSPEEDINFO_GET)             (ADL_CONTEXT_HANDLE, int, int, ADLFanSpeedInfo*);

    // Overdrive 6
    typedef int (*ADL2_OD6_CAPABILITIES_GET)             (ADL_CONTEXT_HANDLE, int, ADLOD6Capabilities*);
    typedef int (*ADL2_OD6_CURRENTPOWER_GET)             (ADL_CONTEXT_HANDLE, int, int, int*);
    typedef int (*ADL2_OD6_TEMPERATURE_GET)              (ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*ADL2_OD6_CURRENTSTATUS_GET)            (ADL_CONTEXT_HANDLE, int, ADLOD6CurrentStatus*);
    typedef int (*ADL2_OD6_FANSPEED_GET)                 (ADL_CONTEXT_HANDLE, int, ADLOD6FanSpeedInfo*);

    // Overdrive N
    typedef int (*ADL2_OVERDRIVEN_TEMPERATURE_GET)       (ADL_CONTEXT_HANDLE, int, int, int*);
    typedef int (*ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET) (ADL_CONTEXT_HANDLE, int, ADLODNPerformanceStatus*);

    // PMLog / OD8 share-memory
    typedef int (*ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET)   (ADL_CONTEXT_HANDLE, int, int*, int**);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_START)      (ADL_CONTEXT_HANDLE, int, int, int, int*, ADL_D3DKMT_HANDLE*, void**, int);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_STOP)       (ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE*);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT)    (ADL_CONTEXT_HANDLE, int, int*, int);
    typedef int (*ADL2_OD8_PMLOG_SHAREMEMORY_READ)       (ADL_CONTEXT_HANDLE, int, int, int*, void**, ADLPMLogDataOutput*);
    typedef int (*ADL2_DEVICE_PMLOG_DEVICE_CREATE)       (ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE*);
    typedef int (*ADL2_DEVICE_PMLOG_DEVICE_DESTROY)      (ADL_CONTEXT_HANDLE, ADL_D3DKMT_HANDLE);

    // -----------------------------------------------------------------------
    // Resolved function pointers
    // -----------------------------------------------------------------------
    ADL2_MAIN_CONTROL_CREATE              adl2_main_control_create = nullptr;
    ADL2_MAIN_CONTROL_DESTROY             adl2_main_control_destroy = nullptr;

    ADL2_ADAPTER_NUMBEROFADAPTERS_GET     adl2_adapter_numberofadapters_get = nullptr;
    ADL2_ADAPTER_ADAPTERINFO_GET          adl2_adapter_adapterinfo_get = nullptr;
    ADL2_ADAPTER_ID_GET                   adl2_adapter_id_get = nullptr;
    ADL2_ADAPTER_ACTIVE_GET               adl2_adapter_active_get = nullptr;

    ADL2_ADAPTER_VRAMUSAGE_GET            adl2_adapter_vramusage_get = nullptr;
    ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET   adl2_adapter_dedicatedvramusage_get = nullptr;
    ADL2_ADAPTER_MEMORYINFOX4_GET         adl2_adapter_memoryinfox4_get = nullptr;

    ADL2_ADAPTER_FRAMEMETRICS_CAPS        adl2_adapter_framemetrics_caps = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_GET         adl2_adapter_framemetrics_get = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_START       adl2_adapter_framemetrics_start = nullptr;
    ADL2_ADAPTER_FRAMEMETRICS_STOP        adl2_adapter_framemetrics_stop = nullptr;

    ADL2_OVERDRIVE_CAPS                   adl2_overdrive_caps = nullptr;

    ADL2_OD5_ODPARAMETERS_GET             adl2_od5_odparameters_get = nullptr;
    ADL2_OD5_CURRENTACTIVITY_GET          adl2_od5_currentactivity_get = nullptr;
    ADL2_OD5_TEMPERATURE_GET              adl2_od5_temperature_get = nullptr;
    ADL2_OD5_FANSPEED_GET                 adl2_od5_fanspeed_get = nullptr;
    ADL2_OD5_FANSPEEDINFO_GET             adl2_od5_fanspeedinfo_get = nullptr;

    ADL2_OD6_CAPABILITIES_GET             adl2_od6_capabilities_get = nullptr;
    ADL2_OD6_CURRENTPOWER_GET             adl2_od6_currentpower_get = nullptr;
    ADL2_OD6_TEMPERATURE_GET              adl2_od6_temperature_get = nullptr;
    ADL2_OD6_CURRENTSTATUS_GET            adl2_od6_currentstatus_get = nullptr;
    ADL2_OD6_FANSPEED_GET                 adl2_od6_fanspeed_get = nullptr;

    ADL2_OVERDRIVEN_TEMPERATURE_GET       adl2_overdriven_temperature_get = nullptr;
    ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET adl2_overdriven_performancestatus_get = nullptr;

    ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET   adl2_od8_pmlogsenortype_support_get = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_START      adl2_od8_pmlog_sharememory_start = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_STOP       adl2_od8_pmlog_sharememory_stop = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT    adl2_od8_pmlog_sharememory_support = nullptr;
    ADL2_OD8_PMLOG_SHAREMEMORY_READ       adl2_od8_pmlog_sharememory_read = nullptr;
    ADL2_DEVICE_PMLOG_DEVICE_CREATE       adl2_device_pmlog_device_create = nullptr;
    ADL2_DEVICE_PMLOG_DEVICE_DESTROY      adl2_device_pmlog_device_destroy = nullptr;

    // -----------------------------------------------------------------------
    // Malloc callback required by ADL2_Main_Control_Create
    // -----------------------------------------------------------------------
    static void* __stdcall mallocCallback(int size);

private:
    bool _initialized = false;

    // Resolves a symbol from adl_module; throws if not found.
    FARPROC resolve(const char* name);
};