#include "AMDLiveGPUMetrics.h"
#include <adl_structures.h>
#include <iostream>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
AMDLiveGPUMetrics::AMDLiveGPUMetrics(int adapter_idx, std::string name,
    std::shared_ptr<ADL> adl)
    : A_HardwareDevice(Vendor::AMD, HardwareType::GPU, std::move(name))
    , active_backend_(Backend::ADL)
    , adl_(std::move(adl))
    , adl_adapter_idx_(adapter_idx)
{
}

AMDLiveGPUMetrics::AMDLiveGPUMetrics(adlx::IADLXGPUPtr gpu,
    std::shared_ptr<ADLX> adlx)
    : A_HardwareDevice(Vendor::AMD, HardwareType::GPU, "AMD GPU")
    , active_backend_(Backend::ADLX)
    , adlx_(std::move(adlx))
    , adlx_gpu_(std::move(gpu))
{
    // Resolve a human-readable name from ADLX if available
    const char* gpu_name = nullptr;
    if (adlx_gpu_ && ADLX_SUCCEEDED(adlx_gpu_->Name(&gpu_name)) && gpu_name)
        name = gpu_name;
}

// ---------------------------------------------------------------------------
// fetchMetrics dispatch
// ---------------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchMetrics() {
    if (active_backend_ == Backend::ADL)
        fetchADLMetrics();
    else
        fetchADLXMetrics();
}

// ---------------------------------------------------------------------------
// fetchADLMetrics
// ---------------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchADLMetrics() {
    const int idx = adl_adapter_idx_;
    ADL& a = *adl_;

    int od_supported = 0, od_enabled = 0, od_version = 0;
    if (a.adl2_overdrive_caps(a.context, idx, &od_supported, &od_enabled, &od_version) != ADL_OK) {
        if (a.adl_overdrive_caps(idx, &od_supported, &od_enabled, &od_version) != ADL_OK) {
            std::cerr << "[AMDLiveGPUMetrics] Failed to retrieve ADL overdrive caps for adapter "
                << idx << "\n";
            //return;
        }
        /*std::cerr << "[AMDLiveGPUMetrics] Failed to retrieve ADL2 overdrive caps for adapter "
            << idx << "\n";
        std::cerr << "context: " << a.context << "\n";
        std::cerr << "Od supported val: " << od_supported << "\n";
        std::cerr << "Od supported val: " << od_enabled << "\n";
        std::cerr << "Od supported val: " << od_version << "\n";
        return;*/
    }

    // ---- OD8 (share-memory PMLog path) ----
    if (od_version >= 8) {
        int sharememory_supported = 0;
        if (a.adl2_od8_pmlog_sharememory_support(a.context, idx, &sharememory_supported, 0) != ADL_OK
            || sharememory_supported == ADL_ERR_NOT_SUPPORTED) {
            std::cerr << "[AMDLiveGPUMetrics] PMLog ShareMemory not supported for adapter " << idx << "\n";
            return;
        }

        ADL_D3DKMT_HANDLE device_handle = 0;
        if (a.adl2_device_pmlog_device_create(a.context, idx, &device_handle) != ADL_OK) {
            std::cerr << "[AMDLiveGPUMetrics] PMLog device create failed for adapter " << idx << "\n";
            return;
        }

        void* shared_memory = nullptr;
        if (a.adl2_od8_pmlog_sharememory_start(
            a.context, idx, 1000, -1, nullptr, &device_handle, &shared_memory, 0) != ADL_OK) {
            std::cerr << "[AMDLiveGPUMetrics] PMLog ShareMemory start failed for adapter " << idx << "\n";
            a.adl2_device_pmlog_device_destroy(a.context, device_handle);
            return;
        }

        int  sensor_count = 0;
        int* sensor_list = nullptr;
        if (a.adl2_od8_pmlogsenortype_support_get(a.context, idx, &sensor_count, &sensor_list) != ADL_OK) {
            std::cerr << "[AMDLiveGPUMetrics] PMLogSenorType_Support_Get failed for adapter " << idx << "\n";
            a.adl2_od8_pmlog_sharememory_stop(a.context, idx, &device_handle);
            a.adl2_device_pmlog_device_destroy(a.context, device_handle);
            return;
        }

        ADLPMLogDataOutput data = {};
        if (a.adl2_od8_pmlog_sharememory_read(
            a.context, idx, sensor_count, sensor_list, &shared_memory, &data) == ADL_OK) {

            for (int i = 0; i < sensor_count; ++i) {
                int   sensor_id = sensor_list[i];
                if (!data.sensors[sensor_id].supported) continue;
                float val = static_cast<float>(data.sensors[sensor_id].value);

                switch (sensor_id) {
                    // Clocks
                case PMLOG_CLK_GFXCLK:    addSensor<Sensors::SensorType::CLOCK>("Core Clock Speed", val); break;
                case PMLOG_CLK_MEMCLK:    addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed", val); break;
                case PMLOG_CLK_SOCCLK:    addSensor<Sensors::SensorType::CLOCK>("SoC Clock Speed", val); break;
                case PMLOG_CLK_UVDCLK1:   addSensor<Sensors::SensorType::CLOCK>("UVD Clock 1", val); break;
                case PMLOG_CLK_UVDCLK2:   addSensor<Sensors::SensorType::CLOCK>("UVD Clock 2", val); break;
                case PMLOG_CLK_VCECLK:    addSensor<Sensors::SensorType::CLOCK>("VCE Clock Speed", val); break;
                case PMLOG_CLK_VCNCLK:    addSensor<Sensors::SensorType::CLOCK>("VCN Clock Speed", val); break;
                case PMLOG_CLK_VCN1CLK1:  addSensor<Sensors::SensorType::CLOCK>("VCN1 Clock 1", val); break;
                case PMLOG_CLK_VCN1CLK2:  addSensor<Sensors::SensorType::CLOCK>("VCN1 Clock 2", val); break;
                case PMLOG_CLK_FCLK:      addSensor<Sensors::SensorType::CLOCK>("Fabric Clock Speed", val); break;
                case PMLOG_CLK_CPUCLK:    addSensor<Sensors::SensorType::CLOCK>("CPU Clock Speed", val); break;
                case PMLOG_BUS_SPEED:     addSensor<Sensors::SensorType::CLOCK>("PCIe Bus Speed", val); break;

                    // Temperatures
                case PMLOG_TEMPERATURE_EDGE:        addSensor<Sensors::SensorType::TEMPERATURE>("GPU Edge Temperature", val); break;
                case PMLOG_TEMPERATURE_MEM:         addSensor<Sensors::SensorType::TEMPERATURE>("Memory Temperature", val); break;
                case PMLOG_TEMPERATURE_LIQUID:      addSensor<Sensors::SensorType::TEMPERATURE>("Liquid Cooling Temperature", val); break;
                case PMLOG_TEMPERATURE_HOTSPOT:     addSensor<Sensors::SensorType::TEMPERATURE>("GPU Hotspot Temperature", val); break;
                case PMLOG_TEMPERATURE_GFX:         addSensor<Sensors::SensorType::TEMPERATURE>("GFX Temperature", val); break;
                case PMLOG_TEMPERATURE_SOC:         addSensor<Sensors::SensorType::TEMPERATURE>("SoC Temperature", val); break;
                case PMLOG_TEMPERATURE_CPU:         addSensor<Sensors::SensorType::TEMPERATURE>("CPU Temperature", val); break;
                case PMLOG_TEMPERATURE_HOTSPOT_GCD: addSensor<Sensors::SensorType::TEMPERATURE>("Hotspot GCD Temperature", val); break;
                case PMLOG_TEMPERATURE_HOTSPOT_MCD: addSensor<Sensors::SensorType::TEMPERATURE>("Hotspot MCD Temperature", val); break;

                    // Fan
                case PMLOG_FAN_RPM: addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed", val); break;

                    // Utilization
                case PMLOG_INFO_ACTIVITY_GFX: addSensor<Sensors::SensorType::USAGE>("GPU Utilization", val); break;
                case PMLOG_INFO_ACTIVITY_MEM: addSensor<Sensors::SensorType::USAGE>("Memory Utilization", val); break;

                    // Voltage
                case PMLOG_SOC_VOLTAGE: addSensor<Sensors::SensorType::VOLTAGE>("SoC Voltage", val); break;
                case PMLOG_GFX_VOLTAGE: addSensor<Sensors::SensorType::VOLTAGE>("GFX Voltage", val); break;
                case PMLOG_MEM_VOLTAGE: addSensor<Sensors::SensorType::VOLTAGE>("Memory Voltage", val); break;

                    // Power
                case PMLOG_ASIC_POWER:         addSensor<Sensors::SensorType::POWER>("ASIC Power", val); break;
                case PMLOG_SOC_POWER:          addSensor<Sensors::SensorType::POWER>("SoC Power", val); break;
                case PMLOG_GFX_POWER:          addSensor<Sensors::SensorType::POWER>("GFX Power", val); break;
                case PMLOG_CPU_POWER:          addSensor<Sensors::SensorType::POWER>("CPU Power", val); break;
                case PMLOG_BOARD_POWER:        addSensor<Sensors::SensorType::POWER>("Board Power", val); break;
                case PMLOG_SSTOTAL_POWERLIMIT: addSensor<Sensors::SensorType::POWER>("Total Power Limit", val); break;
                case PMLOG_SSAPU_POWERLIMIT:   addSensor<Sensors::SensorType::POWER>("APU Power Limit", val); break;
                case PMLOG_SSDGPU_POWERLIMIT:  addSensor<Sensors::SensorType::POWER>("dGPU Power Limit", val); break;

                    // Throttle diagnostics
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_GFX: addSensor<Sensors::SensorType::USAGE>("Throttle % (GFX Temp)", val); break;
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_MEM: addSensor<Sensors::SensorType::USAGE>("Throttle % (Mem Temp)", val); break;
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_VR:  addSensor<Sensors::SensorType::USAGE>("Throttle % (VR Temp)", val); break;
                case PMLOG_THROTTLE_PERCENTAGE_POWER:    addSensor<Sensors::SensorType::USAGE>("Throttle % (Power)", val); break;
                case PMLOG_THROTTLE_PERCENTAGE_TDC:      addSensor<Sensors::SensorType::USAGE>("Throttle % (TDC)", val); break;
                case PMLOG_THROTTLE_PERCENTAGE_VMAX:     addSensor<Sensors::SensorType::USAGE>("Throttle % (Vmax)", val); break;

                default: break;
                }
            }
        }
        else {
            std::cerr << "[AMDLiveGPUMetrics] PMLog ShareMemory read failed for adapter " << idx << "\n";
        }

        a.adl2_od8_pmlog_sharememory_stop(a.context, idx, &device_handle);
        a.adl2_device_pmlog_device_destroy(a.context, device_handle);
    }

    // ---- ODN (Overdrive N, version 7) ----
    else if (od_version == 7) {
        // Temperature
        int temp = 0;
        if (a.adl2_overdriven_temperature_get(a.context, idx, 0, &temp) == ADL_OK)
            addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature",
                static_cast<float>(temp));
        else
            std::cerr << "[AMDLiveGPUMetrics] ADL_ODN_Temperature_Get failed for adapter " << idx << "\n";

        // Clocks + utilization
        ADLODNPerformanceStatus perf = {};
        if (a.adl2_overdriven_performancestatus_get(a.context, idx, &perf) == ADL_OK) {
            if (perf.iCoreClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed",
                    static_cast<float>(perf.iCoreClock));
            if (perf.iMemoryClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed",
                    static_cast<float>(perf.iMemoryClock));
            addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization",
                static_cast<float>(perf.iGPUActivityPercent));
        }
        else {
            std::cerr << "[AMDLiveGPUMetrics] ADL_ODN_PerformanceStatus_Get failed for adapter " << idx << "\n";
        }

        // TODO: ODN fan speed — check if OD5/OD6 fan path is needed as fallback
    }

    // ---- OD6 ----
    else if (od_version == 6) {
        // Temperature
        int temp_milli = 0;
        if (a.adl2_od6_temperature_get(a.context, idx, &temp_milli) == ADL_OK)
            addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature",
                static_cast<float>(temp_milli) / 1000.0f);
        else
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD6_Temperature_Get failed for adapter " << idx << "\n";

        // Clocks + utilization
        ADLOD6CurrentStatus status = {};
        if (a.adl2_od6_currentstatus_get(a.context, idx, &status) == ADL_OK) {
            if (status.iEngineClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed",
                    static_cast<float>(status.iEngineClock) * 0.01f);
            if (status.iMemoryClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed",
                    static_cast<float>(status.iMemoryClock) * 0.01f);
            addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization",
                static_cast<float>(status.iActivityPercent));
        }
        else {
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD6_CurrentStatus_Get failed for adapter " << idx << "\n";
        }

        // Power
        int power_mw = 0;
        if (a.adl2_od6_currentpower_get(a.context, idx, 0, &power_mw) == ADL_OK)
            addSensor<Sensors::SensorType::POWER>("GPU Power Draw",
                static_cast<float>(power_mw) / 1000.0f);
        else
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD6_CurrentPower_Get failed for adapter " << idx << "\n";

        // Fan
        ADLOD6FanSpeedInfo fan = {};
        if (a.adl2_od6_fanspeed_get(a.context, idx, &fan) == ADL_OK) {
            if (fan.iSpeedType & ADL_OD6_FANSPEED_TYPE_RPM)
                addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed",
                    static_cast<float>(fan.iFanSpeedRPM));
        }
        else {
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD6_FanSpeed_Get failed for adapter " << idx << "\n";
        }
    }

    // ---- OD5 ----
    else if (od_version == 5) {
        // Temperature
        ADLTemperature temp_data = {};
        temp_data.iSize = sizeof(ADLTemperature);
        if (a.adl2_od5_temperature_get(a.context, idx, 0, &temp_data) == ADL_OK)
            addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature",
                temp_data.iTemperature / 1000.0f);
        else
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD5_Temperature_Get failed for adapter " << idx << "\n";

        // Clocks + utilization
        ADLPMActivity activity = {};
        activity.iSize = sizeof(ADLPMActivity);
        if (a.adl2_od5_currentactivity_get(a.context, idx, &activity) == ADL_OK) {
            if (activity.iEngineClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed",
                    static_cast<float>(activity.iEngineClock) * 0.01f);
            if (activity.iMemoryClock > 0)
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed",
                    static_cast<float>(activity.iMemoryClock) * 0.01f);
            addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization",
                static_cast<float>(activity.iActivityPercent));
        }
        else {
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD5_CurrentActivity_Get failed for adapter " << idx << "\n";
        }

        // Fan
        ADLFanSpeedValue fan_val = {};
        fan_val.iSize = sizeof(ADLFanSpeedValue);
        fan_val.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
        if (a.adl2_od5_fanspeed_get(a.context, idx, 0, &fan_val) == ADL_OK)
            addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed",
                static_cast<float>(fan_val.iFanSpeed));
        else
            std::cerr << "[AMDLiveGPUMetrics] ADL_OD5_FanSpeed_Get failed for adapter " << idx << "\n";
    }
    else {
        std::cerr << "[AMDLiveGPUMetrics] Unhandled Overdrive version " << od_version
            << " for adapter " << idx << "\n";
    }

    // TODO - Temp, delete later
    outputMetrics();
}

// ---------------------------------------------------------------------------
// fetchADLXMetrics
// ---------------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchADLXMetrics() {
    ADLX& ax = *adlx_;

    adlx::IADLXGPUMetricsSupportPtr metrics_support;
    ADLX_RESULT res = ax.perf_monitoring->GetSupportedGPUMetrics(adlx_gpu_, &metrics_support);
    if (ADLX_FAILED(res) || !metrics_support) {
        std::cerr << "[AMDLiveGPUMetrics] Failed to get IADLXGPUMetricsSupport (ADLX_RESULT: "
            << res << ")\n";
        return;
    }

    adlx::IADLXAllMetricsPtr all_metrics;
    res = ax.perf_monitoring->GetCurrentAllMetrics(&all_metrics);
    if (ADLX_FAILED(res) || !all_metrics) {
        std::cerr << "[AMDLiveGPUMetrics] Failed to get IADLXAllMetrics (ADLX_RESULT: "
            << res << ")\n";
        return;
    }

    adlx::IADLXGPUMetricsPtr gpu_metrics;
    res = all_metrics->GetGPUMetrics(adlx_gpu_, &gpu_metrics);
    if (ADLX_FAILED(res) || !gpu_metrics) {
        std::cerr << "[AMDLiveGPUMetrics] Failed to get IADLXGPUMetrics (ADLX_RESULT: "
            << res << ")\n";
        return;
    }

    adlx_bool supported = false;

    // Temperature
    metrics_support->IsSupportedGPUTemperature(&supported);
    if (supported) {
        adlx_double temp = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUTemperature(&temp)))
            addSensor<Sensors::SensorType::TEMPERATURE>("GPU Edge Temperature",
                static_cast<float>(temp));
    }

    metrics_support->IsSupportedGPUHotspotTemperature(&supported);
    if (supported) {
        adlx_double temp = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUHotspotTemperature(&temp)))
            addSensor<Sensors::SensorType::TEMPERATURE>("GPU Hotspot Temperature",
                static_cast<float>(temp));
    }

    // Clocks
    metrics_support->IsSupportedGPUClockSpeed(&supported);
    if (supported) {
        adlx_int clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUClockSpeed(&clk)))
            addSensor<Sensors::SensorType::CLOCK>("Core Clock Speed",
                static_cast<float>(clk));
    }

    metrics_support->IsSupportedGPUVRAMClockSpeed(&supported);
    if (supported) {
        adlx_int mem_clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUVRAMClockSpeed(&mem_clk)))
            addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed",
                static_cast<float>(mem_clk));
    }

    // Utilization
    metrics_support->IsSupportedGPUUsage(&supported);
    if (supported) {
        adlx_double usage = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUUsage(&usage)))
            addSensor<Sensors::SensorType::USAGE>("GPU Utilization",
                static_cast<float>(usage));
    }

    // VRAM utilization (derived from used / max)
    metrics_support->IsSupportedGPUVRAM(&supported);
    if (supported) {
        adlx_int vram_used_mb = 0, vram_min = 0, vram_max = 0;
        gpu_metrics->GPUVRAM(&vram_used_mb);
        metrics_support->GetGPUVRAMRange(&vram_min, &vram_max);
        if (vram_max > 0)
            addSensor<Sensors::SensorType::USAGE>("Frame Buffer Utilization",
                static_cast<float>(vram_used_mb) / static_cast<float>(vram_max) * 100.0f);
    }

    // Fan speed
    metrics_support->IsSupportedGPUFanSpeed(&supported);
    if (supported) {
        adlx_int fan_rpm = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUFanSpeed(&fan_rpm)))
            addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed",
                static_cast<float>(fan_rpm));
    }

    // Note: no direct video-engine utilization equivalent in ADLX
    // (ADL_PMLOG_INFO_ACTIVITY_UVD has no IADLXGPUMetrics counterpart)
    // TODO: revisit if a future ADLX SDK revision exposes this

    // TODO - Temp, delete later
    outputMetrics();
}