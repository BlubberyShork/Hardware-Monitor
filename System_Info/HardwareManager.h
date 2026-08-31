#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "hardware/pipeline_workers/IHardwarePipelineWorker.h"
#include "wmi/GraphicsProcessor.h"
#include "wmi/HardwareQueries.h"
#include "wmi/WbemManager.h"
#include "wmi/motherboard.h"
#include "wmi/processor.h"
#include "wmi/storagedevice.h"

class ClientQueue;
class WbemManager;
class ThreadManager;

struct Hardware_List_Container {
    std::vector<GraphicsProcessor> gpus;
    std::vector<Processor> cpus;
    std::vector<Motherboard> motherboards;
    std::vector<StorageDevice> storage_dvcs;
};

class HardwareManager {
public:
    [[deprecated]] HardwareManager(WbemManager* wbem, std::shared_ptr<ClientQueue> queue);
    HardwareManager(std::shared_ptr<ClientQueue> queue);
    ~HardwareManager();

    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

    void InitializeAllWorkers();
    void StartPolling(std::chrono::milliseconds poll_interval = std::chrono::milliseconds{1250});
    void StopPolling();

    const Hardware_List_Container& GetHardwareData() const { return hw_data_; }
    Hardware_List_Container* GetHardwareDataPtr() { return &hw_data_; }

private:
    [[deprecated]] void QueryWmiHardware();
    [[deprecated]] void infoPhysicalDrive();

    WbemManager* wbem_mngr_;
    std::shared_ptr<ClientQueue> queue_;
    std::unique_ptr<ThreadManager> thrd_mngr_;
    std::vector<std::unique_ptr<IHardwarePipelineWorker>> workers_;
    std::vector<std::jthread> worker_threads_;
    std::mutex mtx_;
    Hardware_List_Container hw_data_;
    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> disks_;
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> partitions_;
    std::unordered_map<wchar_t, Volume> volumes_;
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> phys_disks_;
};
