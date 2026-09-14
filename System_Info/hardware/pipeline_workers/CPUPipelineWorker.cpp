#include "CPUPipelineWorker.h"

#include "../../../OPC_UA/ClientQueue.h"

CPUPipelineWorker::CPUPipelineWorker(ClientQueue& queue)
    : IHardwarePipelineWorker(queue) {}

void CPUPipelineWorker::initialize() {
    if (driver_client_.isValid()) {
        device_ = std::make_unique<CPULiveMetrics>(driver_client_);
        device_->fetchMetrics();
    }
}

void CPUPipelineWorker::execute() {
    if (!device_) {
        return;
    }
    device_->fetchMetrics();
    queue_.push(device_->snapshot());
}
