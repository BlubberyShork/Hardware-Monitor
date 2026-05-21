#include "cpudriver.h"

UNICODE_STRING DEVICE_NAME = 
    RTL_CONSTANT_STRING(L"\\Device\\CPUMonitorDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\??\\CPUMonitorDriver");
    
WDFDEVICE dev = NULL;

FAST_MUTEX smn_mutex;

//4d36e97d-e325-11ce-bfc1-08002be10318
DEFINE_GUID(GUID_DEVINTERFACE_HWMONITOR,
    0x4d36e97dL, 0xe324, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);

CPU_VENDOR DetectCpuVendor(VOID) {
    int cpu_info[4];
    CHAR vendor[CPU_VENDOR_STRING_LEN];
    RtlZeroMemory(vendor, sizeof(vendor));

    int eax = 0; 
    int ebx = 1; 
    int ecx = 2;
    int edx = 3;
    __cpuid(cpu_info, eax);
        
    memcpy(&vendor[0], &cpu_info[ebx], sizeof(int));
    memcpy(&vendor[4], &cpu_info[edx], sizeof(int));
    memcpy(&vendor[8], &cpu_info[ecx], sizeof(int));
    vendor[12] = '\0';

    // Comparison doesn't include null terminator
    if(RtlCompareMemory(vendor, "GenuineIntel", CPU_VENDOR_STRING_LEN - 1) == CPU_VENDOR_STRING_LEN - 1) {
        return CPU_VENDOR_INTEL; 
    } else if(RtlCompareMemory(vendor, "AuthenticAMD", CPU_VENDOR_STRING_LEN - 1) == CPU_VENDOR_STRING_LEN - 1) {
        return CPU_VENDOR_AMD; 
    } else {
        return CPU_VENDOR_UNKNOWN;
    }
}

AMD_MODEL_AND_FAMILY DetectAMDModelAndFamily(VOID) {
    AMD_MODEL_AND_FAMILY info;
    RtlZeroMemory(&info, sizeof(AMD_MODEL_AND_FAMILY));

    int cpu_info[4];
    __cpuid(cpu_info, 0x1);
    // https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/56255_OSRR.pdf pg. 50
    // EAX layout:
    // bits 11:8  - base family
    // bits 19:16 - extended family  
    // bits 7:4   - base model
    // bits 19:16 - extended model (bits 7:4 of this field)

    ULONG base_family    = (cpu_info[0] >> 8)  & 0xF;
    ULONG ext_family     = (cpu_info[0] >> 20) & 0xFF;
    ULONG base_model     = (cpu_info[0] >> 4)  & 0xF;
    ULONG ext_model      = (cpu_info[0] >> 16) & 0xF;

    ULONG family = base_family + ext_family;
    ULONG model  = (ext_model << 4) | base_model;

    info.family = family;
    info.model = model;
    return info;
}

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  driver_obj,
    _In_ PUNICODE_STRING registry_path
) {
    KdPrint(("DriverEntry ENTERED\n"));
   
    NTSTATUS                status;
    WDFDRIVER               h_driver;
    PWDFDEVICE_INIT         p_init = NULL;
    WDF_DRIVER_CONFIG       config;
    WDF_OBJECT_ATTRIBUTES   attributes;

    ExInitializeFastMutex(&smn_mutex);

    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = EvtDriverUnload;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = EvtDriverContextCleanup;
    status = WdfDriverCreate(
        driver_obj,
        registry_path,
        &attributes,
        &config,
        &h_driver
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreate failed: 0x%x\n", status));
        return status;
    }

    p_init = WdfControlDeviceInitAllocate(
        h_driver,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R
    );
    if (p_init == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    status = NonPnpDeviceAdd(h_driver, p_init);

    return status;
}

VOID EvtDriverUnload(_In_ WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);

    KdPrint(("Unloading KMDF Driver..."));
    if (dev) {
        //IoDeleteSymbolicLink(&SYMLINK_NAME); --> Symbolic link cleaned up automatically
        WdfObjectDelete(dev);
        dev = NULL;
    }
    KdPrint(("KMDF driver unloaded\n"));
}

// WDFDRIVER is a wrapper for a PDRIVER_OBJECT
// Creates WDF device, which represents the MSR reading device for us to use
NTSTATUS NonPnpDeviceAdd(
    _In_    WDFDRIVER       driver,
    _Inout_ PWDFDEVICE_INIT device_init
) {
    KdPrint(("NonPnpDeviceAdd ENTERED\n"));
 
    UNREFERENCED_PARAMETER(driver);

    NTSTATUS                status;
    WDFDEVICE               control_device;
    WDF_OBJECT_ATTRIBUTES   attributes;

    WdfDeviceInitSetExclusive(device_init, TRUE);
    WdfDeviceInitSetIoType(device_init, WdfDeviceIoBuffered);

    status = WdfDeviceInitAssignSDDLString(device_init, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to set SDDL: 0x%x\n", status));
        goto END;
    }

    status = WdfDeviceInitAssignName(device_init, &DEVICE_NAME);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceInitAssignName failed: 0x%x\n", status));
        goto END;
    }

    WdfControlDeviceInitSetShutdownNotification(device_init, Shutdown, WdfDeviceShutdown);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CONTROL_DEVICE_EXTENSION);

    status = WdfDeviceCreate(&device_init, &attributes, &control_device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreate failed: 0x%x\n", status));
        goto END;
    }

    status = WdfDeviceCreateSymbolicLink(control_device, &SYMLINK_NAME);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x\n", status));
        goto END;
    }

    dev = control_device;

    WDF_IO_QUEUE_CONFIG queue_config;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config, WdfIoQueueDispatchSequential);
    queue_config.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(control_device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoQueueCreate failed: 0x%x\n", status));
        goto END;
    }

    WdfControlFinishInitializing(control_device);

END:
    if (device_init != NULL) {
        WdfDeviceInitFree(device_init);
    }
    return status;
}

VOID EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
) {
    KdPrint(("Entered EvtIoDeviceControl"));

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(Queue);

    PCPU_DATA_BUFFER  outbuffer = NULL;
    NTSTATUS          status;
    size_t            bytes_to_cpy = 0;

    if (IoControlCode != IOCTL_GET_DATA) {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    ULONG total_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    ULONG required_size = sizeof(CPU_DATA_HEADER) + total_procs * sizeof(CPU_DATA);

    // Must have room for header
    if (OutputBufferLength < sizeof(CPU_DATA_HEADER)) {
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_TOO_SMALL, 0);
        return;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(CPU_DATA_HEADER), &outbuffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    outbuffer->header.required_size = required_size;
    outbuffer->header.processor_count = total_procs;

    if (OutputBufferLength < required_size) {
        // Buffer too small, return header only
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_OVERFLOW, sizeof(CPU_DATA_HEADER));
        return;
    }

    GROUP_AFFINITY old_affinity;
    GROUP_AFFINITY new_affinity;
    RtlZeroMemory(&old_affinity, sizeof(GROUP_AFFINITY));
    RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));

    KeSetSystemGroupAffinityThread(&new_affinity, &old_affinity);

    CPU_VENDOR vendor = DetectCpuVendor();
    AMD_MODEL_AND_FAMILY amd_info = { 0 };
    if (vendor == CPU_VENDOR_AMD) {
        amd_info = DetectAMDModelAndFamily();
    }
    for (ULONG i = 0; i < total_procs; i++) { // Loops thru all logical processors
        PROCESSOR_NUMBER proc_num = { 0 };
        if (!NT_SUCCESS(KeGetProcessorNumberFromIndex(i, &proc_num)))
            continue;

        RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));
        new_affinity.Group = proc_num.Group;
        new_affinity.Mask = (KAFFINITY)(1ULL << proc_num.Number);
        KeSetSystemGroupAffinityThread(&new_affinity, NULL);

        switch (vendor) {
        case(CPU_VENDOR_INTEL):
            KdPrint(("CPU Vendor Intel detected\n"));
            if (!ReadCoreEraIntelMsrs(outbuffer, i, total_procs)) {
                KdPrint(("Failed to return MSR data\n"));   
            }
            break;
        case(CPU_VENDOR_AMD):
            KdPrint(("CPU Vendor AMD detected\n"));
            if (!ReadAMD17HTemps(outbuffer, i, total_procs, amd_info)) {
                KdPrint(("Failed to return MSR data\n"));
            }
            break;
        default:
            KdPrint(("Invalid vendor\n"));
            break;
        }
    }
    KeRevertToUserGroupAffinityThread(&old_affinity);

    bytes_to_cpy = required_size;
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytes_to_cpy);
}

VOID EvtDriverContextCleanup(
    _In_ WDFOBJECT driver
) {
    UNREFERENCED_PARAMETER(driver);
    return;
}

VOID Shutdown(
    WDFDEVICE device
) {
    UNREFERENCED_PARAMETER(device);
    return;
}

ULONG ReadSmn(ULONG address, ULONG* result) {
    PCI_SLOT_NUMBER slot = {0};
    slot.u.bits.DeviceNumber   = 0;
    slot.u.bits.FunctionNumber = 0;

    // Write SMN address to index register
    ExAcquireFastMutex(&smn_mutex);
    ULONG written = HalSetBusDataByOffset(
        PCIConfiguration,
        0,
        slot.u.AsULONG,
        &address,
        0x60,
        sizeof(ULONG)
    );
    if (written != sizeof(ULONG)) {
        KdPrint(("ReadSmn: Failed to write SMN address\n"));
        ExReleaseFastMutex(&smn_mutex);
        return FALSE;
    }

    // Read result from data register
    ULONG read = HalGetBusDataByOffset(
        PCIConfiguration,
        0,
        slot.u.AsULONG,
        result,
        0x64,
        sizeof(ULONG)
    );
    if (read != sizeof(ULONG)) {
        KdPrint(("ReadSmn: Failed to read SMN data\n"));
        ExReleaseFastMutex(&smn_mutex);
        return FALSE;
    }

    ExReleaseFastMutex(&smn_mutex);

    return TRUE;
}

BOOLEAN ReadCoreEraIntelMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt) {
    if (!outbuffer || cpu_idx >= cpu_cnt) {
        KdPrint(("ReadCoreEraIntelMsrs: Buffer invalid or indexed CPU out of bounds of CPU count\n"));
        return FALSE;
    }

    int cpu_info[4];
    RtlZeroMemory(cpu_info, sizeof(cpu_info));
    __cpuid(cpu_info, INTEL_CPUID_THERM_SENSOR_LEAF);

    if ((cpu_info[0] & INTEL_CPUID_THERM_SENSOR_DTS_BIT) == 0) {
        KdPrint(("ReadCoreEraIntelMsrs: Digital Thermal Sensor not supported\n"));
        return FALSE; // Digital Thermal Sensor not supported
    }

    ULONGLONG therm_status = __readmsr(INTEL_CORE_ERA_THERM_STATUS);

    if ((therm_status & (1ULL << 31)) == 0) {
        KdPrint(("ReadCoreEraIntelMsrs: Invalid temperature bit\n"));
        return FALSE; // Invalid temperature bit
    }
    ULONGLONG therm_target = __readmsr(INTEL_CORE_ERA_THERM_TARGET);

    ULONG temp_max = (therm_target >> 16) & 0xFF;
    ULONG temp_offset = (therm_status >> 16) & 0x7F; //32bit is safer here

    if (temp_offset > temp_max) {
        KdPrint(("ReadCoreEraIntelMsrs: Temp offset > temp max\n"));
        return FALSE;
    }
    USHORT real_temp = (USHORT)(temp_max - temp_offset);

    CPU_DATA* curr_data = &outbuffer->data[cpu_idx];
    curr_data->temp = real_temp;
    // TODO - curr_data->load = load;
    curr_data->cpu_id = cpu_idx;

    KdPrint(("ReadCoreEraIntelMsrs: Returning true...\n"));
    return TRUE;
}

BOOLEAN ReadAMD17HTemps(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt, AMD_MODEL_AND_FAMILY amd_info) {
    KdPrint(("AMD model: %d\n", amd_info.model));
    switch (amd_info.model) {
    case AMD_MODEL_ZEN:
    case AMD_MODEL_ZEN_APU:
        //if (!ReadZenAmdData(outbuffer, cpu_idx)) {
        //    KdPrint(("Failed to read Zen model data\n"));
        //}
        break;
    case AMD_MODEL_ZEN_PLUS:
    case AMD_MODEL_ZEN_PLUS_APU:
        if (!ReadZenPlusAmdData(outbuffer, cpu_idx, cpu_cnt)) {
            KdPrint(("Failed to read Zen+ model temperature data\n"));
        }
        break;
    case AMD_MODEL_ZEN2:
    case AMD_MODEL_ZEN2_APU:
    case AMD_MODEL_ZEN2_THREADRIPPER:
        //ReadZen2AmdData(outbuffer, cpu_idx);
        break;
    default:
        KdPrint(("ReadZenEraAmdMsrs: Unrecognized AMD model: 0x%x\n", amd_info.model));
        return FALSE;
        break;
    }
    return TRUE;
}

BOOLEAN ReadZenPlusAmdData(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt) {
    if (!outbuffer || cpu_idx >= cpu_cnt) {
        KdPrint(("ReadCoreEraIntelMsrs: Buffer invalid or indexed CPU out of bounds of CPU count\n"));
        return FALSE;
    }

    ULONG temperature = 0;
    ULONG address = (ULONG)AMD_FAMILY_17H_M01H_THM_TCON_CUR_TEMP;
    ULONG status = ReadSmn(address, &temperature);

    if (status == 0) return FALSE;

    // Pulled from LibreHardwareMonitor : https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LibreHardwareMonitorLib/Hardware/Cpu/Amd17Cpu.cs#L23 
    BOOLEAN tempOffsetFlag = ((temperature & AMD_FAMILY_17H_M01H_THM_TCON_TEMP_RANGE_SEL_MASK) != 0)
        || ((temperature & AMD_FAMILY_17H_TEMP_TJ_SEL_MASK) == AMD_FAMILY_17H_TEMP_TJ_SEL_MASK);
    temperature = (temperature >> 21) * 125; // Raw 11-bit value from [31:21]

    LONG real_temp_milli = (LONG)(temperature);
    if (tempOffsetFlag) {
        real_temp_milli -= 49000;
    }

    if (real_temp_milli < 0 || real_temp_milli > 150000) {
        KdPrint(("ReadZenPlusAmdData: Suspicious temperature: %d mdeg\n", real_temp_milli));
        return FALSE;
    }

    CPU_DATA* curr_data = &outbuffer->data[cpu_idx];
    curr_data->temp = (USHORT)(real_temp_milli / 1000); // loses accuracy slightly, but its fine enough for now.
    curr_data->cpu_id = cpu_idx;

    KdPrint(("Retrieved AMD Zen+ temp: %d\n", curr_data->temp));

    return TRUE;
}

/*
boolean readZenEraAMDMsrs(cpu_data_buffer* outbuffer, ulong cpu_idx, ulong cpu_cnt) {
    if (!outbuffer || cpu_idx >= cpu_cnt) {
        kdprint(("readzeneraamdmsrs: buffer invalid or indexed cpu out of bounds of cpu count\n"));
        return false;
    }

    ulonglong therm_target = __readmsr(amd_zen_era_temperature_target);
    ulong temp_max = (therm_target >> 16) & 0xff;
    if (temp_max == 0) {
        kdprint(("readzeneraamdmsrs: temp max == 0\n"));
        return false;
    }

    ulonglong therm_status = __readmsr(amd_zen_era_therm_status);
    
    ulong temp_offset = therm_status & 0x7f;
    if (temp_offset > temp_max) {
        kdprint(("readzeneraamdmsrs: temp offset > temp max\n"));
        return false;
    }

    boolean temp_reading_supported = (therm_status & (1ull << 31)) != 0;
    if (!temp_reading_supported) {
        temp_offset = 0; 
    }

    ushort real_temp = (ushort)(temp_max - temp_offset);

    cpu_data* curr_data = &outbuffer->data[cpu_idx];
    curr_data->temp = real_temp;
    curr_data->cpu_id = cpu_idx;

    kdprint(("readzeneraamdmsrs: returning true...\n"));
    return true;
}
*/



