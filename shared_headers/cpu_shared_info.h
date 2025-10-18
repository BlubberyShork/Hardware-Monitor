#ifndef cpu_shared_info.h
#define cpu_shared_info.h

typedef struct cpuData() {
    uint32_t core_cnt;
    uint16_t temp;  // There exists some beastly AMD cpu with 192 cores
    uint64_t cpu_load;
} CPU_DATA *PCPU_DATA

#define IOCTL_GET_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif
