#include "WbemManager.h"

WbemManager::WbemManager() {
    connectAll();
}

void WbemManager::connectAll() {
    connect(w32_loc, w32_svcs, L"ROOT\\CIMV2");
    connect(msft_loc, msft_svcs, L"ROOT\\Microsoft\\Windows\\Storage");
}

void WbemManager::connect(
    winrt::com_ptr<IWbemLocator>& loc,
    winrt::com_ptr<IWbemServices>& svcs,
    const wchar_t* ns)
{
    HRESULT hr = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(loc.put())
    );

    if (FAILED(hr)) {
        std::cout << "Failed to create IWbemLocator. HRESULT=0x"
            << std::hex << hr << "\n";
    }

    hr = loc->ConnectServer(
        _bstr_t(ns),     // Namespace
        nullptr,         // User
        nullptr,         // Password
        0,               // Locale
        0,               // Security flags
        0,               // Authority
        0,               // Context
        svcs.put()
    );

    if (FAILED(hr)) {
        std::cout << "ConnectServer failed for namespace "
            << wideToUtf8(ns) << " HRESULT=0x"
            << std::hex << hr << "\n";
    }

    setProxySecurity(svcs.get());
}

void WbemManager::setProxySecurity(IWbemServices* services) {
    if (!services)
        return;

    HRESULT hr = CoSetProxyBlanket(
        services,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::cout << "CoSetProxyBlanket failed. HRESULT=0x"
            << std::hex << hr << "\n";
    }
}