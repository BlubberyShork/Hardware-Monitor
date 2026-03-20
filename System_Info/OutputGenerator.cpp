//#include "OutputGenerator.h"
//
//void printOutput() {
//    OUTPUT_HEADER("Motherboard");
//    for (int i = 0; i < mboard_list.size(); i++) {
//        mboard_list[i].outputMotherboardInfo();
//    }
//    std::wcout << std::endl;
//
//    OUTPUT_HEADER("GPUs and Graphics Processors");
//    for (int i = 0; i < gpu_list.size(); i++) {
//        gpu_list[i].outputGPUInfo();
//    }
//    std::wcout << std::endl;
//
//    OUTPUT_HEADER("Processors");
//    for (int i = 0; i < proc_list.size(); i++) {
//        proc_list[i].outProcInfo();
//    }
//    std::wcout << "\n";
//
//    OUTPUT_HEADER("Processor Temp/Load");
//    for (std::size_t i = 0; i < cpu_info->header.required_size; i += sizeof(CPU_DATA)) {
//        std::wcout << L"CPU ID: " << cpu_info->data[i].cpu_id << L"C\n";
//        std::wcout << L"Temp: " << cpu_info->data[i].temp << L"C\n";
//    }
//
//    OUTPUT_HEADER("Storage Devices");
//    for (int i = 0; i < sd_list.size(); i++) {
//        sd_list[i].outSDInfo();
//    }
//    std::wcout << std::endl;
//}