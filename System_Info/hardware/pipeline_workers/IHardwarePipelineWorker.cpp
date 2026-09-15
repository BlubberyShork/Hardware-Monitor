#include "IHardwarePipelineWorker.h"
#include "../../../shared/PerformanceLogger.h"

#include <thread>

void IHardwarePipelineWorker::setPerfLogger(std::shared_ptr<PerformanceLogger> logger) {
    perf_logger_ = std::move(logger);
}

void IHardwarePipelineWorker::run(std::stop_token stop_token, std::chrono::milliseconds poll_interval) {
    while (!stop_token.stop_requested()) {
        if (perf_logger_) perf_logger_->start(worker_name());
        execute();
        if (perf_logger_) perf_logger_->stop();

        std::this_thread::sleep_for(poll_interval);
    }
}
