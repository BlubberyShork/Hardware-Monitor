#ifdef _WIN32
#define _WIN32_DCOM

#include "DriverClient.h"
#include "OutputGenerator.h"
#include "ComManager.h"
#include "WbemManager.h"
#include "OutputHandler.h"

#include <ctime>
#include <chrono>

int main(int arcg, char* argv[])
{
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

    OutputHandler out(hw_mngr.GetHardwareData(), dc);
    out.output();
    
    std::cout << "Threads took: " << dur.count() << " ms";
    
    return 0;
}
    
#endif



