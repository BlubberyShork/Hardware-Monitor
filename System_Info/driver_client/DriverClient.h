#pragma once
#include <Windows.h>
#include <vector>
#include "..\..\shared_headers\cpu_shared_info.h"

// TODO - Detect and expose the CPU vendor instead of assuming Intel.
class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    bool isValid() const;
    std::vector<CPU_DATA> runDriver();
    void printDriverOutput();

private:
    HANDLE h_device = INVALID_HANDLE_VALUE;
    std::vector<CPU_DATA> ret_data;
};
