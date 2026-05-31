#pragma once
#include <iostream>
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <dxgi.h>
#include "projutils.h"
#include ".\..\..\third_party\nvapi\nvapi.h"

struct GraphicsProcessor
{
	bstr_t	name;
	ULONG	adapter_RAM;
	bstr_t	device_id;
	USHORT	availability;
	ULONG	curr_ref_rate;
	bstr_t  status;

	// Output Functions
	void outputGPUInfo();
};
