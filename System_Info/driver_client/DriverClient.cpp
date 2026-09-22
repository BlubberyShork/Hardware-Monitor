#include "DriverClient.h"
#include <cstddef>
#include <iostream>

DriverClient::DriverClient() {
    vendor = DetectCpuVendor();

    const wchar_t* device_path = nullptr;
    switch (vendor) {
    case CPU_VENDOR_INTEL:
        device_path = L"\\\\.\\WindowsIntelCPUDriver";
        break;
    case CPU_VENDOR_AMD:
        device_path = L"\\\\.\\WindowsAMDCPUDriver";
        break;
    default:
        std::cout << "[DriverClient] Unknown CPU vendor, cannot open driver\n";
        return;
    }

    h_device = CreateFile(
        device_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (h_device == INVALID_HANDLE_VALUE) {
        std::cout << "[DriverClient] CreateFile failed. Error: " << GetLastError() << "\n";
    } else {
        std::cout << "[DriverClient] Handle opened successfully\n";
    }
}

DriverClient::~DriverClient() {
    if (h_device != INVALID_HANDLE_VALUE)
        CloseHandle(h_device);

}

bool DriverClient::isValid() const {
    return h_device != INVALID_HANDLE_VALUE;
}

CPU_VENDOR DriverClient::getVendor() const {
    return vendor;
}

std::vector<CPU_DATA> DriverClient::runDriver() {
    if (!isValid()) {
        std::cout << "[DriverClient] runDriver: handle is invalid, skipping\n";
        return {};
    }

    DWORD bytes_ret = 0;
    DWORD buffer_size = sizeof(CPU_DATA_HEADER);

    BYTE* buffer = (BYTE*)malloc(buffer_size);
    if (!buffer) {
        std::cout << "[DriverClient] Initial malloc failed\n";
        return {};
    }

    bool first_pass = true;
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
            const auto* result = reinterpret_cast<const CPU_DATA_BUFFER*>(buffer);
            const size_t header_size = offsetof(CPU_DATA_BUFFER, data);
            const size_t required_size = header_size +
                static_cast<size_t>(result->header.entry_count) * sizeof(CPU_DATA);

            std::cout << "[DriverClient] DeviceIoControl succeeded. bytes_ret=" << bytes_ret
                      << " processor_count=" << result->header.processor_count
                      << " entry_count=" << result->header.entry_count
                      << " required_size=" << required_size << "\n";

            if (bytes_ret < required_size) {
                std::cout << "[DriverClient] bytes_ret < required_size, returning empty\n";
                free(buffer);
                return {};
            }

            for (ULONG i = 0; i < result->header.entry_count; ++i) {
                std::cout << "[DriverClient] " << result->data[i].name
                          << " cpu_id=" << result->data[i].cpu_id
                          << " value=" << result->data[i].value
                          << " type=" << result->data[i].type
                          << " unit=" << result->data[i].unit << "\n";
            }

            ret_data.assign(result->data, result->data + result->header.entry_count);
            free(buffer);
            return ret_data;
        }
        else {
            DWORD err = GetLastError();

            if(first_pass) {
                if (err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
                    first_pass = false;
                    CPU_DATA_HEADER* hdr = (CPU_DATA_HEADER*)buffer;
                    std::cout << "[DriverClient] Buffer too small. Resizing to " << hdr->required_size << "\n";
                    buffer_size = hdr->required_size;
                    BYTE* new_buffer = (BYTE*)realloc(buffer, buffer_size);
                    if (!new_buffer) {
                        std::cout << "[DriverClient] realloc failed\n";
                        free(buffer);
                        return {};
                    }
                    buffer = new_buffer;
                }
                else {
                    std::cout << "[DriverClient] DeviceIoControl failed permanently. Error: " << err << "\n";
                    free(buffer);
                    return {};
                }
            } else { // Second pass, aggressively allocate 2x size for the telemetry fields
                if(err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
                    CPU_DATA_HEADER* hdr = (CPU_DATA_HEADER*)buffer;
                    buffer_size = hdr->required_size * 2;
                    BYTE* new_buffer = (BYTE*)realloc(buffer, buffer_size);
                    if (!new_buffer) {
                        std::cout << "[DriverClient] realloc failed\n";
                        free(buffer);
                        return {};
                    }
                    buffer = new_buffer;
                }
            }
        }
    }
}

void DriverClient::printDriverOutput() {
    for (const auto& entry : ret_data) {
        std::cout << entry.name << ": " << entry.value
                  << " (cpu_id=" << entry.cpu_id
                  << " type=" << entry.type
                  << " unit=" << entry.unit << ")\n";
    }
}
