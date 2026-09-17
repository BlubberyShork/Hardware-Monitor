#pragma once

#include "IHardwarePipelineWorker.h"
#include "../GPU/DxgiHandler.h"

#include <vector>

class A_HardwareDevice;

class GPUPipelineWorker final : public IHardwarePipelineWorker {
public:
    explicit GPUPipelineWorker(ClientQueue& queue);
    void initialize() override;
    void execute() override;
    std::string_view worker_name() const override { return "gpu_loop"; }

private:
    DxgiHandler dxgi_;
    std::vector<A_HardwareDevice*> devices_;
};
