#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

struct SensorSnapshot {
    std::string name;
    float value{};
    std::string unit;
};

struct TelemetrySnapshot {
    std::string name;
    std::string vendor;
    std::string hardware_type;
    std::vector<SensorSnapshot> sensors;
};

class ClientQueue {
public:
    ClientQueue() = default;
    ClientQueue(const ClientQueue&) = delete;
    ClientQueue& operator=(const ClientQueue&) = delete;

    void push(TelemetrySnapshot snapshot);
    std::vector<TelemetrySnapshot> drain();
    void shutdown();

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<TelemetrySnapshot> queue_;
    bool shutting_down_ = false;
};
