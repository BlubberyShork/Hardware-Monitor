#include "LiveGPUHandler.h"

LiveGPUHandler::LiveGPUHandler() {
    constexpr UINT VENDOR_ID_NVIDIA = 0x10DE;
    constexpr UINT VENDOR_ID_AMD = 0x1002;

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create DXGI factory");
    }

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        if (desc.VendorId == VENDOR_ID_NVIDIA)
            vendor = Vendor::NVIDIA;
        else if (desc.VendorId == VENDOR_ID_AMD)
            vendor = Vendor::AMD;

        adapter->Release();
        break;
    }
    factory->Release();

    if (vendor == Vendor::NVIDIA) {
        NvAPI_Status res = NvAPI_Initialize();
        checkAndHandleError("NVAPI Initialization failed: ", res);
    }
    else if (vendor == Vendor::AMD) {
        // TODO - ADL initialization
    }
    else {
        throw std::runtime_error("Unsupported or no GPU vendor detected");
    }
}

LiveGPUHandler::~LiveGPUHandler() {
    if (vendor == Vendor::NVIDIA) {
        NvAPI_Unload();
    }
}

    void LiveGPUHandler::fetchCurrentLiveGPUMetrics() {
        switch (this->vendor) {
        case (Vendor::NVIDIA):
            fetchLiveNvidiaGPUMetrics();
            break;
        case (Vendor::AMD):
            // TODO 
            break;
        default:
            break;
        }
    }


void LiveGPUHandler::fetchLiveNvidiaGPUMetrics() {
    NvPhysicalGpuHandle nv_GPU_Handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32 gpu_cnt = 0;

    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(nv_GPU_Handles, &gpu_cnt);
    checkAndHandleError("Failed to enumerate physical gpus: ", status);

    for (NvU32 i = 0; i < gpu_cnt; i++) {
        NvPhysicalGpuHandle handle = nv_GPU_Handles[i];
        if (handle == nullptr) continue;

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
                GPULiveData::ClockEntry clk_entry = {};
                clk_entry.clk_type = clk_id;
                clk_entry.clk_spd = clk_freq.domain[clk_id].frequency * 0.001;
                live_data.clks.push_back(clk_entry);
            }
        }
    
        // Utilization
        NV_GPU_DYNAMIC_PSTATES_INFO_EX pstates_info_ex = {};
        pstates_info_ex.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
        status = NvAPI_GPU_GetDynamicPstatesInfoEx(handle, &pstates_info_ex);
        checkAndHandleError("Failed to get pstateEx info: ", status);

        if (pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].bIsPresent)
            live_data.curr_graphics_utilization = pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].percentage;
        if (pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].bIsPresent)
            live_data.curr_frame_buffer_utilization = pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].percentage;
        if (pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].bIsPresent)
            live_data.curr_video_engine_utilization = pstates_info_ex.utilization[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].percentage;

        // TODO - Fan speed, need to update library for modern features
        //NvU32 fan_spd = {};
        //status = NvAPI_GPU_GetTachReading(handle, &fan_spd);
        //if (status != NVAPI_OK) {
        //    fan_spd = 0;
        //}
        //live_data.fan_speed = fan_spd;

        //Finally put it all together
        all_gpu_live_data[handle] = live_data;
    }
}

void LiveGPUHandler::checkAndHandleError(const char* custom_msg, NvAPI_Status status) {
    if (status != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(status, err_msg);
        throw std::runtime_error(custom_msg + std::string(err_msg));
    }
}


NvS32 LiveGPUHandler::avgTemp(NV_GPU_THERMAL_SETTINGS thermal_settings) {
    NvS32 sum = 0;
    for (NvU32 i = 0; i < thermal_settings.count; i++) {
        sum += thermal_settings.sensor[i].currentTemp;
    }
    return sum / thermal_settings.count;
}

NvS32 LiveGPUHandler::hotspotTemp(NV_GPU_THERMAL_SETTINGS thermal_settings) {
    NvS32 max = 0;
    for (NvU32 i = 0; i < thermal_settings.count; i++) {
        if (thermal_settings.sensor[i].currentTemp > max)
            max = thermal_settings.sensor[i].currentTemp;
    }
    return max;
}

void LiveGPUHandler::outputLiveGPUMetrics() {
    for (auto& [handle, live_data] : all_gpu_live_data) {
        std::wcout << "-- Live GPU Data --\n";
        std::wcout << "Avg Temp:     "             << live_data.curr_avg_temp << "C\n";
        std::wcout << "Hotspot Temp: "             << live_data.curr_hotspot_temp << "C\n\n";

        std::wcout << "Graphics Utilization:     " << live_data.curr_graphics_utilization << "%\n";
        std::wcout << "Frame Buffer Utilization: " << live_data.curr_frame_buffer_utilization << "%\n";
        std::wcout << "Video Engine Utilization: " << live_data.curr_video_engine_utilization << "%\n";

        std::wcout << "\n-- Clock Speeds --\n";
        for (auto& clk : live_data.clks) {
            switch (clk.clk_type) {
            case NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS:  std::wcout << "Graphics:  "; break;
            case NVAPI_GPU_PUBLIC_CLOCK_MEMORY:    std::wcout << "Memory:    "; break;
            case NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR: std::wcout << "Processor: "; break;
            case NVAPI_GPU_PUBLIC_CLOCK_VIDEO:     std::wcout << "Video:     "; break;
            default:                               std::wcout << "Unknown:   "; break;
            }
            std::wcout << clk.clk_spd << " MHz\n";
        }

        std::wcout << "\n -- Fan Speed -- \n";
        std::wcout << "Fan Speed: " << live_data.fan_speed << "\n";
        std::wcout << "\n";
    }
}