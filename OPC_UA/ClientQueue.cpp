#include "ClientQueue.h"

#include <utility>

void ClientQueue::push(TelemetrySnapshot snapshot) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (shutting_down_) {
            return;
        }
        queue_.push(std::move(snapshot));
    }
    cv_.notify_one();
}

std::vector<TelemetrySnapshot> ClientQueue::drain() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return !queue_.empty() || shutting_down_; });

    std::vector<TelemetrySnapshot> snapshots;
    snapshots.reserve(queue_.size());
    while (!queue_.empty()) {
        snapshots.push_back(std::move(queue_.front()));
        queue_.pop();
    }
    return snapshots;
}

void ClientQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        shutting_down_ = true;
    }
    cv_.notify_all();
}
