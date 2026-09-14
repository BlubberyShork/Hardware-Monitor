#include "HardwareManager.h"

#include "../OPC_UA/ClientQueue.h"
#include "ThreadManager.h"
#include "hardware/pipeline_workers/CPUPipelineWorker.h"
#include "hardware/pipeline_workers/GPUPipelineWorker.h"
#include "wmi/WbemManager.h"

#include <functional>

[[deprecated("WMI is being phased out of the project, do not use this functionality")]]
HardwareManager::HardwareManager(WbemManager* wbem, std::shared_ptr<ClientQueue> queue)
    : wbem_mngr_(wbem)
    , queue_(std::move(queue))
    , thrd_mngr_(std::make_unique<ThreadManager>()) {
    workers_.push_back(std::make_unique<GPUPipelineWorker>(*queue_));
    workers_.push_back(std::make_unique<CPUPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<MotherboardPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<NetworkPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<StorageDevicePipelineWorker>(*queue_));
}

// TODO - Implement NetworkPipelineWorker, StorageDevicePipelineWorker, and MotherboardPipelineWorker
HardwareManager::HardwareManager(std::shared_ptr<ClientQueue> queue)
    : queue_(std::move(queue))
    , thrd_mngr_(std::make_unique<ThreadManager>()) {
    workers_.push_back(std::make_unique<GPUPipelineWorker>(*queue_));
    workers_.push_back(std::make_unique<CPUPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<MotherboardPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<NetworkPipelineWorker>(*queue_));
    // workers_.push_back(std::make_unique<StorageDevicePipelineWorker>(*queue_));
}

HardwareManager::~HardwareManager() {
    StopPolling();
}

void HardwareManager::InitializeAllWorkers() {
    std::vector<std::function<void()>> setup_tasks;
    setup_tasks.reserve(workers_.size() + 1);
    //setup_tasks.push_back([this] { QueryWmiHardware(); });
    for (auto& worker : workers_) {
        setup_tasks.push_back([worker = worker.get()] { worker->initialize(); });
    }
    thrd_mngr_->ExecuteThreadPool(setup_tasks);
    //infoPhysicalDrive();
}

void HardwareManager::StartPolling(std::chrono::milliseconds poll_interval) {
    if (!worker_threads_.empty()) {
        return;
    }
    worker_threads_.reserve(workers_.size());
    for (auto& worker : workers_) {
        worker_threads_.emplace_back(
            [worker = worker.get(), poll_interval](std::stop_token stop_token) {
                worker->run(stop_token, poll_interval);
            });
    }
}

void HardwareManager::StopPolling() {
    for (auto& worker_thread : worker_threads_) {
        worker_thread.request_stop();
    }
    worker_threads_.clear();
}

[[deprecated("WMI is being phased out of the project, do not use this functionality")]]
void HardwareManager::QueryWmiHardware() {
    std::vector<std::function<void()>> queries = {
        [this] { HardwareQueries::QueryCPUs(wbem_mngr_->getW32Services(), wmi_mtx_, hw_data_.cpus); },
        [this] { HardwareQueries::QueryMotherboards(wbem_mngr_->getW32Services(), wmi_mtx_, hw_data_.motherboards); },
        [this] { HardwareQueries::QueryDisks(wbem_mngr_->getMsftServices(), wmi_mtx_, disks_); },
        [this] { HardwareQueries::QueryPartitions(wbem_mngr_->getMsftServices(), wmi_mtx_, partitions_); },
        [this] { HardwareQueries::QueryVolumes(wbem_mngr_->getMsftServices(), wmi_mtx_, volumes_); },
        [this] { HardwareQueries::QueryPhysicalDisks(wbem_mngr_->getMsftServices(), wmi_mtx_, phys_disks_); }
    };
    ThreadManager query_manager;
    query_manager.ExecuteThreadPool(queries);
}

[[deprecated("WMI is being phased out of the project, do not use this functionality")]]
void HardwareManager::infoPhysicalDrive() {
    for (const auto& disk_pair : disks_) {
        StorageDevice storage_device;
        const Disk& disk = disk_pair.second;
        storage_device.setDisk(disk);
        for (size_t i = 0; i < disk.num_partitions; ++i) {
            const Partition::partition_id partition_id{disk.disk_num, static_cast<ULONG>(i)};
            const auto partition = partitions_.find(partition_id);
            if (partition == partitions_.end()) {
                continue;
            }
            storage_device.getPartitions().push_back(partition->second);
            const auto volume = volumes_.find(partition->second.drv_ltr);
            if (partition->second.drv_ltr != 0 && volume != volumes_.end()) {
                storage_device.getVolumes().push_back(volume->second);
            }
        }
        const auto physical_disk = phys_disks_.find(disk.disk_num);
        if (physical_disk != phys_disks_.end()) {
            storage_device.setPhysicalDisk(physical_disk->second);
        }
        hw_data_.storage_dvcs.push_back(std::move(storage_device));
    }
}
