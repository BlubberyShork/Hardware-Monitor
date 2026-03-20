#include "HardwareManager.h"

void HardwareManager::ExecuteQueryThreadPool() {
    std::vector<std::function<void()>> queries = {
        [&] { HardwareQueries::QueryGPUs(wbem_mngr->getW32Services(), mtx, hw_data.gpus); },
        [&] { HardwareQueries::QueryCPUs(wbem_mngr->getW32Services(), mtx, hw_data.cpus); },
        [&] { HardwareQueries::QueryMotherboards(wbem_mngr->getW32Services(), mtx, hw_data.motherboards); },
        [&] { HardwareQueries::QueryDisks(wbem_mngr->getMsftServices(), mtx, disks); },
        [&] { HardwareQueries::QueryPartitions(wbem_mngr->getMsftServices(), mtx, partitions); },
        [&] { HardwareQueries::QueryVolumes(wbem_mngr->getMsftServices(), mtx, volumes); },
        [&] { HardwareQueries::QueryPhysicalDisks(wbem_mngr->getMsftServices(), mtx, phys_disks); }
    };

    thrd_mngr.ExecuteThreadPool(queries);

    infoPhysicalDrive(hw_data.storage_dvcs, disks, partitions, volumes, phys_disks);
}

void HardwareManager::infoPhysicalDrive(
    std::vector<StorageDevice>& sd_list,
    std::unordered_map<bstr_t, Disk, bstrHash, bstrEqual>& d_hmap,
    std::unordered_map<Partition::partition_id, Partition, Partition::pid_hash>& p_hmap,
    std::unordered_map<wchar_t, Volume>& v_hmap,
    std::unordered_map<ULONG, PhysDisk, ULONGHash, ULONGEqual>& pd_hmap
) {
    // Build sd_list from the collected data
    for (const auto& disk_pair : d_hmap) {  // O(d) time, d = num_disks
        StorageDevice sd;
        bstr_t d_unq_id = disk_pair.first;
        Disk disk = disk_pair.second;
        ULONG d_disk_num = disk.disk_num;
        sd.setDisk(disk);

        // Add partitions
        for (size_t i = 0; i < disk.num_partitions; ++i) {
            ULONG part_num = static_cast<ULONG>(i);
            Partition::partition_id pid = { d_disk_num, part_num };
            if (p_hmap.find(pid) != p_hmap.end()) {
                sd.getPartitions().push_back(p_hmap[pid]);
                Partition p = p_hmap[pid];
                // Add volume if it exists
                if (p.drv_ltr != 0 && v_hmap.find(p.drv_ltr) != v_hmap.end()) {
                    sd.getVolumes().push_back(v_hmap[p.drv_ltr]);
                }
            }
        }

        // Add physical disk
        if (pd_hmap.find(d_disk_num) != pd_hmap.end()) {
            sd.setPhysicalDisk(pd_hmap[d_disk_num]);
        }

        sd_list.push_back(sd);
    }
}