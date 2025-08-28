#define _WIN32_DCOM

#include "projutils.h"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

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

void infoGPU(IWbemLocator*& loc, IWbemServices*& svcs, std::vector<GraphicsProcessor>& gpu_list) {
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

    while (GPU_enumerator) {
        GraphicsProcessor gpu;
        
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

        gpu.setName(name.bstrVal);
        gpu.setAdapterRAM(adapter_RAM.ulVal);
        gpu.setDeviceId(device_ID.bstrVal);
        gpu.setAvailability(availability.uiVal);
        gpu.setCurrRefreshRate(availability.ulVal);
        gpu.setStatus(status.bstrVal);

        VariantClear(&name);
        VariantClear(&adapter_RAM);
        VariantClear(&device_ID);
        VariantClear(&availability);
        VariantClear(&curr_refresh_rate);
        VariantClear(&status);

        gpu_list.push_back(gpu);
    }
}

void infoMotherboard(IWbemLocator*& loc, IWbemServices*& svcs) {
    //TODO - use the smbios table maybe if we want more but I think we are good here for now
    //GetSystemFirmwareTable

    IEnumWbemClassObject *mboard_enumerator = nullptr;
    IWbemClassObject *mboard = nullptr;
    ULONG u_ret = 0;

    HRESULT svcs_res = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_BaseBoard"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &mboard_enumerator
    );

    if (FAILED(svcs_res)) {
        std::cout << "Win32_BaseBoard error";
        svcs->Release();
        loc->Release();
        CoUninitialize();
    }

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Motherboard **\n\n";
    while (mboard_enumerator) {
        HRESULT mboard_res = mboard_enumerator->Next(WBEM_INFINITE, 1, &mboard, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT description, hostingBoard, poweredOn, product, status;

        VariantInit(&description);
        VariantInit(&hostingBoard);
        VariantInit(&poweredOn);
        VariantInit(&product);
        VariantInit(&status);
        
        mboard->Get(L"Description", 0, &description, 0, 0); //bstr_t
        mboard->Get(L"HostingBoard", 0, &hostingBoard, 0, 0);   //BOOL
        mboard->Get(L"PoweredOn", 0, &poweredOn, 0, 0); //BOOL
        mboard->Get(L"Product", 0, &product, 0, 0); //bstr_t
        mboard->Get(L"Status", 0, &status, 0, 0); //bstr_t
        
        // Output with null checks
        if (description.vt == VT_BSTR && description.bstrVal != NULL)
            std::wcout << "Description: " << description.bstrVal << std::endl;

        if (hostingBoard.vt == VT_BOOL)
            std::wcout << "HostingBoard: " << (hostingBoard.boolVal == VARIANT_TRUE ? "True" : "False") << std::endl;

        if (poweredOn.vt == VT_BOOL)
            std::wcout << "PoweredOn: " << (poweredOn.boolVal == VARIANT_TRUE ? "True" : "False") << std::endl;

        if (product.vt == VT_BSTR && product.bstrVal != NULL)
            std::wcout << "Product: " << product.bstrVal << std::endl;

        if (status.vt == VT_BSTR && status.bstrVal != NULL)
            std::wcout << "Status: " << status.bstrVal << std::endl;

        // Clear all variants
        VariantClear(&description);
        VariantClear(&hostingBoard);
        VariantClear(&poweredOn);
        VariantClear(&product);
        VariantClear(&status);

        std::wcout << "\n";
    }

}

void infoPhysicalDrive(IWbemLocator*& loc, IWbemServices*& svcs) {
    // TODO - Change these to populate data structures so we can keep info together and speed up refreshed info
    IEnumWbemClassObject *disk_enumerator = nullptr;
    IEnumWbemClassObject *msft_enumerator = nullptr;
    IEnumWbemClassObject* ld_enumerator = nullptr;
    IWbemClassObject *disk = nullptr;
    IWbemClassObject *msft_phys = nullptr;
    IWbemClassObject* ld = nullptr;
    ULONG u_ret = 0;

    HRESULT dd_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &disk_enumerator
    );

    if (FAILED(dd_query)) {
        std::wcout << "Win32_DiskDrive Error. HRESULT: 0x" << std::hex << dd_query << std::endl;
        svcs->Release();
        loc->Release();
        CoUninitialize();
        return;
    }

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Storage Devices (Physical Drives) **\n\n";
    while (disk_enumerator) {
        HRESULT disk_res = disk_enumerator->Next(WBEM_INFINITE, 1, &disk, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT name, manufacturer, model;

        VariantInit(&name);
        VariantInit(&manufacturer);
        VariantInit(&model);

        disk->Get(L"Name", 0, &name, 0, 0);
        disk->Get(L"Manufacturer", 0, &manufacturer, 0, 0);
        disk->Get(L"Model", 0, &model, 0, 0);

        std::wcout << "Name: " << name.bstrVal << "\n";
        std::wcout << "Manufacturer: " << manufacturer.bstrVal << "\n";
        std::wcout << "Model: " << model.bstrVal << "\n";

        VariantClear(&name);
        VariantClear(&manufacturer);
        VariantClear(&model);
    }

    HRESULT hr = loc->ConnectServer(
        bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
        NULL,
        NULL, 
        0, 
        NULL, 
        0, 
        0, 
        &svcs
    );

    HRESULT pd_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_PhysicalDisk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &msft_enumerator
    );

    if (FAILED(pd_query)) {
        std::wcout << "MSFT_PhysicalDisk Error. HRESULT: 0x" << std::hex << pd_query << std::endl;
        svcs->Release();
        loc->Release();
        CoUninitialize();
        return;
    }

    while (msft_enumerator) {
        HRESULT msft_res = msft_enumerator->Next(WBEM_INFINITE, 1, &msft_phys, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT spindle_speed;

        VariantInit(&spindle_speed);

        msft_phys->Get(L"SpindleSpeed", 0, &spindle_speed, 0, 0);

        if (spindle_speed.uintVal == 0) {
            std::wcout << "Type: SSD" << std::endl;
        }
        else {
            std::wcout << "Type: HDD, SpindleSpeed: " << spindle_speed.uintVal << std::endl;
        }

        VariantClear(&spindle_speed);

        hr = loc->ConnectServer(
            BSTR(L"ROOT\\CIMV2"),   // namespace
            NULL,                   // User name
            NULL,                   // User password
            0,                      // Locale
            NULL,                   // Security flags
            0,                      // Authority
            0,                      // Context object
            &svcs);                 // IWbemServices proxy

        HRESULT ld_query = svcs->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_LogicalDisk"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &ld_enumerator
        );

        if (FAILED(ld_query)) {
            std::wcout << "Win32_LogicalDisk Error. HRESULT: 0x" << std::hex << ld_query << std::endl;
            svcs->Release();
            loc->Release();
            CoUninitialize();
            return;
        }

        while (ld_enumerator) {
            ld_enumerator->Next(WBEM_INFINITE, 1, &ld, &u_ret);
            if (u_ret == 0) {
                break;
            }

        }
    }
}


int main()
{
    IWbemLocator* loc = nullptr;
    IWbemServices* svcs = nullptr;
    IWbemRefresher* refresher = nullptr;

    InitializeCOM();
    setupWBEM(loc, svcs);

    std::vector<GraphicsProcessor> gpu_list;
    //std::vector<MotherBoard> mboard_list;

    infoMotherboard(loc, svcs);
    infoGPU(loc, svcs, gpu_list);
    //infoCPU(loc, svcs);   TODO!
    infoPhysicalDrive(loc, svcs);   
    //infoTemperatures();

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** GPUs & Video Controllers ** \n\n";
    for (int i = 0; i < gpu_list.size(); i++) {
        gpu_list[i].outputGPUInfo();
    }



    // Check for mem leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    std::wcout << "\n\n\n\n\n\n\n\n\n\n\n";
//#if defined(_WIN64)
//    std::cout << "Running as 64-bit process.\n";
//#else
//    std::cout << "Running as 32-bit process.\n";
//#endif

    return 0;
}