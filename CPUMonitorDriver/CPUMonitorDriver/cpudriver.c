
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT     driver_obj,
    _In_ PUNICODE_STRING    registry_path
) {
    NTSTATUS status = STATUS_SUCCESS;
    WDFDRIVER h_device;

    WDF_DRIVER_CONFIG config;

    // Run code from other func
   


    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    

    status = WdfDriverCreate(driver_obj,
                             registry_path,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             WDF_NO_HANDLE);
    if(!NT_SUCCESS(status)){
        KdPrint(("WdfDriverCreate failed: 0x%x\n", status));
        return status;
    }

    //TODO - create the IOqueue for EvtIoDeviceControl;

    return status;
}

//WDFDRIVER is a wrapper for a PDRIVER_OBJECT
NTSTATUS EvtDeviceAdd(_In_ WDFDRIVER driver, _Inout_ PWDFDEVICE_INIT device_init) {

    NTSTATUS status;
    WDFDEVICE h_device;

    status = WdfDriverCreate(device_init,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &driver);
    if(!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreate failed: 0x%x\n"), status);
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(SYMLINK_NAME, DEVICE_NAME);
    if(!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x), status);
        return status;
    }
}

VOID EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {
{
    
    uint64_t THERM_STATUS = rdmsr(0x19CH);
    uint32_t temp_offset = (THERM_STATUS >> 16) & 0x7F; //32bit num is safer here

    UINT64_T THERM_TARGET = rdmsr(0x1A2H);
    uint32_t temp_max = (THERM_TARGET >> 16) & 0xFF;

    int16_t real_temp = (int16_t) temp_max - (int16_t) temp_offset;

    //TODO - store temp and also cpu load in a struct container
    //WdfRequestRetrieveOutputBuffer() and 
    //WdfRequestCompleteWithInformation() 

    WDFDEVICE h_device;

    status = WdfDriverCreate(&device_init,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &h_device);

    return status;
}




