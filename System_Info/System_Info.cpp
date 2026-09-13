#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #ifndef _WIN32_DCOM
        #define _WIN32_DCOM
    #endif
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
#else
    #error "Unsupported Operating System"
#endif

#if defined(PLATFORM_WINDOWS)
    #include "HardwareManager.h"
    #include "wmi/ComManager.h"
    #include "wmi/WbemManager.h"
    #include "SystemInfoClient.h"
#endif

#if defined(PLATFORM_LINUX)
#endif

#include "../OPC_UA/ClientQueue.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

int main() {
#if defined(PLATFORM_WINDOWS)
    //ComManager com_mngr;
    //WbemManager wbem_mngr;
    
    auto queue = std::make_shared<ClientQueue>();

    const std::filesystem::path project_root =
        std::filesystem::current_path().parent_path().parent_path();

    //HardwareManager hardware_manager(&wbem_mngr, queue);
    HardwareManager hardware_manager(queue);

    SystemInfoClient client("system_info", project_root, queue);

    char* server_ip_raw{};
    size_t server_ip_sz{};
    _dupenv_s(&server_ip_raw, &server_ip_sz, "SERVER_IP");
    if (!server_ip_raw) {
        std::cerr << "SERVER_IP is not set\n";
    }
    std::string server_ip(server_ip_raw);
    std::free(server_ip_raw);

    client.connect("opc.tcp://" + server_ip + ":4840");
    client.addNodes(); 

    std::cout << "Initializing all workers\n";
    hardware_manager.InitializeAllWorkers();
    std::cout << "polling\n";
    hardware_manager.StartPolling();

    client.sendTelemetryPayload();
    while (true) {
        if (!client.sendTelemetryPayload()) {
            std::cerr << "sendTelemetryPayload failed -- addNodes() not completed?\n";
            break;
        }
    }

#elif defined(PLATFORM_LINUX)
    return 0;
#else
    throw std::runtime_error("Unsupported OS platform.");
#endif
}
