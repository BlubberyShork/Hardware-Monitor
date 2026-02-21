#pragma once
#include <thread>
#include <vector>
#include <functional>

class ThreadManager {
public:
    ThreadManager() = default;
    ~ThreadManager() = default;

    void AddThread(std::function<void()> func) {
        threads.emplace_back(func);
    }

    void JoinAll() {
        for (auto& t : threads) if (t.joinable()) t.join();
        threads.clear();
    }

    template<typename Container>
    void ExecuteThreadPool(Container& threadFuncs) {
        for (auto& func : threadFuncs)
            threads.emplace_back(func);

        JoinAll();
    }

private:
    std::vector<std::thread> threads;
};