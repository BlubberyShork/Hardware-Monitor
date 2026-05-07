#include "OutputHandler.h"

void OutputHandler::output() {
	OUTPUT_HEADER("Motherboard")
	for (auto& motherboard : hw_data.motherboards) {
		motherboard.outputMotherboardInfo();
	}
	std::wcout << "\n";

	OUTPUT_HEADER("GPU")
	for (auto& gpu : hw_data.gpus) {
		gpu.outputGPUInfo();
	}
	std::wcout << "\n";

	OUTPUT_HEADER("Active GPU Hardware Details")
	live_gpu_handler.outputLiveGPUMetrics();

	OUTPUT_HEADER("Processors")
	for (auto& cpu : hw_data.cpus) {
		cpu.outProcInfo();
	}
	std::wcout << "\n";

	OUTPUT_HEADER("Active CPU Hardware Details")
	dc.printDriverOutput();
	std::wcout << "\n";

	OUTPUT_HEADER("Storage Devices")
	for (auto& storage_dev : hw_data.storage_dvcs) {
		storage_dev.outSDInfo();
	}
	std::wcout << "\n";
}

void OutputHandler::outputNoDriver() {
	OUTPUT_HEADER("Motherboard")
		for (auto& motherboard : hw_data.motherboards)
			motherboard.outputMotherboardInfo();
	std::wcout << "\n";

	OUTPUT_HEADER("GPU")
		for (auto& gpu : hw_data.gpus)
			gpu.outputGPUInfo();
	std::wcout << "\n";

	OUTPUT_HEADER("Active GPU Hardware Details")
		live_gpu_handler.outputLiveGPUMetrics();
	std::wcout << "\n";

	OUTPUT_HEADER("Processors")
		for (auto& cpu : hw_data.cpus)
			cpu.outProcInfo();
	std::wcout << "\n";

	OUTPUT_HEADER("Storage Devices")
		for (auto& storage_dev : hw_data.storage_dvcs)
			storage_dev.outSDInfo();
	std::wcout << "\n";
}