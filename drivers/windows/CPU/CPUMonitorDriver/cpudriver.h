
#ifndef CPU_DRIVER_H
#define CPU_DRIVER_H

#include <ntddk.h>
#include <intrin.h>
#include <wdf.h>
#include <wdfdevice.h>
#include <wdmsec.h>

#include "../../../../shared_headers/cpu_shared_info.h"

// TODO - Good comments for these
#define INTEL_CORE_ERA_THERM_STATUS         0x19C
#define INTEL_CORE_ERA_THERM_TARGET         0x1A2
#define INTEL_CPUID_THERM_SENSOR_LEAF       0x06
#define INTEL_CPUID_THERM_SENSOR_DTS_BIT    0x1

#define AMD_MODEL_ZEN                       0x01  
#define AMD_MODEL_ZEN_APU                   0x11  
#define AMD_MODEL_ZEN_PLUS                  0x08  
#define AMD_MODEL_ZEN_PLUS_APU              0x18  
#define AMD_MODEL_ZEN2                      0x71  
#define AMD_MODEL_ZEN2_APU                  0x60  
#define AMD_MODEL_ZEN2_THREADRIPPER         0x31

#define AMD_FAMILY_17H_M01H_THM_TCON_CUR_TEMP   0x00059800
#define AMD_FAMILY_17H_M01H_THM_TCON_TEMP_RANGE_SEL_MASK 0x80000
#define AMD_FAMILY_17H_TEMP_TJ_SEL_MASK         0x30000

#define DEVICE_SDDL L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"

extern UNICODE_STRING DEVICE_NAME;
extern UNICODE_STRING SYMLINK_NAME;
extern WDFDEVICE      dev;          // The driver device

typedef struct _CONTROL_DEVICE_EXTENSION {
    HANDLE fileHandle;
} CONTROL_DEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTROL_DEVICE_EXTENSION, ControlGetData)

typedef enum _CPU_VENDOR {
    CPU_VENDOR_UNKNOWN = 0,
    CPU_VENDOR_INTEL, 
    CPU_VENDOR_AMD
} CPU_VENDOR;
#define CPU_VENDOR_STRING_LEN 13
CPU_VENDOR DetectCpuVendor(VOID);

typedef struct _AMD_MODEL_AND_FAMILY {
    ULONG       family;
    ULONG       model;
    // TODO - brand string
} AMD_MODEL_AND_FAMILY;
AMD_MODEL_AND_FAMILY DetectAMDModelAndFamily(VOID); 

DRIVER_INITIALIZE DriverEntry;

NTSTATUS DriverEntry(
    _In_    PDRIVER_OBJECT      driver_obj,
    _In_    PUNICODE_STRING     registry_path
);

VOID EvtDriverUnload(
    _In_    WDFDRIVER           Driver
);

NTSTATUS NonPnpDeviceAdd(
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

VOID Shutdown(
    WDFDEVICE device
);

VOID EvtDriverContextCleanup(
    _In_    WDFOBJECT           Driver
);

/***************************************************
*                   Helper Funcs                   *
***************************************************/
ULONG ReadSmn(ULONG address, ULONG* result); 
BOOLEAN ReadCoreEraIntelMsrs(CPU_DATA_BUFFER* buffer, ULONG cpu_idx, ULONG cpu_cnt);
BOOLEAN ReadAMD17HTemps(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt, AMD_MODEL_AND_FAMILY amd_info);
BOOLEAN ReadZenPlusAmdData(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt);
//BOOLEAN ReadZenEraAmdMsrs(CPU_DATA_BUFFER* outbuffer, ULONG cpu_idx, ULONG cpu_cnt);

#endif // CPU_DRIVER_H
