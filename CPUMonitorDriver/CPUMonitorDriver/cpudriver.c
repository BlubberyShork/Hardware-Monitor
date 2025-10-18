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
NTSTATUS EvtDeviceAdd(_In_ WDFDRIVER driver, _Inout_ PWDFDEVICE_INIT device_init) {

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

VOID EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {
    PVOID outbuffer;
    PVOID inbuffer;
    size_t realoutbuffersz;
    NTSTATUS status;

    status = WdfRequestRetrieveInputBuffer(
        Request,
        InputBufferLength,
        inbuffer,
        sizeof(int)
    );    

    int num_cores = *(int*)inbuffer;
    CPU_DATA *cpu_data_list = (PCPU_DATA) malloc(n_cores * sizeof(CPU_DATA));
    uint32_t cpu_data_list_size = n_cores * sizeof(CPU_DATA);

    for(uint32_t i = 0; i < num_cores; i++) {
        CPU_DATA curr_data;

        uint64_t THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);
        uint32_t temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit num is safer here

        uint64_t THERM_TARGET = __rdmsr(INTEL_THERM_TARGET);
        uint32_t temp_max = (THERM_TARGET >> 16) & 0xFF;

        int16_t real_temp = (int16_t) temp_max - (int16_t) temp_offset;

        curr_data.core_cnt = num_cores;
        curr_data.temp = real_temp;
        // TODO - curr_data.load = load;
    }

    if(IoControlCode == IOCTL_GET_DATA) {
        status = WdfRequestOutputBuffer(
            Request,
            cpu_data_list_size,
            outbuffer,
            realoutbuffersz
        );
        if(NT_SUCCESS(status)) {
            RtlCopyMemory(outbuffer, cpu_data_list, cpu_data_list_size);
            WdfRequestComplete(Request, STATUS_SUCCESS, cpu_data_list_size);
        } else {
            WdfRequestComplete(Request, status);
        }
    } else {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
    }

}




