#pragma once

#include <chrono>
#include <memory>
#include <stop_token>
#include <string_view>

class ClientQueue;
class PerformanceLogger;

class IHardwarePipelineWorker {
public:
    explicit IHardwarePipelineWorker(ClientQueue& queue) : queue_(queue) {}
    virtual ~IHardwarePipelineWorker() = default;

    IHardwarePipelineWorker(const IHardwarePipelineWorker&) = delete;
    IHardwarePipelineWorker& operator=(const IHardwarePipelineWorker&) = delete;

    virtual void initialize() = 0;
    virtual void execute() = 0;
    virtual std::string_view worker_name() const = 0;

    void setPerfLogger(std::shared_ptr<PerformanceLogger> logger);
    void run(std::stop_token stop_token, std::chrono::milliseconds poll_interval);

protected:
    ClientQueue& queue_;

private:
    std::shared_ptr<PerformanceLogger> perf_logger_;
};
