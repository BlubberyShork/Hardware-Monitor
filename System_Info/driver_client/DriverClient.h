#pragma once
#include <Windows.h>
#include <vector>
#include "..\..\kernel_common\cpu_shared_info.h"

class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    bool isValid() const;
    CPU_VENDOR getVendor() const;
    std::vector<CPU_DATA> runDriver();
    void printDriverOutput();

private:
    HANDLE h_device = INVALID_HANDLE_VALUE;
    CPU_VENDOR vendor = CPU_VENDOR_UNKNOWN;
    std::vector<CPU_DATA> ret_data;
};
