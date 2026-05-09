# Windows Hardware Monitor

> **Proof-of-concept only.** This kernel-mode driver has not been production-signed. Only run it on a dedicated test machine or VM.

---

## Overview

Windows 10/11 hardware monitoring tool that combines a **kernel-mode driver** (KMDF/C) with a **user-space application** (C++/WMI) to expose hardware specifications that standard user-space APIs can not reach.

The driver currently supports AMD Zen architecture processors and Intel Core processors with Digital Thermal Sensor(DTS).

WIP: 
1. More reliable CPU core temperature and CPU core load readings (For processors without DTS capability).
2. GPU clock speed, utilization, and temperature readings.
---

## Architecture

- **Kernel-mode driver**: Written in C using the Kernel-Mode Driver Framework (KMDF). Handles low-level hardware access that requires elevated privilege.
- **User-space app**: Written in C++. Queries hardware via Windows WMI/WBEM and communicates with the driver through IOCTLs. Compiles to an executable.

---

## Prerequisites

- Windows 10/11 — dedicated test machine or VM
- Visual Studio 2022 with the **Desktop development with C++** workload
- **Windows Driver Kit (WDK)** 
- Test signing enabled on the target machine:
  ```cmd
  bcdedit /set testsigning on
  ```

---

## Building & Running

1. Clone the repo and open `CPUMonitorDriver.sln` in Visual Studio 2022.
2. Set configuration to `x64`-> `Debug` or `Release`, then build the solution.
3. Deploy the driver to your test machine using `pnputil`, `sc.exe`, `devcon`, or Visual Studio's built-in remote deployment. The driver must be installed and running before the next step. I personally used `sc.exe` during development and testing.
4. Navigate to the build output folder (`x64/Debug` or `x64/Release`) and run:
   ```cmd
   HardwareMonitor.exe
   ```

To uninstall/delete the driver, use `pnputil`, `sc.exe`, or `devcon`.