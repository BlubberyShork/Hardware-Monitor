#pragma once
#include "GraphicsProcessor.h"
#include "processor.h"
#include "motherboard.h"
#include "storagedevice.h"
#include "ThreadManager.h"
#include "WbemManager.h"
#include "HardwareQueries.h"
#include "LiveGPUHandler.h"
#include <mutex>

typedef struct _Hardware_List_Container {
    std::vector<GraphicsProcessor>  gpus;
    std::vector<Processor>          cpus;
    std::vector<Motherboard>        motherboards;
    std::vector<StorageDevice>      storage_dvcs;
} Hardware_List_Container;

class HardwareManager {
public:

    HardwareManager(WbemManager *wbem) : wbem_mngr(wbem) {}

    void ExecuteQueryThreadPool();

    // Accessors
    const Hardware_List_Container& GetHardwareData() const { return hw_data; }
    const LiveGPUHandler& GetLiveGPUHandler() const { return live_gpu_handler; }

    void infoPhysicalDrive(
        std::vector<StorageDevice>& sd_list,
        std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>& d_hmap,
        std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash>& p_hmap,
        std::unordered_map<wchar_t, Volume>& v_hmap,
        std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>& pd_hmap
    );

private:
    WbemManager             *wbem_mngr;
    ThreadManager           thrd_mngr;
    std::mutex              mtx;
    Hardware_List_Container hw_data;
    LiveGPUHandler          live_gpu_handler;

    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>                       disks;
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> partitions;
    std::unordered_map<wchar_t, Volume>                                         volumes;
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>                  phys_disks;
};