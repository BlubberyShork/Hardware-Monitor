#include "ADL.h"
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
ADL::~ADL() {
    shutdown();
}

void ADL::init() {
    adl_module = LoadLibraryA("atiadlxx.dll");
    if (!adl_module)
        adl_module = LoadLibraryA("atiadlxy.dll"); // 32-bit fallback
    if (!adl_module)
        throw std::runtime_error("Failed to load ADL DLL (atiadlxx.dll / atiadlxy.dll)");

    // Context / lifecycle
    adl2_main_control_create = reinterpret_cast<ADL2_MAIN_CONTROL_CREATE> (resolve("ADL2_Main_Control_Create"));
    adl2_main_control_destroy = reinterpret_cast<ADL2_MAIN_CONTROL_DESTROY>(resolve("ADL2_Main_Control_Destroy"));

    // Adapter enumeration
    adl2_adapter_numberofadapters_get = reinterpret_cast<ADL2_ADAPTER_NUMBEROFADAPTERS_GET>(resolve("ADL2_Adapter_NumberOfAdapters_Get"));
    adl2_adapter_adapterinfo_get = reinterpret_cast<ADL2_ADAPTER_ADAPTERINFO_GET>     (resolve("ADL2_Adapter_AdapterInfo_Get"));
    adl2_adapter_id_get = reinterpret_cast<ADL2_ADAPTER_ID_GET>              (resolve("ADL2_Adapter_ID_Get"));
    adl2_adapter_active_get = reinterpret_cast<ADL2_ADAPTER_ACTIVE_GET>          (resolve("ADL2_Adapter_Active_Get"));

    // Memory
    adl2_adapter_vramusage_get = reinterpret_cast<ADL2_ADAPTER_VRAMUSAGE_GET>          (resolve("ADL2_Adapter_VRAMUsage_Get"));
    adl2_adapter_dedicatedvramusage_get = reinterpret_cast<ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET> (resolve("ADL2_Adapter_DedicatedVRAMUsage_Get"));
    adl2_adapter_memoryinfox4_get = reinterpret_cast<ADL2_ADAPTER_MEMORYINFOX4_GET>       (resolve("ADL2_Adapter_MemoryInfoX4_Get"));

    // Frame metrics
    adl2_adapter_framemetrics_caps = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_CAPS> (resolve("ADL2_Adapter_FrameMetrics_Caps"));
    adl2_adapter_framemetrics_get = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_GET>  (resolve("ADL2_Adapter_FrameMetrics_Get"));
    adl2_adapter_framemetrics_start = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_START>(resolve("ADL2_Adapter_FrameMetrics_Start"));
    adl2_adapter_framemetrics_stop = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_STOP> (resolve("ADL2_Adapter_FrameMetrics_Stop"));

    // Overdrive caps / version
    adl2_overdrive_caps = reinterpret_cast<ADL2_OVERDRIVE_CAPS>(resolve("ADL2_Overdrive_Caps"));

    // Overdrive 5
    adl2_od5_odparameters_get = reinterpret_cast<ADL2_OD5_ODPARAMETERS_GET>   (resolve("ADL2_Overdrive5_ODParameters_Get"));
    adl2_od5_currentactivity_get = reinterpret_cast<ADL2_OD5_CURRENTACTIVITY_GET>(resolve("ADL2_Overdrive5_CurrentActivity_Get"));
    adl2_od5_temperature_get = reinterpret_cast<ADL2_OD5_TEMPERATURE_GET>    (resolve("ADL2_Overdrive5_Temperature_Get"));
    adl2_od5_fanspeed_get = reinterpret_cast<ADL2_OD5_FANSPEED_GET>       (resolve("ADL2_Overdrive5_FanSpeed_Get"));
    adl2_od5_fanspeedinfo_get = reinterpret_cast<ADL2_OD5_FANSPEEDINFO_GET>   (resolve("ADL2_Overdrive5_FanSpeedInfo_Get"));

    // Overdrive 6
    adl2_od6_capabilities_get = reinterpret_cast<ADL2_OD6_CAPABILITIES_GET> (resolve("ADL2_Overdrive6_Capabilities_Get"));
    adl2_od6_currentpower_get = reinterpret_cast<ADL2_OD6_CURRENTPOWER_GET> (resolve("ADL2_Overdrive6_CurrentPower_Get"));
    adl2_od6_temperature_get = reinterpret_cast<ADL2_OD6_TEMPERATURE_GET>  (resolve("ADL2_Overdrive6_Temperature_Get"));
    adl2_od6_currentstatus_get = reinterpret_cast<ADL2_OD6_CURRENTSTATUS_GET>(resolve("ADL2_Overdrive6_CurrentStatus_Get"));
    adl2_od6_fanspeed_get = reinterpret_cast<ADL2_OD6_FANSPEED_GET>     (resolve("ADL2_Overdrive6_FanSpeed_Get"));

    // Overdrive N
    adl2_overdriven_temperature_get = reinterpret_cast<ADL2_OVERDRIVEN_TEMPERATURE_GET>      (resolve("ADL2_OverdriveN_Temperature_Get"));
    adl2_overdriven_performancestatus_get = reinterpret_cast<ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET>(resolve("ADL2_OverdriveN_PerformanceStatus_Get"));

    // PMLog / OD8 share-memory
    adl2_od8_pmlogsenortype_support_get = reinterpret_cast<ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET>(resolve("ADL2_Overdrive8_PMLogSenorType_Support_Get"));
    adl2_od8_pmlog_sharememory_start = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_START>   (resolve("ADL2_Overdrive8_ShareMemory_Start"));
    adl2_od8_pmlog_sharememory_stop = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_STOP>    (resolve("ADL2_Overdrive8_ShareMemory_Stop"));
    adl2_od8_pmlog_sharememory_support = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT> (resolve("ADL2_Overdrive8_ShareMemory_Support"));
    adl2_od8_pmlog_sharememory_read = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_READ>    (resolve("ADL2_Overdrive8_ShareMemory_Read"));
    adl2_device_pmlog_device_create = reinterpret_cast<ADL2_DEVICE_PMLOG_DEVICE_CREATE>    (resolve("ADL2_Device_PMLog_Device_Create"));
    adl2_device_pmlog_device_destroy = reinterpret_cast<ADL2_DEVICE_PMLOG_DEVICE_DESTROY>   (resolve("ADL2_Device_PMLog_Device_Destroy"));

    int res = adl2_main_control_create(mallocCallback, 1 /*iEnumConnectedAdapters*/, &context);
    if (res != ADL_OK)
        throw std::runtime_error("ADL2_Main_Control_Create failed, ADL_RESULT: " + std::to_string(res));

    _initialized = true;
}

void ADL::shutdown() {
    if (!_initialized) return;

    if (adl2_main_control_destroy && context) {
        adl2_main_control_destroy(context);
        context = nullptr;
    }
    if (adl_module) {
        FreeLibrary(adl_module);
        adl_module = nullptr;
    }
    _initialized = false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
FARPROC ADL::resolve(const char* name) {
    FARPROC func = GetProcAddress(adl_module, name);
    if (!func)
        throw std::runtime_error(std::string("ADL symbol not found: ") + name);
    return func;
}

void* __stdcall ADL::mallocCallback(int size) {
    return malloc(size);
}