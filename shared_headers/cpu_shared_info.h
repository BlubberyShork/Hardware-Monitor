#ifndef CPU_SHARED_INFO_H
#define CPU_SHARED_INFO_H

#include <stdint.h>
#include <winioctl.h>

typedef struct cpuData {
    uint16_t temp;  // There exists some beastly AMD cpu with 192 cores
    uint64_t cpu_load;
    uint32_t cpu_id;
} CPU_DATA, *PCPU_DATA;

#define IOCTL_GET_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif // CPU_SHARED_INFO_H
