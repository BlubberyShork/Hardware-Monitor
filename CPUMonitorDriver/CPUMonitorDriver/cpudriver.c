#include "cpudriver.h"

UNICODE_STRING DEVICE_NAME =
    RTL_CONSTANT_STRING(L"\\Device\\CPUMonitorDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\DosDevices\\CPUMonitorDriver");

CPU_VENDOR DetectCpuVendor(VOID) {
    size_t vendor_str_len = 13;

    int cpu_info[4];
    CHAR vendor[vendor_str_len];

    int eax = 0;
    int ebx = 1;
    int ecx = 2;
    int edx = 3;
    __cpuid(cpu_info, eax);

    memcpy(&vendor[0], &cpu_info[ebx], sizeof(cpu_info[0]));
    memcpy(&vendor[4], &cpu_info[edx], sizeof(cpu_info[4]));
    memcpy(&vendor[8], &cpu_info[ecx], sizeof(cpu_info[8]));
    vendor[12] = '\0';

    // Check this condition
    // TODO - add DbgPrint's
    if(RtlCompareMemory(&vendor, "GenuineIntel", 12) == 13) {
        return CPU_VENDOR_INTEL; 
    }
    else if(RtlCompareMemory(&vendor, "AuthenticAMD", 12) == 13) {
        return CPU_VENDOR_AMD; 
    } else {
        return CPU_VENDOR_UNKNOWN;
    }
}

/* Setup taken by https://www.ired.team/miscellaneous-reversing-forensics/windows-kernel-internals/sending-commands-from-userland-to-your-kernel-driver-using-ioctl#driver.c */
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT     driver_obj,
    _In_ PUNICODE_STRING    registry_path
) {
    NTSTATUS status = STATUS_SUCCESS;

    driver_obj->DriverUnload = DriverUnload;
    
    driver_obj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HandleIOCTLDispatchRoutine;
    driver_obj->MajorFunction[IRP_MJ_CREATE] = MajorFunction;
    driver_obj->MajorFunction[IRP_MJ_CLOSE] = MajorFunction;

	DbgPrint("Driver loaded");

	status = IoCreateDevice(driver_obj, 0, &DEVICE_NAME, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &driver_obj->DeviceObj);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Could not create device %wZ", DEVICE_NAME);
	} else  {
		DbgPrint("Device %wZ created", DEVICE_NAME);
	}

	status = IoCreateSymbolicLink(&SYMLINK_NAME) &DEVICE_NAME);
	if (NT_SUCCESS(status)) {
		DbgPrint("Symbolic link %wZ created", SYMLINK_NAME);
	} else {
		DbgPrint("Error creating symbolic link %wZ", SYMLINK_NAME);
	} 

    return status;
}

NTSTATUS HandleIOCTLDispatchRoutine(PDEVICE_OBJECT dev_obj, PIRP irp) {
    UNREFERENCED_PARAMETER(dev_obj);

	PIO_STACK_LOCATION stack_loc = NULL;
    stack_loc = IoGetCurrentIrpStackLocation(irp);

    PCPU_DATA_BUFFER outbuffer = NULL;
    size_t bytes_to_cpy = 0;
    NTSTATUS status;

    if (irp->CurrentStackLocation->IoControlCode != IOCTL_GET_DATA) {
        Irp->IoStatus.Information = 0;  
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST; 
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return;
    }

    ULONG total_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    ULONG required_size = sizeof(CPU_DATA_HEADER) + total_procs * sizeof(CPU_DATA);

    // Must have room for header
    if (stack_loc->DeviceIoControl.OutputBufferLength < sizeof(CPU_DATA_HEADER)) {
        Irp->IoStatus.Information = 0;  
        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL; 
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return;
    }

    outbuffer->header.required_size = required_size;
    outbuffer->header.processor_count = total_procs;

    if (stack_loc->DeviceIoControl.OutputBufferLength < required_size) {
        // Buffer too small, return header only
        Irp->IoStatus.Information = sizeof(CPU_DATA_HEADER);  
        Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW; 
        IoCompleteRequest(irp, IO_NO_INCREMENT);
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

    Irp->IoStatus.Information = bytes_to_cpy; //TODO - Number of bytes 
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS MajorFunction(PDEVICE_OBJECT dev_obj, PIRP irp) {
    PIO_STACK_LOCATION stack_loc = NULL;
    stack_loc = IoGetCurrentIrpStackLocation(irp);

    switch (stack_loc->MajorFunction)
	{
	case IRP_MJ_CREATE:
		DbgPrint("Handle to symbolink link %wZ opened", DEVICE_SYMBOLIC_NAME);
		break;
	case IRP_MJ_CLOSE:
		DbgPrint("Handle to symbolink link %wZ closed", DEVICE_SYMBOLIC_NAME);
		break;
	default:
		break;
	}

    Irp->IoStatus.Information = 0; 
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceUnload(_In_ WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);
    DbgPrint("WDF driver unloaded\n");

    //Dont actually need to do work here, this is basically empty
    //the routine by default deletes the driver thru config.
}

void ReadIntelMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_index) {
    ULONGLONG THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);
    ULONGLONG THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);

    // TODO - If CPUID.06H:EAX[0] = 1 -> add this check before both
    UINT temp_max = (THERM_TARGET >> 16) & 0xFF;
    UINT temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit int is safer here
    WORD real_temp = (int16_t)(temp_max - temp_offset);

    CPU_DATA curr_data;
    curr_data.temp = real_temp;
    // TODO - curr_data.load = load;
    curr_data.cpu_id = cpu_index;
    outbuffer->data[cpu_index] = curr_data; 
}

////////////////////////////////////////////////////////
/* Deprecated Code */

/*
VOID EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue, 
    _In_ WDFREQUEST Request, 
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength, 
    _In_ ULONG      IoControlCode
) {
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(Queue);

    CPU_DATA_BUFFER* outbuffer = NULL;
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

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(CPU_DATA_HEADER), (VOID**)&outbuffer, NULL);
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
*/



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


