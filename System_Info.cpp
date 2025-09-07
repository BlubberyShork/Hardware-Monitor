#define _WIN32_DCOM

#include "GraphicsProcessor.h"
#include "motherboard.h"
#include "storagedevice.h"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#include <iostream>
#include <wbemidl.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <variant>


#pragma comment(lib, "wbemuuid.lib")


// Code is pulled from the example code on https://learn.microsoft.com/en-us/windows/win32/wmisdk/initializing-com-for-a-wmi-application
void InitializeCOM()
{
    // First step: Initialize COM
    HRESULT hr;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::cout << "Failed to initialize COM library. Error code = 0x"
            << std::hex << hr << std::endl;
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
        std::cout << "Failed to initialize security. Error code = 0x"
            << std::hex << hr << std::endl;
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
        (LPVOID*)&loc);

    if (FAILED(hr))
    {
        std::cout << "Failed to create IWbemLocator object. Error code = 0x"
            << std::hex << hr << std::endl;
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
        std::cout << "Could not connect. Error code = 0x"
            << std::hex << hr << std::endl;
        loc->Release();
        CoUninitialize();
    }
    //cout << "Connected to WMI\n\n";
}

void infoGPU(IWbemLocator*& loc, IWbemServices*& svcs, std::vector<GraphicsProcessor>& gpu_list) {
    IEnumWbemClassObject *GPU_enumerator = nullptr;
    IWbemClassObject *gpu_class_obj = nullptr;
    ULONG u_ret = 0;

    HRESULT gpu_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_VideoController"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &GPU_enumerator);

    if (FAILED(gpu_query)) {
        std::cout << "Win32_VideoController Error HRESULT: 0x" 
            << std::hex << gpu_query << "\n";
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
        gpu.setCurrentRefreshRate(availability.ulVal);
        gpu.setStatus(status.bstrVal);

        gpu_list.push_back(gpu);

        VariantClear(&name);
        VariantClear(&adapter_RAM);
        VariantClear(&device_ID);
        VariantClear(&availability);
        VariantClear(&curr_refresh_rate);
        VariantClear(&status);

        gpu_class_obj->Release();
    }
    GPU_enumerator->Release();
}

void infoMotherboard(IWbemLocator*& loc, IWbemServices*& svcs, std::vector<Motherboard>& mboard_list) {
    //TODO - use the smbios table maybe if we want more but I think we are good here for now
    //GetSystemFirmwareTable

    IEnumWbemClassObject *mboard_enumerator = nullptr;
    IWbemClassObject *mboard = nullptr;
    ULONG u_ret = 0;

    HRESULT mboard_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_BaseBoard"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &mboard_enumerator);

    if (FAILED(mboard_query)) {
        std::cout << "Win32_BaseBoard error. HRESULT: 0x"
            << std::hex << mboard_query << "\n";
        svcs->Release();
        loc->Release();
        CoUninitialize();
    }

    while (mboard_enumerator) {
        HRESULT mboard_res = mboard_enumerator->Next(WBEM_INFINITE, 1, &mboard, &u_ret);
        if (u_ret == 0) {
            break;
        }

        Motherboard mboard_obj;
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
        
        mboard_obj.setDesc(description.bstrVal);
        BOOL hb_b = hostingBoard.boolVal == VARIANT_TRUE ? TRUE : FALSE;
        mboard_obj.setHostingBoard(hb_b);
        mboard_obj.setPoweredOn(BOOL(poweredOn.pboolVal));
        mboard_obj.setProduct(product.bstrVal);
        mboard_obj.setStatus(status.bstrVal);
        
        mboard_list.push_back(mboard_obj);

        // Clear all variants
        VariantClear(&description);
        VariantClear(&hostingBoard);
        VariantClear(&poweredOn);
        VariantClear(&product);
        VariantClear(&status);
        mboard->Release();
    }
    mboard_enumerator->Release();

}

//TODO - producer consumer thread handle this
void infoPhysicalDrive(IWbemLocator*& loc, IWbemServices*& svcs, 
    std::vector<StorageDevice>& sd_list) {
    
    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> d_hmap;
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> p_hmap;
    std::unordered_map<wchar_t, Volume> v_hmap;
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> pd_hmap;

    IEnumWbemClassObject *disk_enumerator = nullptr;
    IEnumWbemClassObject *part_enumerator = nullptr;
    IEnumWbemClassObject *msft_enumerator = nullptr;
    IEnumWbemClassObject *vol_enumerator = nullptr;
    IWbemClassObject *disk_obj = nullptr;
    IWbemClassObject *part_obj = nullptr;
    IWbemClassObject *vol_obj = nullptr;
    IWbemClassObject *msft_phys = nullptr;

    ULONG u_ret = 0;

    //Prepping for MSFT queries
    HRESULT hr = loc->ConnectServer(
        bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &svcs);

    if (FAILED(hr)) {
        std::wcout << L"Failed to connect to storage namespace\n";
        return;
    }

    //////////////////////////////
    // Direct Disk Query
    //////////////////////////////
    HRESULT disk_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &disk_enumerator);

    if (FAILED(disk_query)) {
        std::wcout << L"MSFT_Disk Error. HRESULT: 0x"
            << std::hex << disk_query << std::endl;
        return;
    }

    while (disk_enumerator) {
        disk_enumerator->Next(WBEM_INFINITE, 1, &disk_obj, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT unq_id, num, fname, manufacturer, model, d_sz, num_partitions;
        VariantInit(&unq_id);
        VariantInit(&num);
        VariantInit(&fname);
        VariantInit(&manufacturer);
        VariantInit(&model);
        VariantInit(&d_sz);
        VariantInit(&num_partitions);

        disk_obj->Get(L"UniqueId", 0, &unq_id, 0, 0);
        disk_obj->Get(L"Number", 0, &num, 0, 0);
        disk_obj->Get(L"FriendlyName", 0, &fname, 0, 0);
        disk_obj->Get(L"Manufacturer", 0, &manufacturer, 0, 0);
        disk_obj->Get(L"Model", 0, &model, 0, 0);
        disk_obj->Get(L"Size", 0, &d_sz, 0, 0);
        disk_obj->Get(L"NumberOfPartitions", 0, &num_partitions, 0, 0);

        Disk disk;
        disk.unq_id = bstr_t(unq_id.bstrVal);
        disk.manufacturer = bstr_t(manufacturer.bstrVal);
        disk.model = bstr_t(model.bstrVal);
        disk.fname = bstr_t(fname.bstrVal);
        disk.num_partitions = num_partitions.ulVal;
        disk.disk_num = num.ulVal;
        disk.sz = VTConvertNumeric(d_sz);

        bstr_t bstr_unq_id = bstr_t(unq_id.bstrVal);
        if (d_hmap.find(bstr_unq_id) == d_hmap.end()) {
            d_hmap.insert({ bstr_unq_id, disk });
        }

        VariantClear(&unq_id);
        VariantClear(&fname);
        VariantClear(&manufacturer);
        VariantClear(&model);
        VariantClear(&d_sz);
        VariantClear(&num_partitions);
        disk_obj->Release();
    }
    disk_enumerator->Release();
     
    //////////////////////////////
    // Direct Partition Query  
    //////////////////////////////
    HRESULT part_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Partition"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &part_enumerator);

    if (FAILED(part_query)) {
        std::wcout << L"MSFT_Partition Error. HRESULT: 0x"
            << std::hex << part_query << std::endl;
        return;
    }

    while (part_enumerator) {
        part_enumerator->Next(WBEM_INFINITE, 1, &part_obj, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT disk_num, part_num, drv_ltr, p_sz;
        VariantInit(&disk_num);
        VariantInit(&part_num);
        VariantInit(&drv_ltr);
        VariantInit(&p_sz);

        part_obj->Get(L"DiskNumber", 0, &disk_num, 0, 0);
        part_obj->Get(L"PartitionNumber", 0, &part_num, 0, 0);
        part_obj->Get(L"DriveLetter", 0, &drv_ltr, 0, 0);
        part_obj->Get(L"Size", 0, &p_sz, 0, 0);

        Partition partition;
        partition.id.disk_num = disk_num.ulVal;
        partition.id.part_num = part_num.ulVal;
        partition.drv_ltr = static_cast<wchar_t>(drv_ltr.uiVal);
        partition.sz = VTConvertNumeric(p_sz);

        if (p_hmap.find(partition.id) == p_hmap.end()) {
            p_hmap.insert({ partition.id, partition });
        }

        VariantClear(&disk_num);
        VariantClear(&part_num);
        VariantClear(&drv_ltr);
        VariantClear(&p_sz);
        part_obj->Release();
    }
    part_enumerator->Release();

    /////////////////////////////////
    ////// Volume Query
    /////////////////////////////////
    HRESULT vol_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Volume"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &vol_enumerator);

    if (FAILED(vol_query)) {
        std::wcout << L"MSFT_Volume Error. HRESULT: 0x"
            << std::hex << vol_query << std::endl;
        return;
    }

    while (vol_enumerator) {
        vol_enumerator->Next(WBEM_INFINITE, 1, &vol_obj, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT drv_ltr, sz, sz_rmng, hstatus;

        VariantInit(&drv_ltr);
        VariantInit(&sz);
        VariantInit(&sz_rmng);
        VariantInit(&hstatus);

        vol_obj->Get(L"DriveLetter", 0, &drv_ltr, 0, 0);
        vol_obj->Get(L"Size", 0, &sz, 0, 0);
        vol_obj->Get(L"SizeRemaining", 0, &sz_rmng, 0, 0);
        vol_obj->Get(L"HealthStatus", 0, &hstatus, 0, 0);

        Volume vol;
        vol.drv_ltr = static_cast<wchar_t>(drv_ltr.uiVal);
        vol.sz = VTConvertNumeric(sz);
        vol.sz_rmng = VTConvertNumeric(sz_rmng);
        vol.hstatus = hstatus.uiVal;

        if (v_hmap.find(vol.drv_ltr) == v_hmap.end()) {
            v_hmap.insert({vol.drv_ltr, vol});
        }

        vol_obj->Release();
    }
    vol_enumerator->Release();

    /////////////////////////////
    // MSFT_PhysicalDisk Query
    /////////////////////////////
    HRESULT pd_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_PhysicalDisk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &msft_enumerator);

    if (FAILED(pd_query)) {
        std::wcout << "MSFT_PhysicalDisk Error. HRESULT: 0x" 
            << std::hex << pd_query << std::endl;
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

        VARIANT device_id, unq_id_frmt, spindle_speed;
        VariantInit(&device_id);
        VariantInit(&unq_id_frmt);
        VariantInit(&spindle_speed);

        auto extractIndex = [](const bstr_t& dev_id) {
            assert(dev_id);

            std::wstring ws_dev_id(dev_id);
            auto pos = ws_dev_id.find_last_of(L"0123456789");
            return (ULONG)std::stoi(ws_dev_id.substr(pos));
        };

        msft_phys->Get(L"SpindleSpeed", 0, &spindle_speed, 0, 0);
        msft_phys->Get(L"DeviceId", 0, &device_id, 0, 0);
        msft_phys->Get(L"UniqueIdFormat", 0, &unq_id_frmt, 0, 0);

        PhysDisk pd;
        pd.device_id = bstr_t(device_id.bstrVal);
        pd.spindle_speed = spindle_speed.ullVal;
        pd.unq_id_frmt = unq_id_frmt.uiVal;
        pd.disk_num = extractIndex(pd.device_id);

        if (pd_hmap.find(pd.disk_num) == pd_hmap.end()) {
            pd_hmap.insert({ pd.disk_num, pd });
        }

        VariantClear(&spindle_speed);
        VariantClear(&unq_id_frmt);
        VariantClear(&device_id);

        msft_phys->Release();
    }
    msft_enumerator->Release();

    ///////////////////////////
    // Storing data from maps
    ///////////////////////////
    for (auto disk_pair : d_hmap) {
        StorageDevice sd = StorageDevice();

        // Disk data
        bstr_t d_unq_id = std::get<0>(disk_pair);
        Disk disk = std::get<1>(disk_pair);
        ULONG d_disk_num = disk.disk_num;
        sd.setDisk(disk);

        // Partitions
        for (int i = 0; i < disk.num_partitions; i++) {
            ULONG part_num = (ULONG)i;
            Partition p = p_hmap[{d_disk_num, part_num}];
            sd.getPartitions().push_back(p);

            // Volume
            if (p.drv_ltr != 0 && v_hmap.find(p.drv_ltr) != v_hmap.end()) {
                sd.getVolumes().push_back(v_hmap[p.drv_ltr]);
            }
        }

        // Physical Disk
        PhysDisk pd = pd_hmap[d_disk_num];
        sd.setPhysicalDisk(pd);

        sd_list.push_back(sd);
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
}

int main()
{
    IWbemLocator *loc = nullptr;
    IWbemServices *svcs = nullptr;
    IWbemRefresher *refresher = nullptr;

    InitializeCOM();
    setupWBEM(loc, svcs);

    std::vector<GraphicsProcessor> gpu_list;
    std::vector<Motherboard> mboard_list;
    std::vector<StorageDevice> sd_list; 

    infoMotherboard(loc, svcs, mboard_list);
    infoGPU(loc, svcs, gpu_list);
    //infoCPU(loc, svcs);   TODO!
    infoPhysicalDrive(loc, svcs, sd_list);   
    //infoTemperatures();   TODO - Will need to make call to kernel driver

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** GPUs & Video Controllers ** \n\n";
    for (int i = 0; i < gpu_list.size(); i++) {
        gpu_list[i].outputGPUInfo();
    }
    std::wcout << std::endl;

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Motherboard ** \n\n";
    for (int i = 0; i < mboard_list.size(); i++) {
        mboard_list[i].outputMotherboardInfo();
    }
    std::wcout << std::endl;

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Storage Device ** \n\n";
    std::wcout << "Size of sd_list: " << sd_list.size() << "\n";
    for (int i = 0; i < sd_list.size(); i++) {
        sd_list[i].outSDInfo();
    }
    std::wcout << std::endl;


    // Check for mem leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    std::wcout << "\n\n\n\n\n\n\n\n\n\n\n";


    /*#if defined(_WIN64)
        std::cout << "Running as 64-bit process.\n";
    #else
        std::cout << "Running as 32-bit process.\n";
    #endif*/

    return 0;
}