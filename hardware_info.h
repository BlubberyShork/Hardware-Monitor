#ifndef HARDWARE_INFO_H
#define HARDWARE_INFO_H

#include "GraphicsProcessor.h"
#include "motherboard.h"
#include "storagedevice.h"
#include "processor.h"
#include "projutils.h"

#include <iostream>
#include <comdef.h>
#include <thread>
#include <wbemidl.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

// Function declarations
void InitializeCOM();
void setupWBEM(IWbemLocator*& loc, IWbemServices*& w_svcs, IWbemServices*& m_svcs);

void infoGPU(IWbemLocator*& loc, IWbemServices*& svcs,
	std::mutex& mtx, std::vector<GraphicsProcessor>& gpu_list); 
void infoMotherboard(IWbemLocator*& loc, IWbemServices*& svcs,
	std::mutex& mtx, std::vector<Motherboard>& mboard_list);
void infoCPU(IWbemLocator*& loc, IWbemServices*& svcs,
	std::mutex& mtx, std::vector<Processor>& cpu_list);

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
