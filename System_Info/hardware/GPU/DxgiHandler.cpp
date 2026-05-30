#include "DxgiHandler.h"

DxgiHandler::DxgiHandler() {
    enumerateAdapters();
}

void DxgiHandler::enumerateAdapters() {
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

std::vector<std::unique_ptr<A_HardwareDevice>> DxgiHandler::createGPUDevices() {
    std::vector<std::unique_ptr<A_HardwareDevice>> devices;

    for (auto& adapter : detected_adapters) {
        switch (adapter.vendor) {
        case Vendor::NVIDIA:
            devices.push_back(std::make_unique<NvidiaLiveGPUMetrics>(adapter.luid, adapter.name));
            // TODO -> Call fetchMetrics()
            break;
        case Vendor::AMD:
            devices.push_back(std::make_unique<AMDLiveGPUMetrics>(adapter.luid, adapter.name));
            // TODO -> Call fetchMetrics()
            break;
        default:
            break;
        }
    }

    return devices;
}