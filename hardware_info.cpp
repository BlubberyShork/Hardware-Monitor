#include "hardware_info.h"

void InitializeCOM() {
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
}

void setupWBEM(IWbemLocator*& loc) {
    HRESULT hr;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&loc);

    if (FAILED(hr)) {
        std::cout << "Failed to create IWbemLocator object. Error code = 0x"
            << std::hex << hr << std::endl;
        CoUninitialize();
    }
}

void infoGPU(IWbemLocator*& loc, std::vector<GraphicsProcessor>& gpu_list) {
    IWbemServices* svcs = nullptr;
    IEnumWbemClassObject* GPU_enumerator = nullptr;
    IWbemClassObject* gpu_class_obj = nullptr;
    ULONG u_ret = 0;

    HRESULT hr = loc->ConnectServer(
        BSTR(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &svcs);

    if (FAILED(hr)) {
        std::cout << "Could not connect. Error code = 0x"
            << std::hex << hr << std::endl;
        loc->Release();
        CoUninitialize();
        return;
    }

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
        return;
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
        gpu.setCurrentRefreshRate(curr_refresh_rate.ulVal);
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
    svcs->Release();
}

void infoMotherboard(IWbemLocator*& loc, std::vector<Motherboard>& mboard_list) {
    IWbemServices* svcs = nullptr;
    IEnumWbemClassObject* mboard_enumerator = nullptr;
    IWbemClassObject* mboard = nullptr;
    ULONG u_ret = 0;

    HRESULT hr = loc->ConnectServer(
        BSTR(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &svcs);

    if (FAILED(hr)) {
        std::cout << "Could not connect. Error code = 0x"
            << std::hex << hr << std::endl;
        loc->Release();
        CoUninitialize();
        return;
    }

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
        return;
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

        mboard->Get(L"Description", 0, &description, 0, 0);
        mboard->Get(L"HostingBoard", 0, &hostingBoard, 0, 0);
        mboard->Get(L"PoweredOn", 0, &poweredOn, 0, 0);
        mboard->Get(L"Product", 0, &product, 0, 0);
        mboard->Get(L"Status", 0, &status, 0, 0);

        mboard_obj.setDesc(description.bstrVal);
        BOOL hb_b = hostingBoard.boolVal == VARIANT_TRUE ? TRUE : FALSE;
        mboard_obj.setHostingBoard(hb_b);
        mboard_obj.setPoweredOn(BOOL(poweredOn.pboolVal));
        mboard_obj.setProduct(product.bstrVal);
        mboard_obj.setStatus(status.bstrVal);

        mboard_list.push_back(mboard_obj);

        VariantClear(&description);
        VariantClear(&hostingBoard);
        VariantClear(&poweredOn);
        VariantClear(&product);
        VariantClear(&status);

        mboard->Release();
    }
    mboard_enumerator->Release();
    svcs->Release();
}

void infoCPU(IWbemLocator*& loc, std::vector<Processor>& proc_list) {
    IEnumWbemClassObject* cpu_enumerator = nullptr;
    IWbemClassObject* cpu_obj = nullptr;
    IWbemServices* svcs = nullptr;
    ULONG u_ret = 0;

    HRESULT hr = loc->ConnectServer(
        BSTR(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &svcs);

    if (FAILED(hr)) {
        std::cout << "Could not connect. Error code = 0x"
            << std::hex << hr << std::endl;
        loc->Release();
        CoUninitialize();
        return;
    }

    HRESULT cpu_query = svcs->ExecQuery(
        bstr_t(L"WQL"),
        bstr_t(L"SELECT * FROM Win32_Processor"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &cpu_enumerator);

    if (FAILED(cpu_query)) {
        std::cout << "Win32_Processor error. HRESULT: 0x"
            << std::hex << cpu_query << "\n";
        svcs->Release();
        loc->Release();
        CoUninitialize();
        return;
    }

    while (cpu_enumerator) {
        HRESULT cpu_res = cpu_enumerator->Next(WBEM_INFINITE, 1, &cpu_obj, &u_ret);
        if (u_ret == 0) {
            break;
        }

        VARIANT vtProp;
        Processor cpu;

        cpu_obj->Get(L"UniqueId", 0, &vtProp, 0, 0);
        cpu.unq_id = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);

        cpu_obj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        cpu.dev_id = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);

        cpu_obj->Get(L"ProcessorId", 0, &vtProp, 0, 0);
        cpu.proc_id = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);

        cpu_obj->Get(L"ProcessorType", 0, &vtProp, 0, 0);
        cpu.proc_type = static_cast<USHORT>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"Family", 0, &vtProp, 0, 0);
        cpu.family = static_cast<USHORT>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"Architecture", 0, &vtProp, 0, 0);
        cpu.architecture = static_cast<USHORT>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
        cpu.manufacturer = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);

        cpu_obj->Get(L"Name", 0, &vtProp, 0, 0);
        cpu.name = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);

        cpu_obj->Get(L"NumberOfCores", 0, &vtProp, 0, 0);
        cpu.num_cores = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0);
        cpu.num_log_proc = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"ThreadCount", 0, &vtProp, 0, 0);
        cpu.thread_cnt = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"CurrentClockSpeed", 0, &vtProp, 0, 0);
        cpu.curr_clk_spd = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"CurrentVoltage", 0, &vtProp, 0, 0);
        cpu.curr_vltg = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        cpu_obj->Get(L"DataWidth", 0, &vtProp, 0, 0);
        cpu.data_width = static_cast<ULONG>(VTConvertNumeric(vtProp));
        VariantClear(&vtProp);

        proc_list.push_back(cpu);
        cpu_obj->Release();
    }

    cpu_enumerator->Release();
    svcs->Release();
}

void infoPhysicalDrive(IWbemLocator*& loc, std::vector<StorageDevice>& sd_list) {
    IWbemServices* svcs = nullptr;
    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> d_hmap;
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> p_hmap;
    std::unordered_map<wchar_t, Volume> v_hmap;
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> pd_hmap;

    IEnumWbemClassObject* disk_enumerator = nullptr;
    IEnumWbemClassObject* part_enumerator = nullptr;
    IEnumWbemClassObject* msft_enumerator = nullptr;
    IEnumWbemClassObject* vol_enumerator = nullptr;
    IWbemClassObject* disk_obj = nullptr;
    IWbemClassObject* part_obj = nullptr;
    IWbemClassObject* vol_obj = nullptr;
    IWbemClassObject* msft_phys = nullptr;

    ULONG u_ret = 0;

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

    HRESULT disk_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &disk_enumerator);

    if (FAILED(disk_query)) {
        std::wcout << L"MSFT_Disk Error. HRESULT: 0x"
            << std::hex << disk_query << std::endl;
        svcs->Release();
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
        VariantClear(&num);
        VariantClear(&fname);
        VariantClear(&manufacturer);
        VariantClear(&model);
        VariantClear(&d_sz);
        VariantClear(&num_partitions);
        disk_obj->Release();
    }
    disk_enumerator->Release();

    HRESULT part_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Partition"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &part_enumerator);

    if (FAILED(part_query)) {
        std::wcout << L"MSFT_Partition Error. HRESULT: 0x"
            << std::hex << part_query << std::endl;
        svcs->Release();
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

    HRESULT vol_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Volume"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &vol_enumerator);

    if (FAILED(vol_query)) {
        std::wcout << L"MSFT_Volume Error. HRESULT: 0x"
            << std::hex << vol_query << std::endl;
        svcs->Release();
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
            v_hmap.insert({ vol.drv_ltr, vol });
        }

        VariantClear(&drv_ltr);
        VariantClear(&sz);
        VariantClear(&sz_rmng);
        VariantClear(&hstatus);
        vol_obj->Release();
    }
    vol_enumerator->Release();

    HRESULT pd_query = svcs->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_PhysicalDisk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &msft_enumerator);

    if (FAILED(pd_query)) {
        std::wcout << L"MSFT_PhysicalDisk Error. HRESULT: 0x"
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

    for (auto disk_pair : d_hmap) {
        StorageDevice sd;
        bstr_t d_unq_id = std::get<0>(disk_pair);
        Disk disk = std::get<1>(disk_pair);
        ULONG d_disk_num = disk.disk_num;
        sd.setDisk(disk);

        for (int i = 0; i < disk.num_partitions; i++) {
            ULONG part_num = (ULONG)i;
            Partition p = p_hmap[{d_disk_num, part_num}];
            sd.getPartitions().push_back(p);
            if (p.drv_ltr != 0 && v_hmap.find(p.drv_ltr) != v_hmap.end()) {
                sd.getVolumes().push_back(v_hmap[p.drv_ltr]);
            }
        }

        PhysDisk pd = pd_hmap[d_disk_num];
        sd.setPhysicalDisk(pd);
        sd_list.push_back(sd);
    }
    svcs->Release();
}