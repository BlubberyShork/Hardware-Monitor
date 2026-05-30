#include "AMDPool.h"
#include "AMDLiveGPUMetrics.h"
#include <adl_structures.h>
#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------
// enumerateDevices 
// ---------------------------------------------------------------------------
void AMDPool::enumerateDevices() {
    try {
        adl_ = std::make_shared<ADL>();
        adl_->init();
        active_backend_ = Backend::ADL;
        enumerateADL();
        return;
    }
    catch (const std::exception& adl_err) {
        std::cerr << "[AMDPool] ADL unavailable (" << adl_err.what()
            << "), falling back to ADLX\n";
        adl_.reset();
    }

    // ---- Fall back to ADLX ----
    try {
        adlx_ = std::make_shared<ADLX>();
        adlx_->init();
        active_backend_ = Backend::ADLX;
        enumerateADLX();
        return;
    }
    catch (const std::exception& adlx_err) {
        std::cerr << "[AMDPool] ADLX fallback also unavailable (" << adlx_err.what()
            << "), AMD GPU metrics disabled\n";
        adlx_.reset();
    }
}

// ---------------------------------------------------------------------------
// enumerateADL
// ---------------------------------------------------------------------------
void AMDPool::enumerateADL() {
    int adapter_cnt = 0;
    int res = adl_->adl2_adapter_numberofadapters_get(adl_->context, &adapter_cnt);
    if (res != ADL_OK || adapter_cnt <= 0) {
        std::cerr << "[AMDPool] ADL2_Adapter_NumberOfAdapters_Get failed or returned 0 adapters"
            " (ADL_RESULT: " << res << ")\n";
        return;
    }

    std::vector<AdapterInfo> adapter_info(adapter_cnt);
    res = adl_->adl2_adapter_adapterinfo_get(
        adl_->context,
        adapter_info.data(),
        static_cast<int>(sizeof(AdapterInfo) * adapter_cnt)
    );
    if (res != ADL_OK) {
        std::cerr << "[AMDPool] ADL2_Adapter_AdapterInfo_Get failed (ADL_RESULT: " << res << ")\n";
        return;
    }

    // Deduplicate: one physical card can appear as multiple logical adapters.
    // Track seen bus numbers so each physical card registers only once.
    std::vector<int> seen_bus_numbers;

    for (int i = 0; i < adapter_cnt; ++i) {
        int active = 0;
        if (adl_->adl2_adapter_active_get(adl_->context, i, &active) != ADL_OK || !active)
            continue;

        int bus = adapter_info[i].iBusNumber;
        if (std::find(seen_bus_numbers.begin(), seen_bus_numbers.end(), bus)
            != seen_bus_numbers.end())
            continue;

        seen_bus_numbers.push_back(bus);

        std::string adapter_name = adapter_info[i].strAdapterName;

        // Construct device, immediately fetch metrics, store.
        auto device = std::make_unique<AMDLiveGPUMetrics>(i, adapter_name, adl_);
        device->fetchMetrics();
        devices_.push_back(std::move(device));
    }

    if (devices_.empty())
        std::cerr << "[AMDPool] No active ADL adapters found after enumeration\n";
}

// ---------------------------------------------------------------------------
// enumerateADLX
// ---------------------------------------------------------------------------
void AMDPool::enumerateADLX() {
    adlx::IADLXGPUListPtr gpu_list;
    ADLX_RESULT res = adlx_->adlx_system->GetGPUs(&gpu_list);
    if (ADLX_FAILED(res) || !gpu_list) {
        std::cerr << "[AMDPool] Failed to enumerate ADLX GPUs (ADLX_RESULT: " << res << ")\n";
        return;
    }

    for (adlx_uint i = gpu_list->Begin(); i != gpu_list->End(); ++i) {
        adlx::IADLXGPUPtr gpu;
        res = gpu_list->At(i, &gpu);
        if (ADLX_FAILED(res) || !gpu) {
            std::cerr << "[AMDPool] Failed to get ADLX GPU at index " << i << ", skipping\n";
            continue;
        }

        auto device = std::make_unique<AMDLiveGPUMetrics>(gpu, adlx_);
        device->fetchMetrics();
        devices_.push_back(std::move(device));
    }

    if (devices_.empty())
        std::cerr << "[AMDPool] No ADLX GPUs found after enumeration\n";
}