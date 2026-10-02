#include "amd_cpudriver.h"

UNICODE_STRING DEVICE_NAME =
    RTL_CONSTANT_STRING(L"\\Device\\WindowsAMDCPUDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\??\\WindowsAMDCPUDriver");

WDFDEVICE dev = NULL;

static FAST_MUTEX       smn_mutex;
static BOOLEAN          g_has_mperf         = FALSE;
static AMD_MODEL_AND_FAMILY g_amd_info      = { 0 };
static ULONG            g_svi2_core_addr    = 0;
static ULONG            g_svi2_soc_addr     = 0;

static AMD_CORE_DELTA_STATE* g_core_state   = NULL;
static AMD_PKG_DELTA_STATE   g_pkg_state    = { 0 };
static ULONG                 g_max_procs    = 0;
static LARGE_INTEGER         g_qpc_freq     = { 0 };

//4d36e97d-e325-11ce-bfc1-08002be10318
DEFINE_GUID(GUID_DEVINTERFACE_HWMONITOR,
    0x4d36e97dL, 0xe324, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);

/***************************************************
*              Feature Detection                   *
***************************************************/

static AMD_MODEL_AND_FAMILY DetectAMDModelAndFamily(void) {
    AMD_MODEL_AND_FAMILY info;
    RtlZeroMemory(&info, sizeof(AMD_MODEL_AND_FAMILY));

    int cpu_info[4];
    __cpuid(cpu_info, 0x01);

    ULONG base_family = (ULONG)((cpu_info[0] & CPUID_01H_BASE_FAMILY) >> 8);
    ULONG ext_family  = (ULONG)((cpu_info[0] & CPUID_01H_EXT_FAMILY)  >> 20);
    ULONG base_model  = (ULONG)((cpu_info[0] & CPUID_01H_BASE_MODEL)  >> 4);
    ULONG ext_model   = (ULONG)((cpu_info[0] & CPUID_01H_EXT_MODEL)   >> 16);

    info.family = base_family + ext_family;
    info.model  = (ext_model << 4) | base_model;
    return info;
}

static void DetectFeatures(void) {
    g_amd_info = DetectAMDModelAndFamily();

    int info[4];
    __cpuid(info, CPUID_THERM_POWER_LEAF);
    g_has_mperf = (info[2] & CPUID_06H_ECX_MPERF_BIT) != 0;

    // LibreHardwareMonitor AmdCpu.cs: family 19h+ swaps SVI2 plane mapping
    if (g_amd_info.family >= AMD_FAMILY_ZEN3) {
        g_svi2_core_addr = AMD_SVI2_PLANE1_ADDR;
        g_svi2_soc_addr  = AMD_SVI2_PLANE0_ADDR;
    } else {
        g_svi2_core_addr = AMD_SVI2_PLANE0_ADDR;
        g_svi2_soc_addr  = AMD_SVI2_PLANE1_ADDR;
    }

    KdPrint(("AMD features: Family=0x%x Model=0x%x MPERF=%d CoreSVI2=0x%x SoCSVI2=0x%x\n",
        g_amd_info.family, g_amd_info.model, g_has_mperf, g_svi2_core_addr, g_svi2_soc_addr));
}

/***************************************************
*               Driver Lifecycle                   *
***************************************************/

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  driver_obj,
    _In_ PUNICODE_STRING registry_path
) {
    KdPrint(("DriverEntry ENTERED\n"));

    NTSTATUS                status;
    WDFDRIVER               h_driver;
    PWDFDEVICE_INIT         p_init = NULL;
    WDF_DRIVER_CONFIG       config;
    WDF_OBJECT_ATTRIBUTES   attributes;

    ExInitializeFastMutex(&smn_mutex);
    DetectFeatures();
    KeQueryPerformanceCounter(&g_qpc_freq);

    // Linux kernel amd_energy: energy unit from MSR_RAPL_POWER_UNIT[12:8]
    if (g_amd_info.family >= AMD_FAMILY_ZEN) {
        ULONGLONG rapl_unit = __readmsr(AMD_MSR_RAPL_POWER_UNIT);
        ULONG energy_unit_shift = (ULONG)((rapl_unit & AMD_RAPL_ESU) >> 8);
        g_pkg_state.energy_unit_divisor = 1U << energy_unit_shift;
    }

    g_max_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    g_core_state = (AMD_CORE_DELTA_STATE*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        (SIZE_T)g_max_procs * sizeof(AMD_CORE_DELTA_STATE),
        'AMDC'
    );
    if (g_core_state) {
        RtlZeroMemory(g_core_state, (SIZE_T)g_max_procs * sizeof(AMD_CORE_DELTA_STATE));
    }

    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = EvtDriverUnload;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = EvtDriverContextCleanup;
    status = WdfDriverCreate(
        driver_obj,
        registry_path,
        &attributes,
        &config,
        &h_driver
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreate failed: 0x%x\n", status));
        return status;
    }

    p_init = WdfControlDeviceInitAllocate(
        h_driver,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R
    );
    if (p_init == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    status = NonPnpDeviceAdd(h_driver, p_init);

    return status;
}

VOID EvtDriverUnload(_In_ WDFDRIVER driver) {
    UNREFERENCED_PARAMETER(driver);

    KdPrint(("Unloading KMDF Driver...\n"));
    if (g_core_state) {
        ExFreePoolWithTag(g_core_state, 'AMDC');
        g_core_state = NULL;
    }
    if (dev) {
        WdfObjectDelete(dev);
        dev = NULL;
    }
    KdPrint(("KMDF driver unloaded\n"));
}

NTSTATUS NonPnpDeviceAdd(
    _In_    WDFDRIVER       driver,
    _Inout_ PWDFDEVICE_INIT device_init
) {
    KdPrint(("NonPnpDeviceAdd ENTERED\n"));

    UNREFERENCED_PARAMETER(driver);

    NTSTATUS                status;
    WDFDEVICE               control_device;
    WDF_OBJECT_ATTRIBUTES   attributes;

    WdfDeviceInitSetExclusive(device_init, TRUE);
    WdfDeviceInitSetIoType(device_init, WdfDeviceIoBuffered);

    status = WdfDeviceInitAssignSDDLString(device_init, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to set SDDL: 0x%x\n", status));
        goto END;
    }

    status = WdfDeviceInitAssignName(device_init, &DEVICE_NAME);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceInitAssignName failed: 0x%x\n", status));
        goto END;
    }

    WdfControlDeviceInitSetShutdownNotification(device_init, Shutdown, WdfDeviceShutdown);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CONTROL_DEVICE_EXTENSION);

    status = WdfDeviceCreate(&device_init, &attributes, &control_device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDeviceCreate failed: 0x%x\n", status));
        goto END;
    }

    status = WdfDeviceCreateSymbolicLink(control_device, &SYMLINK_NAME);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfDriverCreateSymbolicLink failed: 0x%x\n", status));
        goto END;
    }

    dev = control_device;

    WDF_IO_QUEUE_CONFIG queue_config;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config, WdfIoQueueDispatchSequential);
    queue_config.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(control_device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoQueueCreate failed: 0x%x\n", status));
        goto END;
    }

    WdfControlFinishInitializing(control_device);

END:
    if (device_init != NULL) {
        WdfDeviceInitFree(device_init);
    }
    return status;
}

/***************************************************
*                 IOCTL Handler                    *
***************************************************/

VOID EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
) {
    KdPrint(("Entered EvtIoDeviceControl\n"));

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(Queue);

    PCPU_DATA_BUFFER  outbuffer = NULL;
    NTSTATUS          status;

    if (IoControlCode != IOCTL_GET_DATA) {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    ULONG total_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    ULONG total_entries = METRICS_PER_CORE * total_procs + METRICS_AGGREGATE;
    ULONG required_size = (ULONG)(sizeof(CPU_DATA_HEADER) + (SIZE_T)total_entries * sizeof(CPU_DATA));

    if (OutputBufferLength < sizeof(CPU_DATA_HEADER)) {
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_TOO_SMALL, 0);
        return;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(CPU_DATA_HEADER), &outbuffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    outbuffer->header.required_size = required_size;
    outbuffer->header.processor_count = total_procs;
    outbuffer->header.entry_count = 0;

    if (OutputBufferLength < required_size) {
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_OVERFLOW, sizeof(CPU_DATA_HEADER));
        return;
    }

    CPU_VENDOR vendor = DetectCpuVendor();
    if (vendor != CPU_VENDOR_AMD) {
        KdPrint(("AMD CPU driver loaded on non-AMD CPU\n"));
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    GROUP_AFFINITY old_affinity;
    GROUP_AFFINITY new_affinity;
    RtlZeroMemory(&old_affinity, sizeof(GROUP_AFFINITY));
    BOOLEAN affinity_saved = FALSE;
    USHORT buf_idx = 0;
    BOOLEAN buf_ok = TRUE;

    for (ULONG i = 0; i < total_procs && buf_ok; i++) {
        PROCESSOR_NUMBER proc_num = { 0 };
        if (!NT_SUCCESS(KeGetProcessorNumberFromIndex(i, &proc_num)))
            continue;

        RtlZeroMemory(&new_affinity, sizeof(GROUP_AFFINITY));
        new_affinity.Group = proc_num.Group;
        new_affinity.Mask = (KAFFINITY)(1ULL << proc_num.Number);
        KeSetSystemGroupAffinityThread(&new_affinity, affinity_saved ? NULL : &old_affinity);
        affinity_saved = TRUE;

        buf_ok = ReadPerCoreAmdMetrics(outbuffer, (ULONG)OutputBufferLength, i, &buf_idx);
    }

    if (affinity_saved) {
        KeRevertToUserGroupAffinityThread(&old_affinity);
    }

    if (buf_ok) {
        buf_ok = ReadAmdAggregateMetrics(outbuffer, (ULONG)OutputBufferLength, &buf_idx);
    }

    if (!buf_ok) {
        outbuffer->header.required_size =
            (ULONG)(sizeof(CPU_DATA_HEADER) + ((SIZE_T)buf_idx + METRICS_PER_CORE + METRICS_AGGREGATE) * sizeof(CPU_DATA));
        WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_OVERFLOW, sizeof(CPU_DATA_HEADER));
        return;
    }

    outbuffer->header.entry_count = buf_idx;
    ULONG bytes_written = (ULONG)(sizeof(CPU_DATA_HEADER) + (SIZE_T)buf_idx * sizeof(CPU_DATA));
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytes_written);
}

VOID EvtDriverContextCleanup(
    _In_ WDFOBJECT driver
) {
    UNREFERENCED_PARAMETER(driver);
    return;
}

VOID Shutdown(
    WDFDEVICE device
) {
    UNREFERENCED_PARAMETER(device);
    return;
}

/***************************************************
*               SMN Bus Access                     *
*                                                  *
*  LibreHardwareMonitor: InpcIo.cs / AmdCpu.cs     *
*  PCI config space 0x60/0x64 index/data pair      *
***************************************************/

BOOLEAN ReadSmn(ULONG address, ULONG* result) {
    PCI_SLOT_NUMBER slot = { 0 };
    slot.u.bits.DeviceNumber   = 0;
    slot.u.bits.FunctionNumber = 0;

    ExAcquireFastMutex(&smn_mutex);

    ULONG written = HalSetBusDataByOffset(
        PCIConfiguration, 0, slot.u.AsULONG,
        &address, SMN_INDEX_REGISTER, sizeof(ULONG)
    );
    if (written != sizeof(ULONG)) {
        KdPrint(("ReadSmn: Failed to write SMN address\n"));
        ExReleaseFastMutex(&smn_mutex);
        return FALSE;
    }

    ULONG read = HalGetBusDataByOffset(
        PCIConfiguration, 0, slot.u.AsULONG,
        result, SMN_DATA_REGISTER, sizeof(ULONG)
    );
    if (read != sizeof(ULONG)) {
        KdPrint(("ReadSmn: Failed to read SMN data\n"));
        ExReleaseFastMutex(&smn_mutex);
        return FALSE;
    }

    ExReleaseFastMutex(&smn_mutex);
    return TRUE;
}

/***************************************************
*              Buffer Management                   *
***************************************************/

BOOLEAN tryAddTelemetryData(
    CPU_DATA_BUFFER* buffer,
    ULONG            buffer_capacity,
    CPU_DATA         data,
    USHORT           buffer_idx
) {
    ULONG needed = (ULONG)(sizeof(CPU_DATA_HEADER) + ((SIZE_T)buffer_idx + 1) * sizeof(CPU_DATA));
    if (needed > buffer_capacity) {
        buffer->header.required_size = needed;
        return FALSE;
    }
    buffer->data[buffer_idx] = data;
    return TRUE;
}

/***************************************************
*           Per-Core Metric Collection             *
***************************************************/

BOOLEAN ReadPerCoreAmdMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    ULONG            cpu_idx,
    USHORT*          buffer_idx
) {
    // AMD PPR: P-state clock from MSR PStateDef FID/DID
    CPU_DATA clock = retCoreClockSpeed(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, clock, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    // Intel SDM Vol. 4: IA32_MPERF (architectural, AMD-compatible)
    CPU_DATA load = retCoreLoad(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, load, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    // Linux kernel amd_energy: MSR_CORE_ENERGY_STATUS per-core delta
    CPU_DATA power = retCorePower(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, power, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    return TRUE;
}

/***************************************************
*          Aggregate Metric Collection             *
***************************************************/

BOOLEAN ReadAmdAggregateMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    USHORT*          buffer_idx
) {
    // LibreHardwareMonitor AmdCpu.cs: THM_TCON_CUR_TEMP via SMN
    CPU_DATA tdie = retTDieTemp();
    if (!tryAddTelemetryData(buffer, output_buffer_len, tdie, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    // LibreHardwareMonitor AmdCpu.cs: SVI2 voltage via SMN
    CPU_DATA core_v = retCoreVoltage();
    if (!tryAddTelemetryData(buffer, output_buffer_len, core_v, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    CPU_DATA soc_v = retSoCVoltage();
    if (!tryAddTelemetryData(buffer, output_buffer_len, soc_v, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    // Linux kernel amd_energy: MSR_PKG_ENERGY_STATUS delta
    CPU_DATA pkg_pwr = retPackagePower();
    if (!tryAddTelemetryData(buffer, output_buffer_len, pkg_pwr, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    return TRUE;
}

/***************************************************
*          Per-Core Telemetry Helpers              *
***************************************************/

static void BuildCoreName(char* dst, ULONG size, ULONG idx, const char* suffix) {
    char prefix[] = "Core ";
    ULONG pos = 0;

    for (ULONG i = 0; prefix[i] && pos < size - 1; i++)
        dst[pos++] = prefix[i];

    char digits[4];
    ULONG d = 0;
    ULONG n = idx;
    do {
        digits[d++] = '0' + (char)(n % 10);
        n /= 10;
    } while (n > 0 && d < sizeof(digits));
    while (d > 0 && pos < size - 1)
        dst[pos++] = digits[--d];

    if (pos < size - 1)
        dst[pos++] = ' ';

    for (ULONG i = 0; suffix[i] && pos < size - 1; i++)
        dst[pos++] = suffix[i];

    dst[pos] = '\0';
}

// AMD PPR Family 17h: Core::X86::Msr::PStateDef
// Frequency = 200 * CpuFid[7:0] / CpuDfsId[13:8] MHz
CPU_DATA retCoreClockSpeed(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_CLOCK_SPEED;
    data.unit   = (USHORT)UNIT_MHZ;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "Clock");

    ULONGLONG pstate_status = __readmsr(AMD_MSR_PSTATE_STATUS);
    ULONG cur_pstate = (ULONG)(pstate_status & PSTATE_STATUS_CUR);

    ULONGLONG pstate_def = __readmsr(AMD_MSR_PSTATE_DEF_BASE + cur_pstate);
    ULONG fid    = (ULONG)(pstate_def & PSTATE_DEF_CPU_FID);
    ULONG dfs_id = (ULONG)((pstate_def & PSTATE_DEF_CPU_DFS_ID) >> 8);

    if (dfs_id > 0) {
        data.value = (AMD_PSTATE_FREQ_BASE * fid) / dfs_id;
    }

    return data;
}

// Intel SDM Vol. 4: IA32_MPERF (0xE7) C0 residency
// MPERF counts only during C0, TSC counts always
CPU_DATA retCoreLoad(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_LOAD;
    data.unit   = (USHORT)UNIT_PERCENT;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "Load");

    if (!g_has_mperf || !g_core_state || cpu_idx >= g_max_procs) {
        return data;
    }

    ULONGLONG curr_mperf = __readmsr(IA32_MPERF);
    ULONGLONG curr_tsc   = __rdtsc();

    AMD_CORE_DELTA_STATE* state = &g_core_state[cpu_idx];
    if (state->valid) {
        ULONGLONG mperf_delta = curr_mperf - state->prev_mperf;
        ULONGLONG tsc_delta   = curr_tsc   - state->prev_tsc;

        if (tsc_delta > 0) {
            ULONG load = (ULONG)((mperf_delta * 100ULL) / tsc_delta);
            data.value = (load > 100) ? 100 : load;
        }
    }

    state->prev_mperf = curr_mperf;
    state->prev_tsc   = curr_tsc;
    state->valid      = TRUE;

    return data;
}

// Linux kernel amd_energy driver: MSR_CORE_ENERGY_STATUS (0xC001029A)
// 32-bit accumulator, energy unit from MSR_RAPL_POWER_UNIT[12:8]
CPU_DATA retCorePower(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_POWER;
    data.unit   = (USHORT)UNIT_MILLIWATTS;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "Power");

    if (g_amd_info.family < AMD_FAMILY_ZEN || !g_core_state || cpu_idx >= g_max_procs) {
        return data;
    }

    ULONG curr_energy = (ULONG)(__readmsr(AMD_MSR_CORE_ENERGY_STATUS) & GENMASK(31, 0));
    LARGE_INTEGER curr_qpc = KeQueryPerformanceCounter(NULL);

    AMD_CORE_DELTA_STATE* state = &g_core_state[cpu_idx];
    if (state->valid && g_pkg_state.energy_unit_divisor > 0 && g_qpc_freq.QuadPart > 0) {
        ULONG energy_delta;
        if (curr_energy >= state->prev_core_energy) {
            energy_delta = curr_energy - state->prev_core_energy;
        } else {
            energy_delta = (MAXULONG - state->prev_core_energy) + curr_energy + 1;
        }

        ULONGLONG qpc_delta = (ULONGLONG)(curr_qpc.QuadPart - state->prev_core_qpc.QuadPart);
        ULONGLONG time_ms = (qpc_delta * 1000ULL) / (ULONGLONG)g_qpc_freq.QuadPart;

        if (time_ms > 0) {
            ULONGLONG energy_mj = ((ULONGLONG)energy_delta * 1000ULL) / g_pkg_state.energy_unit_divisor;
            data.value = (ULONG)((energy_mj * 1000ULL) / time_ms);
        }
    }

    state->prev_core_energy = curr_energy;
    state->prev_core_qpc   = curr_qpc;

    return data;
}

/***************************************************
*         Aggregate Telemetry Helpers              *
***************************************************/

// LibreHardwareMonitor AmdCpu.cs: THM_TCON_CUR_TEMP (SMN 0x00059800)
// Raw 11-bit value in [31:21], 1/8 deg C units
// 49C Tctl-to-Tdie offset when RANGE_SEL[19] or TJ_SEL[17:16]==0x3
CPU_DATA retTDieTemp(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_TEMP;
    data.unit   = (USHORT)UNIT_DEGREES_C;
    RtlCopyMemory(data.name, "Tdie Temp", sizeof("Tdie Temp"));

    ULONG raw = 0;
    if (!ReadSmn(AMD_THM_TCON_CUR_TEMP_ADDR, &raw)) {
        return data;
    }

    BOOLEAN offset_flag = ((raw & THM_CUR_TEMP_RANGE_SEL) != 0)
                       || ((raw & THM_CUR_TEMP_TJ_SEL) == THM_CUR_TEMP_TJ_SEL);

    ULONG raw_temp = (ULONG)((raw & THM_CUR_TEMP_RAW) >> 21);
    LONG temp_mdeg = (LONG)(raw_temp * AMD_TEMP_UNIT_MDEG);

    if (offset_flag) {
        temp_mdeg -= AMD_TEMP_OFFSET_MDEG;
    }

    if (temp_mdeg >= 0 && temp_mdeg <= 150000) {
        data.value = (ULONG)(temp_mdeg / 1000);
    }

    return data;
}

// LibreHardwareMonitor AmdCpu.cs: SVI2 telemetry via SMN
// Voltage = 1.55V - VID * 6.25mV
// Integer: mV = (6200 - VID * 25 + 2) / 4
CPU_DATA retCoreVoltage(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_VOLTAGE;
    data.unit   = (USHORT)UNIT_MILLIVOLTS;
    RtlCopyMemory(data.name, "Core Voltage", sizeof("Core Voltage"));

    ULONG raw = 0;
    if (!ReadSmn(g_svi2_core_addr, &raw)) {
        return data;
    }

    ULONG vid = (ULONG)((raw & SVI2_VID_FIELD) >> 16);
    LONG mv = (LONG)(SVI2_BASE_MV_X4 - vid * SVI2_STEP_MV_X4 + SVI2_ROUNDING) / 4;
    data.value = (mv > 0) ? (ULONG)mv : 0;

    return data;
}

CPU_DATA retSoCVoltage(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_VOLTAGE;
    data.unit   = (USHORT)UNIT_MILLIVOLTS;
    RtlCopyMemory(data.name, "SoC Voltage", sizeof("SoC Voltage"));

    ULONG raw = 0;
    if (!ReadSmn(g_svi2_soc_addr, &raw)) {
        return data;
    }

    ULONG vid = (ULONG)((raw & SVI2_VID_FIELD) >> 16);
    LONG mv = (LONG)(SVI2_BASE_MV_X4 - vid * SVI2_STEP_MV_X4 + SVI2_ROUNDING) / 4;
    data.value = (mv > 0) ? (ULONG)mv : 0;

    return data;
}

// Linux kernel amd_energy driver: MSR_PKG_ENERGY_STATUS (0xC001029B)
// 32-bit accumulator, same energy unit as per-core
CPU_DATA retPackagePower(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_POWER;
    data.unit   = (USHORT)UNIT_MILLIWATTS;
    RtlCopyMemory(data.name, "Package Power", sizeof("Package Power"));

    if (g_amd_info.family < AMD_FAMILY_ZEN) {
        return data;
    }

    ULONG curr_energy = (ULONG)(__readmsr(AMD_MSR_PKG_ENERGY_STATUS) & GENMASK(31, 0));
    LARGE_INTEGER curr_qpc = KeQueryPerformanceCounter(NULL);

    if (g_pkg_state.valid && g_pkg_state.energy_unit_divisor > 0 && g_qpc_freq.QuadPart > 0) {
        ULONG energy_delta;
        if (curr_energy >= g_pkg_state.prev_pkg_energy) {
            energy_delta = curr_energy - g_pkg_state.prev_pkg_energy;
        } else {
            energy_delta = (MAXULONG - g_pkg_state.prev_pkg_energy) + curr_energy + 1;
        }

        ULONGLONG qpc_delta = (ULONGLONG)(curr_qpc.QuadPart - g_pkg_state.prev_pkg_qpc.QuadPart);
        ULONGLONG time_ms = (qpc_delta * 1000ULL) / (ULONGLONG)g_qpc_freq.QuadPart;

        if (time_ms > 0) {
            ULONGLONG energy_mj = ((ULONGLONG)energy_delta * 1000ULL) / g_pkg_state.energy_unit_divisor;
            data.value = (ULONG)((energy_mj * 1000ULL) / time_ms);
        }
    }

    g_pkg_state.prev_pkg_energy = curr_energy;
    g_pkg_state.prev_pkg_qpc    = curr_qpc;
    g_pkg_state.valid           = TRUE;

    return data;
}
