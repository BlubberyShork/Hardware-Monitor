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

	typedef struct _GPULiveData {
		// Temperatures
		signed int curr_avg_temp = 0;
		signed int curr_hotspot_temp = 0;

		// Clock speeds
		unsigned int curr_core_clock_speed = 0;
		unsigned int curr_memory_clock_speed = 0;
		unsigned int curr_shader_clock_speed = 0;

		//Utilization
		unsigned int curr_core_utilization = 0;
		unsigned int curr_frame_buffer_utilization = 0;
		unsigned int curr_video_engine_utilization = 0;
		unsigned int curr_bus_interface_utilization = 0;
		unsigned int curr_memory_utilization = 0;
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