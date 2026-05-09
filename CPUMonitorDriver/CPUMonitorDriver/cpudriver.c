#include "cpudriver.h"

UNICODE_STRING DEVICE_NAME = 
    RTL_CONSTANT_STRING(L"\\Device\\CPUMonitorDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\??\\CPUMonitorDriver");
    
WDFDEVICE dev = NULL;

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
        IoDeleteSymbolicLink(&SYMLINK_NAME);
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
            if (!ReadZenEraAmdMsrs(outbuffer, i, total_procs)) {
                KdPrint(("Failed to return AMD MSR data\n"));
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

BOOLEAN ReadZenEraAmdMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt) {
    if (!outbuffer || cpu_idx >= cpu_cnt) {
        KdPrint(("ReadZenEraAmdMsrs: Buffer invalid or indexed CPU out of bounds of CPU count\n"));
        return FALSE;
    }

    ULONGLONG therm_target = __readmsr(AMD_ZEN_ERA_TEMPERATURE_TARGET);
    ULONG temp_max = (therm_target >> 16) & 0xFF;
    if (temp_max == 0) {
        KdPrint(("ReadZenEraAmdMsrs: Temp Max == 0\n"));
        return FALSE;
    }

    ULONGLONG therm_status = __readmsr(AMD_ZEN_ERA_THERM_STATUS);
    
    ULONG temp_offset = therm_status & 0x7F;
    if (temp_offset > temp_max) {
        KdPrint(("ReadZenEraAmdMsrs: Temp offset > temp max\n"));
        return FALSE;
    }

    BOOLEAN temp_reading_supported = (therm_status & (1ULL << 31)) != 0;
    if (!temp_reading_supported) {
        temp_offset = 0; 
    }

    USHORT real_temp = (USHORT)(temp_max - temp_offset);

    CPU_DATA* curr_data = &outbuffer->data[cpu_idx];
    curr_data->temp = real_temp;
    curr_data->cpu_id = cpu_idx;

    KdPrint(("ReadZenEraAmdMsrs: Returning true...\n"));
    return TRUE;
}
