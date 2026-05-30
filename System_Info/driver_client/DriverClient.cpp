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

    if (ret_data) {
        free(ret_data);
    }
}

bool DriverClient::isValid() const {
    return h_device != INVALID_HANDLE_VALUE;
}

void DriverClient::runDriver() {
    if (!isValid())
        return;

    DWORD bytes_ret = 0;
    DWORD buffer_size = sizeof(CPU_DATA_HEADER);

    BYTE* buffer = (BYTE*)malloc(buffer_size);
    if (!buffer) {
        std::cout << "Initial malloc failed\n";
        return;
    }

    while (true) {
        BOOL success = DeviceIoControl(
            h_device,
            IOCTL_GET_DATA,
            nullptr, 0,
            buffer, buffer_size,
            &bytes_ret,
            nullptr
        );

        if (success) {
            // Allocate final return buffer
            /*std::cout << "success bytes_ret: " << bytes_ret << "\n";
            std::cout << "proc_count in buffer: " << ((CPU_DATA_BUFFER*)buffer)->header.processor_count << "\n";*/
            ret_data = (CPU_DATA_BUFFER*)malloc(bytes_ret);

            if (!ret_data) {
                std::cout << "ret_data malloc failed\n";
                free(buffer);
                return;
            }

            memcpy(ret_data, buffer, bytes_ret);
            free(buffer);
            return;
        }
        else {
            /*std::cout << "success: " << success << " err: " << GetLastError() << " bytes_ret: " << bytes_ret << "\n";
            std::cout << "header required_size: " << ((CPU_DATA_HEADER*)buffer)->required_size << "\n";
            std::cout << "header proc_count: " << ((CPU_DATA_HEADER*)buffer)->processor_count << "\n"*/;
            DWORD err = GetLastError();

            if (err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
                CPU_DATA_HEADER* hdr = (CPU_DATA_HEADER*)buffer;

                buffer_size = hdr->required_size;
                BYTE* new_buffer = (BYTE*)realloc(buffer, buffer_size);

                if (!new_buffer) {
                    std::cout << "realloc failed\n";
                    std::cout << "DeviceIoControl error: " << GetLastError() << " bytes_ret: " << bytes_ret << "\n";
                    free(buffer);
                    return;
                }

                buffer = new_buffer;
            }
            else {
                std::cout << "DeviceIoControl failed permanently. Error: " << err << "\n";
                free(buffer);
                return;
            }
        }
    }
}

void DriverClient::printDriverOutput() {
    for (ULONG i = 0; i < ret_data->header.processor_count; i++) {
        std::wcout << L"CPU ID: " << ret_data->data[i].cpu_id;
        std::wcout << L"  Temp: " << ret_data->data[i].temp << L"C\n";
    }
}