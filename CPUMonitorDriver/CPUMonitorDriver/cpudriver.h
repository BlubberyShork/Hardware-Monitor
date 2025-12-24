
#ifndef CPU_DRIVER_H
#define CPU_DRIVER_H

#include "../shared_headers/cpu_shared_info.h"
#include <ntddk.h>
#include <wdf.h>
#include <intrin.h>
#include <stdint.h>
#include <winnt.h>

#define DEVICE_NAME L"\\Device\\CPUMonitorDriver"
#define SYMLINK_NAME L"\\DosDevices\\CPUMonitorDriver"

#define INTEL_THERM_STATUS 0x19C
#define INTEL_THERM_TARGET 0x1A2

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

/***************************************************
*                   Helper Funcs                   *
***************************************************/

int getCurrentApicId()

#endif
