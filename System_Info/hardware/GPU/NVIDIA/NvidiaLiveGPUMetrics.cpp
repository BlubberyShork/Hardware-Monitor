#include "NvidiaLiveGPUMetrics.h"
#include <iostream>
#include <stdexcept>

// TODO - LUID and name check
NvidiaLiveGPUMetrics::NvidiaLiveGPUMetrics(NvPhysicalGpuHandle& handle)
    : _handle(handle),
      A_HardwareDevice(Vendor::NVIDIA, HardwareType::GPU, "NVIDIA GPU") {
}

NvidiaLiveGPUMetrics::~NvidiaLiveGPUMetrics() {}

void NvidiaLiveGPUMetrics::fetchMetrics() {
    if (!_handle) return;

    // Thermal
    NV_GPU_THERMAL_SETTINGS ts = {};
    ts.version = NV_GPU_THERMAL_SETTINGS_VER_2;

    NvAPI_Status status = NvAPI_GPU_GetThermalSettings(_handle, NVAPI_THERMAL_TARGET_ALL, &ts);
    checkAndHandleError("Failed to get thermal settings: ", status);

    addSensor<Sensors::SensorType::TEMPERATURE>("GPU Avg Temp", static_cast<float>(avgTemp(ts)));
    addSensor<Sensors::SensorType::TEMPERATURE>("GPU Hotspot Temp", static_cast<float>(hotspotTemp(ts)));

    // Clocks
    NV_GPU_PERF_PSTATES20_INFO pstate_info = {};
    pstate_info.version = NV_GPU_PERF_PSTATES20_INFO_VER;
    status = NvAPI_GPU_GetPstates20(_handle, &pstate_info);
    checkAndHandleError("Failed to get pstate info: ", status);

    NV_GPU_CLOCK_FREQUENCIES clk_freq = {};
    clk_freq.version = NV_GPU_CLOCK_FREQUENCIES_VER_3;
    status = NvAPI_GPU_GetAllClockFrequencies(_handle, &clk_freq);
    checkAndHandleError("Failed to get clock frequencies: ", status);

    for (NvU32 c = 0; c < pstate_info.numClocks; c++) {
        NV_GPU_PUBLIC_CLOCK_ID clk_id = pstate_info.pstates[0].clocks[c].domainId;
        if (clk_freq.domain[clk_id].bIsPresent) {
            float mhz = clk_freq.domain[clk_id].frequency * 0.001f;
            switch (clk_id) {
            case NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS:  addSensor<Sensors::SensorType::CLOCK>("GPU Graphics Clock", mhz); break;
            case NVAPI_GPU_PUBLIC_CLOCK_MEMORY:    addSensor<Sensors::SensorType::CLOCK>("GPU Memory Clock", mhz); break;
            case NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR: addSensor<Sensors::SensorType::CLOCK>("GPU Processor Clock", mhz); break;
            case NVAPI_GPU_PUBLIC_CLOCK_VIDEO:     addSensor<Sensors::SensorType::CLOCK>("GPU Video Clock", mhz); break;
            default: break;
            }
        }
    }

    // Utilization
    NV_GPU_DYNAMIC_PSTATES_INFO_EX pstates_ex = {};
    pstates_ex.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
    status = NvAPI_GPU_GetDynamicPstatesInfoEx(_handle, &pstates_ex);
    checkAndHandleError("Failed to get dynamic pstate info: ", status);
    auto& util = pstates_ex.utilization;

    if (util[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].bIsPresent)
        addSensor<Sensors::SensorType::USAGE>("GPU Graphics Utilization",
            static_cast<float>(util[NV_GPU_CLIENT_UTIL_DOMAIN_GRAPHICS].percentage));

    if (util[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].bIsPresent)
        addSensor<Sensors::SensorType::USAGE>("GPU Frame Buffer Utilization",
            static_cast<float>(util[NV_GPU_CLIENT_UTIL_DOMAIN_FRAME_BUFFER].percentage));

    if (util[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].bIsPresent)
        addSensor<Sensors::SensorType::USAGE>("GPU Video Engine Utilization",
            static_cast<float>(util[NV_GPU_CLIENT_UTIL_DOMAIN_VIDEO].percentage));

    // Fan speed TODO: update library for modern fan APIs
    NvU32 fan_spd = 0;
    if (NvAPI_GPU_GetTachReading(_handle, &fan_spd) == NVAPI_OK)
        addSensor<Sensors::SensorType::FAN_SPEED>("GPU Fan Speed", static_cast<float>(fan_spd));
    else
        addSensor<Sensors::SensorType::FAN_SPEED>("GPU Fan Speed", 0.0f);

    //outputMetrics();
}

void NvidiaLiveGPUMetrics::checkAndHandleError(const char* custom_msg, NvAPI_Status status) {
    if (status != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(status, err_msg);
        std::cerr << custom_msg << err_msg << "\n";
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
