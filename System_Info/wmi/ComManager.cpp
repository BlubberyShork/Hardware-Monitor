#include "ComManager.h"


// TODO - This seems unsafe and error prone if it fails on construct
    // Might not be a smart way of doing this
ComManager::ComManager() {
    HRESULT hr;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cout << "Failed to initialize COM library. Error code = 0x"
            << std::hex << hr << std::endl;
    }

    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL);

    if (FAILED(hr)) {
        std::cout << "Failed to initialize security. Error code = 0x"
            << std::hex << hr << std::endl;
        CoUninitialize();
    }

    initialized = true;
}

ComManager::~ComManager() {
	
    if (initialized) {
        CoUninitialize();
    }
}