
#ifndef CPU_DRIVER_H
#define CPU_DRIVER_H

#include <ntddk.h>
#include <wdf.h>
#include <intrin.h>
//#include <stdint.h>
//#include <winnt.h>

#include "../shared_headers/cpu_shared_info.h"

#define INTEL_THERM_STATUS 0x19C
#define INTEL_THERM_TARGET 0x1A2

extern UNICODE_STRING device_name;
extern UNICODE_STRING symlink_name;

typedef enum _CPU_VENDOR {
    CPU_VENDOR_UNKNOWN = 0,
    CPUT_VENDOR_INTEL,
    CPU_VENDOR_AMD
} CPU_VENDOR;

CPU_VENDOR DetectCpuVendor(VOID);

// TODO!
// Forward declarations
NTSTATUS DriverEntry(
    _In_    PDRIVER_OBJECT      driver_obj,
    _In_    PUNICODE_STRING     registry_path
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

VOID EvtDriverUnload(
    _In_ WDFDRIVER Driver
);

/***************************************************
*                   Helper Funcs                   *
***************************************************/
void ReadIntelMsrs(CPU_DATA_BUFFER* buffer);


//int32_t getCurrentApicId()

#endif
