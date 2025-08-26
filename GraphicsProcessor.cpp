#include "GraphicsProcessor.h"

GraphicsProcessor::GraphicsProcessor() {

}

GraphicsProcessor::~GraphicsProcessor() {

}

// Setters
void GraphicsProcessor::setName(char*& name)
{
    this->name = name;
}

void GraphicsProcessor::setDeviceId(const std::string& device_id)
{
    this->device_id = device_id;
}

void GraphicsProcessor::setSystemName(const std::string& system_name)
{
    this->system_name = system_name;
}

void GraphicsProcessor::setStatus(const std::string& status)
{
    this->status = status;
}

void GraphicsProcessor::setStatusInfo(uint16_t status_info)
{
    this->status_info = status_info;
}

// Getters
char*& GraphicsProcessor::getName() 
{
    return name;
}

const std::string& GraphicsProcessor::getDeviceId() const
{
    return device_id;
}

const std::string& GraphicsProcessor::getSystemName() const
{
    return system_name;
}

const std::string& GraphicsProcessor::getStatus() const
{
    return status;
}

uint16_t GraphicsProcessor::getStatusInfo() const
{
    return status_info;
}