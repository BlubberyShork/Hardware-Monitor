
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
        KdPrint(("WdfDriverCreate failed: 0x%x\n"), status);
        return status;
    }

    // creates link for user mode code to interact with our device
    status = WdfDeviceCreateSymbolicLink(SYMLINK_NAME, DEVICE_NAME);
    if(!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x\n)", status);
        return status;
    }
    
    //TODO - WdfIoQueueCreate() -> see documentation
}

NTSTATUS EvtDeviceUnload(WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);
    KdPrint(("WDF driver unloaded\n"));

    //Dont actually need to do work here, this is basically empty
    //the routine by default deletes the driver thru config.
}

VOID EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {
{
    // in buffer will be empty

    // get core count from __cpuid in user space and use that data in input buffer length (include the core count and a list of each cpu id)
        // Do I want to create the struct elsewhere and pass it in with list of all cores in the processor struct so I can populate the vector with
        // id's, temps, and load
    // loop thru core count creating structs, put in array and return

    uint64_t THERM_STATUS = __rdmsr(INTEL_THERM_STATUS);
    uint32_t temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit num is safer here

    UINT64_T THERM_TARGET = __rdmsr(INTEL_THERM_TARGET);
    uint32_t temp_max = (THERM_TARGET >> 16) & 0xFF;

    int16_t real_temp = (int16_t) temp_max - (int16_t) temp_offset;

    //TODO - store temp and also cpu load in a struct container
        // WdfRequestRetrieveOutputBuffer() and 
        // WdfRequestCompleteWithInformation() 

}




