#pragma once
#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>

class GraphicsProcessor
{
private:
	char*		  name;
	uint16_t	  availability;
	std::string   device_id;
	std::string   system_name;
	std::string   status;
	uint16_t	  status_info;

public:
	GraphicsProcessor();
	virtual ~GraphicsProcessor();

	// Setters
	void setName(char*& name);
	void setDeviceId(const std::string& device_id);
	void setSystemName(const std::string& system_name);
	void setStatus(const std::string& status);
	void setStatusInfo(uint16_t status_info);

	// Getters
	char*& getName();
	const std::string& getDeviceId() const;
	const std::string& getSystemName() const;
	const std::string& getStatus() const;
	uint16_t getStatusInfo() const;
};

