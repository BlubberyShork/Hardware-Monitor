#pragma once
#include <Windows.h>
#include <vector>
#include "../shared_headers/cpu_shared_info.h"

class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    bool isValid() const;
    void runDriver();
    void printDriverOutput();

private:
    HANDLE h_device = INVALID_HANDLE_VALUE;
    std::vector<CPU_DATA_BUFFER> ret_data;
};