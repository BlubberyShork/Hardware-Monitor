#ifdef _WIN32
#define _WIN32_DCOM
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "driver_client\DriverClient.h"
#include "wmi\ComManager.h"
#include "wmi\WbemManager.h"
#include "output_generator\OutputHandler.h"
#endif

#if defined(__linux__)

#endif

#include <ctime>
#include <chrono>
#include <iostream>

int main(int argc, char* argv[])
{
#if defined(_WIN32) || defined(_WIN64)
    //Windows pipeline

    std::cout << "before initializations\n";
    auto start = std::chrono::high_resolution_clock::now();

    ComManager  com_mngr;
    WbemManager wbem_mngr; 

    HardwareManager hw_mngr(&wbem_mngr);
    hw_mngr.ExecuteQueryThreadPool();
    
    DriverClient dc;
    dc.runDriver();

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    //OutputHandler out(hw_mngr.GetHardwareData(), dc);
    //out.output();
    OutputHandler out(hw_mngr.GetHardwareData());
    out.outputNoDriver();
    
    std::cout << "Threads took: " << dur.count() << " ms";
    
    return 0;
#elif defined(__linux__)
    // Linux pipeline


    return 0;
#else 
#error "Unsupported Operating System"

#endif
}




