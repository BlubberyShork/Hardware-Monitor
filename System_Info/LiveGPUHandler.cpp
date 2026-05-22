#include "LiveGPUHandler.h"
#include "NvidiaLiveGPUMetrics.h"
#include "AMDLiveGPUMetrics.h"
#include <stdexcept>

LiveGPUHandler::~LiveGPUHandler() = default;

LiveGPUHandler::Vendor LiveGPUHandler::detectVendor() const {
    constexpr UINT VENDOR_ID_NVIDIA = 0x10DE;
    constexpr UINT VENDOR_ID_AMD    = 0x1002;

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DXGI factory");

    Vendor detected = Vendor::UNKNOWN;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        adapter->Release();

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (desc.VendorId == VENDOR_ID_NVIDIA) detected = Vendor::NVIDIA;
        else if (desc.VendorId == VENDOR_ID_AMD) detected = Vendor::AMD;
        break;
    }
    factory->Release();
    return detected;
}

LiveGPUHandler::LiveGPUHandler() {
    switch (detectVendor()) {
    case Vendor::NVIDIA:
        backend = std::make_unique<NvidiaLiveGPUMetrics>();
        break;
    case Vendor::AMD:
        backend = std::make_unique<AMDLiveGPUMetrics>();
        break;
    default:
        throw std::runtime_error("Unsupported or no GPU vendor detected");
    }
}

void LiveGPUHandler::fetchCurrentLiveGPUMetrics() {
    backend->fetchMetrics();
}

void LiveGPUHandler::outputLiveGPUMetrics() const {
    backend->outputMetrics();
}