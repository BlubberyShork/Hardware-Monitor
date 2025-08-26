#define _WIN32_DCOM

#include "projutils.h"

#include <iostream>
#include <wbemidl.h>
#include "GraphicsProcessor.h"
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

using namespace ::std;

// Code is pulled from the example code on https://learn.microsoft.com/en-us/windows/win32/wmisdk/initializing-com-for-a-wmi-application
void InitializeCOM()
{

    // First step: Initialize COM
    HRESULT hr;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        cout << "Failed to initialize COM library. Error code = 0x"
            << hex << hr << endl;
    }

    // Second step: Setting security levels
    hr = CoInitializeSecurity(
        NULL,                        // Security descriptor
        -1,                          // COM negotiates authentication service
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication level for proxies
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation level for proxies
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities of the client or server
        NULL);                       // Reserved

    if (FAILED(hr))
    {
        cout << "Failed to initialize security. Error code = 0x"
            << hex << hr << endl;
        CoUninitialize();
    }
}

// Code is pulled from the example code on https://learn.microsoft.com/en-us/windows/win32/wmisdk/initializing-com-for-a-wmi-application
void setupWBEM(IWbemLocator*& loc, IWbemServices*& svcs) // *& grabs the actual pointer by reference instead of makign a copy
{
    HRESULT hr;

    hr = CoCreateInstance(
        CLSID_WbemLocator, 
        0,
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, 
        (LPVOID*)&loc
    );

    if (FAILED(hr))
    {
        cout << "Failed to create IWbemLocator object. Error code = 0x"
            << hex << hr << endl;
        CoUninitialize();
    }

    hr = loc->ConnectServer(
        BSTR(L"ROOT\\CIMV2"),   // namespace
        NULL,                   // User name
        NULL,                   // User password
        0,                      // Locale
        NULL,                   // Security flags
        0,                      // Authority
        0,                      // Context object
        &svcs);                 // IWbemServices proxy

    if (FAILED(hr))
    {
        cout << "Could not connect. Error code = 0x"
            << hex << hr << endl;
        loc->Release();
        CoUninitialize();
    }
    //cout << "Connected to WMI\n\n";
}

void infoGPU(IWbemLocator*& loc, IWbemServices*& svcs) {
    //std::vector<GraphicsProcessor> gpu_list;
    IEnumWbemClassObject *GPU_enumerator = nullptr;
    IWbemClassObject* gpu_class_obj = nullptr;
    ULONG u_ret = 0;

    HRESULT res = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_VideoController"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &GPU_enumerator
    );

    if (FAILED(res)) {
        std::cout << "Win32_VideoController error";
        svcs->Release();
        loc->Release();
        CoUninitialize();
    }

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** GPUs & Video Controllers **\n\n";
    while (GPU_enumerator) {
        //GraphicsProcessor gpu;
        
        HRESULT gpu_res = GPU_enumerator->Next(WBEM_INFINITE, 1, &gpu_class_obj, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT name, adapter_RAM, device_ID, availability, curr_refresh_rate, status;

        VariantInit(&name);
        VariantInit(&adapter_RAM);
        VariantInit(&device_ID);
        VariantInit(&availability);
        VariantInit(&curr_refresh_rate);
        VariantInit(&status);

        gpu_class_obj->Get(L"Name", 0, &name, 0, 0);
        gpu_class_obj->Get(L"AdapterRAM", 0, &adapter_RAM, 0, 0);
        gpu_class_obj->Get(L"DeviceID", 0, &device_ID, 0, 0);
        gpu_class_obj->Get(L"Availability", 0, &availability, 0, 0);
        gpu_class_obj->Get(L"CurrentRefreshRate", 0, &curr_refresh_rate, 0, 0);
        gpu_class_obj->Get(L"Status", 0, &status, 0, 0);

        std::wcout << "Name: " << name.bstrVal << std::endl;
        _bstr_t RAM_output = simplifyBytesAsString(adapter_RAM.ulVal);
        std::wcout << "Total Adapter RAM: " << RAM_output << std::endl;
        std::wcout << "Device ID: " << device_ID.bstrVal << std::endl;
        _bstr_t avail_output = explainAvailability(availability.uiVal);
        std::wcout << "Availability: " << avail_output << std::endl; 
        std::wcout << "Current Refresh Rate: " << curr_refresh_rate.ulVal << std::endl;
        std::wcout << "Status: " << status.bstrVal << std::endl;

        VariantClear(&name);
        VariantClear(&adapter_RAM);
        VariantClear(&device_ID);
        VariantClear(&availability);
        VariantClear(&curr_refresh_rate);
        VariantClear(&status);

        //gpu_list.push_back(gpu);
        std::wcout << "\n";
    }
    std::wcout << "--------------------------------------------------------------\n\n";

}

void infoMotherboard(IWbemLocator*& loc, IWbemServices*&) {

}


int main()
{
    IWbemLocator* loc = nullptr;
    IWbemServices* svcs = nullptr;
    IWbemRefresher* refresher = nullptr;

    InitializeCOM();
    setupWBEM(loc, svcs);

    infoMotherboard(loc, svcs);
    infoGPU(loc, svcs);
    //infoCPU(loc, svcs);   TODO!
    //infoPhysicalDrives(loc, svcs);    TODO!

    //infoTemperatures();


    std::wcout << "\n\n\n\n\n\n\n\n\n\n\n";
    return 0;
}