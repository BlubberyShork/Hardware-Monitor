#ifndef CPU_SHARED_INFO_H
#define CPU_SHARED_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

// User-Space include
#ifndef _KERNEL_MODE
#include <winioctl.h>
#include <intrin.h>
#include <string.h>
#else
#include <wdm.h>
#include <intrin.h>
#endif

typedef enum _CPU_VENDOR {
    CPU_VENDOR_UNKNOWN = 0,
    CPU_VENDOR_INTEL,
    CPU_VENDOR_AMD
} CPU_VENDOR;

typedef enum _METRIC_TYPE {
    METRIC_TEMP = 0,
    METRIC_VOLTAGE,
    METRIC_CLOCK_SPEED,
    METRIC_LOAD,
    METRIC_POWER
} METRIC_TYPE;

typedef enum _METRIC_UNIT {
    UNIT_NONE = 0,
    UNIT_DEGREES_C,
    UNIT_MILLIVOLTS,
    UNIT_MHZ,
    UNIT_PERCENT,
    UNIT_MILLIWATTS
} METRIC_UNIT;

#define CPU_VENDOR_STRING_LEN 13

static __inline CPU_VENDOR DetectCpuVendor(void) {
    int cpu_info[4];
    char vendor[CPU_VENDOR_STRING_LEN];

    __cpuid(cpu_info, 0);
    memcpy(&vendor[0], &cpu_info[1], sizeof(int));
    memcpy(&vendor[4], &cpu_info[3], sizeof(int));
    memcpy(&vendor[8], &cpu_info[2], sizeof(int));
    vendor[12] = '\0';

    if (memcmp(vendor, "GenuineIntel", CPU_VENDOR_STRING_LEN - 1) == 0) {
        return CPU_VENDOR_INTEL;
    } else if (memcmp(vendor, "AuthenticAMD", CPU_VENDOR_STRING_LEN - 1) == 0) {
        return CPU_VENDOR_AMD;
    }
    return CPU_VENDOR_UNKNOWN;
}

typedef struct _CPU_DATA {
    ULONG           value;
    char            name[32];
    USHORT          type;       // METRIC_TYPE
    USHORT          unit;       // METRIC_UNIT
    USHORT          cpu_id;
} CPU_DATA, *PCPU_DATA;

typedef struct _CPU_DATA_HEADER {
    ULONG           required_size;
    ULONG           processor_count;
    ULONG           entry_count;
} CPU_DATA_HEADER, *PCPU_DATA_HEADER;

typedef struct _CPU_DATA_BUFFER {
    CPU_DATA_HEADER header;
    CPU_DATA        data[1];   // [1] instead of [0] for C compliance
} CPU_DATA_BUFFER, *PCPU_DATA_BUFFER;

// shared IOCTL code for kernel mode and user mode
#define IOCTL_GET_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef __cplusplus
}
#endif

#endif // CPU_SHARED_INFO_H
