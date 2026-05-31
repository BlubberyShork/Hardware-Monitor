#include "ADLX.h"
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
ADLX::~ADLX() {
    shutdown();
}

void ADLX::init() {
    ADLX_RESULT res = adlx_helper.Initialize();
    if (ADLX_FAILED(res))
        throw std::runtime_error("ADLXHelper::Initialize failed, ADLX_RESULT: " + std::to_string(res));

    adlx_system = adlx_helper.GetSystemServices();
    if (!adlx_system)
        throw std::runtime_error("Failed to acquire IADLXSystem from ADLXHelper");

    res = adlx_system->GetPerformanceMonitoringServices(&perf_monitoring);
    if (ADLX_FAILED(res) || !perf_monitoring)
        throw std::runtime_error("Failed to acquire IADLXPerformanceMonitoringServices, ADLX_RESULT: " + std::to_string(res));

    _initialized = true;
}

void ADLX::shutdown() {
    if (!_initialized) return;

    if (perf_monitoring) {
        perf_monitoring->Release();
        perf_monitoring = nullptr;
    }

    adlx_helper.Terminate();
    adlx_system = nullptr;
    _initialized = false;
}