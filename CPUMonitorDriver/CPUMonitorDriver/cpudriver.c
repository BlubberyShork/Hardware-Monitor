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
    ULONG required_size = sizeof(CPU_DATA_HEADER) + total_procs * sizeof(CPU_DATA)

    // Must have room for header
    if (OutputBufferLength < sizeof(CPU_DATA_HEADER)) {
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_TOO_SMALL, 0);
        return;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(CPU_DATA_HEADER), (PVOID*)&outbuffer, NULL);
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

    // Full buffer available, retrieve full pointer
    status = WdfRequestRetrieveOutputBuffer(Request, required_size, (PVOID*)&outbuffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    GROUP_AFFINITY old_affinity = {0};
    GROUP_AFFINITY new_affinity = {0};
    KeSetSystemGroupAffinityThread(&new_affinity, &old_affinity);

    // Loops thru all logical processors
    for(ULONG i = 0; i < total_procs; i++) {
        PROCESSOR_NUMBER proc_num = {0};
        if(!NT_SUCCESS(KeGetProcessorNumberFromIndex(i, &proc_num)))
            continue;

        RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));
        new_affinity.Group = proc_num.Group;
        new_affinity.Mask = (KAFFINITY)(1ULL << proc_num.Number);

        KeSetSystemGroupAffinityThread(&new_affinity, NULL);

        //TODO - Make this be based on processor type (w/ macros)
        uint64_t THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);
        uint64_t THERM_TARGET = __rdmsr(INTEL_THERM_TARGET);

        // TODO - If CPUID.06H:EAX[0] = 1 -> add this check before both
        uint32_t temp_max = (THERM_TARGET >> 16) & 0xFF;
        uint32_t temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit int is safer here
        
        int16_t real_temp = (int16_t) temp_max - (int16_t) temp_offset;

        CPU_DATA curr_data;
        curr_data.temp = real_temp;
        // TODO - curr_data.load = load;
        curr_data.core_id = i;
        cpu_data_list[i] = curr_data; 
    }
    KeSetSystemGroupAffinityThread(&old_affinity, NULL);
    
    bytesToCopy = requiredSize;
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytesToCopy);
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


