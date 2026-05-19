#pragma once
#include "HardwareManager.h"
#include "DriverClient.h"

#define OUTPUT_HEADER(header_msg) \
        std::cout << "--------------------------------------------------------------\n"; \
        std::cout << "     ** " << header_msg << "** \n\n";

class OutputHandler
{
public:
	OutputHandler() = default;
	OutputHandler(
		const Hardware_List_Container& container, 
		const LiveGPUHandler& live_gpu_handler,
		DriverClient driver) 
		: hw_data(container), live_gpu_handler(live_gpu_handler), dc(driver) {}

	// Test constructor: no driver
	OutputHandler(
		const Hardware_List_Container& container,
		const LiveGPUHandler& gpu_handler)
		: hw_data(container), live_gpu_handler(gpu_handler) {
	}
	~OutputHandler() = default;

	void output();
	void outputNoDriver();

private:
	Hardware_List_Container hw_data;
	LiveGPUHandler			live_gpu_handler;
	DriverClient			dc;
};

