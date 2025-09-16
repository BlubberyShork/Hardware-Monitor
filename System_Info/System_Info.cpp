#define _WIN32_DCOM

#include "hardware_info.h"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#include <functional>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <iostream>


#pragma comment(lib, "wbemuuid.lib")

int coreCount();

int main()
{
    IWbemLocator *loc = nullptr;
    IWbemServices *w32_svcs = nullptr;
    IWbemServices *msft_svcs = nullptr;
    IWbemRefresher *refresher = nullptr;

    // figure out if intel cpu or AMD, then do two diff things of code
    int NUM_CORES = coreCount();
    std::cout << "Number of cores read by CPUID:" << NUM_CORES << "\n";

    const int NUM_THREADS = 7;
    const int NUM_W32_THREADS = 3;
    const int NUM_MSFT_THREADS = 4;
    const int NUM_SVCS = 2;

    std::cout << "before initializations\n";
    auto start = std::chrono::high_resolution_clock::now();

    InitializeCOM();
    
    std::thread svcs_threads[NUM_SVCS];
    std::function<void(IWbemLocator*&)> svcs_init_funcs[NUM_THREADS] = {
        [&w32_svcs](IWbemLocator*& loc) { setupW32Wbem(loc, w32_svcs); },
        [&msft_svcs](IWbemLocator*& loc) { setupMSFTWbem(loc, msft_svcs); }
    };

    for (int i = 0; i < NUM_SVCS; i++) {
        svcs_threads[i] = std::thread(svcs_init_funcs[i], std::ref(loc));
    }
    
    for (int i = 0; i < NUM_SVCS; i++) {
        svcs_threads[i].join();
    }

    std::cout << "Finished initializations\n";
    std::vector<Motherboard> mboard_list;
    std::vector<GraphicsProcessor> gpu_list;
    std::vector<Processor> proc_list;
    std::vector<StorageDevice> sd_list; 
    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> d_hmap;
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> p_hmap;
    std::unordered_map<wchar_t, Volume> v_hmap;
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> pd_hmap;

    std::mutex w32_mtx;
    std::mutex msft_mtx;
    std::thread threads[NUM_THREADS];

    // FIX - Optimize this to work based on # cores. NUM_THREADS should depend on if u have 4,6,8 cores
        // For now, it is good enough but the kernel driver can retrieve it from EBX in __cpuid
        // This might be more efficient but we have I/O thru the queries so maybe not?
            // At the least, change the queries to only pull stuff used and
            // use less connectservers synchronized w/ mutexes
    std::function<void(IWbemLocator*&, IWbemServices*&, std::mutex&)> functions[NUM_THREADS] = {
        [&mboard_list](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { infoMotherboard(loc, svcs, mtx, mboard_list); },
        [&gpu_list](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { infoGPU(loc, svcs, mtx, gpu_list); },
        [&proc_list](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { infoCPU(loc, svcs, mtx, proc_list); },
        [&d_hmap](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { sd_DiskQuery(loc, svcs, mtx, d_hmap); },
        [&p_hmap](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { sd_PartitionQuery(loc, svcs, mtx, p_hmap); },
        [&v_hmap](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { sd_VolumeQuery(loc, svcs, mtx, v_hmap); },
        [&pd_hmap](IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx) { sd_PhysicalDiskQuery(loc, svcs, mtx, pd_hmap); }
        //infoTemperatures();   TODO - Will need to make call to kernel driver
    };

    for (int i = 0; i < NUM_W32_THREADS; i++) {
        threads[i] = std::thread(functions[i], std::ref(loc), std::ref(w32_svcs), std::ref(w32_mtx));
    }
    for (int i = NUM_W32_THREADS; i < NUM_THREADS; i++) {
        threads[i] = std::thread(functions[i], std::ref(loc), std::ref(msft_svcs), std::ref(msft_mtx));
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    infoPhysicalDrive(sd_list, d_hmap, p_hmap, v_hmap, pd_hmap);

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

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Threads took: " << dur.count() << " ms";

    if (loc) loc->Release();
    CoUninitialize();

    // Check for mem leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    return 0;
}

int coreCount() {
    int info[4]; 
    __cpuid(info, 1);
    int num_logical_processors = info[1] << 16 & 0xFF;
    int htt_enabled = info[3] << 28 & 0xFF;

    return htt_enabled == 1 ? num_logical_processors / 2 : num_logical_processors;
}




