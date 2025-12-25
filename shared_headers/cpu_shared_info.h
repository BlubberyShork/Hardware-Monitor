#ifndef CPU_SHARED_INFO_H
#define CPU_SHARED_INFO_H

#include <stdint.h>
#include <winioctl.h>

typedef struct _CPU_DATA {
    int16_t temp;  // There exists some beastly AMD cpu with 192 cores
    uint64_t cpu_load;
    uint32_t cpu_id;
} CPU_DATA, *PCPU_DATA;

typedef struct _CPU_DATA_HEADER {
    ULONG required_size;
    ULONG processor_count;
} CPU_DATA_HEADER, *PCPU_DATA_HEADER

typedef struct _CPU_DATA_BUFFER {
    CPU_DATA_HEADER header;
    CPU_DATA data[0];
} CPU_DATA_BUFFER, *PCPU_DATA_BUFFER

#define IOCTL_GET_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif // CPU_SHARED_INFO_H
