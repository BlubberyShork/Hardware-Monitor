#pragma once
#include <..\..\..\third_party/adlx/SDK/ADLXHelper/Windows/Cpp/ADLXHelper.h>
#include <..\..\..\third_party/adlx/SDK/Include/ISystem.h>
#include <..\..\..\third_party/adlx/SDK/Include/IPerformanceMonitoring.h>

// ---------------------------------------------------------------------------
// ADLX
//
// Owns the ADLX SDK lifetime: ADLXHelper, IADLXSystem*, and
// IADLXPerformanceMonitoringServices*.  Constructed once by AMDPool;
// shared (via shared_ptr) with each AMDLiveGPUMetrics instance that was
// created on the ADLX path.
// ---------------------------------------------------------------------------
class ADLX {
public:
    ADLX() = default;
    ~ADLX();

    ADLX(const ADLX&) = delete;
    ADLX& operator=(const ADLX&) = delete;

    // Initializes ADLXHelper, acquires IADLXSystem and
    void init();

    // Releases perf_monitoring, calls adlx_helper.Terminate().
    // Safe to call multiple times.
    void shutdown();

    // -----------------------------------------------------------------------
    // Live state
    // -----------------------------------------------------------------------
    ADLXHelper                                 adlx_helper;
    adlx::IADLXSystem* adlx_system = nullptr;
    adlx::IADLXPerformanceMonitoringServices* perf_monitoring = nullptr;

private:
    bool _initialized = false;
};
