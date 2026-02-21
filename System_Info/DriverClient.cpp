#include "DriverClient.h"
#include <iostream>

DriverClient::DriverClient() {
    h_device = CreateFile(
        L"\\\\.\\CPUMonitorDriver",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
}

DriverClient::~DriverClient() {
    if (h_device != INVALID_HANDLE_VALUE)
        CloseHandle(h_device);
}

bool DriverClient::isValid() const {
    return h_device != INVALID_HANDLE_VALUE;
}

std::vector<BYTE> DriverClient::getCpuData() {
        if (!isValid())
            return {};

        std::vector<BYTE> buffer(sizeof(CPU_DATA_HEADER));
        DWORD bytes_ret = 0;

        while (true) {
            BOOL success = DeviceIoControl(
                h_device,
                IOCTL_GET_DATA,
                nullptr, 0,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytes_ret,
                nullptr
            );

            if (success) {
                buffer.resize(bytes_ret);
                return buffer;
            } else {
                DWORD err = GetLastError();

                if (err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
                    auto* hdr = reinterpret_cast<CPU_DATA_HEADER*>(buffer.data());
                    buffer.resize(hdr->required_size);
                    continue;
                } else {
                    std::cout << "DeviceIoControl failed permanently. Error: " << err << "\n";
                    return {};
                }
            }
        }
    }