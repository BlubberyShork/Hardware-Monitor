#include "AMDLiveGPUMetrics.h"
#include <iostream>
#include <stdexcept>
#include <windows.h>

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------
AMDLiveGPUMetrics::AMDLiveGPUMetrics() {
    try {
        initSystem();
        initPerfMonitoring();
        enumerateGPUs();
        active_backend = Backend::ADLX;
        initialized = true;
    }
    catch (const std::exception& adlx_err) {
        std::cerr << "ADLX unavailable (" << adlx_err.what()
            << "), falling back to ADL\n";

        gpus.clear();
        if (perf_monitoring) { perf_monitoring->Release(); perf_monitoring = nullptr; }
        adlx_helper.Terminate();

        try {
            initADL();
            enumerateADLAdapters();
            active_backend = Backend::ADL;
            initialized = true;
        }
        catch (const std::exception& adl_err) {
            std::cerr << "ADL fallback also unavailable (" << adl_err.what()
                << "), AMD GPU metrics disabled\n";
        }
    }
}

AMDLiveGPUMetrics::~AMDLiveGPUMetrics() {
    if (active_backend == Backend::ADLX) {
        gpus.clear();
        if (perf_monitoring) perf_monitoring->Release();
        adlx_helper.Terminate();
    }
    else {
        shutdownADL();
    }
}

// -----------------------------------------------------------------------
// Fetch / Output
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchMetrics() {
    if (!initialized) return;

    if (active_backend == Backend::ADLX) {
        for (size_t i = 0; i < gpus.size(); i++)
            fetchGPUMetrics(gpus[i], gpu_live_data[i]);
    }
    else {
        for (size_t i = 0; i < adl_adapter_indices.size(); i++)
            fetchADLGPUMetrics(adl_adapter_indices[i], adl_gpu_live_data[i]);
    }
}

void AMDLiveGPUMetrics::outputMetrics() const {
    if (!initialized) return;

    const std::vector<GPULiveData>& data =
        (active_backend == Backend::ADLX) ? gpu_live_data : adl_gpu_live_data;

    for (auto& live_data : data) {
        std::wcout << "-- Live GPU Data --\n";
        std::wcout << "Avg Temp:     " << live_data.curr_avg_temp << "C\n";
        std::wcout << "Hotspot Temp: " << live_data.curr_hotspot_temp << "C\n\n";
        std::wcout << "Graphics Utilization:     " << live_data.curr_graphics_utilization << "%\n";
        std::wcout << "Frame Buffer Utilization: " << live_data.curr_frame_buffer_utilization << "%\n";
        std::wcout << "Video Engine Utilization: " << live_data.curr_video_engine_utilization << "%\n";

        std::wcout << "\n-- Clock Speeds --\n";
        for (auto& clk : live_data.clks) {
            switch (clk.clk_type) {
            case GPUClockDomain::Graphics:  std::wcout << "Core / Graphics / GPU clock:  ";   break;
            case GPUClockDomain::Memory:    std::wcout << "Memory / VRAM clock:    ";         break;
            case GPUClockDomain::Video:     std::wcout << "Video / Media Engine clock:     "; break;
            default:                        std::wcout << "Unknown:   ";                      break;
            }
            std::wcout << clk.clk_spd << " MHz\n";
        }

        std::wcout << "\n-- Fan Speed --\n";
        std::wcout << "Fan Speed: " << live_data.fan_speed << " RPM\n\n";
    }
}

// -----------------------------------------------------------------------
// ADLX Init
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::initSystem() {
    ADLX_RESULT res = adlx_helper.Initialize();
    if (ADLX_FAILED(res))
        throw std::runtime_error("ADLXHelper initialization failed, ADLX_RESULT: " + std::to_string(res));

    adlx_system = adlx_helper.GetSystemServices();
    if (!adlx_system)
        throw std::runtime_error("Failed to acquire IADLXSystem from ADLXHelper");
}

void AMDLiveGPUMetrics::initPerfMonitoring() {
    ADLX_RESULT res = adlx_system->GetPerformanceMonitoringServices(&perf_monitoring);
    if (ADLX_FAILED(res) || !perf_monitoring)
        throw std::runtime_error("Failed to acquire IADLXPerformanceMonitoringServices, ADLX_RESULT: " + std::to_string(res));
}

void AMDLiveGPUMetrics::enumerateGPUs() {
    adlx::IADLXGPUListPtr gpu_list;
    ADLX_RESULT res = adlx_system->GetGPUs(&gpu_list);
    if (ADLX_FAILED(res) || !gpu_list)
        throw std::runtime_error("Failed to enumerate ADLX GPUs, ADLX_RESULT: " + std::to_string(res));

    for (adlx_uint i = gpu_list->Begin(); i != gpu_list->End(); i++) {
        adlx::IADLXGPUPtr gpu;
        res = gpu_list->At(i, &gpu);
        if (ADLX_FAILED(res) || !gpu) {
            std::cerr << "Failed to get GPU at index " << i << ", skipping\n";
            continue;
        }
        gpus.push_back(gpu);
    }

    if (gpus.empty())
        throw std::runtime_error("No ADLX GPUs found after enumeration");

    gpu_live_data.resize(gpus.size());
}

// -----------------------------------------------------------------------
// ADLX Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchGPUMetrics(adlx::IADLXGPU* gpu, GPULiveData& live_data) {
    live_data = {};

    adlx::IADLXGPUMetricsSupportPtr metrics_support;
    ADLX_RESULT res = perf_monitoring->GetSupportedGPUMetrics(gpu, &metrics_support);
    if (ADLX_FAILED(res) || !metrics_support) {
        std::cerr << "Failed to get IADLXGPUMetricsSupport, ADLX_RESULT: " << res << "\n";
        return;
    }

    adlx::IADLXAllMetricsPtr all_metrics;
    res = perf_monitoring->GetCurrentAllMetrics(&all_metrics);
    if (ADLX_FAILED(res) || !all_metrics) {
        std::cerr << "Failed to get IADLXAllMetrics, ADLX_RESULT: " << res << "\n";
        return;
    }

    adlx::IADLXGPUMetricsPtr gpu_metrics;
    res = all_metrics->GetGPUMetrics(gpu, &gpu_metrics);
    if (ADLX_FAILED(res) || !gpu_metrics) {
        std::cerr << "Failed to get IADLXGPUMetrics, ADLX_RESULT: " << res << "\n";
        return;
    }

    adlx_bool supported = false;

    // Temperature
    metrics_support->IsSupportedGPUTemperature(&supported);
    if (supported) {
        adlx_double temp = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUTemperature(&temp)))
            live_data.curr_avg_temp = static_cast<int>(temp);
    }

    metrics_support->IsSupportedGPUHotspotTemperature(&supported);
    if (supported) {
        adlx_double temp = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUHotspotTemperature(&temp)))
            live_data.curr_hotspot_temp = static_cast<int>(temp);
    }

    // Clocks
    //metrics_support->IsSupportedGPUClockSpeed(&supported);
    //if (supported) {
    adlx_int clk = 0;
    //if (ADLX_SUCCEEDED(gpu_metrics->GPUClockSpeed(&clk)))
        live_data.clks.push_back({ GPUClockDomain::Graphics, static_cast<double>(clk) });   // value of 0 if not supported 
    //}

    //metrics_support->IsSupportedGPUVRAMClockSpeed(&supported);
    //if (supported) {
    adlx_int clk = 0;
    //if (ADLX_SUCCEEDED(gpu_metrics->GPUVRAMClockSpeed(&clk)))
        live_data.clks.push_back({ GPUClockDomain::Memory, static_cast<double>(clk) });    // value of 0 if not supported 
   // }

    // Utilization
    metrics_support->IsSupportedGPUUsage(&supported);
    if (supported) {
        adlx_double usage = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUUsage(&usage)))
            live_data.curr_graphics_utilization = static_cast<unsigned int>(usage);
    }

    adlx_int vram_used_mb = 0;
    adlx_int vram_min = 0, vram_max = 0;

    metrics_support->IsSupportedGPUVRAM(&supported);
    if (supported) {
        gpu_metrics->GPUVRAM(&vram_used_mb);
        metrics_support->GetGPUVRAMRange(&vram_min, &vram_max);

        if (vram_max > 0)
            live_data.curr_frame_buffer_utilization = static_cast<unsigned int>(
                (static_cast<double>(vram_used_mb) / vram_max) * 100.0
                );
    }

    // No direct video engine utilization equivalent in ADLX
    // ADL_PMLOG_INFO_ACTIVITY_UVD has no IADLXGPUMetrics counterpart
    // TODO - revisit if ADLX exposes this in a future SDK revision
    live_data.curr_video_engine_utilization = 0;

    // Fan speed
    metrics_support->IsSupportedGPUFanSpeed(&supported);
    if (supported) {
        adlx_int fan_rpm = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUFanSpeed(&fan_rpm)))
            live_data.fan_speed = static_cast<unsigned int>(fan_rpm);
    }
}

// -----------------------------------------------------------------------
// ADL Init / Shutdown
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::initADL() {
    adl_module = LoadLibraryA("atiadlxx.dll");
    if (!adl_module)
        adl_module = LoadLibraryA("atiadlxy.dll"); // 32-bit fallback
    if (!adl_module)
        throw std::runtime_error("Failed to load ADL DLL (atiadlxx.dll / atiadlxy.dll)");

    auto resolve = [&](const char* name) -> FARPROC {
        FARPROC fn = GetProcAddress(adl_module, name);
        if (!fn)
            throw std::runtime_error(std::string("ADL symbol not found: ") + name);
        return fn;
        };

    adl_main_control_create     = reinterpret_cast<ADL_MAIN_CONTROL_CREATE>         (resolve("ADL_Main_Control_Create"));
    adl_main_control_destroy    = reinterpret_cast<ADL_MAIN_CONTROL_DESTROY>        (resolve("ADL_Main_Control_Destroy"));
    adl_adapter_number_get      = reinterpret_cast<ADL_ADAPTER_NUMBEROFADAPTERS_GET>(resolve("ADL_Adapter_NumberOfAdapters_Get"));
    adl_adapter_info_get        = reinterpret_cast<ADL_ADAPTER_ADAPTERINFO_GET>     (resolve("ADL_Adapter_AdapterInfo_Get"));
    adl_adapter_active_get      = reinterpret_cast<ADL_ADAPTER_ACTIVE_GET>          (resolve("ADL_Adapter_Active_Get"));
    adl_od5_temperature_get     = reinterpret_cast<ADL_OD5_TEMPERATURE_GET>         (resolve("ADL_OD5_Temperature_Get"));
    adl_od5_currentactivity_get = reinterpret_cast<ADL_OD5_CURRENTACTIVITY_GET>     (resolve("ADL_OD5_CurrentActivity_Get"));
    adl_od5_fanspeed_get        = reinterpret_cast<ADL_OD5_FANSPEED_GET>            (resolve("ADL_OD5_FanSpeed_Get"));

    int res = adl_main_control_create(adlMallocCallback, 1 /*iEnumConnectedAdapters*/);
    if (res != ADL_OK)
        throw std::runtime_error("ADL_Main_Control_Create failed, ADL_RESULT: " + std::to_string(res));
}

void AMDLiveGPUMetrics::shutdownADL() {
    adl_adapter_indices.clear();
    adl_gpu_live_data.clear();

    if (adl_main_control_destroy)
        adl_main_control_destroy();

    if (adl_module) {
        FreeLibrary(adl_module);
        adl_module = nullptr;
    }
}

// -----------------------------------------------------------------------
// ADL Enumerate
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::enumerateADLAdapters() {
    int res = adl_adapter_number_get(&adl_adapter_cnt);
    if (res != ADL_OK || adl_adapter_cnt <= 0)
        throw std::runtime_error("ADL_Adapter_NumberOfAdapters_Get failed or returned 0 adapters, ADL_RESULT: "
            + std::to_string(res));

    std::vector<AdapterInfo> adapter_info(adl_adapter_cnt);
    res = adl_adapter_info_get(adapter_info.data(),
        static_cast<int>(sizeof(AdapterInfo) * adl_adapter_cnt));
    if (res != ADL_OK)
        throw std::runtime_error("ADL_Adapter_AdapterInfo_Get failed, ADL_RESULT: " + std::to_string(res));

    // Deduplicate: one logical GPU can appear as multiple adapters.
    // Track seen bus numbers so we register each physical card only once.
    std::vector<int> seen_bus_numbers;

    for (int i = 0; i < adl_adapter_cnt; i++) {
        int active = 0;
        if (adl_adapter_active_get(i, &active) != ADL_OK || !active)
            continue;

        int bus = adapter_info[i].iBusNumber;
        if (std::find(seen_bus_numbers.begin(), seen_bus_numbers.end(), bus)
            != seen_bus_numbers.end())
            continue;

        seen_bus_numbers.push_back(bus);
        adl_adapter_indices.push_back(i);
    }

    if (adl_adapter_indices.empty())
        throw std::runtime_error("No active ADL adapters found after enumeration");

    adl_gpu_live_data.resize(adl_adapter_indices.size());
}

// -----------------------------------------------------------------------
// ADL Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchADLGPUMetrics(int adapter_idx, GPULiveData& live_data) {
    live_data = {};

    // Temperature
    {
        ADLTemperature temp_data = {};
        temp_data.iSize = sizeof(ADLTemperature);
        if (adl_od5_temperature_get(adapter_idx, 0, &temp_data) == ADL_OK) {
            // ADL returns millidegrees C
            int temp_c = temp_data.iTemperature / 1000;
            live_data.curr_avg_temp = temp_c;
            live_data.curr_hotspot_temp = temp_c; // ADL OD5 exposes one sensor; no distinct hotspot
        }
        else {
            std::cerr << "ADL_OD5_Temperature_Get failed for adapter " << adapter_idx << "\n";
        }
    }

    // Clocks + Utilization
    {
        ADLPMActivity activity = {};
        activity.iSize = sizeof(ADLPMActivity);
        if (adl_od5_currentactivity_get(adapter_idx, &activity) == ADL_OK) {
            if (activity.iEngineClock > 0)
                live_data.clks.push_back({ GPUClockDomain::Graphics,
                    static_cast<double>(activity.iEngineClock) * 0.01 }); // 10 kHz -> MHz

            if (activity.iMemoryClock > 0)
                live_data.clks.push_back({ GPUClockDomain::Memory,
                    static_cast<double>(activity.iMemoryClock) * 0.01 });

            live_data.curr_graphics_utilization = static_cast<unsigned int>(activity.iActivityPercent);

            // ADL OD5 has no separate frame-buffer or video-engine utilization
            // leave as 0
            live_data.curr_frame_buffer_utilization = 0;
            live_data.curr_video_engine_utilization = 0;
        }
        else {
            std::cerr << "ADL_OD5_CurrentActivity_Get failed for adapter " << adapter_idx << "\n";
        }
    }

    // Fan Speed
    {
        ADLFanSpeedValue fan_val = {};
        fan_val.iSize = sizeof(ADLFanSpeedValue);
        fan_val.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
        if (adl_od5_fanspeed_get(adapter_idx, 0, &fan_val) == ADL_OK)
            live_data.fan_speed = static_cast<unsigned int>(fan_val.iFanSpeed);
        else
            std::cerr << "ADL_OD5_FanSpeed_Get failed for adapter " << adapter_idx << "\n";
    }
}

// -----------------------------------------------------------------------
// ADL Malloc callback
// -----------------------------------------------------------------------
void* __stdcall AMDLiveGPUMetrics::adlMallocCallback(int size) {
    return malloc(size);
}