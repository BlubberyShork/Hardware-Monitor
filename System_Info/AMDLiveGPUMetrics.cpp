#include "AMDLiveGPUMetrics.h"
#include <iostream>
#include <stdexcept>

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------
AMDLiveGPUMetrics::AMDLiveGPUMetrics() {
    initSystem();
    initPerfMonitoring();
    enumerateGPUs();
}

AMDLiveGPUMetrics::~AMDLiveGPUMetrics() {
    // Release GPU pointers before terminating
    gpus.clear();

    if (perf_monitoring)
        perf_monitoring->Release();

    // ADLXHelper::Terminate handles DLL unload and system release
    adlx_helper.Terminate();
}

// -----------------------------------------------------------------------
// Init helpers
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
// Fetch
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::fetchMetrics() {
    for (size_t i = 0; i < gpus.size(); i++)
        fetchGPUMetrics(gpus[i], gpu_live_data[i]);
}

void AMDLiveGPUMetrics::fetchGPUMetrics(adlx::IADLXGPU* gpu, GPULiveData& live_data) {
    live_data = {}; // clear prev fetch

    // Acquire support and metrics interfaces for this GPU
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

    // Temperature 
    adlx_bool supported = false;

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
    metrics_support->IsSupportedGPUClockSpeed(&supported);
    if (supported) {
        adlx_int clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUClockSpeed(&clk)))
            live_data.clks.push_back({ GPUClockDomain::Graphics, static_cast<double>(clk) });
    }

    metrics_support->IsSupportedGPUVRAMClockSpeed(&supported);
    if (supported) {
        adlx_int clk = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUVRAMClockSpeed(&clk)))
            live_data.clks.push_back({ GPUClockDomain::Memory, static_cast<double>(clk) });
    }

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

    // Fan Speed
    metrics_support->IsSupportedGPUFanSpeed(&supported);
    if (supported) {
        adlx_int fan_rpm = 0;
        if (ADLX_SUCCEEDED(gpu_metrics->GPUFanSpeed(&fan_rpm)))
            live_data.fan_speed = static_cast<unsigned int>(fan_rpm);
    }
}

// -----------------------------------------------------------------------
// Output
// -----------------------------------------------------------------------
void AMDLiveGPUMetrics::outputMetrics() const {
    for (auto& live_data : gpu_live_data) {
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