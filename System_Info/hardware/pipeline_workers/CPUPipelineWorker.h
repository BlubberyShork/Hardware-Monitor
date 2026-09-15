#pragma once

#include "IHardwarePipelineWorker.h"
#include "../CPU/CPULiveMetrics.h"
#include "../../driver_client/DriverClient.h"

#include <memory>

class CPUPipelineWorker final : public IHardwarePipelineWorker {
public:
    explicit CPUPipelineWorker(ClientQueue& queue);
    void initialize() override;
    void execute() override;
    std::string_view worker_name() const override { return "cpu_loop"; }

private:
    DriverClient driver_client_;
    std::unique_ptr<CPULiveMetrics> device_;
};
