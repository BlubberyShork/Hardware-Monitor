#include "cpudriver.h"
#include <ntddk.h>
#include <wdf.h>

#define DEVICE_NAME L"\\Device\\CPUMonitorDriver"
#define SYMLINK_NAME L"\\DosDevices\\CPUMonitorDriver"
#define IOCTL_GET_TEMP CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// TODO!
// Forward declarations
NTSTATUS EvtDeviceAdd(
    _In_ WDFDRIVER driver, 
    _Inout_ PWDFDEVICE_INIT device_init
);

VOID EvtIoDeviceControl(
    _In_ WDFQUEUE queue,                     
    _In_ WDFREQUEST request,                     
    _In_ size_t OutputBufferLength, 
    _In_ size_t InputBufferLength, 
    _In_ ULONG IoControlCode
);

//TODO - move to header
struct cpuData() {
    uint32_t core_cnt;
    uint16_t[] temp;
    // TODO - cpu/core load
}

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT     driver_obj,
    _In_ PUNICODE_STRING    registry_path
) {
    NTSTATUS status = STATUS_SUCCESS;

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

    return status;
}

NTSTATUS EvtDeviceAdd(_In_ WDFDRIVER driver, _Inout_ PWDFDEVICE_INIT device_init) {


//TODO will need to use the macros above creating a 
    //Device name and symbolic link

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




