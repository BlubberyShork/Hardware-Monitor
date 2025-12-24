#include "cpudriver.h"

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT     driver_obj,
    _In_ PUNICODE_STRING    registry_path
) {
    NTSTATUS status = STATUS_SUCCESS;

    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    config.EvtDriverUnload = EvtDriverUnload;

    status = WdfDriverCreate(driver_obj,
                             registry_path,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             WDF_NO_HANDLE);
    if(!NT_SUCCESS(status)){
        KdPrint(("WdfDriverCreate failed: 0x%x\n", status));
        return status;
    }

    return status;
}

// WDFDRIVER is a wrapper for a PDRIVER_OBJECT
// Creates WDF device, which represents the MSR reading device for us to use
NTSTATUS EvtDeviceAdd(
    _In_ WDFDRIVER driver, 
    _Inout_ PWDFDEVICE_INIT device_init
) {

    NTSTATUS status;
    WDFDEVICE h_device;
    WDFQUEUE queue;

    UNREFERENCED_PARAMETER(driver);

    status = WdfDeviceCreate(device_init,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &h_device);
    if(!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreate failed: 0x%x\n", status));
        return status;
    }

    // creates link for user mode code to interact with our device
    status = WdfDeviceCreateSymbolicLink(SYMLINK_NAME, DEVICE_NAME);
    if(!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x\n", status));
        return status;
    }

    WDF_IO_QUEUE_CONFIG = io_queue_config;
    WDFQUEUE = h_queue;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &io_queue_config,
        WdfIoQueueDispatchSequential
    );

    status = WdfIoQueueCreate(
        device,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &h_queue
    );
    
    if (!NT_SUCCESS (status)) {
        return status;
    }
}

NTSTATUS EvtDeviceUnload(WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);
    KdPrint(("WDF driver unloaded\n"));

    //Dont actually need to do work here, this is basically empty
    //the routine by default deletes the driver thru config.
}

VOID EvtIoDeviceControl(
    WDFQUEUE Queue, 
    WDFREQUEST Request, 
    size_t OutputBufferLength,
    size_t InputBufferLength, 
    ULONG IoControlCode
) {

    PVOID outbuffer = NULL;
    size_t realoutbuffersz = 0;
    NTSTATUS status;
    ULONG cpu_data_list_sz = 0;
    PCPU_DATA cpu_data_list = NULL;

    if (IoControlCode != IOCTL_GET_DATA) {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    // Get buffer size needed for processor info
    ULONG proc_list_len = 0;
    status = KeQueryLogicalProcessorRelationship(
        NULL,
        RelationProcessorCore, // Share same single processor core 
        NULL,
        &proc_list_len
    );

    if(status != STATUS_INFO_LENGTH_MISMATCH) {
        WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
        return;
    }

    // Allocate buffer for processor info
    // TODO - everything has to be done with the Ke prefixed functions since we are in kernel space
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer = *proc_list; // TODO - proc_list doesnt exist
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX end_buffer = *buffer + buffer.ReturnedLength;

    cpu_data_list_sz = proc_list_len * sizeof(CPU_DATA); 
    cpu_data_list = (PCPU_DATA)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        cpu_data_list_sz,
        'UPCD'
    );

    RtlZeroMemory(cpu_data_list, cpu_data_list_sz); 

    // Loops thru all logical processors
    while((BYTE*)buffer < end_buffer) {
        ULONG core_id = 0;
        HANDLE h_thread = GetCurrentThread(); // TODO - is there a Ke variant?
        GROUP_AFFINITY old_affinity;

        // Run on current thread from the group mask in the buffer (Per core, not logical proc) 
        // TODO - This is user space. Need to use the kernel equivalent with the GROUP_AFFINITY somehow 
        SetThreadGroupAffinity(hThread, &buffer->Processor.GroupMask[0].Mask, &old_affinity); 

        uint64_t THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);
        uint64_t THERM_TARGET = __rdmsr(INTEL_THERM_TARGET);

        // TODO - If CPUID.06H:EAX[0] = 1 -> add this check before both
        uint32_t temp_max = (THERM_TARGET >> 16) & 0xFF;
        uint32_t temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit int is safer here
        
        int16_t real_temp = (int16_t) temp_max - (int16_t) temp_offset;

        CPU_DATA curr_data;
        curr_data.temp = real_temp;
        // TODO - curr_data.load = load;
        curr_data.core_id = core_id;
        cpu_data_list[core_id] = curr_data; 
        
        core_id++;
    }
    KeRevertToUserAffinityThread();

    if(IoControlCode == IOCTL_GET_DATA) {
        status = WdfRequestOutputBuffer(
            Request,
            cpu_data_list_size,
            outbuffer,
            realoutbuffersz
        );
        if(NT_SUCCESS(status)) {
            RtlCopyMemory(outbuffer, cpu_data_list, cpu_data_list_size);
            free(cpu_data_list);
            WdfRequestComplete(Request, STATUS_SUCCESS, cpu_data_list_size);
            return;
        } else {
            WdfRequestComplete(Request, status);
            free(cpu_data_list);
            return;
        }
    } else {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        free(cpu_data_list);
        return;
    }
}


/**
*   Gets the current APIC_ID for the active thread
*
*   If RET_VAL = UINT32_MAX, then an error has occured
*/
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



