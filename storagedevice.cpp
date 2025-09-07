#include "storagedevice.h"

void StorageDevice::outSDInfo()
{
    try
    {
        // --- Disk Info ---
        std::wcout << L"--- Disk Info ---\n";
        std::wcout << L"Disk Number: " << disk.disk_num << L"\n";
        std::wcout << L"Friendly Name: "
            << (disk.fname.length() ? disk.fname : L"<empty>") << L"\n";
        std::wcout << L"Manufacturer: "
            << (disk.manufacturer.length() ? disk.manufacturer : L"<empty>") << L"\n";
        std::wcout << L"Model: "
            << (disk.model.length() ? disk.model : L"<empty>") << L"\n";
        std::wcout << L"Size: " << simplifyBytesAsString(disk.sz) << L" \n";

        ULONG ss = physical_disk.spindle_speed;
        if (ss > 0 && ss < MAXULONG32) {
            std::wcout << L"HDD, Spindle Speed: " << ss << " RPM" << L"\n\n";
        }
        else if (ss == MAXULONG32) {
            std::wcout << L"HDD, Spindle Speed: Unknown" << L"\n\n";
        }
        else {
            std::wcout << L"SSD Drive" << L"\n\n";
        }

        // --- Partitions Info ---
        std::wcout << L"\t--- Partitions ---\n";
        if (partitions.empty())
        {
            std::wcout << L"\tNo partitions found.\n";
        }
        else
        {
            for (const auto& part : partitions)
            {
                std::wcout << L"\tPartition " << part.id.part_num
                    << L" on Disk " << part.id.disk_num << L"\n";
                std::wcout << L"\tDrive Letter: "
                    << (part.drv_ltr ? part.drv_ltr : L'<') << L":" << L"\n";
                std::wcout << L"\tSize: " << simplifyBytesAsString(part.sz) << L"\n";
            }
        }
        std::wcout << L"\n";

        // --- Volumes Info ---
        std::wcout << L"\t\t--- Volumes ---\n";
        if (volumes.empty())
        {
            std::wcout << L"\t\tNo volumes found.\n";
        }
        else
        {
            for (const auto& vol : volumes)
            {
                std::wcout << L"\t\tDrive Letter: "
                    << (vol.drv_ltr ? vol.drv_ltr : L'<') << L":" L"\n";
                std::wcout << L"\t\tSize: " << simplifyBytesAsString(vol.sz) << L"\n";
                std::wcout << L"\t\tRemaining Size: " << simplifyBytesAsString(vol.sz_rmng) << L"\n\n";
            }
        }
        std::wcout << L"\n";

    }
    catch (const _com_error& e)
    {
        std::wcerr << L"COM Error: " << e.ErrorMessage() << L"\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Standard exception: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred while outputting storage device info.\n";
    }
}