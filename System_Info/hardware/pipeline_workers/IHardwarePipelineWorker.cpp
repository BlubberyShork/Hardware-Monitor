#include "IHardwarePipelineWorker.h"
#include "../../../user_space_common/PerformanceLogger.h"

#include <thread>

void IHardwarePipelineWorker::setPerfLogger(std::shared_ptr<PerformanceLogger> logger) {
    perf_logger_ = std::move(logger);
}

void IHardwarePipelineWorker::run(std::stop_token stop_token, std::chrono::milliseconds poll_interval) {
    auto next = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested()) {
        next += poll_interval;

        if (perf_logger_) perf_logger_->start(worker_name());
        execute();
        if (perf_logger_) perf_logger_->stop();

        const auto now = std::chrono::steady_clock::now();
        if (next > now)
            std::this_thread::sleep_until(next);
        else
            next = now;
    }
}
