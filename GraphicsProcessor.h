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
	virtual ~GraphicsProcessor();

	// Setters
	void setName(const bstr_t& name) { this->name = name; }
	void setAdapterRAM(const ULONG& RAM) { this->adapter_RAM = RAM; }
	void setDeviceId(const bstr_t& device_id) { this->device_id = device_id; }
	void setAvailability(const USHORT& avail) { this->availability = avail; }
	void setCurrRefreshRate(ULONG& curr_ref_rate) { this->curr_ref_rate = curr_ref_rate; }
	void setStatus(const bstr_t& status) { this->status = status; }
	//void setSystemName(const bstr_t& system_name);
	//void setStatusInfo(USHORT status_info);

	// Getters
	const bstr_t& getName() const { return this->name; }
	const ULONG& getAdapterRAM() const { return this->adapter_RAM; }
	const bstr_t& getDeviceId() const { return this->device_id; }
	const USHORT& getAvailability() const { return this->availability; }
	const USHORT& getCurrentRefreshRate() const { return this->curr_ref_rate; }
	const bstr_t& getStatus() const { return this->status; }
	//const bstr_t& getSystemName() const;
	//USHORT getStatusInfo() const;

	// Ooutput Functions
	void outputGPUInfo();
};

