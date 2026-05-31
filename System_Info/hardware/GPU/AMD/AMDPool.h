#pragma once
#include "../../IHardwareDevicePool.h"
#include "../../HardwareDevice.h"
#include "Interop/ADL.h"
#include "Interop/ADLX.h"
#include <adl_sdk.h>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// AMDPool : IHardwareDevicePool
//
// Owns the AMD SDK interop lifetime and drives the full enumeration pipeline
// ---------------------------------------------------------------------------
class AMDPool : public IHardwareDevicePool {
public:
    AMDPool() = default;
    ~AMDPool() override = default;

    // Populates devices_ via ADL or ADLX enumeration.
    void enumerateDevices() override;

    const std::vector<std::unique_ptr<A_HardwareDevice>>& getDevices() const { return devices_; }

private:
    // -----------------------------------------------------------------------
    // Backend selection
    // -----------------------------------------------------------------------
    enum class Backend { ADL, ADLX };
    Backend active_backend_ = Backend::ADL;

    // Exactly one of these will be non-null after a successful enumerateDevices() call
    std::shared_ptr<ADL>  adl_;
    std::shared_ptr<ADLX> adlx_;

    std::vector<std::unique_ptr<A_HardwareDevice>> devices_;

    // -----------------------------------------------------------------------
    // Per-backend enumeration helpers
    // -----------------------------------------------------------------------
    void enumerateADL();
    void enumerateADLX();
};