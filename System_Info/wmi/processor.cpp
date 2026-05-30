#include "processor.h"

void Processor::outProcInfo() {
    std::wcout << L"Processor Info:\n";
    std::wcout << L"  Unique ID: " << unq_id << L"\n";
    std::wcout << L"  Device ID: " << dev_id << L"\n";
    std::wcout << L"  Processor ID: " << proc_id << L"\n";
    std::wcout << L"  Processor Type: " << proc_type << L"\n";
    std::wcout << L"  Family: " << family << L"\n";
    std::wcout << L"  Architecture: " << architecture << L"\n";
    std::wcout << L"  Manufacturer: " << manufacturer << L"\n";
    std::wcout << L"  Name: " << name << L"\n";
    std::wcout << L"  Number of Cores: " << num_cores << L"\n";
    std::wcout << L"  Number of Logical Processors: " << num_log_proc << L"\n";
    std::wcout << L"  Thread Count: " << thread_cnt << L"\n";
    std::wcout << L"  Current Clock Speed: " << curr_clk_spd << L" MHz\n";
    std::wcout << L"  Current Voltage: " << curr_vltg << L" V\n";
    std::wcout << L"  Data Width: " << data_width << L" bits\n";
    std::wcout << L"\n";
}