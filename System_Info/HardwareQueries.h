#pragma once
#include "GraphicsProcessor.h"
#include "processor.h"
#include "motherboard.h"
#include "storagedevice.h" 
#include "projutils.h"
#include "WmiHelper.h"
#include <mutex>
#include <vector>
#include <unordered_map>

#define INITV(x)  VARIANT x; VariantInit(&x)
#define CLEARV(x) VariantClear(&x)

namespace HardwareQueries {
    inline void QueryGPUs(
        IWbemServices* svcs, 
        std::mutex& mtx, 
        std::vector<GraphicsProcessor>& gpus) 
    {
        WmiHelper::ExecuteWmiQuery<GraphicsProcessor>(
            svcs,
            L"SELECT Name, AdapterRAM, DeviceID, Availability, CurrentRefreshRate, Status FROM Win32_VideoController",
            [&](IWbemClassObject* obj, GraphicsProcessor& gpu) {
                INITV(name); INITV(adapter_RAM); INITV(device_ID); 
                INITV(availability); INITV(curr_refresh_rate); INITV(status);

                obj->Get(L"Name", 0, &name, 0, 0);
                obj->Get(L"AdapterRAM", 0, &adapter_RAM, 0, 0);
                obj->Get(L"DeviceID", 0, &device_ID, 0, 0);
                obj->Get(L"Availability", 0, &availability, 0, 0);
                obj->Get(L"CurrentRefreshRate", 0, &curr_refresh_rate, 0, 0);
                obj->Get(L"Status", 0, &status, 0, 0);

                gpu.name = name.bstrVal;
                gpu.adapter_RAM = adapter_RAM.ulVal;
                gpu.device_id = device_ID.bstrVal;
                gpu.availability = availability.uiVal;
                gpu.curr_ref_rate = curr_refresh_rate.ulVal;
                gpu.status = status.bstrVal;

                gpus.push_back(gpu);

                CLEARV(name); CLEARV(adapter_RAM); CLEARV(device_ID); CLEARV(availability);
                CLEARV(curr_refresh_rate); CLEARV(status);
            }
        );
    }

    inline void QueryMotherboards(
        IWbemServices* svcs, 
        std::mutex& mtx, 
        std::vector<Motherboard>& boards) 
    {
        WmiHelper::ExecuteWmiQuery<Motherboard>(
            svcs,
            L"SELECT Description, HostingBoard, PoweredOn, Product, Status FROM Win32_BaseBoard",
            [&](IWbemClassObject* obj, Motherboard& board) {
                INITV(description); INITV(hostingBoard); INITV(poweredOn);
                INITV(product); INITV(status);

                obj->Get(L"Description", 0, &description, 0, 0);
                obj->Get(L"HostingBoard", 0, &hostingBoard, 0, 0);
                obj->Get(L"PoweredOn", 0, &poweredOn, 0, 0);
                obj->Get(L"Product", 0, &product, 0, 0);
                obj->Get(L"Status", 0, &status, 0, 0);
                board.description = description.bstrVal;
                board.hosting_board = (hostingBoard.boolVal == VARIANT_TRUE) ? TRUE : FALSE;
                board.powered_on = poweredOn.boolVal == VARIANT_TRUE;    //board.powered_on = BOOL(poweredOn.pboolVal);
                board.product = product.bstrVal;
                board.status = status.bstrVal;

                boards.push_back(board);

                CLEARV(description); CLEARV(hostingBoard); CLEARV(poweredOn);
                CLEARV(product); CLEARV(status);
            }
        );
    }

    inline void QueryCPUs(
        IWbemServices* svcs, 
        std::mutex& mtx, 
        std::vector<Processor>& cpus) 
    {
        WmiHelper::ExecuteWmiQuery<Processor>(
            svcs,
            L"SELECT UniqueId, DeviceID, ProcessorId, ProcessorType, Family, Architecture, Manufacturer, Name, NumberOfCores, NumberOfLogicalProcessors, ThreadCount, CurrentClockSpeed, CurrentVoltage, DataWidth FROM Win32_Processor",
            [&](IWbemClassObject* obj, Processor& cpu) {
                INITV(unq_id); INITV(dev_id); INITV(proc_id); INITV(proc_type); 
                INITV(family); INITV(architecture); INITV(manufacturer); INITV(name); 
                INITV(num_cores); INITV(num_log_proc); INITV(thread_cnt); INITV(curr_clk_spd); 
                INITV(curr_vltg); INITV(data_width);

                obj->Get(L"UniqueId", 0, &unq_id, 0, 0);
                obj->Get(L"DeviceID", 0, &dev_id, 0, 0);
                obj->Get(L"ProcessorId", 0, &proc_id, 0, 0);
                obj->Get(L"ProcessorType", 0, &proc_type, 0, 0);
                obj->Get(L"Family", 0, &family, 0, 0);
                obj->Get(L"Architecture", 0, &architecture, 0, 0);
                obj->Get(L"Manufacturer", 0, &manufacturer, 0, 0);
                obj->Get(L"Name", 0, &name, 0, 0);
                obj->Get(L"NumberOfCores", 0, &num_cores, 0, 0);
                obj->Get(L"NumberOfLogicalProcessors", 0, &num_log_proc, 0, 0);
                obj->Get(L"ThreadCount", 0, &thread_cnt, 0, 0);
                obj->Get(L"CurrentClockSpeed", 0, &curr_clk_spd, 0, 0);
                obj->Get(L"CurrentVoltage", 0, &curr_vltg, 0, 0);
                obj->Get(L"DataWidth", 0, &data_width, 0, 0);

                cpu.unq_id = (unq_id.vt == VT_BSTR && unq_id.bstrVal) ? unq_id.bstrVal : L"";
                cpu.dev_id = (dev_id.vt == VT_BSTR && dev_id.bstrVal) ? dev_id.bstrVal : L"";
                cpu.proc_id = (proc_id.vt == VT_BSTR && proc_id.bstrVal) ? proc_id.bstrVal : L"";
                cpu.proc_type = static_cast<USHORT>(VTConvertNumeric(proc_type));
                cpu.family = static_cast<USHORT>(VTConvertNumeric(family));
                cpu.architecture = static_cast<USHORT>(VTConvertNumeric(architecture));
                cpu.manufacturer = (manufacturer.vt == VT_BSTR && manufacturer.bstrVal) ? manufacturer.bstrVal : L"";
                cpu.name = (name.vt == VT_BSTR && name.bstrVal) ? name.bstrVal : L"";
                cpu.num_cores = static_cast<ULONG>(VTConvertNumeric(num_cores));
                cpu.num_log_proc = static_cast<ULONG>(VTConvertNumeric(num_log_proc));
                cpu.thread_cnt = static_cast<ULONG>(VTConvertNumeric(thread_cnt));
                cpu.curr_clk_spd = static_cast<ULONG>(VTConvertNumeric(curr_clk_spd));
                cpu.curr_vltg = static_cast<ULONG>(VTConvertNumeric(curr_vltg));
                cpu.data_width = static_cast<ULONG>(VTConvertNumeric(data_width));

                cpus.push_back(cpu);

                CLEARV(unq_id); CLEARV(dev_id); CLEARV(proc_id); CLEARV(proc_type);
                CLEARV(family); CLEARV(architecture); CLEARV(manufacturer); CLEARV(name);
                CLEARV(num_cores); CLEARV(num_log_proc); CLEARV(thread_cnt); CLEARV(curr_clk_spd);
                CLEARV(curr_vltg); CLEARV(data_width);
            }
        );
    }

    inline void QueryPhysicalDisks(
        IWbemServices* svcs,
        std::mutex& mtx,
        std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>& pd_hmap)
    {
        WmiHelper::ExecuteWmiQuery<PhysDisk>(
            svcs,
            L"SELECT DeviceId, SpindleSpeed, UniqueIdFormat FROM MSFT_PhysicalDisk",
            [&]/*<typename KeyType>*/(IWbemClassObject * obj, PhysDisk & phys_disk/*, KeyType key*/) {
                INITV(device_id); INITV(unq_id_frmt); INITV(spindle_speed);

                auto extractIndex = [](const bstr_t& dev_id)
                {
                    std::wstring ws_dev_id(dev_id);
                    auto pos = ws_dev_id.find_last_of(L"0123456789");
                    return (ULONG)std::stoi(ws_dev_id.substr(pos));
                };

                obj->Get(L"SpindleSpeed", 0, &spindle_speed, 0, 0);
                obj->Get(L"DeviceId", 0, &device_id, 0, 0);
                obj->Get(L"UniqueIdFormat", 0, &unq_id_frmt, 0, 0);

                phys_disk.device_id = bstr_t(device_id.bstrVal);
                phys_disk.spindle_speed = spindle_speed.ullVal;
                phys_disk.unq_id_frmt = unq_id_frmt.uiVal;
                phys_disk.disk_num = extractIndex(phys_disk.device_id);

                pd_hmap.insert({ phys_disk.disk_num, phys_disk });

                CLEARV(device_id); CLEARV(unq_id_frmt); CLEARV(spindle_speed);
            }
        );
    }

    // Storage queries
    inline void QueryDisks(
        IWbemServices* svcs, 
        std::mutex& mtx,
        std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>& disks)
    {
        WmiHelper::ExecuteWmiQuery<Disk>(
            svcs,
            L"SELECT UniqueId, Number, FriendlyName, Manufacturer, Model, Size, NumberOfPartitions FROM MSFT_Disk",
            [&](IWbemClassObject* obj, Disk& disk) {
                INITV(unq_id); INITV(num); INITV(fname); INITV(manufacturer);
                INITV(model); INITV(d_sz); INITV(num_partitions);

                obj->Get(L"UniqueId", 0, &unq_id, 0, 0);
                obj->Get(L"Number", 0, &num, 0, 0);
                obj->Get(L"FriendlyName", 0, &fname, 0, 0);
                obj->Get(L"Manufacturer", 0, &manufacturer, 0, 0);
                obj->Get(L"Model", 0, &model, 0, 0);
                obj->Get(L"Size", 0, &d_sz, 0, 0);
                obj->Get(L"NumberOfPartitions", 0, &num_partitions, 0, 0);

                disk.unq_id = bstr_t(unq_id.bstrVal);
                disk.manufacturer = bstr_t(manufacturer.bstrVal);
                disk.model = bstr_t(model.bstrVal);
                disk.fname = bstr_t(fname.bstrVal);
                disk.num_partitions = num_partitions.ulVal;
                disk.disk_num = num.ulVal;
                disk.sz = VTConvertNumeric(d_sz);

                disks.insert({ disk.unq_id, disk });

                CLEARV(unq_id); CLEARV(num); CLEARV(fname); CLEARV(manufacturer);
                CLEARV(model); CLEARV(d_sz); CLEARV(num_partitions);
            }
        );
    }

    inline void QueryPartitions(
        IWbemServices* svcs, 
        std::mutex& mtx,
        std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash>& parts)
    {
        WmiHelper::ExecuteWmiQuery<Partition>(
            svcs,
            L"SELECT DiskNumber, PartitionNumber, DriveLetter, Size FROM MSFT_Partition",
            [&](IWbemClassObject* obj, Partition& partition) {
                INITV(disk_num); INITV(part_num); INITV(drv_ltr); INITV(p_sz);

                obj->Get(L"DiskNumber", 0, &disk_num, 0, 0);
                obj->Get(L"PartitionNumber", 0, &part_num, 0, 0);
                obj->Get(L"DriveLetter", 0, &drv_ltr, 0, 0);
                obj->Get(L"Size", 0, &p_sz, 0, 0);

                partition.id.disk_num = disk_num.ulVal;
                partition.id.part_num = part_num.ulVal;
                partition.drv_ltr = static_cast<wchar_t>(drv_ltr.uiVal);
                partition.sz = VTConvertNumeric(p_sz);

                parts.insert({partition.id, partition});

                CLEARV(disk_num); CLEARV(part_num); CLEARV(drv_ltr); CLEARV(p_sz);
            }
        );
    }

    inline void QueryVolumes(
        IWbemServices* svcs, 
        std::mutex& mtx,
        std::unordered_map<wchar_t, Volume>& volumes)
    {
        WmiHelper::ExecuteWmiQuery<Volume>(
            svcs,
            L"SELECT DriveLetter, Size, SizeRemaining, HealthStatus FROM MSFT_Volume",
            [&](IWbemClassObject* obj, Volume& vol) {
                INITV(drv_ltr); INITV(sz); INITV(sz_rmng); INITV(hstatus);

                obj->Get(L"DriveLetter", 0, &drv_ltr, 0, 0);
                obj->Get(L"Size", 0, &sz, 0, 0);
                obj->Get(L"SizeRemaining", 0, &sz_rmng, 0, 0);
                obj->Get(L"HealthStatus", 0, &hstatus, 0, 0);

                vol.drv_ltr = static_cast<wchar_t>(drv_ltr.uiVal);
                vol.sz = VTConvertNumeric(sz);
                vol.sz_rmng = VTConvertNumeric(sz_rmng);
                vol.hstatus = hstatus.uiVal;

                volumes.insert({ vol.drv_ltr, vol });

                CLEARV(drv_ltr); CLEARV(sz); CLEARV(sz_rmng); CLEARV(hstatus);
            }
        );
    }

} // namespace HardwareQueries