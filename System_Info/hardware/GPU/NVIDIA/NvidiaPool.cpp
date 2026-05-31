#include "NvidiaPool.h"
#include <iostream>
#include <stdexcept>

NvidiaPool::NvidiaPool() {
    NvAPI_Status res = NvAPI_Initialize();
    if (res != NVAPI_OK) {
        NvAPI_ShortString err_msg;
        NvAPI_GetErrorMessage(res, err_msg);
        throw std::runtime_error("NVAPI Initialization failed: " + std::string(err_msg) + "\n");
    }
}

NvidiaPool::~NvidiaPool() {
    NvAPI_Unload();
}

void NvidiaPool::enumerateDevices() {
    NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32 gpu_cnt = 0;

    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(handles, &gpu_cnt);
    if(status != NVAPI_OK) std::cerr << "Failed to enumerate physical GPUs: " << status << "\n";
    for (NvU32 i = 0; i < gpu_cnt; i++) {
        auto dev = std::make_unique<NvidiaLiveGPUMetrics>(handles[i]);
        nvidia_adapters.push_back(std::move(dev));
        nvidia_adapters.back().get()->fetchMetrics();
    }
}