#ifndef CPU_SHARED_INFO_H
#define CPU_SHARED_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

// Needs separate handling
#ifndef _KERNEL_MODE
#include <winioctl.h>
#else

// kernel mode CTL_CODE from wdm.h/ntddk.h
#include <wdm.h>
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define FILE_DEVICE_UNKNOWN 0x00000022
#define METHOD_BUFFERED     0
#define FILE_ANY_ACCESS     0
#endif

// kernel-compatible types
typedef struct _CPU_DATA {
    USHORT temp;        
    ULONGLONG cpu_load;
    ULONG cpu_id;
} CPU_DATA, *PCPU_DATA;

typedef struct _CPU_DATA_HEADER {
    ULONG required_size;
    ULONG processor_count;
} CPU_DATA_HEADER, *PCPU_DATA_HEADER;

typedef struct _CPU_DATA_BUFFER {
    CPU_DATA_HEADER header;
    CPU_DATA data[1];   // [1] instead of [0] for C compliance
} CPU_DATA_BUFFER, *PCPU_DATA_BUFFER;

// shared IOCTL code for kernel mode and user mode 
#define IOCTL_GET_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef __cplusplus
}
#endif

#endif // CPU_SHARED_INFO_H
