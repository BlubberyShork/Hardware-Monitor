#include "AMDPool.h"
#include <iostream>
#include <stdexcept>
#include <windows.h>

AMDPool::AMDPool() {
    try {
        initADL();
        // TODO -> caller of this constructor (later will be AMDCollection, should be responsible for calling fetchMetrics, which THAT should
            //  call enumerate GPUs)
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

            active_backend = Backend::ADLX;
            initialized = true;
        }
        catch (const std::exception& adlx_err) {
            std::cerr << "ADLX fallback also unavailable (" << adlx_err.what()
                << "), AMD GPU metrics disabled\n";
        }
    }
}

void AMDPool::enumerateDevices() {
    if (!initialized) return;

    if (active_backend == Backend::ADLX) {
        for (size_t i = 0; i < gpus.size(); i++)
            enumerateADLAdapters();
    }
    else {
        for (size_t i = 0; i < adl_adapter_indices.size(); i++)
            enumerateADLXGpus();
    }
}

// ============================================================
// ADLX
// ============================================================
void AMDPool::initSystem() {
    ADLX_RESULT res = adlx_helper.Initialize();
    if (ADLX_FAILED(res))
        throw std::runtime_error("ADLXHelper initialization failed, ADLX_RESULT: " + std::to_string(res));

    adlx_system = adlx_helper.GetSystemServices();
    if (!adlx_system)
        throw std::runtime_error("Failed to acquire IADLXSystem from ADLXHelper");
}

void AMDPool::initPerfMonitoring() {
    ADLX_RESULT res = adlx_system->GetPerformanceMonitoringServices(&perf_monitoring);
    if (ADLX_FAILED(res) || !perf_monitoring)
        throw std::runtime_error("Failed to acquire IADLXPerformanceMonitoringServices, ADLX_RESULT: " + std::to_string(res));
}



// ============================================================
// ADL
// ============================================================



void* __stdcall AMDPool::adlMallocCallback(int size) {
    return malloc(size);
}