#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Sensors {

	enum class SensorType {
		CLOCK,        // MHz
		USAGE,        // %
		TEMPERATURE,  // °C
		POWER,        // W
		MEMORY,       // MB
		VOLTAGE,      // mV
		FAN_SPEED     // RPM
	};

	// Unit is derived from SensorType via a traits struct
	template<SensorType T>
	struct SensorTraits {
		static constexpr const char* unit = "unknown";
		static_assert(sizeof(T) == 0, "SensorTraits must be specialized for this SensorType");
	};

	template<> struct SensorTraits<SensorType::CLOCK>		{ static constexpr const char* unit = "MHz"; };
	template<> struct SensorTraits<SensorType::USAGE>		{ static constexpr const char* unit = "%"; };
	template<> struct SensorTraits<SensorType::TEMPERATURE> { static constexpr const char* unit = "°C"; };
	template<> struct SensorTraits<SensorType::POWER>		{ static constexpr const char* unit = "W"; };
	template<> struct SensorTraits<SensorType::MEMORY>		{ static constexpr const char* unit = "MB"; };
	template<> struct SensorTraits<SensorType::VOLTAGE>		{ static constexpr const char* unit = "mV"; };
	template<> struct SensorTraits<SensorType::FAN_SPEED>	{ static constexpr const char* unit = "RPM"; };

	// TODO - This might not even need to exist, slightly overengineered for now
	// Keep it around for now in case a reason arises and we need another sensor implementation 
	class IDeviceSensor { 
	public:
		IDeviceSensor() = default;
		virtual ~IDeviceSensor() = default;

		IDeviceSensor(const IDeviceSensor&) = delete;
		IDeviceSensor& operator=(const IDeviceSensor&) = delete;

		IDeviceSensor(IDeviceSensor&&) = default;
		IDeviceSensor& operator=(IDeviceSensor&&) = default;

		virtual const std::string& getName()  const = 0;
		virtual float              getValue() const = 0;
		virtual const char*		   getUnit()  const = 0;
		virtual SensorType         getType()  const = 0;
	};

	template<SensorType T>
	class DeviceSensor : public IDeviceSensor {
	public:
		DeviceSensor(std::string name, float value)
			: name(std::move(name)), value(value) {}

		DeviceSensor(const DeviceSensor&) = default;
		DeviceSensor& operator=(const DeviceSensor&) = default;

		DeviceSensor(DeviceSensor&&) = default;
		DeviceSensor& operator=(DeviceSensor&&) = default;

		const std::string& getName()  const override { return name; }
		float              getValue() const override { return value; }
		const char*		   getUnit()  const override { return SensorTraits<T>::unit; }
		SensorType         getType()  const override { return T; }

	private:
		std::string name;
		float		value{};
	};

}