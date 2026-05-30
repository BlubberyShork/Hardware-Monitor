#pragma once
#include <memory>
#include <vector>
#include <string>
#include "DeviceSensor.h"

// Just combine these into an abstract class: A_HardwareDevice
class A_HardwareDevice {
public:
	enum class Vendor {
		NVIDIA,
		AMD,
		INTEL
	};

	enum class HardwareType {
		MOTHERBOARD,
		CPU,
		GPU,
		PSU,
		NETWORK,
		STORAGE
	};

	A_HardwareDevice(Vendor ven, HardwareType ht, std::string name)
		: vendor(ven), hw_type(ht), name(std::move(name)) {}

	A_HardwareDevice() = default;
    virtual ~A_HardwareDevice() = default;
    virtual void fetchMetrics() = 0;

	A_HardwareDevice(const A_HardwareDevice&) = delete;
	A_HardwareDevice& operator=(const A_HardwareDevice&) = delete;
	A_HardwareDevice(A_HardwareDevice&&) = default;
	A_HardwareDevice& operator=(A_HardwareDevice&&) = default;

	const std::vector<std::unique_ptr<Sensors::IDeviceSensor>>& getSensors() const { return dev_sensors; }
	const std::string&  getName()   const { return name; }
	Vendor              getVendor() const { return vendor; }
	HardwareType        getType()   const { return hw_type; }

protected:
	Vendor			vendor;
	HardwareType	hw_type;
	std::string		name;

	template<Sensors::SensorType T>
	void addSensor(std::string name, float init_val = {}) {
		dev_sensors.push_back(std::make_unique<DeviceSensor<T>>(std::move(name), init_val));
	}

private:
	std::vector<std::unique_ptr<Sensors::IDeviceSensor>> dev_sensors;
};
 