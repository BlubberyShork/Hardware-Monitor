#pragma once

#include <chrono>
#include <stop_token>
#include <thread>

class ClientQueue;

class IHardwarePipelineWorker {
public:
    explicit IHardwarePipelineWorker(ClientQueue& queue) : queue_(queue) {}
    virtual ~IHardwarePipelineWorker() = default;

    IHardwarePipelineWorker(const IHardwarePipelineWorker&) = delete;
    IHardwarePipelineWorker& operator=(const IHardwarePipelineWorker&) = delete;

    virtual void initialize() = 0;
    virtual void execute() = 0;

    void run(std::stop_token stop_token, std::chrono::milliseconds poll_interval) {
        while (!stop_token.stop_requested()) {
            execute();
            std::this_thread::sleep_for(poll_interval);
        }
    }

protected:
    ClientQueue& queue_;
};
