#pragma once
#include "GraphicsProcessor.h"
#include "processor.h"
#include "motherboard.h"
#include "storagedevice.h"
#include "WmiHelper.h"
#include <mutex>
#include <vector>
#include <unordered_map>

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
                VARIANT name, ram, id, avail, refresh, status;
                VariantInit(&name); VariantInit(&ram); VariantInit(&id);
                VariantInit(&avail); VariantInit(&refresh); VariantInit(&status);

                obj->Get(L"Name", 0, &name, nullptr, nullptr);
                obj->Get(L"AdapterRAM", 0, &ram, nullptr, nullptr);
                obj->Get(L"DeviceID", 0, &id, nullptr, nullptr);
                obj->Get(L"Availability", 0, &avail, nullptr, nullptr);
                obj->Get(L"CurrentRefreshRate", 0, &refresh, nullptr, nullptr);
                obj->Get(L"Status", 0, &status, nullptr, nullptr);

                gpu.name = name.bstrVal;
                gpu.adapter_RAM = ram.ulVal;
                gpu.device_id = id.bstrVal;
                gpu.availability = avail.uiVal;
                gpu.curr_ref_rate = refresh.ulVal;
                gpu.status = status.bstrVal;

                VariantClear(&name); VariantClear(&ram); VariantClear(&id);
                VariantClear(&avail); VariantClear(&refresh); VariantClear(&status);
            },
            gpus
        );
    }

    inline void QueryMotherboards(
        IWbemServices* svcs, std::mutex& mtx, 
        std::vector<Motherboard>& boards) 
    {
        WmiHelper::ExecuteWmiQuery<Motherboard>(
            svcs,
            L"SELECT Description, HostingBoard, PoweredOn, Product, Status FROM Win32_BaseBoard",
            [&](IWbemClassObject* obj, Motherboard& board) {
                VARIANT desc, host, power, product, status;
                VariantInit(&desc); VariantInit(&host); VariantInit(&power);
                VariantInit(&product); VariantInit(&status);

                obj->Get(L"Description", 0, &desc, nullptr, nullptr);
                obj->Get(L"HostingBoard", 0, &host, nullptr, nullptr);
                obj->Get(L"PoweredOn", 0, &power, nullptr, nullptr);
                obj->Get(L"Product", 0, &product, nullptr, nullptr);
                obj->Get(L"Status", 0, &status, nullptr, nullptr);

                board.description = desc.bstrVal;
                board.hosting_board = (host.boolVal == VARIANT_TRUE);
                board.powered_on = (power.boolVal == VARIANT_TRUE);
                board.product = product.bstrVal;
                board.status = status.bstrVal;

                VariantClear(&desc); VariantClear(&host); VariantClear(&power);
                VariantClear(&product); VariantClear(&status);
            },
            boards
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
                VARIANT vtProp;

                cpu.unq_id = GetBSTR(L"UniqueId");
                cpu.dev_id = GetBSTR(L"DeviceID");
                cpu.proc_id = GetBSTR(L"ProcessorId");
                cpu.proc_type = static_cast<USHORT>(GetUInt(L"ProcessorType"));
                cpu.family = static_cast<USHORT>(GetUInt(L"Family"));
                cpu.architecture = static_cast<USHORT>(GetUInt(L"Architecture"));
                cpu.manufacturer = GetBSTR(L"Manufacturer");
                cpu.name = GetBSTR(L"Name");
                cpu.num_cores = GetUInt(L"NumberOfCores");
                cpu.num_log_proc = GetUInt(L"NumberOfLogicalProcessors");
                cpu.thread_cnt = GetUInt(L"ThreadCount");
                cpu.curr_clk_spd = GetUInt(L"CurrentClockSpeed");
                cpu.curr_vltg = GetUInt(L"CurrentVoltage");
                cpu.data_width = GetUInt(L"DataWidth");
            },
            cpus
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
                VARIANT unq_id, num, fname, manufacturer, model, size, num_parts;
                VariantInit(&unq_id); VariantInit(&num); VariantInit(&fname);
                VariantInit(&manufacturer); VariantInit(&model); VariantInit(&size); VariantInit(&num_parts);

                //        auto extractIndex = [](const bstr_t& dev_id) {
//            //assert(dev_id);
//            std::wstring ws_dev_id(dev_id);
//            auto pos = ws_dev_id.find_last_of(L"0123456789");
//            return (ULONG)std::stoi(ws_dev_id.substr(pos));
//            };

                obj->Get(L"UniqueId", 0, &unq_id, nullptr, nullptr);
                obj->Get(L"Number", 0, &num, nullptr, nullptr);
                obj->Get(L"FriendlyName", 0, &fname, nullptr, nullptr);
                obj->Get(L"Manufacturer", 0, &manufacturer, nullptr, nullptr);
                obj->Get(L"Model", 0, &model, nullptr, nullptr);
                obj->Get(L"Size", 0, &size, nullptr, nullptr);
                obj->Get(L"NumberOfPartitions", 0, &num_parts, nullptr, nullptr);

                disk.unq_id = unq_id.bstrVal;
                disk.disk_num = num.ulVal;
                disk.fname = fname.bstrVal;
                disk.manufacturer = manufacturer.bstrVal;
                disk.model = model.bstrVal;
                disk.sz = VTConvertNumeric(size);
                disk.num_partitions = num_parts.ulVal;

                VariantClear(&unq_id); VariantClear(&num); VariantClear(&fname);
                VariantClear(&manufacturer); VariantClear(&model); VariantClear(&size); VariantClear(&num_parts);

                disks.insert({ disk.unq_id, disk });
            },
            std::vector<Disk>() 
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
            [&](IWbemClassObject* obj, Partition& p) {
                VARIANT disk_num, part_num, drv_ltr, sz;
                VariantInit(&disk_num); VariantInit(&part_num); VariantInit(&drv_ltr); VariantInit(&sz);

                obj->Get(L"DiskNumber", 0, &disk_num, nullptr, nullptr);
                obj->Get(L"PartitionNumber", 0, &part_num, nullptr, nullptr);
                obj->Get(L"DriveLetter", 0, &drv_ltr, nullptr, nullptr);
                obj->Get(L"Size", 0, &sz, nullptr, nullptr);

                p.id.disk_num = disk_num.ulVal;
                p.id.part_num = part_num.ulVal;
                p.drv_ltr = static_cast<wchar_t>(drv_ltr.uiVal);
                p.sz = VTConvertNumeric(sz);

                parts.insert({ p.id, p });

                VariantClear(&disk_num); VariantClear(&part_num);
                VariantClear(&drv_ltr); VariantClear(&sz);
            },
            std::vector<Partition>()
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
            [&](IWbemClassObject* obj, Volume& v) {
                VARIANT drv, sz, sz_rem, hstat;
                VariantInit(&drv); VariantInit(&sz); VariantInit(&sz_rem); VariantInit(&hstat);

                obj->Get(L"DriveLetter", 0, &drv, nullptr, nullptr);
                obj->Get(L"Size", 0, &sz, nullptr, nullptr);
                obj->Get(L"SizeRemaining", 0, &sz_rem, nullptr, nullptr);
                obj->Get(L"HealthStatus", 0, &hstat, nullptr, nullptr);

                v.drv_ltr = static_cast<wchar_t>(drv.uiVal);
                v.sz = VTConvertNumeric(sz);
                v.sz_rmng = VTConvertNumeric(sz_rem);
                v.hstatus = hstat.uiVal;

                volumes.insert({ v.drv_ltr, v });

                VariantClear(&drv); VariantClear(&sz); VariantClear(&sz_rem); VariantClear(&hstat);
            },
            std::vector<Volume>()
        );
    }

} // namespace HardwareQueries


namespace {
    auto GetBSTR(VARIANT vtProp, IWbemClassObject* obj, const wchar_t* prop) {
        VariantInit(&vtProp);
        obj->Get(prop, 0, &vtProp, nullptr, nullptr);
        std::wstring res = (vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) ? vtProp.bstrVal : L"";
        VariantClear(&vtProp);
        return res;
    };

    auto GetUInt(VARIANT vtProp, IWbemClassObject* obj, const wchar_t* prop) {
        VariantInit(&vtProp);
        obj->Get(prop, 0, &vtProp, nullptr, nullptr);
        auto val = VTConvertNumeric(vtProp);
        VariantClear(&vtProp);
        return val;
    };

} // Private helpers