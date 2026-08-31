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
    #include "../OPC_UA/client.h"
#endif

#include "../OPC_UA/ClientQueue.h"

#include <iostream>
#include <memory>

int main() {
#if defined(PLATFORM_WINDOWS)
    //ComManager com_mngr;
    //WbemManager wbem_mngr;
    
    auto queue = std::make_shared<ClientQueue>();
    //HardwareManager hardware_manager(&wbem_mngr, queue);
    HardwareManager hardware_manager(queue);
    SystemInfoClient client("system_info", queue);

    std::cout << "Initializing all workers\n";
    hardware_manager.InitializeAllWorkers();
    std::cout << "polling\n";
    hardware_manager.StartPolling();

    while (true) {
        for (const auto& snapshot : queue->drain()) {
            std::cout << "[" << snapshot.vendor << "] " << snapshot.name
                      << " (" << snapshot.hardware_type << ") - "
                      << snapshot.sensors.size() << " sensors\n";
            for (const auto& sensor : snapshot.sensors) {
                std::cout << "  " << sensor.name << ": " << sensor.value
                          << " " << sensor.unit << "\n";
            }
        }
        std::cout << "\n";
    }
#elif defined(PLATFORM_LINUX)
    return 0;
#endif
}
