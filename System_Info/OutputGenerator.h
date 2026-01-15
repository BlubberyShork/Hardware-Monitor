//#pragma once
//
//#include "hardware_info.h"
//#include "../shared_headers/cpu_shared_info.h"
//#include <iomanip>
//#include <iostream>
//#include <vector>
//
//#define OUTPUT_HEADER(header_msg) \
//        std::cout << "--------------------------------------------------------------\n"; \
//        std::cout << "     ** " << header_msg << "** \n\n";
//
//    // Constructor
//    OutputGenerator();
//    OutputGenerator(
//        CPU_DATA_BUFFER* cpu_info,
//        std::vector<Motherboard> mboard_list,
//        std::vector<GraphicsProcessor> gpu_list,
//        std::vector<Processor> proc_list,
//        std::vector<StorageDevice> sd_list,
//        std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> d_hmap,
//        std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> p_hmap,
//        std::unordered_map<wchar_t, Volume> v_hmap,
//        std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> pd_hmap
//    );
//    // Destructor
//
//    void print();
//
//private:
//    CPU_DATA_BUFFER* cpu_info;
//    std::vector<Motherboard> mboard_list;
//    std::vector<GraphicsProcessor> gpu_list;
//    std::vector<Processor> proc_list;
//    std::vector<StorageDevice> sd_list;
//    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual> d_hmap;
//    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash> p_hmap;
//    std::unordered_map<wchar_t, Volume> v_hmap;
//    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual> pd_hmap;
//
//};

