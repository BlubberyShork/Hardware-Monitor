#pragma once
#include <iostream>
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <dxgi.h>
#include "projutils.h"
#include ".\..\third_party\nvapi\nvapi.h"

// TODO - Wrap these in a namespace for organizational purposes
// namespace GPUContainers {

// TODO - AMD ADLX handling for AMD GPU's

typedef struct _GPULiveData {
	// Temperatures
	signed int curr_avg_temp = 0;
	signed int curr_hotspot_temp = 0;

	// Clock speeds
	typedef struct ClockEntry {
		NV_GPU_PUBLIC_CLOCK_ID clk_type;
		unsigned int clk_spd;
	} ClockEntry;
	std::vector<ClockEntry> clks;

	// Utilization
	unsigned int curr_graphics_utilization = 0;
	unsigned int curr_frame_buffer_utilization = 0;
	unsigned int curr_video_engine_utilization = 0;

	// Fan Speed
	unsigned int fan_speed = 0;
} GPULiveData;

// TODO - Refactor into a class
struct GraphicsProcessor
{
private:
	GPULiveData live_data;

public:

	bstr_t	name;
	ULONG	adapter_RAM;
	bstr_t	device_id;
	USHORT	availability;
	ULONG	curr_ref_rate;
	bstr_t  status;
	//bstr_t  system_name;
	//USHORT  status_info;

	// Output Functions
	void outputGPUInfo();
	void setLiveData(GPULiveData& live_data) { this->live_data = live_data; };
	GPULiveData getLiveData() { return this->live_data; };

};

// }