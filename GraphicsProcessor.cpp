#include "GraphicsProcessor.h"

GraphicsProcessor::GraphicsProcessor() {
    this->name = bstr_t();
    this->adapter_RAM = ULONG();
    this->device_id = bstr_t();
    this->availability = USHORT();
    this->curr_ref_rate = ULONG();
    this->status = bstr_t();
}

GraphicsProcessor::~GraphicsProcessor() {

}

void GraphicsProcessor::outputGPUInfo() {
	std::wcout << "Name: " << name << std::endl;
    bstr_t RAM_output = simplifyBytesAsString(adapter_RAM);
    std::wcout << "Total Adapter RAM: " << RAM_output << std::endl;
    std::wcout << "Device ID: " << device_id << std::endl;
    bstr_t avail_output = explainAvailability(availability);
    std::wcout << "Availability: " << avail_output << std::endl;
    std::wcout << "Current Refresh Rate: " << curr_ref_rate << std::endl;
    std::wcout << "Status: " << status << std::endl;
    std::wcout << "\n";
}