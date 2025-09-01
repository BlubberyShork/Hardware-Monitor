#pragma once
#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

class GraphicsProcessor
{
private:
	bstr_t	name;
	ULONG	adapter_RAM;
	bstr_t	device_id;
	USHORT	availability;
	ULONG	curr_ref_rate;
	bstr_t  status;
	//bstr_t  system_name;
	//USHORT  status_info;

public:
	GraphicsProcessor();
	//todo - constructor with all 
	virtual ~GraphicsProcessor();

	// Setters
	void setName(const bstr_t& n) { name = n; }
	void setAdapterRAM(ULONG RAM) { adapter_RAM = RAM; }
	void setDeviceId(const bstr_t& id) { device_id = id; }
	void setAvailability(USHORT avail) { availability = avail; }
	void setCurrentRefreshRate(ULONG crr) { curr_ref_rate = crr; }
	void setStatus(const bstr_t& s) { status = s; }

	// Getters
	const bstr_t& getName() const { return name; }
	ULONG getAdapterRAM() const { return adapter_RAM; }
	const bstr_t& getDeviceId() const { return device_id; }
	USHORT getAvailability() const { return availability; }
	ULONG getCurrentRefreshRate() const { return curr_ref_rate; }
	const bstr_t& getStatus() const { return status; }

	// Ooutput Functions
	void outputGPUInfo();
};

