#define _WIN32_DCOM

#include "GraphicsProcessor.h"
#include "motherboard.h"
#include "storagedevice.h"
#include "processor.h"
#include "hardware_info.h"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#pragma comment(lib, "wbemuuid.lib")

int main()
{
    IWbemLocator *loc = nullptr;
    IWbemRefresher *refresher = nullptr;

    InitializeCOM();
    setupWBEM(loc);

    const int num_threads = 4;
    std::thread threads[num_threads];

    std::vector<Motherboard> mboard_list;
    std::vector<GraphicsProcessor> gpu_list;
    std::vector<Processor> proc_list;
    std::vector<StorageDevice> sd_list; 
    
    // FIX: make this a list of std::function using lambdas to set threads[i] = ...function[i](loc)
    threads[0] = std::thread(infoMotherboard, std::ref(loc), std::ref(mboard_list));    // Mboard thread
    threads[1] = std::thread(infoGPU, std::ref(loc), std::ref(gpu_list));               // GPU thread
    threads[2] = std::thread(infoCPU, std::ref(loc), std::ref(proc_list));              // CPU thread
    threads[3] = std::thread(infoPhysicalDrive, std::ref(loc), std::ref(sd_list));      // StorageDrive thread
            // TODO ^ Break this up into separate functions per query and thread each with a mutex
    //infoTemperatures();   TODO - Will need to make call to kernel driver

    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Motherboard ** \n\n";
    for (int i = 0; i < mboard_list.size(); i++) {
        mboard_list[i].outputMotherboardInfo();
    }
    std::wcout << std::endl;

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** GPUs & Video Controllers ** \n\n";
    for (int i = 0; i < gpu_list.size(); i++) {
        gpu_list[i].outputGPUInfo();
    }
    std::wcout << std::endl;

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Processors ** \n\n";
    for (int i = 0; i < proc_list.size(); i++) {    
        proc_list[i].outProcInfo();
    }
    std::wcout << std::endl;

    std::wcout << "--------------------------------------------------------------\n";
    std::wcout << "     ** Storage Device ** \n\n";
    for (int i = 0; i < sd_list.size(); i++) {
        sd_list[i].outSDInfo();
    }
    std::wcout << std::endl;

    if (loc) loc->Release();
    CoUninitialize();

    // Check for mem leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    return 0;
}