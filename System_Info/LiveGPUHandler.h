#pragma once
#include <dxgi.h>
#include <memory>
#include "GPULiveData.h"

class ILiveGPUMetrics {
public:
    virtual ~ILiveGPUMetrics() = default;
    virtual void fetchMetrics() = 0;
    virtual void outputMetrics() const = 0;
};
 
class LiveGPUHandler {
public:
    LiveGPUHandler();
    ~LiveGPUHandler();

    LiveGPUHandler(LiveGPUHandler&&) = default;
    LiveGPUHandler& operator=(LiveGPUHandler&&) = default;

    void fetchCurrentLiveGPUMetrics();
    void outputLiveGPUMetrics() const;

private:
    enum class Vendor { NVIDIA, AMD, UNKNOWN };

    Vendor detectVendor() const;
    std::unique_ptr<ILiveGPUMetrics> backend;
};
