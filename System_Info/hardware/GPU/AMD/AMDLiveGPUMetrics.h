#pragma once
#include "..\..\HardwareDevice.h"
#include "Interop/ADL.h"
#include "Interop/ADLX.h"
#include <..\..\..\third_party/adlx/SDK/Include/ISystem.h>
#include <..\..\..\third_party/adlx/SDK/Include/IPerformanceMonitoring.h>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// AMDLiveGPUMetrics : A_HardwareDevice
//
// Represents a single AMD GPU adapter.
// ---------------------------------------------------------------------------
class AMDLiveGPUMetrics : public A_HardwareDevice {
public:
    // ADL path constructor
    AMDLiveGPUMetrics(int adapter_idx, std::string name,
        std::shared_ptr<ADL> adl);

    // ADLX path constructor
    AMDLiveGPUMetrics(adlx::IADLXGPUPtr gpu,
        std::shared_ptr<ADLX> adlx);

    // Shared_ptrs release themselves; IADLXGPUPtr is already a smart ptr.
    ~AMDLiveGPUMetrics() override = default;

    void fetchMetrics() override;

private:
    // -----------------------------------------------------------------------
    // Backend selection
    // -----------------------------------------------------------------------
    enum class Backend { ADL, ADLX };
    Backend active_backend_;

    // -----------------------------------------------------------------------
    // ADL path state
    // -----------------------------------------------------------------------
    std::shared_ptr<ADL> adl_;
    int adl_adapter_idx_ = -1;

    // -----------------------------------------------------------------------
    // ADLX path state
    // -----------------------------------------------------------------------
    std::shared_ptr<ADLX> adlx_;
    adlx::IADLXGPUPtr            adlx_gpu_;

    // -----------------------------------------------------------------------
    // Per-adapter fetch implementations
    // -----------------------------------------------------------------------
    void fetchADLMetrics();
    void fetchADLXMetrics();
};