#include "cpudriver.h"

UNICODE_STRING DEVICE_NAME =
    RTL_CONSTANT_STRING(L"\\Device\\CPUMonitorDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\DosDevices\\CPUMonitorDriver");

CPU_VENDOR DetectCpuVendor(VOID) {
    int cpu_info[4];
    CHAR vendor[CPU_VENDOR_STRING_LEN];

    int eax = 0; 
    int ebx = 1; 
    int ecx = 2;
    int edx = 3;
    __cpuid(cpu_info, eax);

    memcpy(&vendor[0], &cpu_info[ebx], sizeof(cpu_info[0]));
    memcpy(&vendor[4], &cpu_info[edx], sizeof(cpu_info[4]));
    memcpy(&vendor[8], &cpu_info[ecx], sizeof(cpu_info[8]));
    vendor[12] = '\0';

    if(RtlCompareMemory(&vendor, "GenuineIntel", CPU_VENDOR_STRING_LEN) == CPU_VENDOR_STRING_LEN) {
        return CPU_VENDOR_INTEL; 
    } else if(RtlCompareMemory(&vendor, "AuthenticAMD", CPU_VENDOR_STRING_LEN) == CPU_VENDOR_STRING_LEN) {
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

    PCPU_DATA_BUFFER outbuffer = NULL;
    size_t bytes_to_cpy = 0;
    NTSTATUS status;

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

    GROUP_AFFINITY old_affinity = {0};
    GROUP_AFFINITY new_affinity = {0};
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

        // Check CPU Vendor
        // then handle
        switch(vendor) {
            case(CPU_VENDOR_INTEL):
                ReadIntelMsrs(&outbuffer, i);
                break;
            case(CPU_VENDOR_AMD):
                //TODO- ReadAMDMsrs(&outbuffer, i); 
                break;
            default:
                break;
        }
        KeRevertToUserGroupAffinityThread(&old_affinity);
    }
     
    bytes_to_cpy = required_size;
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytes_to_cpy);
}

void ReadIntelMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_index) {
    ULONGLONG therm_target = __rdmsr(INTEL_THERM_TARGET);
    ULONGLONG therm_status = __rdmsr(INTEL_THERM_STATUS);

    // TODO - If CPUID.06H:EAX[0] = 1 -> add this check before both
    ULONG temp_max = (therm_target >> 16) & 0xFF;
    ULONG temp_offset = (therm_status >> 16) & 0x7F; //32bit int is safer here
    USHORT real_temp = (USHORT)(temp_max - temp_offset);

    CPU_DATA curr_data;
    curr_data.temp = real_temp;
    // TODO - curr_data.load = load;
    curr_data.cpu_id = cpu_index;
    outbuffer->data[cpu_index] = curr_data;
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


//NTSTATUS HandleIOCTLDispatchRoutine(PDEVICE_OBJECT dev_obj, PIRP irp) {
//    UNREFERENCED_PARAMETER(dev_obj);
//
//	PIO_STACK_LOCATION stack_loc = NULL;
//    stack_loc = IoGetCurrentIrpStackLocation(irp);
//
//    PCPU_DATA_BUFFER outbuffer = NULL;
//    size_t bytes_to_cpy = 0;
//    NTSTATUS status;
//
//    if (stack_loc->IoControlCode != IOCTL_GET_DATA) {
//        irp->IoStatus.Information = 0;  
//        irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST; 
//        IoCompleteRequest(irp, IO_NO_INCREMENT);
//        return;
//    }
//
//    ULONG total_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
//    ULONG required_size = sizeof(CPU_DATA_HEADER) + total_procs * sizeof(CPU_DATA);
//
//    // Must have room for header
//    if (stack_loc->DeviceIoControl.OutputBufferLength < sizeof(CPU_DATA_HEADER)) {
//        irp->IoStatus.Information = 0;
//        irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
//        IoCompleteRequest(irp, IO_NO_INCREMENT);
//        return;
//    }
//
//    outbuffer->header.required_size = required_size;
//    outbuffer->header.processor_count = total_procs;
//
//    if (stack_loc->DeviceIoControl.OutputBufferLength < required_size) {
//        // Buffer too small, return header only
//        irp->IoStatus.Information = sizeof(CPU_DATA_HEADER);
//        irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
//        IoCompleteRequest(irp, IO_NO_INCREMENT);
//        return;
//    }
//
//    GROUP_AFFINITY old_affinity = {0};
//    GROUP_AFFINITY new_affinity = {0};
//    KeSetSystemGroupAffinityThread(&new_affinity, &old_affinity);
//
//    CPU_VENDOR vendor = DetectCpuVendor();
//    // Loops thru all logical processors
//    for(ULONG i = 0; i < total_procs; i++) {
//        PROCESSOR_NUMBER proc_num = {0};
//        if(!NT_SUCCESS(KeGetProcessorNumberFromIndex(i, &proc_num)))
//            continue;
//
//        RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));
//        new_affinity.Group = proc_num.Group;
//        new_affinity.Mask = (KAFFINITY)(1ULL << proc_num.Number);
//        KeSetSystemGroupAffinityThread(&new_affinity, NULL);
//
//        // Check CPU Vendor
//        // then handle
//        switch(vendor) {
//            case(CPU_VENDOR_INTEL):
//                ReadIntelMsrs(&outbuffer, i);
//                break;
//            case(CPU_VENDOR_AMD):
//                //TODO- ReadAMDMsrs(&outbuffer, i); 
//                break;
//            default:
//                break;
//        }
//    
//        KeRevertToUserGroupAffinityThread(&old_affinity);
//    }
//    bytes_to_cpy = required_size;
//
//    irp->IoStatus.Information = bytes_to_cpy; //TODO - Number of bytes 
//    irp->IoStatus.Status = STATUS_SUCCESS;
//    IoCompleteRequest(irp, IO_NO_INCREMENT);
//
//    return STATUS_SUCCESS;
//}
//
//NTSTATUS MajorFunction(PDEVICE_OBJECT dev_obj, PIRP irp) {
//    PIO_STACK_LOCATION stack_loc = NULL;
//    stack_loc = IoGetCurrentIrpStackLocation(irp);
//
//    switch (stack_loc->MajorFunction)
//	{
//	case IRP_MJ_CREATE:
//		DbgPrint("Handle to symbolink link %wZ opened", SYMLINK_NAME);
//		break;
//	case IRP_MJ_CLOSE:
//		DbgPrint("Handle to symbolink link %wZ closed", SYMLINK_NAME);
//		break;
//	default:
//		break;
//	}
//
//    irp->IoStatus.Information = 0;
//    irp->IoStatus.Status = STATUS_SUCCESS;
//    IoCompleteRequest(irp, IO_NO_INCREMENT);
//
//    return STATUS_SUCCESS;
//}


