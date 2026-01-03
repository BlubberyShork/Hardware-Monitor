#ifdef _WIN32 //TODO - Should it be _WIN64?

    #define _WIN32_DCOM

    #include "hardware_info.h"
    #include "../shared_headers/cpu_shared_info.h"

    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>

    #include <functional>
    #include <mutex>
    #include <ctime>
    #include <iomanip>
    #include <iostream>
    #include <chrono>

    #pragma comment(lib, "wbemuuid.lib")

    constexpr int NUM_THREADS = 7;
    constexpr int NUM_W32_THREADS = 3;
    constexpr int NUM_MSFT_THREADS = 4;
    constexpr int NUM_SVCS = 2;

    #define OUTPUT_HEADER(header_msg) \
        std::cout << "--------------------------------------------------------------\n"; \
        std::cout << "     ** " << header_msg << "** \n\n";

    // TODO - Most of this code should not be in main 
    int main(int arcg, char *argv[])
    {
        // TODO make a COM pointer -> Do I need this?
        IWbemLocator *loc = nullptr;
        IWbemServices *w32_svcs = nullptr;
        IWbemServices *msft_svcs = nullptr;
        IWbemRefresher *refresher = nullptr;

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

        HANDLE h_device = CreateFile(
            L"\\\\.\\CPUMonitorDriver", // Name of the driver
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if(h_device == INVALID_HANDLE_VALUE) {
            std::cout << "Failed to open device. Error: " << GetLastError() << "\n";
            return 1;
        }

        BYTE* buffer = nullptr;
        DWORD buffer_sz = sizeof(CPU_DATA_HEADER); 
        DWORD bytes_returned;
        CPU_DATA_BUFFER* cpu_info = NULL;
        
        buffer = (BYTE*)malloc(buffer_sz);
        while(true) {
            BOOL success = DeviceIoControl(
                    h_device, IOCTL_GET_DATA,
                    NULL, 0,
                    buffer, buffer_sz,   
                    &bytes_returned, NULL
            );
            
            // TODO - Make sure the handling to ensure the size is correct 
            if(success) {
                cpu_info = (CPU_DATA_BUFFER*) buffer;
                OUTPUT_HEADER("Processor Temp/Load");
                for(std::size_t i = 0; i < cpu_info->header.required_size; i += sizeof(CPU_DATA)) {
                    std::wcout << L"CPU ID: " << cpu_info->data[i].cpu_id << L"C\n";
                    std::wcout << L"Temp: " << cpu_info->data[i].temp << L"C\n";
                }
                break;
            }

            DWORD err = GetLastError(); 
            if(err != ERROR_MORE_DATA && err != ERROR_INSUFFICIENT_BUFFER) {
                std::cout << "DeviceIoControl failed permanently. Error: " << err << "\n";
                break;
            }

            if(bytes_returned < sizeof(CPU_DATA_HEADER)) {
               std::cout << "Driver didn't return header.\n";
               break;
            }

            DWORD new_sz = ((CPU_DATA_HEADER*)buffer)->required_size;
            free(buffer);
            buffer = (BYTE*)malloc(new_sz);
            if(!buffer) {
                std::cout << "malloc failed\n";
                break;
            }
            buffer_sz = new_sz;
        }

        if (buffer) free(buffer);
        CloseHandle(h_device);

        OUTPUT_HEADER("Motherboard");
        for (int i = 0; i < mboard_list.size(); i++) {
            mboard_list[i].outputMotherboardInfo();
        }
        std::wcout << std::endl;

        OUTPUT_HEADER("GPUs and Graphics Processors");
        for (int i = 0; i < gpu_list.size(); i++) {
            gpu_list[i].outputGPUInfo();
        }
        std::wcout << std::endl;

        OUTPUT_HEADER("Processors");
        for (int i = 0; i < proc_list.size(); i++) {    
            proc_list[i].outProcInfo(); 
        }
        std::wcout << "\n";

        OUTPUT_HEADER("Storage Devices");
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

#endif



