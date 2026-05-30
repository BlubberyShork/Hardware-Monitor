#pragma once
#include <vector>
#include ".\..\IHardwareDevicePool.h"
#include ".\..\third_party\adlx\SDK\ADLXHelper\Windows\Cpp\ADLXHelper.h"
#include ".\..\third_party\adlx\SDK\Include\ISystem.h"
#include ".\..\third_party\adlx\SDK\Include\IPerformanceMonitoring.h"
#include <adl_sdk.h> // ADL fallback
#include <adl_structures.h>

class AMDPool : IHardwareDevicePool
{
public:
	AMDPool();
	~AMDPool() override = default;

	void enumerateDevices() override;

private:
	enum class Backend { ADLX, ADL };

	bool initialized = false;
	Backend active_backend = Backend::ADL;

// ============================================================
// ADLX
// ============================================================
	ADLXHelper adlx_helper;
	adlx::IADLXSystem* adlx_system = nullptr;
	adlx::IADLXPerformanceMonitoringServices* perf_monitoring = nullptr;
	std::vector<adlx::IADLXGPUPtr> gpus;

	void initSystem();
	void initPerfMonitoring();
	void enumerateADLXGpus();


// ============================================================
// ADL
// ============================================================
	std::vector<int> adl_adapter_indices;
	void        initADL();
	void        shutdownADL();
	void        enumerateADLAdapters();

	static void* __stdcall adlMallocCallback(int size);
};

