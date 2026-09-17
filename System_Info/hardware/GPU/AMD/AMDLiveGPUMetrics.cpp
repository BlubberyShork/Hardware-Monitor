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

    auto validTemp  = [](float v) { return v > 0.0f && v < 150.0f; };
    auto validClock = [](float v) { return v > 0.0f && v < 10000.0f; };
    auto validPct   = [](float v) { return v >= 0.0f && v <= 100.0f; };
    auto validPower = [](float v) { return v >= 0.0f && v < 1000.0f; };
    auto validFan   = [](float v) { return v >= 0.0f && v < 20000.0f; };
    auto validVolt  = [](float v) { return v >= 0.0f && v < 3.0f; };

    int od_supported = 0, od_enabled = 0, od_version = 0;
    if (a.adl2_overdrive_caps(a.context, idx, &od_supported, &od_enabled, &od_version) != ADL_OK) {
        if (a.adl_overdrive_caps(idx, &od_supported, &od_enabled, &od_version) != ADL_OK) {
            std::cerr << "[AMDLiveGPUMetrics] Overdrive caps unavailable for adapter "
                << idx << ", trying all OD levels\n";
        }
    }
    std::cerr << "[AMDLiveGPUMetrics] Adapter " << idx
        << " OD version=" << od_version
        << " supported=" << od_supported
        << " enabled=" << od_enabled << "\n";

    // Waterfall: OD5 → OD6 → OD7 → OD8. Higher levels overwrite lower
    // via addSensor (updates existing sensors by name). Each block is
    // guarded only by null-checks on its function pointers so it runs
    // regardless of what overdrive caps reported.

    // ---- OD5 ----
    if (a.adl2_od5_temperature_get) {
        ADLTemperature temp_data = {};
        temp_data.iSize = sizeof(ADLTemperature);
        if (a.adl2_od5_temperature_get(a.context, idx, 0, &temp_data) == ADL_OK) {
            float t = temp_data.iTemperature / 1000.0f;
            if (validTemp(t))
                addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature", t);
        }
    }
    if (a.adl2_od5_currentactivity_get) {
        ADLPMActivity activity = {};
        activity.iSize = sizeof(ADLPMActivity);
        if (a.adl2_od5_currentactivity_get(a.context, idx, &activity) == ADL_OK) {
            float core = static_cast<float>(activity.iEngineClock) * 0.01f;
            float mem  = static_cast<float>(activity.iMemoryClock) * 0.01f;
            float util = static_cast<float>(activity.iActivityPercent);
            if (validClock(core))
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed", core);
            if (validClock(mem))
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed", mem);
            if (validPct(util))
                addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization", util);
        }
    }
    if (a.adl2_od5_fanspeed_get) {
        ADLFanSpeedValue fan_val = {};
        fan_val.iSize = sizeof(ADLFanSpeedValue);
        fan_val.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
        if (a.adl2_od5_fanspeed_get(a.context, idx, 0, &fan_val) == ADL_OK) {
            float rpm = static_cast<float>(fan_val.iFanSpeed);
            if (validFan(rpm))
                addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed", rpm);
        }
    }

    // ---- OD6 ----
    if (a.adl2_od6_temperature_get) {
        int temp_milli = 0;
        if (a.adl2_od6_temperature_get(a.context, idx, &temp_milli) == ADL_OK) {
            float t = static_cast<float>(temp_milli) / 1000.0f;
            if (validTemp(t))
                addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature", t);
        }
    }
    if (a.adl2_od6_currentstatus_get) {
        ADLOD6CurrentStatus status = {};
        if (a.adl2_od6_currentstatus_get(a.context, idx, &status) == ADL_OK) {
            float core = static_cast<float>(status.iEngineClock) * 0.01f;
            float mem  = static_cast<float>(status.iMemoryClock) * 0.01f;
            float util = static_cast<float>(status.iActivityPercent);
            if (validClock(core))
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed", core);
            if (validClock(mem))
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed", mem);
            if (validPct(util))
                addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization", util);
        }
    }
    if (a.adl2_od6_currentpower_get) {
        int power_mw = 0;
        if (a.adl2_od6_currentpower_get(a.context, idx, 0, &power_mw) == ADL_OK) {
            float w = static_cast<float>(power_mw) / 1000.0f;
            if (validPower(w))
                addSensor<Sensors::SensorType::POWER>("GPU Power Draw", w);
        }
    }
    if (a.adl2_od6_fanspeed_get) {
        ADLOD6FanSpeedInfo fan = {};
        if (a.adl2_od6_fanspeed_get(a.context, idx, &fan) == ADL_OK) {
            if (fan.iSpeedType & ADL_OD6_FANSPEED_TYPE_RPM) {
                float rpm = static_cast<float>(fan.iFanSpeedRPM);
                if (validFan(rpm))
                    addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed", rpm);
            }
        }
    }

    // ---- ODN (Overdrive N / version 7) ----
    if (a.adl2_overdriven_temperature_get) {
        int temp = 0;
        if (a.adl2_overdriven_temperature_get(a.context, idx, 0, &temp) == ADL_OK) {
            float t = static_cast<float>(temp) / 1000.0f;
            if (validTemp(t))
                addSensor<Sensors::SensorType::TEMPERATURE>("Average Core Temperature", t);
        }
    }
    if (a.adl2_overdriven_performancestatus_get) {
        ADLODNPerformanceStatus perf = {};
        if (a.adl2_overdriven_performancestatus_get(a.context, idx, &perf) == ADL_OK) {
            float core = static_cast<float>(perf.iCoreClock) * 0.01f;
            float mem  = static_cast<float>(perf.iMemoryClock) * 0.01f;
            float util = static_cast<float>(perf.iGPUActivityPercent);
            if (validClock(core))
                addSensor<Sensors::SensorType::CLOCK>("Core / Graphics Clock Speed", core);
            if (validClock(mem))
                addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed", mem);
            if (validPct(util))
                addSensor<Sensors::SensorType::USAGE>("Core / Graphics Utilization", util);
        }
    }

    // ---- OD8 (share-memory PMLog path) ----
    if (od_version >= 8
        && a.adl2_od8_pmlog_sharememory_support
        && a.adl2_od8_pmlog_sharememory_start
        && a.adl2_od8_pmlog_sharememory_read
        && a.adl2_od8_pmlog_sharememory_stop
        && a.adl2_device_pmlog_device_create
        && a.adl2_device_pmlog_device_destroy
        && a.adl2_od8_pmlogsenortype_support_get) {

        int sharememory_supported = 0;
        if (a.adl2_od8_pmlog_sharememory_support(a.context, idx, &sharememory_supported, 0) == ADL_OK
            && sharememory_supported != ADL_ERR_NOT_SUPPORTED) {

            ADL_D3DKMT_HANDLE device_handle = 0;
            if (a.adl2_device_pmlog_device_create(a.context, idx, &device_handle) == ADL_OK) {

                void* shared_memory = nullptr;
                if (a.adl2_od8_pmlog_sharememory_start(
                    a.context, idx, 1000, -1, nullptr, &device_handle, &shared_memory, 0) == ADL_OK) {

                    int  sensor_count = 0;
                    int* sensor_list = nullptr;
                    if (a.adl2_od8_pmlogsenortype_support_get(a.context, idx, &sensor_count, &sensor_list) == ADL_OK) {

                        ADLPMLogDataOutput data = {};
                        if (a.adl2_od8_pmlog_sharememory_read(
                            a.context, idx, sensor_count, sensor_list, &shared_memory, &data) == ADL_OK) {

                            for (int i = 0; i < sensor_count; ++i) {
                                int   sensor_id = sensor_list[i];
                                if (!data.sensors[sensor_id].supported) continue;
                                float val = static_cast<float>(data.sensors[sensor_id].value);

                                using sensor_t = Sensors::SensorType;

                                switch (sensor_id) {
                                case PMLOG_CLK_GFXCLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("Core Clock Speed", val); break;
                                case PMLOG_CLK_MEMCLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("Memory Clock Speed", val); break;
                                case PMLOG_CLK_SOCCLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("SoC Clock Speed", val); break;
                                case PMLOG_CLK_UVDCLK1:   if (validClock(val)) addSensor<sensor_t::CLOCK>("UVD Clock 1", val); break;
                                case PMLOG_CLK_UVDCLK2:   if (validClock(val)) addSensor<sensor_t::CLOCK>("UVD Clock 2", val); break;
                                case PMLOG_CLK_VCECLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("VCE Clock Speed", val); break;
                                case PMLOG_CLK_VCNCLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("VCN Clock Speed", val); break;
                                case PMLOG_CLK_VCN1CLK1:  if (validClock(val)) addSensor<sensor_t::CLOCK>("VCN1 Clock 1", val); break;
                                case PMLOG_CLK_VCN1CLK2:  if (validClock(val)) addSensor<sensor_t::CLOCK>("VCN1 Clock 2", val); break;
                                case PMLOG_CLK_FCLK:      if (validClock(val)) addSensor<sensor_t::CLOCK>("Fabric Clock Speed", val); break;
                                case PMLOG_CLK_CPUCLK:    if (validClock(val)) addSensor<sensor_t::CLOCK>("CPU Clock Speed", val); break;
                                case PMLOG_BUS_SPEED:     if (validClock(val)) addSensor<sensor_t::CLOCK>("PCIe Bus Speed", val); break;

                                case PMLOG_TEMPERATURE_EDGE:        if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("GPU Edge Temperature", val); break;
                                case PMLOG_TEMPERATURE_MEM:         if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("Memory Temperature", val); break;
                                case PMLOG_TEMPERATURE_LIQUID:      if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("Liquid Cooling Temperature", val); break;
                                case PMLOG_TEMPERATURE_HOTSPOT:     if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("GPU Hotspot Temperature", val); break;
                                case PMLOG_TEMPERATURE_GFX:         if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("GFX Temperature", val); break;
                                case PMLOG_TEMPERATURE_SOC:         if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("SoC Temperature", val); break;
                                case PMLOG_TEMPERATURE_CPU:         if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("CPU Temperature", val); break;
                                case PMLOG_TEMPERATURE_HOTSPOT_GCD: if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("Hotspot GCD Temperature", val); break;
                                case PMLOG_TEMPERATURE_HOTSPOT_MCD: if (validTemp(val)) addSensor<sensor_t::TEMPERATURE>("Hotspot MCD Temperature", val); break;

                                case PMLOG_FAN_RPM: if (validFan(val)) addSensor<sensor_t::FAN_SPEED>("Fan Speed", val); break;

                                case PMLOG_INFO_ACTIVITY_GFX: if (validPct(val)) addSensor<sensor_t::USAGE>("GPU Utilization", val); break;
                                case PMLOG_INFO_ACTIVITY_MEM: if (validPct(val)) addSensor<sensor_t::USAGE>("Memory Utilization", val); break;

                                case PMLOG_SOC_VOLTAGE: if (validVolt(val)) addSensor<sensor_t::VOLTAGE>("SoC Voltage", val); break;
                                case PMLOG_GFX_VOLTAGE: if (validVolt(val)) addSensor<sensor_t::VOLTAGE>("GFX Voltage", val); break;
                                case PMLOG_MEM_VOLTAGE: if (validVolt(val)) addSensor<sensor_t::VOLTAGE>("Memory Voltage", val); break;

                                case PMLOG_ASIC_POWER:         if (validPower(val)) addSensor<sensor_t::POWER>("ASIC Power", val); break;
                                case PMLOG_SOC_POWER:          if (validPower(val)) addSensor<sensor_t::POWER>("SoC Power", val); break;
                                case PMLOG_GFX_POWER:          if (validPower(val)) addSensor<sensor_t::POWER>("GFX Power", val); break;
                                case PMLOG_CPU_POWER:          if (validPower(val)) addSensor<sensor_t::POWER>("CPU Power", val); break;
                                case PMLOG_BOARD_POWER:        if (validPower(val)) addSensor<sensor_t::POWER>("Board Power", val); break;
                                case PMLOG_SSTOTAL_POWERLIMIT: if (validPower(val)) addSensor<sensor_t::POWER>("Total Power Limit", val); break;
                                case PMLOG_SSAPU_POWERLIMIT:   if (validPower(val)) addSensor<sensor_t::POWER>("APU Power Limit", val); break;
                                case PMLOG_SSDGPU_POWERLIMIT:  if (validPower(val)) addSensor<sensor_t::POWER>("dGPU Power Limit", val); break;

                                case PMLOG_THROTTLE_PERCENTAGE_TEMP_GFX: if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (GFX Temp)", val); break;
                                case PMLOG_THROTTLE_PERCENTAGE_TEMP_MEM: if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (Mem Temp)", val); break;
                                case PMLOG_THROTTLE_PERCENTAGE_TEMP_VR:  if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (VR Temp)", val); break;
                                case PMLOG_THROTTLE_PERCENTAGE_POWER:    if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (Power)", val); break;
                                case PMLOG_THROTTLE_PERCENTAGE_TDC:      if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (TDC)", val); break;
                                case PMLOG_THROTTLE_PERCENTAGE_VMAX:     if (validPct(val)) addSensor<sensor_t::USAGE>("Throttle % (Vmax)", val); break;

                                default: break;
                                }
                            }
                        }
                    }
                    a.adl2_od8_pmlog_sharememory_stop(a.context, idx, &device_handle);
                }
                a.adl2_device_pmlog_device_destroy(a.context, device_handle);
            }
        }
    }
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
