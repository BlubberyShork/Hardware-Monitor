#pragma once

#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

struct Disk {
    bstr_t unq_id;
    bstr_t fname;
    bstr_t manufacturer;
    bstr_t model;
    ULONGLONG sz;
};

struct Partition {
    struct partition_id {
        ULONG disk_num;
        ULONG part_num;

        bool operator==(const partition_id& other) const noexcept {
            return (disk_num == other.disk_num) && (part_num == other.part_num);
        }
    };

    partition_id id;
    wchar_t drv_ltr;
    ULONGLONG sz;

    struct pid_hash {
        std::size_t operator()(const partition_id& p_id) const noexcept {
            std::size_t h1 = std::hash<ULONG>()(p_id.disk_num);
            std::size_t h2 = std::hash<ULONG>()(p_id.part_num);
            return h1 ^ (h2 << 1);
        }
    };
};

struct Volume {
    wchar_t drv_ltr; 
    ULONGLONG sz;
    ULONGLONG sz_rmng;
    USHORT hstatus;
};

struct PhysDisk {
    bstr_t device_id;
    USHORT unq_id_frmt;
    ULONG spindle_speed;
};

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

    // Output
};

