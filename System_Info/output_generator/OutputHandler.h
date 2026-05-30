#pragma once
#include ".\..\HardwareManager.h"
#include ".\..\driver_client\DriverClient.h"

#define OUTPUT_HEADER(header_msg) \
        std::cout << "--------------------------------------------------------------\n"; \
        std::cout << "     ** " << header_msg << "** \n\n";

class OutputHandler
{
public:
	OutputHandler(
		const Hardware_List_Container& container, 
		const HardwareDevice& hardware_device,
		DriverClient &driver) 
		: hw_data(container), hardware_device(hardware_device), dc(driver) {}

	// Test constructor: no driver
	OutputHandler(
		const Hardware_List_Container& container,
		const HardwareDevice& gpu_handler)
		: hw_data(container), hardware_device(gpu_handler) {}

	OutputHandler(const OutputHandler&) = delete;
	OutputHandler& operator=(const OutputHandler&) = delete;

	~OutputHandler() = default;

	void output();
	void outputNoDriver();

private:
	Hardware_List_Container hw_data;
	const HardwareDevice&   hardware_device;
	DriverClient			dc;
};

