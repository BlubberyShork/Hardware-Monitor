#include "CPUPipelineWorker.h"

#include "../../../OPC_UA/ClientQueue.h"

#include <iostream>

CPUPipelineWorker::CPUPipelineWorker(ClientQueue& queue)
    : IHardwarePipelineWorker(queue) {}

void CPUPipelineWorker::initialize() {
    std::cout << "[CPUPipelineWorker] initialize: driver valid=" << driver_client_.isValid() << "\n";
    if (driver_client_.isValid()) {
        device_ = std::make_unique<CPULiveMetrics>(driver_client_);
        device_->fetchMetrics();
    } else {
        std::cout << "[CPUPipelineWorker] Skipping CPU metrics — driver not available\n";
    }
}

void CPUPipelineWorker::execute() {
    if (!device_) {
        return;
    }
    device_->fetchMetrics();
    queue_.push(device_->snapshot());
}
