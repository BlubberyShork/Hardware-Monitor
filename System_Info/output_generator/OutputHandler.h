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
		const DriverClient &driver) 
		: hw_data(container), dc(driver) {}

	// Test constructor: no driver
	OutputHandler(
		const Hardware_List_Container& container)
		: hw_data(container) {}

	OutputHandler(const OutputHandler&) = delete;
	OutputHandler& operator=(const OutputHandler&) = delete;

	~OutputHandler() = default;

	void output();
	void outputNoDriver();

private:
	Hardware_List_Container hw_data;
	DriverClient			dc;
};

