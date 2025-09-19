#include "cpudriver.h"
#include <ntddk.h>
#include <wdf.h>
#include <intrin.h>

#define DEVICE_NAME L"\\Device\\CPUMonitorDriver"
#define SYMLINK_NAME L"\\DosDevices\\CPUMonitorDriver"
#define IOCTL_GET_TEMP CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

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

//TODO - move to header
struct cpuData() {
    uint32_t core_cnt;
    uint16_t temp;  // There exists some beastly AMD cpu with 192 cores
    uint64_t cpu_load;
}
