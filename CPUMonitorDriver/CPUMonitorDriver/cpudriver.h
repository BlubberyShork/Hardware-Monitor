
#ifndef CPU_DRIVER_H
#define CPU_DRIVER_H

#include <ntddk.h>
#include <wdf.h>

#include "../shared_headers/cpu_shared_info.h"

#define INTEL_THERM_STATUS 0x19C
#define INTEL_THERM_TARGET 0x1A2

extern UNICODE_STRING DEVICE_NAME;
extern UNICODE_STRING SYMLINK_NAME;

typedef enum _CPU_VENDOR {
    CPU_VENDOR_UNKNOWN = 0,
    CPU_VENDOR_INTEL, 
    CPU_VENDOR_AMD
} CPU_VENDOR;
#define CPU_VENDOR_STRING_LEN 13
CPU_VENDOR DetectCpuVendor(VOID);


NTSTATUS DriverEntry(
    _In_    PDRIVER_OBJECT      driver_obj,
    _In_    PUNICODE_STRING     registry_path
);

NTSTATUS EvtDriverUnload(
    _In_    WDFDRIVER           Driver
);

NTSTATUS EvtDeviceAdd(
    _In_    WDFDRIVER           driver,
    _Inout_ PWDFDEVICE_INIT     device_init
);

VOID EvtIoDeviceControl(
    _In_    WDFQUEUE            queue,
    _In_    WDFREQUEST          request,
    _In_    size_t              OutputBufferLength,
    _In_    size_t              InputBufferLength,
    _In_    ULONG               IoControlCode
);

/***************************************************
*                   Helper Funcs                   *
***************************************************/
void ReadIntelMsrs(CPU_DATA_BUFFER* buffer, ULONG cpu_index);
//int32_t getCurrentApicId()

#endif
