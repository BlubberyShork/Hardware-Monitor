#ifndef HARDWARE_INFO_H
#define HARDWARE_INFO_H

#include "GraphicsProcessor.h"
#include "motherboard.h"
#include "storagedevice.h"
#include "processor.h"
#include "projutils.h"

#include <windows.h>
#include <iostream>
#include <comdef.h>
#include <thread>
#include <wbemidl.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <intrin.h>

// TODO - Should break this into HardwareQueries.h, ComWbem.h, and StorageQueries.h

void InitializeCOM();
void setupW32Wbem(IWbemLocator*& loc, IWbemServices*& svcs);
void setupMSFTWbem(IWbemLocator*& loc, IWbemServices*& svcs);

void infoGPU(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx, 
	std::vector<GraphicsProcessor>& gpu_list); 
//<nvapi.h> to grab load for gpu

void infoMotherboard(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx, 
	std::vector<Motherboard>& mboard_list);

void infoCPU(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx, 
	std::vector<Processor>& cpu_list);

/* Storage Device */
void sd_DiskQuery(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx,
	std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>& d_hmap);

void sd_PartitionQuery(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx,
	std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash>& p_hmap);

void sd_VolumeQuery(IWbemLocator*& loc, IWbemServices*& svcs, std::mutex& mtx, 
	std::unordered_map<wchar_t, Volume>& v_hmap);

void sd_PhysicalDiskQuery(IWbemLocator*&, IWbemServices*& svcs, std::mutex& mtx,
	std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>& pd_hmap);

void infoPhysicalDrive(std::vector<StorageDevice>& sd_list,
	std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>& d_hmap,
	std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash>& p_hmap,
	std::unordered_map<wchar_t, Volume>& v_hmap,
	std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>& pd_hmap);

#endif // HARDWARE_INFO_H
#pragma once
