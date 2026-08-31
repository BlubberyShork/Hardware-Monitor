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
    #include "driver_client/DriverClient.h"
    #include "wmi/ComManager.h"
    #include "wmi/WbemManager.h"
    #include "output_generator/OutputHandler.h"
#elif defined(PLATFORM_LINUX)
    // #include "linux_backend/LinuxHardware.h"
#endif

#include <chrono>
#include <iostream>
#include "../OPC_UA/client.h"

int main(int argc, char* argv[]) 
{
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_LINUX)
#endif // Windows or linux OS
    std::cout << "before initializations\n";
    auto start = std::chrono::high_resolution_clock::now();

#if defined(PLATFORM_WINDOWS)
    ComManager  com_mngr;
    WbemManager wbem_mngr; 

    HardwareManager hw_mngr(&wbem_mngr);
    hw_mngr.ExecuteQueryThreadPool();
    
    DriverClient dc;
    dc.runDriver();

    //OutputHandler out(hw_mngr.GetHardwareData(), dc);
    //out.output();
    OutputHandler out(hw_mngr.GetHardwareData());
    out.outputNoDriver();
    
#elif defined(PLATFORM_LINUX)
#else 
#error "Unsupported Operating System"

#endif // Platform check
    
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Threads took: " << dur.count() << " ms";

    return 0;
}




