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
	OutputHandler(Hardware_List_Container container, DriverClient driver) : hw_data(container, driver) {}
	~OutputHandler() = default;

	void output();

private:
	Hardware_List_Container hw_data;
	DriverClient			dc;
};

