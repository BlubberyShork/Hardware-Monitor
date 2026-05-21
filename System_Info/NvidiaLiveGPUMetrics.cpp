#include "NvidiaLiveGPUMetrics.h"
#include <iostream>

NvidiaLiveGPUMetrics::NvidiaLiveGPUMetrics() {
    NvAPI_Status res = NvAPI_Initialize();
    checkAndHandleError("NVAPI Initialization failed: ", res);
}

NvidiaLiveGPUMetrics::~NvidiaLiveGPUMetrics() {
    NvAPI_Unload();
}

GPUClockDomain NvidiaLiveGPUMetrics::toClockDomain(NV_GPU_PUBLIC_CLOCK_ID nv_id) {
    switch (nv_id) {
    case NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS:  return GPUClockDomain::Graphics;
    case NVAPI_GPU_PUBLIC_CLOCK_MEMORY:    return GPUClockDomain::Memory;
    case NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR: return GPUClockDomain::Processor;
    case NVAPI_GPU_PUBLIC_CLOCK_VIDEO:     return GPUClockDomain::Video;
    default:                               return GPUClockDomain::Unknown;
    }
}

void NvidiaLiveGPUMetrics::fetchMetrics() {
    NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32 gpu_cnt = 0;

    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(handles, &gpu_cnt);
    checkAndHandleError("Failed to enumerate physical GPUs: ", status);

    for (NvU32 i = 0; i < gpu_cnt; i++) {
        NvPhysicalGpuHandle handle = handles[i];
        if (!handle) continue;

        GPULiveData live_data = {};

        // Thermal
        NV_GPU_THERMAL_SETTINGS ts = {};
        ts.version = NV_GPU_THERMAL_SETTINGS_VER_2;
        status = NvAPI_GPU_GetThermalSettings(handle, NVAPI_THERMAL_TARGET_ALL, &ts);
        checkAndHandleError("Failed to get thermal settings: ", status);

        live_data.curr_avg_temp = avgTemp(ts);
        live_data.curr_hotspot_temp = hotspotTemp(ts);

        // Clocks
        NV_GPU_PERF_PSTATES20_INFO pstate_info = {};
        pstate_info.version = NV_GPU_PERF_PSTATES20_INFO_VER;
        status = NvAPI_GPU_GetPstates20(handle, &pstate_info);
        checkAndHandleError("Failed to get pstate info: ", status);

        NV_GPU_CLOCK_FREQUENCIES clk_freq = {};
        clk_freq.version = NV_GPU_CLOCK_FREQUENCIES_VER_3;
        status = NvAPI_GPU_GetAllClockFrequencies(handle, &clk_freq);
        checkAndHandleError("Failed to get clock frequencies: ", status);

        for (NvU32 c = 0; c < pstate_info.numClocks; c++) {
            NV_GPU_PUBLIC_CLOCK_ID clk_id = pstate_info.pstates[0].clocks[c].domainId;
            if (clk_freq.domain[clk_id].bIsPresent) {
                ClockEntry entry = {};
                entry.clk_type = toClockDomain(clk_id);
                entry.clk_spd = clk_freq.domain[clk_id].frequency * 0.001;
                live_data.clks.push_back(entry);
            }
        }

        // Utilization
        NV_GPU_DYNAMIC_PSTATES_INFO_EX pstates_ex = {};
        pstates_ex.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
        status = NvAPI_GPU_GetDynamicPstatesInfoEx(handle, &pstates_ex);
        checkAndHandleError("Failed to get dynamic pstate info: ", status);

        if (pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].bIsPresent)
            live_data.curr_graphics_utilization = pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].percentage;

        if (pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].bIsPresent)
            live_data.curr_frame_buffer_utilization = pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].percentage;

        if (pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].bIsPresent)
            live_data.curr_video_engine_utilization = pstates_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].percentage;

        // Fan speed — TODO: update library for modern fan APIs
        NvU32 fan_spd = 0;
        if (NvAPI_GPU_GetTachReading(handle, &fan_spd) != NVAPI_OK)
            fan_spd = 0;
        live_data.fan_speed = fan_spd;

        all_gpu_live_data[handle] = live_data;
    }
}

void NvidiaLiveGPUMetrics::outputMetrics() const {
    for (auto& [handle, live_data] : all_gpu_live_data) {
        std::wcout << "-- Live GPU Data --\n";
        std::wcout << "Avg Temp:     " << live_data.curr_avg_temp << "C\n";
        std::wcout << "Hotspot Temp: " << live_data.curr_hotspot_temp << "C\n\n";
        std::wcout << "Graphics Utilization:     " << live_data.curr_graphics_utilization << "%\n";
        std::wcout << "Frame Buffer Utilization: " << live_data.curr_frame_buffer_utilization << "%\n";
        std::wcout << "Video Engine Utilization: " << live_data.curr_video_engine_utilization << "%\n";

        std::wcout << "\n-- Clock Speeds --\n";
        for (auto& clk : live_data.clks) {
            switch (clk.clk_type) {
            case GPUClockDomain::Graphics:  std::wcout << "Graphics:  "; break;
            case GPUClockDomain::Memory:    std::wcout << "Memory:    "; break;
            case GPUClockDomain::Processor: std::wcout << "Processor: "; break;
            case GPUClockDomain::Video:     std::wcout << "Video:     "; break;
            default:                        std::wcout << "Unknown:   "; break;
            }
            std::wcout << clk.clk_spd << " MHz\n";
        }

        std::wcout << "\n-- Fan Speed --\n";
        std::wcout << "Fan Speed: " << live_data.fan_speed << "\n\n";
    }
}

void NvidiaLiveGPUMetrics::checkAndHandleError(const char* custom_msg, NvAPI_Status status) {
    if (status != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(status, err_msg);
        throw std::runtime_error(custom_msg + std::string(err_msg));
    }
}

int NvidiaLiveGPUMetrics::avgTemp(const NV_GPU_THERMAL_SETTINGS& ts) const {
    int sum = 0;
    for (NvU32 i = 0; i < ts.count; i++)
        sum += ts.sensor[i].currentTemp;
    return sum / static_cast<int>(ts.count);
}

int NvidiaLiveGPUMetrics::hotspotTemp(const NV_GPU_THERMAL_SETTINGS& ts) const {
    int max = 0;
    for (NvU32 i = 0; i < ts.count; i++)
        if (ts.sensor[i].currentTemp > max)
            max = ts.sensor[i].currentTemp;
    return max;
}