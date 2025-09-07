#pragma once

#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

/*
    Storage container for MSFT_Disk data
*/
struct Disk {
    bstr_t unq_id;
    ULONG disk_num;
    bstr_t fname;
    bstr_t manufacturer;
    ULONG num_partitions;
    bstr_t model;
    ULONGLONG sz;
};

/*
    Storage container for MSFT_Partition data
*/
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

/*
    Storage container for MSFT_Volume data
*/
struct Volume {
    wchar_t drv_ltr; 
    ULONGLONG sz;
    ULONGLONG sz_rmng;
    USHORT hstatus;
};

/*
    Storage container for MSFT_PhysicalDisk data
*/
struct PhysDisk {
    ULONG disk_num;
    bstr_t device_id;
    USHORT unq_id_frmt;
    ULONG spindle_speed;
};

class StorageDevice
{
private:
    Disk disk;
    std::vector<Partition> partitions;
    std::vector<Volume> volumes;
    PhysDisk physical_disk;

public:
    StorageDevice() = default;
    StorageDevice(Disk disk, std::vector<Partition> partitions,
        std::vector<Volume> volumes)
        : disk(disk), partitions(partitions), volumes(volumes) {}

    Disk& getDisk() { return disk; }
    std::vector<Partition>& getPartitions() { return partitions; }
    std::vector<Volume>& getVolumes() { return volumes; }
    PhysDisk& getPhysicalDisk() { return physical_disk; }

    // Setters
    void setDisk(const Disk& d) { disk = d; }
    void setPartitions(const std::vector<Partition>& p) { partitions = p; }
    void setVolumes(const std::vector<Volume>& v) { volumes = v; }
    void setPhysicalDisk(const PhysDisk& pd) { physical_disk = pd; }

    // Output
    void outSDInfo();
};

