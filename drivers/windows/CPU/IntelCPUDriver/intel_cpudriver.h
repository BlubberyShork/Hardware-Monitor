
#ifndef INTEL_CPU_DRIVER_H
#define INTEL_CPU_DRIVER_H

#include <ntddk.h>
#include <wdf.h>
#include <wdfdevice.h>
#include <wdmsec.h>
#include "../../../../kernel_common/cpu_shared_info.h"

// Pulled from linux kernel
#define GENMASK(h, l) \
    (((~0ULL) >> (64 - 1 - (h))) & ((~0ULL) << (l)))

/***************************************************
*                 MSR Addresses                    *
*                                                  *
*  Intel SDM Vol. 4: Model-Specific Registers      *
*  https://www.intel.com/content/dam/develop/       *
*  external/us/en/documents/335592-sdm-vol-4.pdf   *
***************************************************/

// Thermal (Per-Core)
#define IA32_THERM_STATUS           0x19C
#define IA32_THERM_TARGET           0x1A2

// Thermal (Package)
#define IA32_PACKAGE_THERM_STATUS   0x1B1

// Voltage / Performance State
#define MSR_PERF_STATUS             0x198
#define IA32_PLATFORM_INFO          0xCE

// Load (MPERF)
#define IA32_MPERF                  0xE7

// Power (RAPL)
#define MSR_RAPL_POWER_UNIT         0x606
#define MSR_PKG_ENERGY_STATUS       0x611
#define MSR_PP0_ENERGY_STATUS       0x639

/***************************************************
*             MSR / CPUID Bit Fields               *
*  Named masks using GENMASK(high_bit, low_bit).   *
*  Shift by the low_bit after masking.             *
***************************************************/

// IA32_THERM_STATUS / IA32_PACKAGE_THERM_STATUS
#define THERM_STATUS_VALID_BIT          (1ULL << 31)
#define THERM_STATUS_OFFSET             GENMASK(22, 16)     // [22:16] temp offset from TjMax

// IA32_THERM_TARGET
#define THERM_TARGET_TJMAX              GENMASK(23, 16)     // [23:16] TjMax in degrees C

// MSR_PERF_STATUS (0x198)
#define PERF_STATUS_VID                 GENMASK(47, 32)     // [47:32] core voltage VID
#define PERF_STATUS_PSTATE_RATIO        GENMASK(15, 8)      // [15:8]  current P-state ratio

// IA32_PLATFORM_INFO (0xCE)
#define PLATFORM_INFO_BASE_RATIO        GENMASK(15, 8)      // [15:8]  max non-turbo ratio

// MSR_RAPL_POWER_UNIT (0x606)
#define RAPL_UNIT_ESU                   GENMASK(12, 8)      // [12:8]  energy status unit (power of 2 divisor)

// CPUID leaf 0x06 (Thermal/Power Management)
#define CPUID_THERM_POWER_LEAF          0x06
#define CPUID_06H_EAX_DTS_BIT          (1 << 0)            // EAX[0]  Digital Thermal Sensor
#define CPUID_06H_EAX_PKG_THERM_BIT    (1 << 6)            // EAX[6]  Package thermal management
#define CPUID_06H_ECX_MPERF_BIT        (1 << 0)            // ECX[0]  MPERF/APERF available

// CPUID leaf 0x01 (Processor Info)
#define CPUID_01H_FAMILY                GENMASK(11, 8)      // EAX[11:8]  base family
#define CPUID_01H_EXT_MODEL             GENMASK(19, 16)     // EAX[19:16] extended model
#define CPUID_01H_BASE_MODEL            GENMASK(7, 4)       // EAX[7:4]   base model

/***************************************************
*                   Constants                      *
*                                                  *
*  VID conversion referenced from                  *
*  LibreHardwareMonitor IntelCpu.cs                *
*  https://github.com/LibreHardwareMonitor/        *
*  LibreHardwareMonitor                            *
***************************************************/
#define INTEL_BUS_CLOCK_MHZ         100
#define METRICS_PER_CORE            4   // temp, VID, clock, load
#define METRICS_AGGREGATE           3   // package temp, package power, core domain power
#define RAPL_MIN_MODEL              0x2A // Sandy Bridge

#define DEVICE_SDDL L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"

extern UNICODE_STRING DEVICE_NAME;
extern UNICODE_STRING SYMLINK_NAME;
extern WDFDEVICE      dev;

/***************************************************
*                  Delta State                     *
***************************************************/

typedef struct _CORE_DELTA_STATE {
    ULONGLONG       prev_mperf;
    ULONGLONG       prev_tsc;
    BOOLEAN         valid;
} CORE_DELTA_STATE;

typedef struct _PKG_DELTA_STATE {
    ULONG           prev_pkg_energy;
    LARGE_INTEGER   prev_pkg_qpc;
    BOOLEAN         pkg_valid;
    ULONG           prev_pp0_energy;
    LARGE_INTEGER   prev_pp0_qpc;
    BOOLEAN         pp0_valid;
    ULONG           energy_unit_divisor;
} PKG_DELTA_STATE;

typedef struct _CONTROL_DEVICE_EXTENSION {
    HANDLE fileHandle;
} CONTROL_DEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTROL_DEVICE_EXTENSION, ControlGetData)

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
*              Per-Core Metric Collection          *
***************************************************/
BOOLEAN ReadPerCoreIntelMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    ULONG            cpu_idx,
    USHORT*          buffer_idx
);

/***************************************************
*             Aggregate Metric Collection          *
***************************************************/
BOOLEAN ReadIntelAggregateMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    USHORT*          buffer_idx
);

/***************************************************
*          Per-Core Telemetry Procedures           *
*  Returned CPU_DATA has core index in cpu_id      *
***************************************************/
CPU_DATA retCoreTemp(ULONG cpu_idx);
CPU_DATA retCoreVID(ULONG cpu_idx);
CPU_DATA retCoreClockSpeed(ULONG cpu_idx);
CPU_DATA retCoreLoad(ULONG cpu_idx);

/***************************************************
*          Aggregate Telemetry Procedures          *
*  cpu_id set to 0xFFFF (not per-core)             *
***************************************************/
CPU_DATA retTDieTemp(void);
CPU_DATA retPackagePower(void);
CPU_DATA retCoreDomainPower(void);

/***************************************************
*               Buffer Management                  *
*  Checks capacity before writing. Returns FALSE   *
*  if the buffer is too small (caller should        *
*  return STATUS_BUFFER_OVERFLOW to userspace).     *
***************************************************/
BOOLEAN tryAddTelemetryData(
    CPU_DATA_BUFFER* buffer,
    ULONG            buffer_capacity,
    CPU_DATA         data,
    USHORT           buffer_idx
);

#endif // INTEL_CPU_DRIVER_H
