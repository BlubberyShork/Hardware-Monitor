#pragma once
#include "projutils.h"
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>

struct Processor
{
	bstr_t unq_id;
	bstr_t dev_id;
	bstr_t proc_id;
	USHORT proc_type;		// cpu, math processor, dsp processor, etc
	USHORT family;
	USHORT architecture;
	bstr_t manufacturer;
	bstr_t name;
	ULONG num_cores;
	ULONG num_log_proc;
	ULONG thread_cnt;
	ULONG curr_clk_spd;
	ULONG curr_vltg;
	ULONG data_width;		// 32 or 64 bit

	void outProcInfo();
};

