
#ifndef CPU_DRIVER_H
#define CPU_DRIVER_H

#include <ntddk.h>
#include <wdf.h>

#include "../shared_headers/cpu_shared_info.h"

// TODO - Good comments for these
#define INTEL_CORE_ERA_THERM_STATUS         0x19C
#define INTEL_CORE_ERA_THERM_TARGET         0x1A2
#define INTEL_CPUID_THERM_SENSOR_LEAF       0x06
#define INTEL_CPUID_THERM_SENSOR_DTS_BIT    0x1
#define AMD_ZEN_ERA_THERM_STATUS            0xC0010015
#define AMD_ZEN_ERA_TEMPERATURE_TARGET      0xC0010064

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
BOOLEAN ReadCoreEraIntelMsrs(CPU_DATA_BUFFER* buffer, ULONG cpu_idx, ULONG cpu_cnt);
BOOLEAN ReadZenEraAmdMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt);

//int32_t getCurrentApicId()

#endif
