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

// Function declarations
void InitializeCOM();
void setupWBEM(IWbemLocator*& loc);
void infoGPU(IWbemLocator*& loc, std::vector<GraphicsProcessor>& gpu_list);
void infoMotherboard(IWbemLocator*& loc, std::vector<Motherboard>& mboard_list);
void infoCPU(IWbemLocator*& loc, std::vector<Processor>& proc_list);
void infoPhysicalDrive(IWbemLocator*& loc, std::vector<StorageDevice>& sd_list);

#endif // HARDWARE_INFO_H
#pragma once
