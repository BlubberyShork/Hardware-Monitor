#include "GPUPipelineWorker.h"

#include "../../../OPC_UA/ClientQueue.h"
#include "../HardwareDevice.h"

GPUPipelineWorker::GPUPipelineWorker(ClientQueue& queue)
    : IHardwarePipelineWorker(queue) {}

void GPUPipelineWorker::initialize() {
    dxgi_.createGPUDevices();
    for (const auto& pool : dxgi_.getPools()) {
        for (const auto& device : pool->getDevices()) {
            devices_.push_back(device.get());
        }
    }
}

void GPUPipelineWorker::execute() {
    for (auto* device : devices_) {
        device->fetchMetrics();
        queue_.push(device->snapshot());
    }
}
