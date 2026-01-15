#include "cpudriver.h"

UNICODE_STRING DEVICE_NAME =
    RTL_CONSTANT_STRING(L"\\Device\\CPUMonitorDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\DosDevices\\CPUMonitorDriver");

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
    _In_ PDRIVER_OBJECT     driver_obj,
    _In_ PUNICODE_STRING    registry_path
) {
    NTSTATUS status = STATUS_SUCCESS;

    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    config.EvtDriverUnload = EvtDriverUnload;

    status = WdfDriverCreate(
        driver_obj,
        registry_path,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreate failed: 0x%x\n", status));
        return status;
    }

    return status;
}

//Dont actually need to do work here, this is basically empty
//the routine by default deletes the driver thru config.
NTSTATUS EvtDriverUnload(_In_ WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);
    DbgPrint("WDF driver unloaded\n");
}

// WDFDRIVER is a wrapper for a PDRIVER_OBJECT
// Creates WDF device, which represents the MSR reading device for us to use
NTSTATUS EvtDeviceAdd(
    _In_    WDFDRIVER       driver,
    _Inout_ PWDFDEVICE_INIT device_init
) {
    UNREFERENCED_PARAMETER(driver);

    NTSTATUS status;
    WDFDEVICE h_device;

    status = WdfDeviceCreate(device_init, WDF_NO_OBJECT_ATTRIBUTES, &h_device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreate failed: 0x%x\n", status));
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(h_device, &SYMLINK_NAME);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x\n", status));
        return status;
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
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(Queue);

    PCPU_DATA_BUFFER  outbuffer     = NULL;
    NTSTATUS          status        = NULL;
    size_t            bytes_to_cpy  = 0;

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
    // Loops thru all logical processors
    for(ULONG i = 0; i < total_procs; i++) {
        PROCESSOR_NUMBER proc_num = {0};
        if(!NT_SUCCESS(KeGetProcessorNumberFromIndex(i, &proc_num)))
            continue;

        RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));
        new_affinity.Group = proc_num.Group;
        new_affinity.Mask = (KAFFINITY)(1ULL << proc_num.Number);
        KeSetSystemGroupAffinityThread(&new_affinity, NULL);

        switch(vendor) {
            case(CPU_VENDOR_INTEL):
                KdPrint(("CPU Vendor Intel detected\n"));
                if (!ReadCoreEraIntelMsrs(&outbuffer, i, total_procs)) {
                    KdPrint(("Failed to return MSR data\n"));
                }
                break;
            case(CPU_VENDOR_AMD):
                KdPrint(("CPU Vendor AMD detected\n"));
                if (!ReadZenEraAmdMsrs(&outbuffer, i, total_procs)) {
                    KdPrint(("Failed to return AMD MSR data\n"));
                }
                break;
            default:
                KdPrint(("Invalid vendor\n"));
                break;
        }
        KeRevertToUserGroupAffinityThread(&old_affinity);
    }
     
    bytes_to_cpy = required_size;
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytes_to_cpy);
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

    ULONGLONG therm_target = __rdmsr(INTEL_CORE_ERA_THERM_TARGET);
    ULONGLONG therm_status = __rdmsr(INTEL_CORE_ERA_THERM_STATUS);

    if ((therm_status & (1ULL << 31)) == 0) {
        KdPrint(("ReadCoreEraIntelMsrs: Invalid temperature bit\n"));
        return FALSE; // Invalid temperature bit
    }

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
    if ((therm_status & (1ULL << 31)) == 0) { // Invalid temperature bit 
        KdPrint(("ReadZenEraAmdMsrs: Invalid temperature bit\n"));
        return FALSE;
    }

    ULONG temp_offset = therm_status & 0x7F;
    if (temp_offset > temp_max) {
        KdPrint(("ReadZenEraAmdMsrs: Temp offset > temp max\n"));
        return FALSE;
    }
    USHORT real_temp = (USHORT)(temp_max - temp_offset);

    CPU_DATA* curr_data = &outbuffer->data[cpu_idx];
    curr_data->temp = real_temp;
    curr_data->cpu_id = cpu_idx;

    KdPrint(("ReadZenEraAmdMsrs: Returning true...\n"));
    return TRUE;
}


/**
*   Gets the current APIC_ID for the active thread
*
*   If RET_VAL = UINT32_MAX, then an error has occured
*/
/* 
int32_t getCurrentApicId() { 
    uint32_t apic_id = 0;

    int cpu_info[4];

    // Get highest valid func id
    __cpuid(cpu_info, 0x0);
    uint32_t highest_leaf = cpu_info[0];

    // Check if x2APIC is supported
    __cpuid(cpu_info, 0x1);
    bool x2apic_supported = (cpu_info[2] >> 21) & 1;

    if(!x2apic_supported) {
        return cpu_info[1] >> 24 & 0xFF;
    }

    // TODO - comment this because now I have no clue what this is doing 3 months later. What the hell is this
    if(highest_leaf >= 0x1F) {
        __cpuid(cpu_info, 0x1F);
        uint32_t leaf_1fh_valid = cpu_info[0] >> 4 & 0xF;
        if(leaf_1fh_valid > 0) {
            return cpu_info[3] >> 31 & 0xFFFFFFFF;
        }
    } else if(highest_leaf >= 0x0B) {
        __cpuid(cpu_info, 0x0B);
        uint32_t leaf_1fh_valid = cpu_info[0] >> 4 & 0xF;
        if(leaf_1fh_valid > 0) {
            return cpu_info[3] >> 31 & 0xFFFFFFFF;
        }
    }

    return UINT32_MAX; // Error 
}
*/
