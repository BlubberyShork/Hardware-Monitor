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

private:
    DxgiHandler dxgi_;
    std::vector<A_HardwareDevice*> devices_;
};
