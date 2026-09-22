
#ifndef AMD_CPU_DRIVER_H
#define AMD_CPU_DRIVER_H

#include <ntddk.h>
#include <wdf.h>
#include <wdfdevice.h>
#include <wdmsec.h>
#include "../../../../kernel_common/cpu_shared_info.h"

#ifndef GENMASK
#define GENMASK(h, l) \
    (((~0ULL) >> (64 - 1 - (h))) & ((~0ULL) << (l)))
#endif

/***************************************************
*              SMN Register Addresses              *
*                                                  *
*  SMN temperature and SVI2 voltage addresses      *
*  referenced from LibreHardwareMonitor AmdCpu.cs  *
*  https://github.com/LibreHardwareMonitor/        *
*  LibreHardwareMonitor                            *
***************************************************/
#define AMD_THM_TCON_CUR_TEMP_ADDR      0x00059800
#define AMD_SVI2_PLANE0_ADDR            0x0005A00C
#define AMD_SVI2_PLANE1_ADDR            0x0005A010

/***************************************************
*              SMN PCI Config Offsets              *
***************************************************/
#define SMN_INDEX_REGISTER              0x60
#define SMN_DATA_REGISTER               0x64

/***************************************************
*                MSR Addresses                     *
*                                                  *
*  P-state MSRs: AMD PPR (Processor Programming    *
*  Reference) for Family 17h, publicly available   *
*  https://github.com/tpn/pdfs (AMD section)       *
*                                                  *
*  RAPL energy MSRs: Linux kernel amd_energy       *
*  driver (https://www.kernel.org/doc/              *
*  Documentation/hwmon/amd_energy.rst)              *
*  and AMD SMI documentation                       *
*  (https://rocm.docs.amd.com/projects/amdsmi)     *
***************************************************/
#define AMD_MSR_PSTATE_STATUS           0xC0010063
#define AMD_MSR_PSTATE_DEF_BASE         0xC0010064  // +N for P-state N
#define AMD_MSR_RAPL_POWER_UNIT         0xC0010299
#define AMD_MSR_CORE_ENERGY_STATUS      0xC001029A  // Per-core energy counter
#define AMD_MSR_PKG_ENERGY_STATUS       0xC001029B

// Load (same architectural MSR as Intel)
#define IA32_MPERF                      0xE7

/***************************************************
*             Bit Field Masks                      *
*  Named masks using GENMASK(high_bit, low_bit).   *
*  Shift by the low_bit after masking.             *
***************************************************/

// THM_TCON_CUR_TEMP (SMN 0x00059800)
#define THM_CUR_TEMP_RAW                GENMASK(31, 21)     // [31:21] raw temp, 1/8 deg C units
#define THM_CUR_TEMP_RANGE_SEL          GENMASK(19, 19)     // [19]    49C offset flag
#define THM_CUR_TEMP_TJ_SEL            GENMASK(17, 16)     // [17:16] TJ select (49C when both set)

// SVI2 Voltage (SMN registers)
#define SVI2_VID_FIELD                  GENMASK(23, 16)     // [23:16] Voltage ID (8-bit)

// P-State Definition MSR (0xC0010064+N)
#define PSTATE_DEF_CPU_FID              GENMASK(7, 0)       // [7:0]   Frequency ID
#define PSTATE_DEF_CPU_DFS_ID           GENMASK(13, 8)      // [13:8]  DFS Divider ID

// P-State Status MSR (0xC0010063)
#define PSTATE_STATUS_CUR               GENMASK(2, 0)       // [2:0]   Current P-state number

// AMD RAPL Power Unit MSR (0xC0010299)
#define AMD_RAPL_ESU                    GENMASK(12, 8)      // [12:8]  Energy status unit (2^N divisor)

// CPUID.01H:EAX
#define CPUID_THERM_POWER_LEAF          0x06
#define CPUID_06H_ECX_MPERF_BIT        (1 << 0)            // ECX[0] MPERF/APERF available
#define CPUID_01H_BASE_FAMILY           GENMASK(11, 8)      // EAX[11:8]
#define CPUID_01H_EXT_FAMILY            GENMASK(27, 20)     // EAX[27:20]
#define CPUID_01H_BASE_MODEL            GENMASK(7, 4)       // EAX[7:4]
#define CPUID_01H_EXT_MODEL             GENMASK(19, 16)     // EAX[19:16]

/***************************************************
*                  Constants                       *
*                                                  *
*  SVI2 voltage conversion and plane swap logic    *
*  referenced from LibreHardwareMonitor AmdCpu.cs  *
*  https://github.com/LibreHardwareMonitor/        *
*  LibreHardwareMonitor                            *
***************************************************/

// SVI2 voltage: V = 1.55 - VID * 6.25mV
// Integer: mV = (6200 - VID * 25 + 2) / 4    (quarter-mV arithmetic with rounding)
#define SVI2_BASE_MV_X4                 6200    // 1550 mV * 4
#define SVI2_STEP_MV_X4                 25      // 6.25 mV * 4
#define SVI2_ROUNDING                   2       // half-divisor for round-to-nearest

// P-state frequency: MHz = 200 * FID / DFS_ID
#define AMD_PSTATE_FREQ_BASE            200

// AMD family thresholds
#define AMD_FAMILY_ZEN                  0x17    // Zen/Zen+/Zen2
#define AMD_FAMILY_ZEN3                 0x19    // Zen3/Zen4 (SVI2 planes swapped)

// Temperature offset for Tctl-to-Tdie correction (millidegrees)
#define AMD_TEMP_OFFSET_MDEG            49000
// 1/8 deg C = 125 millidegrees
#define AMD_TEMP_UNIT_MDEG              125

#define METRICS_PER_CORE                3       // clock, load, per-core power
#define METRICS_AGGREGATE               4       // Tdie temp, core voltage, SoC voltage, package power

#define DEVICE_SDDL L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"

extern UNICODE_STRING DEVICE_NAME;
extern UNICODE_STRING SYMLINK_NAME;
extern WDFDEVICE      dev;

/***************************************************
*                 Delta State                      *
***************************************************/

typedef struct _AMD_CORE_DELTA_STATE {
    ULONGLONG       prev_mperf;
    ULONGLONG       prev_tsc;
    ULONG           prev_core_energy;
    LARGE_INTEGER   prev_core_qpc;
    BOOLEAN         valid;
} AMD_CORE_DELTA_STATE;

typedef struct _AMD_PKG_DELTA_STATE {
    ULONG           prev_pkg_energy;
    LARGE_INTEGER   prev_pkg_qpc;
    BOOLEAN         valid;
    ULONG           energy_unit_divisor;
} AMD_PKG_DELTA_STATE;

typedef struct _AMD_MODEL_AND_FAMILY {
    ULONG           family;
    ULONG           model;
} AMD_MODEL_AND_FAMILY;

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
*              SMN Bus Access                      *
***************************************************/
BOOLEAN ReadSmn(ULONG address, ULONG* result);

/***************************************************
*           Per-Core Metric Collection             *
***************************************************/
BOOLEAN ReadPerCoreAmdMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    ULONG            cpu_idx,
    USHORT*          buffer_idx
);

/***************************************************
*          Aggregate Metric Collection             *
***************************************************/
BOOLEAN ReadAmdAggregateMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    USHORT*          buffer_idx
);

/***************************************************
*          Per-Core Telemetry Procedures           *
***************************************************/
CPU_DATA retCoreClockSpeed(ULONG cpu_idx);
CPU_DATA retCoreLoad(ULONG cpu_idx);
CPU_DATA retCorePower(ULONG cpu_idx);

/***************************************************
*         Aggregate Telemetry Procedures           *
*  cpu_id set to 0xFFFF (not per-core)             *
***************************************************/
CPU_DATA retTDieTemp(void);
CPU_DATA retCoreVoltage(void);
CPU_DATA retSoCVoltage(void);
CPU_DATA retPackagePower(void);

/***************************************************
*               Buffer Management                  *
***************************************************/
BOOLEAN tryAddTelemetryData(
    CPU_DATA_BUFFER* buffer,
    ULONG            buffer_capacity,
    CPU_DATA         data,
    USHORT           buffer_idx
);

#endif // AMD_CPU_DRIVER_H
