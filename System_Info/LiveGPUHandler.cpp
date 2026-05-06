#include "LiveGPUHandler.h"

// TODO - Ensure its a nvidia GPU, also rename these functions to be for nvidia. Handle this in the constructor

LiveGPUHandler::LiveGPUHandler() {

    // TODO - Check vendor -> dxgi

    NvAPI_Status nvapi_initialization_res = NvAPI_Initialize();
    if (nvapi_initialization_res != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(nvapi_initialization_res, err_msg);
        throw std::runtime_error("NVAPI initialization failed: " + std::string(err_msg));
    }

}

LiveGPUHandler::~LiveGPUHandler() {
    NvAPI_Status nvapi_initialization_res = NvAPI_Unload();
    if (nvapi_initialization_res != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(nvapi_initialization_res, err_msg);
        throw std::runtime_error("NVAPI De-Initialization failed: " + std::string(err_msg));
    }
}

void LiveGPUHandler::fetchCurrentLiveGPUMetrics() {
    NvPhysicalGpuHandle nv_GPU_Handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32 gpu_cnt = 0;

    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(nv_GPU_Handles, &gpu_cnt);
    checkAndHandleError(status);

    std::vector<NV_GPU_THERMAL_SETTINGS> thermal_settings(gpu_cnt);
    std::vector<NV_GPU_CLOCK_FREQUENCIES> clock_frequencies(gpu_cnt);
    std::vector<NV_GPU_DYNAMIC_PSTATES_INFO_EX> p_states(gpu_cnt);

    // Zero initialize
    for (NvU32 i = 0; i < gpu_cnt; i++) {
        // TODO - Add handling for other versions, as they handle different generations of Nvidia GPUS
        thermal_settings[i] = {};
        thermal_settings[i].version = NV_GPU_THERMAL_SETTINGS_VER_2;
        clock_frequencies[i] = {};
        clock_frequencies[i].version = NV_GPU_CLOCK_FREQUENCIES_VER_3;
        clock_frequencies[i].ClockType = 0;
        p_states[i] = {};
        p_states[i].version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;

        if (nv_GPU_Handles[i] != nullptr) {
            status = NvAPI_GPU_GetThermalSettings(nv_GPU_Handles[i], NVAPI_THERMAL_TARGET_ALL, &thermal_settings[i]);
            checkAndHandleError(status);
        }

        if (nv_GPU_Handles[i] != nullptr) {
            status = NvAPI_GPU_GetAllClockFrequencies(nv_GPU_Handles[i], &clock_frequencies[i]);
            checkAndHandleError(status);
        }

        status = NvAPI_GPU_GetDynamicPstatesInfoEx(nv_GPU_Handles[i], &p_states[i]);
    }

    // TODO - Need to figure out how to process all of this data and store it in the vector. We have live data, some with multiple sensors, how do we handle that?
    // https://www.google.com/search?q=NVAPI+how+to+differentiate+different+clock+types&oq=NVAPI+how+to+differentiate+different+clock+types&gs_lcrp=EgZjaHJvbWUyBggAEEUYOTIHCAEQIRigATIHCAIQIRigATIHCAMQIRigATIHCAQQIRirAtIBCDYzMThqMGo3qAIAsAIA&sourceid=chrome&ie=UTF-8
    populateThermalData(thermal_settings);
    //populateClockData(clock_frequencies);
    //populateUtilizationData(p_states);
}

void LiveGPUHandler::checkAndHandleError(NvAPI_Status status) {
    if (status != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(status, err_msg);
        throw std::runtime_error("NVAPI initialization failed: " + std::string(err_msg));
    }
}

NvS32 LiveGPUHandler::populateThermalData(std::vector<NV_GPU_THERMAL_SETTINGS> v_thermal_settings) {
    for (int i = 0; i < v_thermal_settings.size(); i++) {
        this->all_gpu_live_data[i].curr_avg_temp = avgTemp(v_thermal_settings[i]);
        this->all_gpu_live_data[i].curr_hotspot_temp = hotspotTemp(v_thermal_settings[i]);
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

