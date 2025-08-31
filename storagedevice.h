#pragma once

#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

class StorageDevice
{
private:
	bstr_t device_id;
	bstr_t name;
	bstr_t manufacturer;
	bstr_t model;
	ULONG spindle_speed; // ULONG and UINT are the same size, but in legacy systems it might be different
	ULONGLONG size;
	ULONGLONG free_space;

public:
    StorageDevice() = default;
    StorageDevice(const bstr_t& dev_id, const bstr_t& nm, const bstr_t& mfg,
        const bstr_t& mdl, ULONG speed, ULONGLONG sz, ULONGLONG free)
        : device_id(dev_id), name(nm), manufacturer(mfg), model(mdl),
        spindle_speed(speed), size(sz), free_space(free) {
    }

    bstr_t getDeviceID() const { return device_id; }
    bstr_t getName() const { return name; }
    bstr_t getManufacturer() const { return manufacturer; }
    bstr_t getModel() const { return model; }
    ULONG getSpindleSpeed() const { return spindle_speed; }
    ULONGLONG getSize() const { return size; }
    ULONGLONG getFreeSpace() const { return free_space; }

    // Setters
    void setDeviceID(const bstr_t& id) { device_id = id; }
    void setName(const bstr_t& n) { name = n; }
    void setManufacturer(const bstr_t& man) { manufacturer = man; }
    void setModel(const bstr_t& m) { model = m; }
    void setSpindleSpeed(ULONG ss) { spindle_speed = ss; }
    void setSize(ULONGLONG sz) { size = sz; }
    void setFreeSpace(ULONGLONG fs) { free_space = fs; }

};

