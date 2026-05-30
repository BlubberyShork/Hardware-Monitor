#include "DxgiHandler.h"
#include "AMD/AMDPool.h"
#include <iostream>
// #include "NVIDIA/NvidiaPool.h"  // future

DxgiHandler::DxgiHandler() {
    enumerateAdapters();
}

// ---------------------------------------------------------------------------
// DxgiHandler::enumerateAdapters
//
// Enumerates all GPU adapters in the system, populating internal detected_adapters
// vector with the detected vendors
// ---------------------------------------------------------------------------
void DxgiHandler::enumerateAdapters() {
    std::cout << "DxgiHandler::enumerateAdapters called\n";
    constexpr UINT VENDOR_ID_NVIDIA = 0x10DE;
    constexpr UINT VENDOR_ID_AMD    = 0x1002;
    constexpr UINT VENDOR_ID_INTEL  = 0x8086;

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DXGI factory");

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        adapter->Release();

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        Adapter ad = {};
        ad.luid = desc.AdapterLuid;
        ad.name = desc.Description;
        ad.device_id = desc.DeviceId;
        ad.vendor_id = desc.VendorId;

        if (desc.VendorId == VENDOR_ID_NVIDIA) ad.vendor = Vendor::NVIDIA;
        else if (desc.VendorId == VENDOR_ID_AMD)    ad.vendor = Vendor::AMD;
        else if (desc.VendorId == VENDOR_ID_INTEL)  ad.vendor = Vendor::INTEL;
        else                                        ad.vendor = Vendor::UNKNOWN;

        detected_adapters.push_back(ad);
    }
    factory->Release();

    if (detected_adapters.empty())
        throw std::runtime_error("No active GPU adapters detected via DXGI");
}

// ---------------------------------------------------------------------------
// DxgiHandler::createGPUDevices
//
// Returns one IHardwareDevicePool per detected vendor.  Each pool runs its
// own SDK enumeration and populates its internal device list during
// enumerateDevices(), so by the time this function returns every pool's
// getDevices() is already populated with fetchMetrics()-primed devices.
// ---------------------------------------------------------------------------
void DxgiHandler::createGPUDevices() {
    std::cout << "DxgiHandler::createGPUDevices called\n";

    for (auto& adapter : detected_adapters) {
        switch (adapter.vendor) {
        case Vendor::AMD: {
            auto pool = std::make_unique<AMDPool>();
            pool->enumerateDevices();
            this->pools.push_back(std::move(pool));
            break;
        }
        case Vendor::NVIDIA: {
            // TODO: NvidiaPool
            //auto pool = std::make_unique<NvidiaPool>();
            //pool->enumerateDevices();
            //this->pools.push_back(std::move(pool));
            break;
        }
        default:
            break;
        }
    }
}