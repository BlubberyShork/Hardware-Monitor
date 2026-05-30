#include "AMDLiveGPUMetrics.h"
#include <iostream>
#include <stdexcept>
#include <windows.h>

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------
AMDLiveGPUMetrics::AMDLiveGPUMetrics(LUID luid, std::wstring dxgi_name)
    : A_HardwareDevice(Vendor::AMD, HardwareType::GPU, "AMD GPU"), 
      luid(luid), dxgi_name(std::move(dxgi_name)) {

    name = std::string(dxgi_name.begin(), dxgi_name.end());
    try {
        initADL();
        // TODO -> caller of this constructor (later will be AMDCollection, should be responsible for calling fetchMetrics, which THAT should
            //  call enumerate GPUs)
        enumerateADLAdapters();
        active_backend = Backend::ADL;
        initialized = true;
    }
    catch (const std::exception& adl_err) {
        std::cerr << "ADL unavailable (" << adl_err.what()
            << "), falling back to ADLX\n";
        gpus.clear();
        try {
            initSystem();
            initPerfMonitoring();

            // TODO -> Same here
            enumerateGPUs();
            active_backend = Backend::ADLX;
            initialized = true;
        }
        catch (const std::exception& adlx_err) {
            std::cerr << "ADLX fallback also unavailable (" << adlx_err.what()
                << "), AMD GPU metrics disabled\n";
        }
    }
}

AMDLiveGPUMetrics::~AMDLiveGPUMetrics() {
    if (active_backend == Backend::ADL) {
        shutdownADL();
    } else {
        gpus.clear();
        if (perf_monitoring) perf_monitoring->Release();
        adlx_helper.Terminate();
    }
}

// -----------------------------------------------------------------------
// Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchMetrics() {
    if (!initialized) return;

    if (active_backend == Backend::ADLX) {
        for (size_t i = 0; i < gpus.size(); i++)
            fetchGPUMetrics(gpus[i]);
    }
    else {
        for (size_t i = 0; i < adl_adapter_indices.size(); i++)
            fetchADLGPUMetrics(adl_adapter_indices[i]);
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
}

// -----------------------------------------------------------------------
// ADLX Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchGPUMetrics(adlx::IADLXGPU* gpu) {

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
            ;
            //live_data.curr_avg_temp = static_cast<int>(temp);
    }

    metrics_support->IsSupportedGPUHotspotTemperature(&supported);
    if (supported) {
        adlx_double temp = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUHotspotTemperature(&temp)))
            ;
            //live_data.curr_hotspot_temp = static_cast<int>(temp);
    }

    // Clocks
    metrics_support->IsSupportedGPUClockSpeed(&supported);
    if (supported) {
        adlx_int clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUClockSpeed(&clk)))
            ;
            //live_data.clks.push_back({ GPUClockDomain::Graphics, static_cast<double>(clk) });
    }

    metrics_support->IsSupportedGPUVRAMClockSpeed(&supported);
    if (supported) {
        adlx_int mem_clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUVRAMClockSpeed(&mem_clk)))
            ;
            //live_data.clks.push_back({ GPUClockDomain::Memory, static_cast<double>(mem_clk) });
    }

    // Utilization
    metrics_support->IsSupportedGPUUsage(&supported);
    if (supported) {
        adlx_double usage = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUUsage(&usage)))
            ;
            //live_data.curr_graphics_utilization = static_cast<unsigned int>(usage);
    }

    adlx_int vram_used_mb = 0;
    adlx_int vram_min = 0, vram_max = 0;

    metrics_support->IsSupportedGPUVRAM(&supported);
    if (supported) {
        gpu_metrics->GPUVRAM(&vram_used_mb);
        metrics_support->GetGPUVRAMRange(&vram_min, &vram_max);

        if (vram_max > 0)
            ;
            //live_data.curr_frame_buffer_utilization = static_cast<unsigned int>(
            //    (static_cast<double>(vram_used_mb) / vram_max) * 100.0
            //);
    }

    // No direct video engine utilization equivalent in ADLX
    // ADL_PMLOG_INFO_ACTIVITY_UVD has no IADLXGPUMetrics counterpart
    // TODO - revisit if ADLX exposes this in a future SDK revision
   // live_data.curr_video_engine_utilization = 0;

    // Fan speed
    metrics_support->IsSupportedGPUFanSpeed(&supported);
    if (supported) {
        adlx_int fan_rpm = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUFanSpeed(&fan_rpm)))
            ;
            //live_data.fan_speed = static_cast<unsigned int>(fan_rpm);
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
        FARPROC func = GetProcAddress(adl_module, name);
        if (!func)
            throw std::runtime_error(std::string("ADL symbol not found: ") + name);
        return func;
    };

    // Context / lifecycle
    adl2_main_control_create = reinterpret_cast<ADL2_MAIN_CONTROL_CREATE>(resolve("ADL2_Main_Control_Create"));
    adl2_main_control_destroy = reinterpret_cast<ADL2_MAIN_CONTROL_DESTROY>(resolve("ADL2_Main_Control_Destroy"));

    // Adapter enumeration
    adl2_adapter_numberofadapters_get = reinterpret_cast<ADL2_ADAPTER_NUMBEROFADAPTERS_GET>(resolve("ADL2_Adapter_NumberOfAdapters_Get"));
    adl2_adapter_adapterinfo_get = reinterpret_cast<ADL2_ADAPTER_ADAPTERINFO_GET>(resolve("ADL2_Adapter_AdapterInfo_Get"));
    // unused id get
    adl2_adapter_id_get = reinterpret_cast<ADL2_ADAPTER_ID_GET>(resolve("ADL2_Adapter_ID_Get"));
    adl2_adapter_active_get = reinterpret_cast<ADL2_ADAPTER_ACTIVE_GET> (resolve("ADL2_Adapter_Active_Get"));

    // Memory
    adl2_adapter_vramusage_get = reinterpret_cast<ADL2_ADAPTER_VRAMUSAGE_GET>(resolve("ADL2_Adapter_VRAMUsage_Get"));
    adl2_adapter_dedicatedvramusage_get = reinterpret_cast<ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET>(resolve("ADL2_Adapter_DedicatedVRAMUsage_Get"));
    adl2_adapter_memoryinfox4_get = reinterpret_cast<ADL2_ADAPTER_MEMORYINFOX4_GET>(resolve("ADL2_Adapter_MemoryInfoX4_Get"));

    // Overdrive caps / version
    adl2_overdrive_caps = reinterpret_cast<ADL2_OVERDRIVE_CAPS>(resolve("ADL2_Overdrive_Caps"));

    // Overdrive 5
    adl2_od5_odparameters_get = reinterpret_cast<ADL2_OD5_ODPARAMETERS_GET>(resolve("ADL2_Overdrive5_ODParameters_Get"));
    adl2_od5_currentactivity_get = reinterpret_cast<ADL2_OD5_CURRENTACTIVITY_GET>(resolve("ADL2_Overdrive5_CurrentActivity_Get"));
    adl2_od5_temperature_get = reinterpret_cast<ADL2_OD5_TEMPERATURE_GET>(resolve("ADL2_Overdrive5_Temperature_Get"));
    adl2_od5_fanspeed_get = reinterpret_cast<ADL2_OD5_FANSPEED_GET>(resolve("ADL2_Overdrive5_FanSpeed_Get"));
    adl2_od5_fanspeedinfo_get = reinterpret_cast<ADL2_OD5_FANSPEEDINFO_GET>(resolve("ADL2_Overdrive5_FanSpeedInfo_Get"));

    // Overdrive 6
    adl2_od6_capabilities_get = reinterpret_cast<ADL2_OD6_CAPABILITIES_GET>(resolve("ADL2_Overdrive6_Capabilities_Get"));
    adl2_od6_currentpower_get = reinterpret_cast<ADL2_OD6_CURRENTPOWER_GET>(resolve("ADL2_Overdrive6_CurrentPower_Get"));
    adl2_od6_temperature_get = reinterpret_cast<ADL2_OD6_TEMPERATURE_GET>(resolve("ADL2_Overdrive6_Temperature_Get"));
    adl2_od6_currentstatus_get = reinterpret_cast<ADL2_OD6_CURRENTSTATUS_GET>(resolve("ADL2_Overdrive6_CurrentStatus_Get"));
    adl2_od6_fanspeed_get = reinterpret_cast<ADL2_OD6_FANSPEED_GET>(resolve("ADL2_Overdrive6_FanSpeed_Get"));

    // Overdrive N
    adl2_overdriven_temperature_get = reinterpret_cast<ADL2_OVERDRIVEN_TEMPERATURE_GET>(resolve("ADL2_OverdriveN_Temperature_Get"));
    adl2_overdriven_performancestatus_get = reinterpret_cast<ADL2_OVERDRIVEN_PERFORMANCESTATUS_GET>(resolve("ADL2_OverdriveN_PerformanceStatus_Get"));

    // PMLog
    adl2_od8_pmlogsenortype_support_get = reinterpret_cast<ADL2_OD8_PMLOGSENORTYPE_SUPPORT_GET>(resolve("ADL2_Overdrive8_PMLogSenorType_Support_Get"));
    //adl2_adapter_pmlog_start = reinterpret_cast<ADL2_ADAPTER_PMLOG_START>(resolve("ADL2_Adapter_PMLog_Start"));
    //adl2_adapter_pmlog_stop = reinterpret_cast<ADL2_ADAPTER_PMLOG_STOP>(resolve("ADL2_Adapter_PMLog_Stop"));
    adl2_od8_pmlog_sharememory_start = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_START>(resolve("ADL2_Overdrive8_ShareMemory_Start"));
    adl2_od8_pmlog_sharememory_stop = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_STOP>(resolve("ADL2_Overdrive8_ShareMemory_Stop"));
    adl2_od8_pmlog_sharememory_support = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_SUPPORT>(resolve("ADL2_Overdrive8_ShareMemory_Support"));
    adl2_od8_pmlog_sharememory_read = reinterpret_cast<ADL2_OD8_PMLOG_SHAREMEMORY_READ>(resolve("ADL2_Overdrive8_ShareMemory_Read"));
    adl2_device_pmlog_device_create = reinterpret_cast<ADL2_DEVICE_PMLOG_DEVICE_CREATE>(resolve("ADL2_Device_PMLog_Device_Create"));
    adl2_device_pmlog_device_destroy = reinterpret_cast<ADL2_DEVICE_PMLOG_DEVICE_DESTROY>(resolve("ADL2_Device_PMLog_Device_Destroy"));

    // Frame metrics
    adl2_adapter_framemetrics_caps = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_CAPS>(resolve("ADL2_Adapter_FrameMetrics_Caps"));
    adl2_adapter_framemetrics_get = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_GET>(resolve("ADL2_Adapter_FrameMetrics_Get"));
    adl2_adapter_framemetrics_start = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_START>(resolve("ADL2_Adapter_FrameMetrics_Start"));
    adl2_adapter_framemetrics_stop = reinterpret_cast<ADL2_ADAPTER_FRAMEMETRICS_STOP>(resolve("ADL2_Adapter_FrameMetrics_Stop"));

    int res = adl2_main_control_create(adlMallocCallback, 1 /*iEnumConnectedAdapters*/, _context);
    if (res != ADL_OK)
        throw std::runtime_error("ADL_Main_Control_Create failed, ADL_RESULT: " + std::to_string(res));
}

void AMDLiveGPUMetrics::shutdownADL() {
    adl_adapter_indices.clear();

    if (adl2_main_control_destroy && _context) {
        adl2_main_control_destroy(_context);
        _context = nullptr;
    }

    if (adl_module) {
        FreeLibrary(adl_module);
        adl_module = nullptr;
    }
}

// -----------------------------------------------------------------------
// ADL Enumerate
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::enumerateADLAdapters() {
    int res = adl2_adapter_numberofadapters_get(_context, &adl_adapter_cnt);
    if (res != ADL_OK || adl_adapter_cnt <= 0)
        throw std::runtime_error("ADL_Adapter_NumberOfAdapters_Get failed or returned 0 adapters, ADL_RESULT: "
            + std::to_string(res));

    std::vector<AdapterInfo> adapter_info(adl_adapter_cnt);
    res = adl2_adapter_adapterinfo_get(_context, adapter_info.data(), static_cast<int>(sizeof(AdapterInfo) * adl_adapter_cnt));
    if (res != ADL_OK)
        throw std::runtime_error("ADL_Adapter_AdapterInfo_Get failed, ADL_RESULT: " + std::to_string(res));

    // Deduplicate: one logical GPU can appear as multiple adapters.
    // Track seen bus numbers so we register each physical card only once.
    std::vector<int> seen_bus_numbers;

    for (int i = 0; i < adl_adapter_cnt; i++) {
        int active = 0;
        if (adl2_adapter_active_get(_context, i, &active) != ADL_OK || !active)
            continue;

        int bus = adapter_info[i].iBusNumber;
        if (std::find(seen_bus_numbers.begin(), seen_bus_numbers.end(), bus)
            != seen_bus_numbers.end())
            continue;

        seen_bus_numbers.push_back(bus);
        adl_adapter_indices.push_back(i);

        // TODO -> A_HardwareDevice needs to be edited heavily since each hardware device may find multiple active gpus in their slots
        //  DXGI will allow us to track this early before we begin and store it in a list, looping through each and finding the right vendor for each with their own handling        
        //  Will need consideration on how to ensure no duplication in the case of multiple AMD gpus on this specific loop
        //  Probably using a map with the handle, like we had in Nvidia's pipeline
        this->name = adapter_info[i].strAdapterName; 
    }

    if (adl_adapter_indices.empty())
        throw std::runtime_error("No active ADL adapters found after enumeration");
}

// -----------------------------------------------------------------------
// ADL Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchADLGPUMetrics(int adapter_idx) {

    int od_supported, od_enabled, od_version;
    if (adl2_overdrive_caps(_context, adapter_idx, &od_supported, &od_enabled, &od_version) != ADL_OK) {
        std::cerr << "Failed to retrieve overdrive caps, ADL hardware telemetry failed \n";
        // Going beyond this point would be messy and unreliable
        return;
    }

    // TODO - Ensure check with od_supported and od_enabled
    if (od_version >= 8) {
        int sharememory_supported = 0;
        if (adl2_od8_pmlog_sharememory_support(_context, adapter_idx, &sharememory_supported, 0) != ADL_OK) {
            std::cerr << "ADL_Overdrive8_ShareMemory_Support failed for adapter " << adapter_idx << "\n";
            return;
        }
        if (sharememory_supported == ADL_ERR_NOT_SUPPORTED) {
            std::cerr << "ADL_Overdrive8_ShareMemory_Support failed for adapter " << adapter_idx << "\n";
            return;
        }

        ADL_D3DKMT_HANDLE device_handle = 0;
        if (adl2_device_pmlog_device_create(_context, adapter_idx, &device_handle) != ADL_OK) {
            std::cerr << "ADL_Device_PMLog_Device_Create failed for adapter " << adapter_idx << "\n";
            return;
        }

        ADLPMLogStartOutput start_output = {};
        void* shared_memory = NULL;
        if (adl2_od8_pmlog_sharememory_start(_context, adapter_idx, 1000, -1, NULL, &device_handle, &shared_memory, 0) != ADL_OK) {
            std::cerr << "ADL_Adapter_PMLog_Start failed for adapter " << adapter_idx << "\n";
            adl2_device_pmlog_device_destroy(_context, device_handle);
            return;
        }

        int sensor_count;
        int* sensor_list;
        if (ADL_OK != adl2_od8_pmlogsenortype_support_get(_context, adapter_idx, &sensor_count, &sensor_list)) {
            std::cerr << "adl2_od8_pmlogsensortype_support_get failed\n";
            return;
        }

        ADLPMLogDataOutput data;
        memset(&data, 0, sizeof(ADLPMLogDataOutput));
        if (adl2_od8_pmlog_sharememory_read(_context, adapter_idx, sensor_count,
            sensor_list, &shared_memory, &data) == ADL_OK) {

            for (int i = 0; i < sensor_count; i++) {
                int sensor_id = sensor_list[i];
                if (!data.sensors[sensor_id].supported) continue;
                float val = static_cast<float>(data.sensors[sensor_id].value);

                switch (sensor_id) {
                    // Clocks
                case PMLOG_CLK_GFXCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("Core Clock Speed", val);
                    break;
                case PMLOG_CLK_MEMCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("Memory Clock Speed", val);
                    break;
                case PMLOG_CLK_SOCCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("SoC Clock Speed", val);
                    break;
                case PMLOG_CLK_UVDCLK1:
                    this->addSensor<Sensors::SensorType::CLOCK>("UVD Clock 1", val);
                    break;
                case PMLOG_CLK_UVDCLK2:
                    this->addSensor<Sensors::SensorType::CLOCK>("UVD Clock 2", val);
                    break;
                case PMLOG_CLK_VCECLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("VCE Clock Speed", val);
                    break;
                case PMLOG_CLK_VCNCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("VCN Clock Speed", val);
                    break;
                case PMLOG_CLK_VCN1CLK1:
                    this->addSensor<Sensors::SensorType::CLOCK>("VCN1 Clock 1", val);
                    break;
                case PMLOG_CLK_VCN1CLK2:
                    this->addSensor<Sensors::SensorType::CLOCK>("VCN1 Clock 2", val);
                    break;
                case PMLOG_CLK_FCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("Fabric Clock Speed", val);
                    break;
                case PMLOG_CLK_CPUCLK:
                    this->addSensor<Sensors::SensorType::CLOCK>("CPU Clock Speed", val);
                    break;
                case PMLOG_BUS_SPEED:
                    this->addSensor<Sensors::SensorType::CLOCK>("PCIe Bus Speed", val);
                    break;

                    // Temperatures
                case PMLOG_TEMPERATURE_EDGE:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("GPU Edge Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_MEM:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("Memory Temperature", val);
                    break;

                case PMLOG_TEMPERATURE_LIQUID:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("Liquid Cooling Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_HOTSPOT:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("GPU Hotspot Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_GFX:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("GFX Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_SOC:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("SoC Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_CPU:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("CPU Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_HOTSPOT_GCD:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("Hotspot GCD Temperature", val);
                    break;
                case PMLOG_TEMPERATURE_HOTSPOT_MCD:
                    this->addSensor<Sensors::SensorType::TEMPERATURE>("Hotspot MCD Temperature", val);
                    break;

                    // Fan
                case PMLOG_FAN_RPM:
                    this->addSensor<Sensors::SensorType::FAN_SPEED>("Fan Speed", val);
                    break;

                    // Utilization
                case PMLOG_INFO_ACTIVITY_GFX:
                    this->addSensor<Sensors::SensorType::USAGE>("GPU Utilization", val);
                    break;
                case PMLOG_INFO_ACTIVITY_MEM:
                    this->addSensor<Sensors::SensorType::USAGE>("Memory Utilization", val);
                    break;

                    // Voltage
                case PMLOG_SOC_VOLTAGE:
                    this->addSensor<Sensors::SensorType::VOLTAGE>("SoC Voltage", val);
                    break;
                case PMLOG_GFX_VOLTAGE:
                    this->addSensor<Sensors::SensorType::VOLTAGE>("GFX Voltage", val);
                    break;
                case PMLOG_MEM_VOLTAGE:
                    this->addSensor<Sensors::SensorType::VOLTAGE>("Memory Voltage", val);
                    break;

                    // Power
                case PMLOG_ASIC_POWER:
                    this->addSensor<Sensors::SensorType::POWER>("ASIC Power", val);
                    break;
                case PMLOG_SOC_POWER:
                    this->addSensor<Sensors::SensorType::POWER>("SoC Power", val);
                    break;
                case PMLOG_GFX_POWER:
                    this->addSensor<Sensors::SensorType::POWER>("GFX Power", val);
                    break;
                case PMLOG_CPU_POWER:
                    this->addSensor<Sensors::SensorType::POWER>("CPU Power", val);
                    break;
                case PMLOG_BOARD_POWER:
                    this->addSensor<Sensors::SensorType::POWER>("Board Power", val);
                    break;
                case PMLOG_SSTOTAL_POWERLIMIT:
                    this->addSensor<Sensors::SensorType::POWER>("Total Power Limit", val);
                    break;
                case PMLOG_SSAPU_POWERLIMIT:
                    this->addSensor<Sensors::SensorType::POWER>("APU Power Limit", val);
                    break;
                case PMLOG_SSDGPU_POWERLIMIT:
                    this->addSensor<Sensors::SensorType::POWER>("dGPU Power Limit", val);
                    break;

                    // Throttle percentages � useful for diagnostics
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_GFX:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (GFX Temp)", val);
                    break;
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_MEM:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (Mem Temp)", val);
                    break;
                case PMLOG_THROTTLE_PERCENTAGE_TEMP_VR:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (VR Temp)", val);
                    break;
                case PMLOG_THROTTLE_PERCENTAGE_POWER:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (Power)", val);
                    break;
                case PMLOG_THROTTLE_PERCENTAGE_TDC:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (TDC)", val);
                    break;
                case PMLOG_THROTTLE_PERCENTAGE_VMAX:
                    this->addSensor<Sensors::SensorType::USAGE>("Throttle % (Vmax)", val);
                    break;

                default:
                    break;
                }
            }
        }
        else {
            std::cerr << "ADL_OD8_PMLog_ShareMemory_Read failed for adapter " << adapter_idx << "\n";
        }

        adl2_od8_pmlog_sharememory_stop(_context, adapter_idx, &device_handle);
        adl2_device_pmlog_device_destroy(_context, device_handle);
    }
    else if (od_version == 7) { // OdN
        // Temperature
        {
            int temp = 0;
            if (adl2_overdriven_temperature_get(_context, adapter_idx, 0, &temp) == ADL_OK) {
                this->addSensor<Sensors::SensorType::TEMPERATURE>(
                    "Average Core Temperature",
                    static_cast<float>(temp)  // ODN returns degrees C directly
                );
            }
            else {
                std::cerr << "ADL_ODN_Temperature_Get failed for adapter " << adapter_idx << "\n";
            }
        }

        // Clock speeds & Utilization
        {
            ADLODNPerformanceStatus perf = {};
            if (adl2_overdriven_performancestatus_get(_context, adapter_idx, &perf) == ADL_OK) {
                if (perf.iCoreClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Core / Graphics Clock Speed",
                        static_cast<float>(perf.iCoreClock)  // ODN returns MHz directly
                    );
                if (perf.iMemoryClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Memory Clock Speed",
                        static_cast<float>(perf.iMemoryClock)
                    );
                this->addSensor<Sensors::SensorType::USAGE>(
                    "Core / Graphics Utilization",
                    static_cast<float>(perf.iGPUActivityPercent)
                );
            }
            else {
                std::cerr << "ADL_ODN_PerformanceStatus_Get failed for adapter " << adapter_idx << "\n";
            }
        }

        // TODO - Check if fan speed exists for odn, otherwise use od5 or 6
    }
    else if (od_version == 6) {
        // Temperature
        {
            int temp_milli = 0;
            if (adl2_od6_temperature_get(_context, adapter_idx, &temp_milli) == ADL_OK) {
                this->addSensor<Sensors::SensorType::TEMPERATURE>(
                    "Average Core Temperature",
                    static_cast<float>(temp_milli) / 1000.0f
                );
            }
            else {
                std::cerr << "ADL_OD6_Temperature_Get failed for adapter " << adapter_idx << "\n";
            }
        }
        // Clocks & Utilization
        {
            ADLOD6CurrentStatus status = {};
            if (adl2_od6_currentstatus_get(_context, adapter_idx, &status) == ADL_OK) {
                if (status.iEngineClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Core / Graphics Clock Speed",
                        static_cast<float>(status.iEngineClock) * 0.01f
                    );
                if (status.iMemoryClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Memory Clock Speed",
                        static_cast<float>(status.iMemoryClock) * 0.01f
                    );
                this->addSensor<Sensors::SensorType::USAGE>(
                    "Core / Graphics Utilization",
                    static_cast<float>(status.iActivityPercent)
                );
            }
            else {
                std::cerr << "ADL_OD6_CurrentStatus_Get failed for adapter " << adapter_idx << "\n";
            }
        }
        // Power
        {
            int power_mw = 0;
            if (adl2_od6_currentpower_get(_context, adapter_idx, 0, &power_mw) == ADL_OK) {
                this->addSensor<Sensors::SensorType::POWER>(
                    "GPU Power Draw",
                    static_cast<float>(power_mw) / 1000.0f
                );
            }
            else {
                std::cerr << "ADL_OD6_CurrentPower_Get failed for adapter " << adapter_idx << "\n";
            }
        }
        // Fan
        {
            ADLOD6FanSpeedInfo fan = {};
            if (adl2_od6_fanspeed_get(_context, adapter_idx, &fan) == ADL_OK) {
                if (fan.iSpeedType & ADL_OD6_FANSPEED_TYPE_RPM)
                    this->addSensor<Sensors::SensorType::FAN_SPEED>(
                        "Fan Speed",
                        static_cast<float>(fan.iFanSpeedRPM)
                    );
            }
            else {
                std::cerr << "ADL_OD6_FanSpeed_Get failed for adapter " << adapter_idx << "\n";
            }
        }
    }
    else if (od_version == 5) {
        // Temperature
        {
            ADLTemperature temp_data = {};
            temp_data.iSize = sizeof(ADLTemperature);
            if (adl2_od5_temperature_get(_context, adapter_idx, 0, &temp_data) == ADL_OK) {
                // ADL returns millidegrees C
                float temp_c = temp_data.iTemperature / 1000;
                // ADL OD5 exposes one sensor; no distinct hotspot
                this->addSensor<Sensors::SensorType::TEMPERATURE>(
                    "Average Core Temperature", 
                    temp_c
                );
            }
            else {
                std::cerr << "ADL_OD5_Temperature_Get failed for adapter " << adapter_idx << "\n";
            }
        }

        // Clocks + Utilization !TODO - expand on this 
        {
            ADLPMActivity activity = {};
            activity.iSize = sizeof(ADLPMActivity);
            if (adl2_od5_currentactivity_get(_context, adapter_idx, &activity) == ADL_OK) {
                if (activity.iEngineClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Core / Graphics Clock Speed",
                        static_cast<float>(activity.iEngineClock) * 0.01 // 10 kHz -> MHz
                    );

                if (activity.iMemoryClock > 0)
                    this->addSensor<Sensors::SensorType::CLOCK>(
                        "Memory Clock Speed",
                        static_cast<float>(activity.iMemoryClock) * 0.01
                    );

                this->addSensor<Sensors::SensorType::USAGE>(
                    "Core / Graphics Utilization",
                    static_cast<float>(activity.iEngineClock) * 0.01
                );

                // ADL OD5 has no separate frame-buffer or video-engine utilization
            }
            else {
                std::cerr << "ADL_OD5_CurrentActivity_Get failed for adapter " << adapter_idx << "\n";
            }
        }

        // Fan Speed !TODO - expand on this 
        {
            ADLFanSpeedValue fan_val = {};
            fan_val.iSize = sizeof(ADLFanSpeedValue);
            fan_val.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
            if (adl2_od5_fanspeed_get(_context, adapter_idx, 0, &fan_val) == ADL_OK)
                this->addSensor<Sensors::SensorType::USAGE>(
                    "Fan Speed",
                    static_cast<float>(fan_val.iFanSpeed)
                );
            else
                std::cerr << "ADL_OD5_FanSpeed_Get failed for adapter " << adapter_idx << "\n";
        }
    }
    else {
        std::cerr << "Unhandled AMD GPU Overdrive version\n";
    }
}

// -----------------------------------------------------------------------
// ADL Malloc callback
// -----------------------------------------------------------------------
void* __stdcall AMDLiveGPUMetrics::adlMallocCallback(int size) {
    return malloc(size);
}