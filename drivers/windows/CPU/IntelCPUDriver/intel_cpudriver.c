#include "intel_cpudriver.h"

UNICODE_STRING DEVICE_NAME =
    RTL_CONSTANT_STRING(L"\\Device\\WindowsIntelCPUDriver");

UNICODE_STRING SYMLINK_NAME =
    RTL_CONSTANT_STRING(L"\\??\\WindowsIntelCPUDriver");

WDFDEVICE dev = NULL;

static BOOLEAN          g_has_mperf      = FALSE;
static BOOLEAN          g_has_pkg_therm  = FALSE;
static BOOLEAN          g_has_rapl       = FALSE;
static ULONG            g_base_freq_mhz  = 0;

static CORE_DELTA_STATE* g_core_state    = NULL;
static PKG_DELTA_STATE   g_pkg_state     = { 0 };
static ULONG             g_max_procs     = 0;
static LARGE_INTEGER     g_qpc_freq      = { 0 };

//4d36e97d-e325-11ce-bfc1-08002be10318
DEFINE_GUID(GUID_DEVINTERFACE_HWMONITOR,
    0x4d36e97dL, 0xe324, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);

/***************************************************
*              Feature Detection                   *
***************************************************/

static void DetectFeatures(void) {
    int info[4];
    __cpuid(info, CPUID_THERM_POWER_LEAF);

    g_has_mperf     = (info[2] & CPUID_06H_ECX_MPERF_BIT)    != 0;
    g_has_pkg_therm = (info[0] & CPUID_06H_EAX_PKG_THERM_BIT) != 0;

    // RAPL: available on Sandy Bridge+ (family 6, model >= 0x2A)
    __cpuid(info, 0x01);
    ULONG family     = (ULONG)((info[0] & CPUID_01H_FAMILY)     >> 8);
    ULONG ext_model  = (ULONG)((info[0] & CPUID_01H_EXT_MODEL)  >> 16);
    ULONG base_model = (ULONG)((info[0] & CPUID_01H_BASE_MODEL) >> 4);
    ULONG model      = (ext_model << 4) | base_model;
    g_has_rapl = (family == 6 && model >= RAPL_MIN_MODEL);

    // IA32_PLATFORM_INFO[15:8] = max non-turbo ratio
    ULONGLONG platform_info = __readmsr(IA32_PLATFORM_INFO);
    g_base_freq_mhz = (ULONG)(((platform_info & PLATFORM_INFO_BASE_RATIO) >> 8) * INTEL_BUS_CLOCK_MHZ);

    KdPrint(("Intel features: MPERF=%d PkgTherm=%d RAPL=%d BaseFreq=%luMHz\n",
        g_has_mperf, g_has_pkg_therm, g_has_rapl, g_base_freq_mhz));
}

/***************************************************
*                Driver Lifecycle                  *
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

    DetectFeatures();

    KeQueryPerformanceCounter(&g_qpc_freq);

    if (g_has_rapl) {
        ULONGLONG rapl_unit = __readmsr(MSR_RAPL_POWER_UNIT);
        ULONG energy_unit_shift = (ULONG)((rapl_unit & RAPL_UNIT_ESU) >> 8);
        g_pkg_state.energy_unit_divisor = 1U << energy_unit_shift;
    }

    g_max_procs = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    if (g_has_mperf) {
        g_core_state = (CORE_DELTA_STATE*)ExAllocatePool2(
            POOL_FLAG_NON_PAGED,
            (SIZE_T)g_max_procs * sizeof(CORE_DELTA_STATE),
            'CPUD'
        );
        if (g_core_state) {
            RtlZeroMemory(g_core_state, (SIZE_T)g_max_procs * sizeof(CORE_DELTA_STATE));
        }
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
        ExFreePoolWithTag(g_core_state, 'CPUD');
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
*                  IOCTL Handler                   *
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
    if (vendor != CPU_VENDOR_INTEL) {
        KdPrint(("Intel CPU driver loaded on non-Intel CPU\n"));
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

        buf_ok = ReadPerCoreIntelMetrics(outbuffer, (ULONG)OutputBufferLength, i, &buf_idx);
    }

    if (affinity_saved) {
        KeRevertToUserGroupAffinityThread(&old_affinity);
    }

    if (buf_ok) {
        buf_ok = ReadIntelAggregateMetrics(outbuffer, (ULONG)OutputBufferLength, &buf_idx);
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
*               Buffer Management                  *
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

BOOLEAN ReadPerCoreIntelMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    ULONG            cpu_idx,
    USHORT*          buffer_idx
) {
    CPU_DATA temp = retCoreTemp(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, temp, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    CPU_DATA vid = retCoreVID(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, vid, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    CPU_DATA clock = retCoreClockSpeed(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, clock, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    CPU_DATA load = retCoreLoad(cpu_idx);
    if (!tryAddTelemetryData(buffer, output_buffer_len, load, *buffer_idx)) return FALSE;
    (*buffer_idx)++;

    return TRUE;
}

/***************************************************
*          Aggregate Metric Collection             *
***************************************************/

BOOLEAN ReadIntelAggregateMetrics(
    CPU_DATA_BUFFER* buffer,
    ULONG            output_buffer_len,
    USHORT*          buffer_idx
) {
    if (g_has_pkg_therm) {
        CPU_DATA tdie = retTDieTemp();
        if (!tryAddTelemetryData(buffer, output_buffer_len, tdie, *buffer_idx)) return FALSE;
        (*buffer_idx)++;
    }

    if (g_has_rapl) {
        CPU_DATA pkg_pwr = retPackagePower();
        if (!tryAddTelemetryData(buffer, output_buffer_len, pkg_pwr, *buffer_idx)) return FALSE;
        (*buffer_idx)++;

        CPU_DATA pp0_pwr = retCoreDomainPower();
        if (!tryAddTelemetryData(buffer, output_buffer_len, pp0_pwr, *buffer_idx)) return FALSE;
        (*buffer_idx)++;
    }

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

CPU_DATA retCoreTemp(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_TEMP;
    data.unit   = (USHORT)UNIT_DEGREES_C;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "Temp");

    int cpu_info[4];
    __cpuid(cpu_info, CPUID_THERM_POWER_LEAF);
    if ((cpu_info[0] & CPUID_06H_EAX_DTS_BIT) == 0) {
        return data;
    }

    ULONGLONG therm_status = __readmsr(IA32_THERM_STATUS);
    if ((therm_status & THERM_STATUS_VALID_BIT) == 0) {
        return data;
    }

    ULONGLONG therm_target = __readmsr(IA32_THERM_TARGET);
    ULONG temp_max    = (ULONG)((therm_target & THERM_TARGET_TJMAX) >> 16);
    ULONG temp_offset = (ULONG)((therm_status & THERM_STATUS_OFFSET) >> 16);

    if (temp_offset <= temp_max) {
        data.value = temp_max - temp_offset;
    }

    return data;
}

CPU_DATA retCoreVID(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_VOLTAGE;
    data.unit   = (USHORT)UNIT_MILLIVOLTS;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "VID");

    // Intel SDM Vol4: MSR_PERF_STATUS[47:32] = core voltage VID
    // Voltage (V) = VID / 8192.  Store as millivolts with rounding.
    ULONGLONG perf_status = __readmsr(MSR_PERF_STATUS);
    ULONG vid_bits = (ULONG)((perf_status & PERF_STATUS_VID) >> 32);

    data.value = (vid_bits * 1000 + (1 << 12)) >> 13;

    return data;
}

CPU_DATA retCoreClockSpeed(ULONG cpu_idx) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = (USHORT)cpu_idx;
    data.type   = (USHORT)METRIC_CLOCK_SPEED;
    data.unit   = (USHORT)UNIT_MHZ;
    BuildCoreName(data.name, sizeof(data.name), cpu_idx, "Clock");

    // MSR_PERF_STATUS[15:8] = current P-state ratio. Multiply by bus clock for MHz.
    ULONGLONG perf_status = __readmsr(MSR_PERF_STATUS);
    ULONG pstate_ratio = (ULONG)((perf_status & PERF_STATUS_PSTATE_RATIO) >> 8);
    data.value = pstate_ratio * INTEL_BUS_CLOCK_MHZ;

    return data;
}

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

    // MPERF ticks at base frequency only while core is in C0.
    // TSC ticks at base frequency always (invariant TSC).
    // C0 residency = MPERF_delta / TSC_delta = utilization.
    ULONGLONG curr_mperf = __readmsr(IA32_MPERF);
    ULONGLONG curr_tsc   = __rdtsc();

    CORE_DELTA_STATE* state = &g_core_state[cpu_idx];
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

/***************************************************
*         Aggregate Telemetry Helpers              *
***************************************************/

CPU_DATA retTDieTemp(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_TEMP;
    data.unit   = (USHORT)UNIT_DEGREES_C;
    RtlCopyMemory(data.name, "Package Temp", sizeof("Package Temp"));

    ULONGLONG pkg_therm = __readmsr(IA32_PACKAGE_THERM_STATUS);
    if ((pkg_therm & THERM_STATUS_VALID_BIT) == 0) {
        return data;
    }

    ULONGLONG therm_target = __readmsr(IA32_THERM_TARGET);
    ULONG temp_max    = (ULONG)((therm_target & THERM_TARGET_TJMAX) >> 16);
    ULONG temp_offset = (ULONG)((pkg_therm   & THERM_STATUS_OFFSET) >> 16);

    if (temp_offset <= temp_max) {
        data.value = temp_max - temp_offset;
    }

    return data;
}

static ULONG ComputePowerMilliwatts(
    ULONG curr_energy,
    ULONG prev_energy,
    LARGE_INTEGER curr_qpc,
    LARGE_INTEGER prev_qpc,
    BOOLEAN has_prev
) {
    if (!has_prev || g_pkg_state.energy_unit_divisor == 0 || g_qpc_freq.QuadPart == 0) {
        return 0;
    }

    // Handle 32-bit energy counter wrap
    ULONG energy_delta;
    if (curr_energy >= prev_energy) {
        energy_delta = curr_energy - prev_energy;
    } else {
        energy_delta = (MAXULONG - prev_energy) + curr_energy + 1;
    }

    ULONGLONG qpc_delta = (ULONGLONG)(curr_qpc.QuadPart - prev_qpc.QuadPart);
    ULONGLONG time_ms = (qpc_delta * 1000ULL) / (ULONGLONG)g_qpc_freq.QuadPart;
    if (time_ms == 0) {
        return 0;
    }

    // energy_mj = (energy_delta * 1000) / energy_unit_divisor
    // power_mw  = (energy_mj * 1000) / time_ms
    ULONGLONG energy_mj = ((ULONGLONG)energy_delta * 1000ULL) / g_pkg_state.energy_unit_divisor;
    return (ULONG)((energy_mj * 1000ULL) / time_ms);
}

CPU_DATA retPackagePower(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_POWER;
    data.unit   = (USHORT)UNIT_MILLIWATTS;
    RtlCopyMemory(data.name, "Package Power", sizeof("Package Power"));

    ULONG curr_energy = (ULONG)(__readmsr(MSR_PKG_ENERGY_STATUS) & GENMASK(31, 0));
    LARGE_INTEGER curr_qpc = KeQueryPerformanceCounter(NULL);

    data.value = ComputePowerMilliwatts(
        curr_energy, g_pkg_state.prev_pkg_energy,
        curr_qpc, g_pkg_state.prev_pkg_qpc,
        g_pkg_state.pkg_valid
    );

    g_pkg_state.prev_pkg_energy = curr_energy;
    g_pkg_state.prev_pkg_qpc   = curr_qpc;
    g_pkg_state.pkg_valid       = TRUE;

    return data;
}

CPU_DATA retCoreDomainPower(void) {
    CPU_DATA data;
    RtlZeroMemory(&data, sizeof(CPU_DATA));
    data.cpu_id = 0xFFFF;
    data.type   = (USHORT)METRIC_POWER;
    data.unit   = (USHORT)UNIT_MILLIWATTS;
    RtlCopyMemory(data.name, "Core Domain Power", sizeof("Core Domain Power"));

    ULONG curr_energy = (ULONG)(__readmsr(MSR_PP0_ENERGY_STATUS) & GENMASK(31, 0));
    LARGE_INTEGER curr_qpc = KeQueryPerformanceCounter(NULL);

    data.value = ComputePowerMilliwatts(
        curr_energy, g_pkg_state.prev_pp0_energy,
        curr_qpc, g_pkg_state.prev_pp0_qpc,
        g_pkg_state.pp0_valid
    );

    g_pkg_state.prev_pp0_energy = curr_energy;
    g_pkg_state.prev_pp0_qpc   = curr_qpc;
    g_pkg_state.pp0_valid       = TRUE;

    return data;
}
